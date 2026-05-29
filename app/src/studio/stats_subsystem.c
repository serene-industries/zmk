/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk_studio, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <zmk/studio/rpc.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

/* Optional: Battery support */
#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
#include <zmk/battery.h>
#endif

/* Optional: WPM support */
#if IS_ENABLED(CONFIG_ZMK_WPM)
#include <zmk/wpm.h>
#endif

ZMK_RPC_SUBSYSTEM(stats)

#define STATS_RESPONSE(type, ...) ZMK_RPC_RESPONSE(stats, type, __VA_ARGS__)

/* Persistent statistics - saved to flash */
static uint64_t total_keypresses = 0;

/* Debug counter to verify listener is called */
static uint32_t listener_calls = 0;

/* Session tracking */
static int64_t session_start_time = 0;

/* Counter for deferred saves - save every N keypresses to reduce flash wear */
#define SAVE_INTERVAL 1000
static uint32_t keypresses_since_save = 0;

/**
 * Save statistics to flash (called from work queue, not interrupt context)
 */
static void stats_save_work_handler(struct k_work *work) {
    LOG_DBG("Saving stats to flash: %u keypresses", (uint32_t)total_keypresses);
    settings_save_one("stats/keypresses", &total_keypresses, sizeof(total_keypresses));
}

/* Work item for deferred flash saves */
static K_WORK_DELAYABLE_DEFINE(stats_save_work, stats_save_work_handler);

/**
 * Schedule a deferred save to flash
 */
static void stats_schedule_save(void) {
    // Cancel any pending save and schedule a new one in 5 seconds
    // This debounces rapid keypresses
    k_work_reschedule(&stats_save_work, K_SECONDS(5));
}

/**
 * Load statistics from flash on boot
 */
static int stats_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (!strcmp(name, "keypresses")) {
        if (len != sizeof(total_keypresses)) {
            return -EINVAL;
        }
        int rc = read_cb(cb_arg, &total_keypresses, sizeof(total_keypresses));
        if (rc < 0) {
            return rc;
        }
        LOG_INF("Loaded keypress count from flash: %u", (uint32_t)total_keypresses);
        return 0;
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(stats, "stats", NULL, stats_settings_set, NULL, NULL);

/**
 * Position event listener - increments counter on physical key press.
 * Uses position_state_changed which fires when physical keys are actuated,
 * before behavior processing. This is more reliable than keycode events.
 */
static int stats_position_listener(const zmk_event_t *eh) {
    // Always increment listener_calls to prove the listener is being invoked
    listener_calls++;

    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev && ev->state) {
        // Key press event (not release) - increment counter
        total_keypresses++;
        keypresses_since_save++;

        // Schedule save every SAVE_INTERVAL keypresses
        if (keypresses_since_save >= SAVE_INTERVAL) {
            keypresses_since_save = 0;
            stats_schedule_save();
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(stats, stats_position_listener);
ZMK_SUBSCRIPTION(stats, zmk_position_state_changed);

/**
 * RPC: Get current statistics
 */
zmk_studio_Response get_stats(const zmk_studio_Request *req) {
    LOG_DBG("get_stats called");

    zmk_stats_Stats resp = zmk_stats_Stats_init_zero;
    resp.total_keypresses = total_keypresses;

    // Calculate uptime
    if (session_start_time > 0) {
        resp.uptime_seconds = (uint32_t)((k_uptime_get() - session_start_time) / 1000);
    } else {
        resp.uptime_seconds = (uint32_t)(k_uptime_get() / 1000);
    }

    // Battery level (255 = not available)
#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
    resp.battery_level = zmk_battery_state_of_charge();
#else
    resp.battery_level = 255;
#endif

    // Words per minute (reusing as debug: listener_calls count)
#if IS_ENABLED(CONFIG_ZMK_WPM)
    resp.wpm = zmk_wpm_get_state();
#else
    // DEBUG: Return listener_calls count instead of WPM to see if listener is being called
    resp.wpm = listener_calls;
#endif

    return STATS_RESPONSE(get_stats, resp);
}

ZMK_RPC_SUBSYSTEM_HANDLER(stats, get_stats, ZMK_STUDIO_RPC_HANDLER_SECURED);

/**
 * RPC: Reset all statistics (DANGEROUS)
 */
zmk_studio_Response reset_stats(const zmk_studio_Request *req) {
    LOG_WRN("reset_stats called - resetting all statistics!");

    total_keypresses = 0;
    keypresses_since_save = 0;
    session_start_time = k_uptime_get();

    // Save immediately
    settings_save_one("stats/keypresses", &total_keypresses, sizeof(total_keypresses));

    LOG_INF("Statistics reset complete");
    return STATS_RESPONSE(reset_stats, true);
}

ZMK_RPC_SUBSYSTEM_HANDLER(stats, reset_stats, ZMK_STUDIO_RPC_HANDLER_SECURED);

/**
 * Event mapper (no notifications for stats)
 */
static int stats_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) {
    return -ENOTSUP;
}

ZMK_RPC_EVENT_MAPPER(stats, stats_event_mapper);

/**
 * Initialize stats subsystem
 */
static int stats_init(void) {
    session_start_time = k_uptime_get();
    LOG_INF("Stats subsystem initialized");
    return 0;
}

SYS_INIT(stats_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
