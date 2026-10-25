/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Displays the battery level of both the central (left, "L") and the
 * peripheral (right, "R") halves on a single label. Ported from
 * Dreeoo/zmk-config (brain shield).
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#include "dual_battery.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Last known peripheral level; updated whenever a peripheral battery
 * event arrives. Central level is always read live. */
static uint8_t peripheral_level = 0;
static bool peripheral_seen = false;

struct dual_battery_state {
    uint8_t central;
    uint8_t peripheral;
    bool peripheral_seen;
};

static void set_battery_text(lv_obj_t *label, struct dual_battery_state state) {
    char text[24] = {};

    if (state.peripheral_seen) {
        snprintf(text, sizeof(text), "L: %u%%  R: %u%%", state.central, state.peripheral);
    } else {
        snprintf(text, sizeof(text), "L: %u%%  R: --", state.central);
    }

    lv_label_set_text(label, text);
}

static void dual_battery_update_cb(struct dual_battery_state state) {
    struct zmk_widget_dual_battery *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_text(widget->obj, state); }
}

static struct dual_battery_state dual_battery_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *pev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (pev != NULL) {
        peripheral_level = pev->state_of_charge;
        peripheral_seen = true;
    }

    return (struct dual_battery_state){
        .central = zmk_battery_state_of_charge(),
        .peripheral = peripheral_level,
        .peripheral_seen = peripheral_seen,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dual_battery, struct dual_battery_state, dual_battery_update_cb,
                            dual_battery_get_state)

ZMK_SUBSCRIPTION(widget_dual_battery, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_dual_battery, zmk_peripheral_battery_state_changed);

int zmk_widget_dual_battery_init(struct zmk_widget_dual_battery *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    sys_slist_append(&widgets, &widget->node);

    widget_dual_battery_init();
    return 0;
}

lv_obj_t *zmk_widget_dual_battery_obj(struct zmk_widget_dual_battery *widget) {
    return widget->obj;
}
