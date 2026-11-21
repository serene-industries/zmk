/*
 * Copyright (c) 2026 Serene Industries
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_dynamic_macro

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/dynamic_macros.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define KP_DEV DEVICE_DT_NAME(DT_INST(0, zmk_behavior_key_press))

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {
        .display_name = "Macro",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
        .range = {.min = 0, .max = ZMK_DYN_MACRO_COUNT - 1},
    },
};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

struct queued_key {
    uint32_t keycode;
    bool press;
    uint32_t wait;
};

static int on_dmacro_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct zmk_dyn_macro *m = zmk_dyn_macro_get(binding->param1);
    if (!m || m->count == 0) {
        LOG_DBG("dynamic macro %d is empty", binding->param1);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    // Build the key event list first so a WAIT step can extend the delay
    // after the preceding key event (the queue delays *after* each item).
    struct queued_key keys[ZMK_DYN_MACRO_MAX_STEPS * 2];
    int n = 0;
    for (int i = 0; i < m->count; i++) {
        const struct zmk_dyn_macro_step *s = &m->steps[i];
        switch (s->action) {
        case ZMK_DYN_MACRO_TAP:
            keys[n++] = (struct queued_key){s->param, true, CONFIG_ZMK_DYNAMIC_MACRO_TAP_MS};
            keys[n++] = (struct queued_key){s->param, false, CONFIG_ZMK_DYNAMIC_MACRO_WAIT_MS};
            break;
        case ZMK_DYN_MACRO_PRESS:
            keys[n++] = (struct queued_key){s->param, true, CONFIG_ZMK_DYNAMIC_MACRO_WAIT_MS};
            break;
        case ZMK_DYN_MACRO_RELEASE:
            keys[n++] = (struct queued_key){s->param, false, CONFIG_ZMK_DYNAMIC_MACRO_WAIT_MS};
            break;
        case ZMK_DYN_MACRO_WAIT:
            if (n > 0) {
                keys[n - 1].wait += s->param;
            }
            break;
        default:
            LOG_WRN("dynamic macro %d: unknown action %d", binding->param1, s->action);
            break;
        }
    }

    for (int i = 0; i < n; i++) {
        struct zmk_behavior_binding kp = {.behavior_dev = KP_DEV, .param1 = keys[i].keycode};
        zmk_behavior_queue_add(&event, kp, keys[i].press, keys[i].wait);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_dmacro_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_dynamic_macro_driver_api = {
    .binding_pressed = on_dmacro_binding_pressed,
    .binding_released = on_dmacro_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

#define DMACRO_INST(n)                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_dynamic_macro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DMACRO_INST)
