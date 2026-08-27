# Aurora Sofle v2 — TPS65 touchpad fixes

> **Status: proposed, nothing implemented.** Review written 2026-07-13 against commit `c0652b0`.
> No code or config in this repo has been changed. Read the *Confidence* section before acting —
> the items are not equally certain, and the suggested commit split at the bottom exists so a
> regression is easy to bisect.

## Context

The touchpad on the right (peripheral) half works, but has three long-standing annoyances:

1. **A gesture (usually tap) silently stops working** until the right half is power-cycled.
2. **Movement glitches while charging over USB-C** with the keyboard powered on.
3. **Tracking is less smooth** than a dedicated mouse, and the pointer feels like it under-travels.

All three trace to specific defects, not to tuning. The git history shows seven commits bouncing
`poll-interval-ms` between 10 and 20 trying to fix (3) empirically; this plan fixes the underlying
cause instead.

Power/battery config (`CONFIG_ZMK_SLEEP=n`, `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=n`) is deliberately
**left untouched** per the user's decision. The one RGB change below is opt-in and trivially
revertible.

Hardware constraint: the PCB has no spare pin, so the IQS5xx RDY line cannot be wired. Polling mode
is permanent for this board. Everything here works within that constraint.

---

## Root causes

### A. Chip resets wipe the config, and the driver never restores it  → symptom (1)

`drivers/input/iqs5xx.c:117-121`:

```c
if (sys_info_0 & IQS5XX_SHOW_RESET) {
    iqs5xx_write_reg8(dev, IQS5XX_SYSTEM_CONTROL_0, IQS5XX_ACK_RESET);
    goto end_comm;          // acks the reset, then does nothing else
}
```

A reset returns every config register to factory defaults — and the factory default for the
gesture-enable registers (`SINGLE_FINGER_GESTURES_CONF` 0x06B7, `MULTI_FINGER_GESTURES_CONF` 0x06B8)
is **all gestures disabled**. Raw relative X/Y needs no configuration, so the cursor keeps moving
while taps/scroll die — exactly the reported symptom. `iqs5xx_setup_device()` runs only at boot, so a
power cycle is the only recovery. The chip's watchdog is enabled (`IQS5XX_WDT`,
`drivers/input/iqs5xx.c:330`), so any brownout or WDT bite triggers this.

### B. Auto-ATI (re-tuning) is disabled  → symptom (2)

`drivers/input/iqs5xx.c:330` writes `SYSTEM_CONFIG_0 = SETUP_COMPLETE | WDT`, leaving the `REATI`
(BIT2) and `ALP_REATI` (BIT3) bits clear. Those bits — already defined and unused in
`drivers/input/iqs5xx.h:29-30` — tell the chip to re-run its Auto-Tuning when sensing conditions
change. Plugging in a USB charger shifts the ground reference of a battery-powered device and injects
switching noise into the capacitive electrodes; without auto re-ATI the chip has no way to recover
and produces garbage until it is power-cycled. The `ATI_ERROR` / `REATI_OCCURRED` status flags in
`SYSTEM_INFO_0` are likewise defined but never checked.

### C. Poll rate and chip report rate are aliasing  → symptom (3)

The IQS5xx runs its own sensing cycle at the **Active Report Rate** register (`0x057A`, 16-bit ms,
factory default 10 ms). The driver never writes this register. Each cycle the chip **overwrites**
`REL_X`/`REL_Y` with that cycle's delta — deltas do not accumulate between reads. Polling at 16 ms
(`config/splitkb_aurora_sofle_right.overlay:33`) therefore reads only the most recent ~10 ms slice
and discards the motion in between: roughly a third of finger travel never reaches the host, and
*which* third varies with the beat frequency between the two clocks. That is both the under-travel
and the micro-jitter.

### D. I2C is slower than it needs to be

Each poll issues 6–8 separate `i2c_write_read_dt` transactions on a bus left at the 100 kHz Zephyr
default. Registers `0x000D`–`0x0015` are contiguous (gestures 0/1, sysinfo 0/1, num_fingers, rel X,
rel Y = 9 bytes) and can be fetched in **one** burst read. One of the current transactions reads
`num_fingers` (`drivers/input/iqs5xx.c:199`) and never uses the value. Beyond the latency, the
multi-transaction read is not atomic — sysinfo and rel X/Y can straddle a chip report cycle, which is
its own source of jitter.

---

## Changes

### 1. Restore config after a chip reset — `drivers/input/iqs5xx.c`

In the `SHOW_RESET` branch, after acking, re-run the full setup instead of bailing. Reuse the existing
deferred-setup machinery rather than calling `iqs5xx_setup_device()` inline (the chip needs a moment
post-reset, and the retry loop is already written):

- ack the reset, set `data->initialized = false`,
- `k_work_schedule(&data->deferred_setup_work, K_MSEC(50))`,
- `goto end_comm`.

`iqs5xx_deferred_setup_handler()` (`drivers/input/iqs5xx.c:233`) already retries every 500 ms until
the device responds and then sets `initialized`. It also restarts the poll timer — make that
`k_timer_start` call idempotent (starting an already-running timer just re-arms it, which is fine, but
confirm intent) or guard it on `!data->initialized`.

Also add an early `if (!data->initialized) return;` guard at the top of `iqs5xx_work_handler()` so
polls during re-setup don't fight the setup writes on the bus.

### 2. Enable auto re-ATI — `drivers/input/iqs5xx.c`

Change the `SYSTEM_CONFIG_0` write (`drivers/input/iqs5xx.c:330`) to include the re-ATI bits:

```c
IQS5XX_SETUP_COMPLETE | IQS5XX_WDT | IQS5XX_REATI | IQS5XX_ALP_REATI
```

Both constants already exist in `drivers/input/iqs5xx.h`. Additionally, in the work handler, if
`sys_info_0 & IQS5XX_ATI_ERROR` is set, trigger a manual re-tune by writing `IQS5XX_AUTO_ATI` to
`SYSTEM_CONTROL_0` (0x0431) — constant already defined at `drivers/input/iqs5xx.h:20`. Log at
`LOG_WRN` so a future USB-charging glitch is diagnosable from a log build rather than by guesswork.

### 3. Sync the chip's report rate to the poll interval — `drivers/input/iqs5xx.c`, `.h`, binding

Add `#define IQS5XX_ACTIVE_REPORT_RATE 0x057A` to `drivers/input/iqs5xx.h` (verified against the
Azoteq register map as used by QMK's `drivers/sensors/azoteq_iqs5xx.c`; 16-bit, milliseconds).

In `iqs5xx_setup_device()`, write `iqs5xx_write_reg16(dev, IQS5XX_ACTIVE_REPORT_RATE, <rate>)`. Default
`<rate>` to `config->poll_interval_ms` so the two clocks are matched by construction and can never
drift apart again. Optionally expose a `report-rate-ms` property in
`dts/bindings/input/azoteq,iqs5xx-common.yaml` for independent tuning, defaulting to the poll interval.

Then set `poll-interval-ms = <10>` in `config/splitkb_aurora_sofle_right.overlay` (100 Hz — matches the
chip's native default and is comfortably within the BLE split link's 7.5 ms connection interval).

### 4. Single burst read — `drivers/input/iqs5xx.c`

Replace the per-register reads at the top of `iqs5xx_work_handler()` with one 9-byte
`i2c_write_read_dt` from `0x000D`, then unpack:

| offset | register            |
|-------:|---------------------|
| 0      | `GESTURE_EVENTS_0`  |
| 1      | `GESTURE_EVENTS_1`  |
| 2      | `SYSTEM_INFO_0`     |
| 3      | `SYSTEM_INFO_1`     |
| 4      | `NUM_FINGERS`       |
| 5–6    | `REL_X` (big-endian)|
| 7–8    | `REL_Y` (big-endian)|

This drops 6–8 transactions to 1, makes the snapshot atomic, and removes the dead `num_fingers` read
as a side effect. Keep the existing "NACK is normal in polling mode, bail silently" handling
(`drivers/input/iqs5xx.c:89-97`) — it applies to the burst read unchanged.

### 5. Raise the I2C bus to 400 kHz — `config/splitkb_aurora_sofle_right.overlay`

Add `clock-frequency = <I2C_BITRATE_FAST>;` to the `&pro_micro_i2c` node (with
`#include <dt-bindings/i2c/i2c.h>`). The IQS5xx supports 400 kHz. Confirm the resulting value in the
generated `zephyr.dts` after building. The OLED is already disabled on this half, so nothing else
shares the bus.

### 6. Pointer sensitivity and scroll feel — `config/splitkb_aurora_sofle_left.overlay`, `.conf`

The input listener (`config/splitkb_aurora_sofle_left.overlay:13`) has **no** `input-processors`, so
raw sensor counts go straight to HID with no sensitivity control anywhere in the config. Add:

```dts
#include <input/processors.dtsi>

&tps43_listener {
    input-processors = <&zip_xy_scaler 3 2>;   // starting point; tune the 3/2 ratio to taste
};
```

Enable `CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y` in `config/splitkb_aurora_sofle.conf` (HID resolution
multiplier) to soften the driver's coarse hardcoded `scroll_div = 32`
(`drivers/input/iqs5xx.c:172`).

Re-tune `bottom-beta` afterwards. It is currently `<2>`
(`config/splitkb_aurora_sofle_right.overlay:54`), and per the binding docs **0 = most filtering /
laggiest**, so 2 is near-maximum smoothing — it was almost certainly lowered to mask the aliasing from
(C). With the rates matched, walk it back up toward the default of 5 (or higher) for a crisper feel.

### 7. Config hygiene — `config/splitkb_aurora_sofle.conf`, `config/splitkb_aurora_sofle_right.conf`

- **Drop `CONFIG_ZMK_MOUSE=y`.** The symbol no longer exists in ZMK; it was replaced by
  `CONFIG_ZMK_POINTING` (already set). It is a dead symbol today.
- **`CONFIG_ZMK_USB=n` on the right half.** It is currently set in the *shared* conf, so the
  peripheral runs a full USB stack it has no use for — a split peripheral never sends HID reports.
  Turning it off means less activity and noise on the board precisely when USB-C is plugged in, which
  is when the glitching happens. **Charging is unaffected** (the nice!nano's LiPo charger is pure
  hardware) and so is UF2 flashing (the bootloader is separate firmware).
- **Resolve the TX-power conflict.** `CONFIG_BT_CTLR_TX_PWR_0=y` (shared conf, line 40) and
  `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y` (right conf, line 14) are members of the same Kconfig `choice`, so
  one silently wins and it is not obvious which. Remove `CONFIG_BT_CTLR_TX_PWR_0` from the shared conf
  and set `PLUS_8` explicitly on both halves — a stronger split link means fewer retransmits on the
  path the touchpad data actually travels.

### 8. Underglow (optional, one line, easy revert) — `config/splitkb_aurora_sofle.keymap`

`chain-length = <35>` (line 16, "29 per-key + 6 underglow") means ZMK drives the 6 underglow LEDs the
case hides. If the underglow sits at the **tail** of the data chain, setting `chain-length = <29>`
leaves them unaddressed and dark — a firmware substitute for the JP2 jumper that won't cooperate.

This is an experiment: if the underglow is physically **first** in the chain instead, you'll see 6
underglow + 23 per-key LEDs lit, which tells you the chain order and the change should simply be
reverted. No risk beyond one reflash either way.

---

## Confidence

**High — verified directly in this repo or against upstream source.**

- *A (reset wipes config).* The `SHOW_RESET` branch acks and `goto end_comm`, and `iqs5xx_setup_device()`
  is only ever called from init — that much is certain from the code. The inference is that this is the
  cause of the dying-gesture symptom; it rests on the chip's factory default for the gesture registers
  being "all off", which is confident but not datasheet-confirmed. The symptom match is strong.
- *D (burst read, 400 kHz).* Pure mechanics. Registers are contiguous; the chip supports 400 kHz.
  No behavioural risk.
- *Config hygiene.* `CONFIG_ZMK_MOUSE` is confirmed gone from ZMK's Kconfig; the two TX-power symbols
  are confirmed members of the same `choice`.
- *No sensitivity control exists.* Fact — the listener has no `input-processors`.

**Medium — sound reasoning, not verified on hardware.**

- *C (report-rate aliasing).* Register `0x057A` cross-checked against QMK's `azoteq_iqs5xx.c`, and the
  driver demonstrably never writes it. The unverified step is that relative X/Y are per-cycle deltas that
  get **overwritten** rather than accumulated. If that's wrong, matching the rates is harmless but won't
  help. The seven poll-interval commits are circumstantial evidence it's right.
- *`CONFIG_ZMK_USB=n` on the peripheral.* Safe (charging is hardware, bootloader is separate firmware),
  but whether it materially helps the glitching is a guess.

**Lower — plausible, may not pan out.**

- *Auto re-ATI fixing the charging glitch.* The bits are definitely unset and re-ATI is the chip's
  designed remedy for this class of problem, but it's unproven that charger noise is the trigger. The
  reset-recovery fix (A) may resolve this symptom on its own, since a brownout while charging produces
  the same "broken until reboot" behaviour.
- *`chain-length = <29>` killing the underglow.* Coin flip — depends on whether the underglow is at the
  head or tail of the data chain, undeterminable remotely. One reflash answers it.

**Main risk.** The driver changes are C, on a chip that can't be observed from here. A bug in the
reset-recovery path could produce a touchpad that re-inits in a loop. Do that as its own commit and live
with it for a day before layering the rest on top.

## Verification

Build both halves via the existing GitHub Actions workflow (`.github/workflows/build.yml`,
`board: nice_nano//zmk`), or locally with `west build`. Then flash both halves and check, in order:

1. **Build sanity** — no Kconfig warnings about undefined symbols (confirms the `ZMK_MOUSE` removal and
   the TX-power choice are clean). Grep the generated `zephyr.dts` for `clock-frequency` on the I2C node
   to confirm 400 kHz took effect.
2. **Smoothness (C, D)** — move the cursor slowly in a straight line. Under-travel and micro-stutter
   should be gone; a slow drag should track your finger 1:1. This is the change you'll feel immediately.
3. **Reset recovery (A)** — the direct test: with the keyboard running, reset *only the touchpad chip*
   (or simply use it until a glitch occurs). Previously a gesture would die permanently; now taps and
   scroll must keep working without a power cycle. A log build (`CONFIG_ZMK_USB_LOGGING=y` on the right
   half, temporarily) will show `Device reset detected` followed by the deferred setup succeeding.
4. **USB charging (B)** — the acceptance test for the whole exercise: plug USB-C into the right half
   while the keyboard is on and in use, and confirm the cursor stays sane. Try it a few times, and
   while RGB is on (current spikes are a plausible brownout trigger for the resets in A).
5. **Sensitivity** — adjust the `&zip_xy_scaler 3 2` ratio until the pointer speed feels right, then
   re-tune `bottom-beta` upward from 2 for crispness.

Suggested commit split, so a regression is easy to bisect: (1) driver reset-recovery + re-ATI,
(2) driver report-rate sync + burst read, (3) config/DT hygiene + processors, (4) the underglow
one-liner.
