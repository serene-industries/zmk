/*
 * Copyright (c) 2026 Serene Industries
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#define ZMK_DYN_MACRO_COUNT CONFIG_ZMK_DYNAMIC_MACRO_COUNT
#define ZMK_DYN_MACRO_MAX_STEPS CONFIG_ZMK_DYNAMIC_MACRO_MAX_STEPS

enum zmk_dyn_macro_action {
    ZMK_DYN_MACRO_TAP = 0,
    ZMK_DYN_MACRO_PRESS = 1,
    ZMK_DYN_MACRO_RELEASE = 2,
    ZMK_DYN_MACRO_WAIT = 3,
};

struct zmk_dyn_macro_step {
    uint8_t action;
    uint32_t param; // encoded keycode, or milliseconds for WAIT
} __packed;

struct zmk_dyn_macro {
    uint8_t count;
    struct zmk_dyn_macro_step steps[ZMK_DYN_MACRO_MAX_STEPS];
} __packed;

const struct zmk_dyn_macro *zmk_dyn_macro_get(uint8_t index);
int zmk_dyn_macro_set(uint8_t index, const struct zmk_dyn_macro *macro);
int zmk_dyn_macro_clear(uint8_t index);
