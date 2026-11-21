/*
 * Copyright (c) 2026 Serene Industries
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <zmk/dynamic_macros.h>

LOG_MODULE_REGISTER(zmk_dynamic_macros, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_dyn_macro macros[ZMK_DYN_MACRO_COUNT];

static size_t stored_size(const struct zmk_dyn_macro *m) {
    return sizeof(m->count) + (size_t)m->count * sizeof(struct zmk_dyn_macro_step);
}

const struct zmk_dyn_macro *zmk_dyn_macro_get(uint8_t index) {
    if (index >= ZMK_DYN_MACRO_COUNT) {
        return NULL;
    }
    return &macros[index];
}

static int save(uint8_t index) {
    char key[16];
    snprintf(key, sizeof(key), "dmacro/%u", index);
    return settings_save_one(key, &macros[index], stored_size(&macros[index]));
}

int zmk_dyn_macro_set(uint8_t index, const struct zmk_dyn_macro *macro) {
    if (index >= ZMK_DYN_MACRO_COUNT || macro->count > ZMK_DYN_MACRO_MAX_STEPS) {
        return -EINVAL;
    }
    memset(&macros[index], 0, sizeof(macros[index]));
    macros[index].count = macro->count;
    memcpy(macros[index].steps, macro->steps, macro->count * sizeof(struct zmk_dyn_macro_step));
    return save(index);
}

int zmk_dyn_macro_clear(uint8_t index) {
    if (index >= ZMK_DYN_MACRO_COUNT) {
        return -EINVAL;
    }
    memset(&macros[index], 0, sizeof(macros[index]));
    return save(index);
}

static int dmacro_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                               void *cb_arg) {
    char *end;
    unsigned long index = strtoul(name, &end, 10);
    if (end == name || *end != '\0' || index >= ZMK_DYN_MACRO_COUNT) {
        return -ENOENT;
    }
    if (len < sizeof(uint8_t) || len > sizeof(struct zmk_dyn_macro)) {
        return -EINVAL;
    }
    memset(&macros[index], 0, sizeof(macros[index]));
    ssize_t rc = read_cb(cb_arg, &macros[index], len);
    if (rc < 0) {
        return rc;
    }
    if (macros[index].count > ZMK_DYN_MACRO_MAX_STEPS ||
        stored_size(&macros[index]) > (size_t)rc) {
        LOG_WRN("Discarding corrupt dynamic macro %lu", index);
        memset(&macros[index], 0, sizeof(macros[index]));
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zmk_dynamic_macros, "dmacro", NULL, dmacro_settings_set, NULL,
                               NULL);
