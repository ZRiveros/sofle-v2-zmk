# Aurora Sofle v2 — ZMK config (Swedish layout + TPS65 touchpad)

ZMK firmware for a [splitkb Aurora Sofle v2](https://splitkb.com/products/aurora-sofle)
wireless split keyboard:

- **Controllers:** nice!nano v2 (nRF52840), one per half
- **Left half:** central — talks to the host over BLE/USB, has the OLED
  (custom status screen: both halves' battery levels, output, active layer)
- **Right half:** peripheral — carries an **Azoteq TPS65 (IQS5xx) touchpad**
  on I2C (polling mode, no RDY pin — the PCB has no spare pin). Its OLED is
  disabled to free the I2C bus.
- **Layout:** Swedish (`config/keys_sv.h`) with US-style shifted symbols via
  mod-morphs; 2 layers (Base, Raise)
- **Lighting:** per-key RGB (underglow disabled via chain-length)
- **Touchpad driver:** vendored in-tree under `drivers/input/` (fork of
  [AYM1607/zmk-driver-azoteq-iqs5xx](https://github.com/AYM1607/zmk-driver-azoteq-iqs5xx)
  with polling-mode support and local fixes). Design notes and root-cause
  analysis: [TOUCHPAD_REVIEW.md](TOUCHPAD_REVIEW.md).

![Keymap](keymap-drawer/splitkb_aurora_sofle.svg)

## Building

Firmware builds on GitHub Actions on every push to `master` (or manually via
*Actions → Build ZMK Firmware → Run workflow*). Each successful `master` build
publishes a dated release with three files:

| File | Flash to |
|---|---|
| `splitkb_aurora_sofle_left-nice_nano.uf2` | left half |
| `splitkb_aurora_sofle_right-nice_nano.uf2` | right half |
| `settings_reset-nice_nano.uf2` | both halves, only when re-pairing (see below) |

ZMK is **pinned** to a known-good revision in `config/west.yml`. Bump the pin
deliberately, as its own commit, and test before relying on it.

## Flashing

1. **Switch the other half OFF** before flashing — and note USB-C also powers
   a half even when its switch is off.
2. Double-tap the reset button to enter the bootloader; the board mounts as a
   `NICENANO` USB drive.
3. Copy the matching `.uf2` onto the drive. It reboots itself when done.

## When the halves won't connect to each other (re-pairing the split)

The left↔right pairing is separate from the host pairing — `BT_CLR` does
**not** fix it. Full reset procedure:

1. Charge **both** halves via USB-C for at least an hour first. Deep sleep is
   deliberately disabled in this config, so an idle half still drains its
   battery in days — a flat right half looks exactly like "won't reconnect".
2. Flash `settings_reset-nice_nano.uf2` to **both** halves (one at a time,
   other half off).
3. Flash the real left/right firmware back onto each half.
4. Power both halves on at (roughly) the same time and let them bond.
5. Remove any old "Aurora Sofle" entry from the host's Bluetooth settings,
   then pair again from the host.

## Host (BLE profile) controls — Raise layer, left half

Hold the Raise thumb key (`MO(1)`, third left thumb key):

- `ESC` position → `BT_CLR` — clears the **currently selected host profile
  only** (not the split pairing, not other profiles)
- Number row `1..5` → select host profile 0–4
- Bottom row also has `RGB_TOG`, `EP_TOG` (ext-power toggle), `RGB_BRI`/`RGB_BRD`

## Touchpad

Sensitivity and feel, in tuning order:

1. `&zip_xy_scaler 3 2` in `config/splitkb_aurora_sofle_left.overlay` —
   pointer speed (multiplier/divisor)
2. `bottom-beta` in `config/splitkb_aurora_sofle_right.overlay` — slow-speed
   filtering (0 = smoothest/laggiest, 255 = most responsive)
3. `poll-interval-ms` (same file) — the driver syncs the chip's report rate
   to this automatically; 10 ms = the chip's native cadence

Gestures: one-finger tap = left click, two-finger tap = right click,
press-and-hold = drag, two-finger drag = scroll (natural Y).
