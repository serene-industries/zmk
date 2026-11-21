/*
 * Copyright (c) 2026 Serene Industries
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk_studio, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <zmk/studio/rpc.h>
#include <zmk/dynamic_macros.h>

ZMK_RPC_SUBSYSTEM(macros)

#define MACROS_RESPONSE(type, ...) ZMK_RPC_RESPONSE(macros, type, __VA_ARGS__)

static zmk_studio_Response get_info(const zmk_studio_Request *req) {
    zmk_macros_Info info = zmk_macros_Info_init_zero;
    info.count = ZMK_DYN_MACRO_COUNT;
    info.max_steps = ZMK_DYN_MACRO_MAX_STEPS;
    return MACROS_RESPONSE(get_info, info);
}

static zmk_studio_Response get_macro(const zmk_studio_Request *req) {
    uint32_t index = req->subsystem.macros.request_type.get_macro;
    const struct zmk_dyn_macro *m = zmk_dyn_macro_get(index);
    if (!m) {
        return ZMK_RPC_SIMPLE_ERR(GENERIC);
    }
    zmk_macros_Macro resp = zmk_macros_Macro_init_zero;
    resp.index = index;
    resp.steps_count = m->count;
    for (int i = 0; i < m->count; i++) {
        resp.steps[i].action = m->steps[i].action;
        resp.steps[i].param = m->steps[i].param;
    }
    return MACROS_RESPONSE(get_macro, resp);
}

static zmk_studio_Response set_macro(const zmk_studio_Request *req) {
    const zmk_macros_Macro *in = &req->subsystem.macros.request_type.set_macro;
    if (in->index >= ZMK_DYN_MACRO_COUNT || in->steps_count > ZMK_DYN_MACRO_MAX_STEPS) {
        return MACROS_RESPONSE(set_macro, false);
    }
    struct zmk_dyn_macro m = {.count = in->steps_count};
    for (int i = 0; i < in->steps_count; i++) {
        m.steps[i].action = in->steps[i].action;
        m.steps[i].param = in->steps[i].param;
    }
    int ret = zmk_dyn_macro_set(in->index, &m);
    LOG_DBG("set dynamic macro %d (%d steps): %d", in->index, in->steps_count, ret);
    return MACROS_RESPONSE(set_macro, ret == 0);
}

static zmk_studio_Response clear_macro(const zmk_studio_Request *req) {
    uint32_t index = req->subsystem.macros.request_type.clear_macro;
    return MACROS_RESPONSE(clear_macro, zmk_dyn_macro_clear(index) == 0);
}

ZMK_RPC_SUBSYSTEM_HANDLER(macros, get_info, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(macros, get_macro, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(macros, set_macro, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(macros, clear_macro, ZMK_STUDIO_RPC_HANDLER_SECURED);
