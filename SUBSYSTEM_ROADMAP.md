# ZMK Studio Subsystem Roadmap

This document outlines planned RPC subsystem implementations for ZMK Studio integration with the SI Configurator.

---

## Current Status

### Implemented Subsystems (Custom Fork)

| Subsystem | Proto File | Status | Notes |
|-----------|------------|--------|-------|
| `core` | core.proto | Official | Device info, lock state, reset |
| `behaviors` | behaviors.proto | Official | List/get behavior details |
| `keymap` | keymap.proto | Official | Full keymap management |
| `lighting` | lighting.proto | Custom | RGB underglow control |
| `ble` | ble.proto | Custom | BLE profile management |
| `stats` | stats.proto | Custom | Basic stats (partial) |

---

## 1. Stats Subsystem Enhancement

**Priority:** High
**Status:** Partially implemented
**Proto:** `stats.proto`

### Current Implementation
```proto
message Stats {
    uint64 total_keypresses = 1;
    uint32 uptime_seconds = 2;
    uint32 battery_level = 3;      // 0-100, 255 = not available
    uint32 wpm = 4;
}
```

### Proposed Additions
```proto
message Stats {
    // Existing fields
    uint64 total_keypresses = 1;
    uint32 uptime_seconds = 2;
    uint32 battery_level = 3;
    uint32 wpm = 4;

    // NEW: Extended statistics
    uint64 total_uptime_seconds = 5;     // Lifetime uptime (persisted)
    uint32 session_keypresses = 6;       // Keypresses this session only
    uint32 peak_wpm = 7;                 // Highest WPM this session
    uint32 average_wpm = 8;              // Average WPM this session

    // NEW: Per-key statistics (optional, heavy)
    // Consider: separate RPC call for this
    // repeated KeyStats key_stats = 10;
}

// Future: Per-key heatmap data
message KeyStats {
    uint32 key_position = 1;
    uint64 press_count = 2;
}

message Request {
    oneof request_type {
        bool get_stats = 1;
        bool reset_stats = 2;
        bool get_key_heatmap = 3;        // NEW: Get per-key press counts
    }
}

message Response {
    oneof response_type {
        Stats get_stats = 1;
        bool reset_stats = 2;
        KeyHeatmap get_key_heatmap = 3;  // NEW
    }
}

message KeyHeatmap {
    repeated KeyStats keys = 1;
}
```

### Implementation Tasks

- [ ] **Firmware (C)**
  - [ ] Add lifetime uptime counter with NVS persistence
  - [ ] Track session vs total keypresses separately
  - [ ] Implement WPM calculation (rolling window)
  - [ ] Add peak/average WPM tracking
  - [ ] (Optional) Per-key press counter with NVS storage

- [ ] **TypeScript Client**
  - [ ] Extend `ZmkStats` interface
  - [ ] Add `getKeyHeatmap()` method
  - [ ] Update studio.js decoder

- [ ] **UI (page.tsx)**
  - [ ] Display extended stats in Device Info accordion
  - [ ] (Optional) Visual key heatmap overlay on keyboard visualizer

### Firmware References
- Keypress counting: Hook into `zmk_hid_keyboard_press()` or key position listener
- WPM calculation: Use `k_uptime_get()` for timing between keypresses
- NVS storage: Use `settings_save_one()` / `settings_load_subtree()`

---

## 2. Firmware Info Subsystem

**Priority:** High
**Status:** Not implemented
**Proto:** `firmware.proto` (new file)

### Purpose
Expose firmware version, build info, and hardware capabilities to the UI for version checking and feature detection.

### Proposed Proto
```proto
syntax = "proto3";

package zmk.firmware;

// Firmware version information
message FirmwareInfo {
    // Version fields
    string version = 1;              // Semantic version: "1.2.3"
    string build_date = 2;           // ISO date: "2024-01-15"
    string build_time = 3;           // Time: "14:30:00"
    string git_commit = 4;           // Short SHA: "a1b2c3d"
    string git_branch = 5;           // Branch name: "icebreaker-studio"

    // Build configuration
    string board = 6;                // "icebreaker_hs"
    string shield = 7;               // Optional shield name
    string zephyr_version = 8;       // "3.5.0"
    string zmk_version = 9;          // ZMK base version

    // Hardware capabilities (feature flags)
    bool has_rgb = 10;
    bool has_backlight = 11;
    bool has_display = 12;
    bool has_encoder = 13;
    bool has_battery = 14;
    bool has_ble = 15;
    bool has_usb = 16;
    bool has_split = 17;
    bool has_studio = 18;            // Always true if responding

    // Studio protocol version
    uint32 studio_protocol_version = 20;  // For compatibility checks
}

// Hardware information
message HardwareInfo {
    string mcu = 1;                  // "nRF52840"
    uint32 flash_size_kb = 2;        // Total flash in KB
    uint32 ram_size_kb = 3;          // Total RAM in KB
    uint32 flash_used_kb = 4;        // Used flash in KB
    uint32 settings_used_bytes = 5;  // NVS/settings usage
}

message Request {
    oneof request_type {
        bool get_firmware_info = 1;
        bool get_hardware_info = 2;
        bool check_for_updates = 3;  // Future: could ping server
    }
}

message Response {
    oneof response_type {
        FirmwareInfo get_firmware_info = 1;
        HardwareInfo get_hardware_info = 2;
        UpdateInfo check_for_updates = 3;
    }
}

// Future: Update availability info
message UpdateInfo {
    bool update_available = 1;
    string latest_version = 2;
    string release_notes_url = 3;
    string download_url = 4;
}

message Notification {
    // No notifications needed
}
```

### Implementation Tasks

- [ ] **Build System**
  - [ ] Generate `version.h` at build time with git info
  - [ ] Add CMake rules to embed build timestamp
  - [ ] Create `CONFIG_ZMK_FIRMWARE_VERSION` Kconfig option

- [ ] **Firmware (C)**
  - [ ] Create `firmware_subsystem.c`
  - [ ] Implement `get_firmware_info` handler
  - [ ] Query Kconfig for feature flags
  - [ ] Use Zephyr `hwinfo` API for hardware details
  - [ ] Expose flash/RAM usage via `sys_heap_runtime_stats` or linker symbols

- [ ] **TypeScript Client**
  - [ ] Add `ZmkFirmwareInfo` interface
  - [ ] Add `getFirmwareInfo()` method to `ZMKStudioClient`
  - [ ] Extend studio.js with firmware subsystem decoder

- [ ] **UI (page.tsx)**
  - [ ] Display firmware version in Device Info accordion
  - [ ] Show feature capabilities
  - [ ] Version mismatch warning banner

### Firmware References
```c
// Build-time version (generated header)
#include "version.h"
// VERSION_STRING, BUILD_DATE, GIT_COMMIT, GIT_BRANCH

// Zephyr version
#include <version.h>
// KERNEL_VERSION_STRING

// Feature detection via Kconfig
#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
    resp.has_rgb = true;
#endif

// Flash usage (linker symbols)
extern char __rom_region_end[];
extern char __rom_region_start[];
size_t flash_used = (size_t)__rom_region_end - (size_t)__rom_region_start;
```

---

## 3. Firmware Update (DFU) Subsystem

**Priority:** Medium
**Status:** Not implemented
**Proto:** `dfu.proto` (new file)

### Purpose
Enable over-the-air (OTA) firmware updates via the USB serial connection, eliminating the need for manual bootloader entry.

### Proposed Proto
```proto
syntax = "proto3";

package zmk.dfu;

// DFU state machine
enum DfuState {
    DFU_STATE_IDLE = 0;
    DFU_STATE_READY = 1;           // Ready to receive firmware
    DFU_STATE_DOWNLOADING = 2;     // Receiving data
    DFU_STATE_VALIDATING = 3;      // Verifying checksum
    DFU_STATE_FLASHING = 4;        // Writing to flash
    DFU_STATE_COMPLETE = 5;        // Success, ready to reboot
    DFU_STATE_ERROR = 6;           // Failed
}

enum DfuError {
    DFU_ERROR_NONE = 0;
    DFU_ERROR_INVALID_IMAGE = 1;
    DFU_ERROR_SIGNATURE_FAILED = 2;
    DFU_ERROR_FLASH_WRITE = 3;
    DFU_ERROR_SIZE_MISMATCH = 4;
    DFU_ERROR_CHECKSUM_MISMATCH = 5;
    DFU_ERROR_CANCELLED = 6;
    DFU_ERROR_NO_SPACE = 7;
}

message DfuStatus {
    DfuState state = 1;
    DfuError error = 2;
    uint32 bytes_received = 3;
    uint32 bytes_total = 4;
    uint32 progress_percent = 5;
}

message Request {
    oneof request_type {
        bool get_dfu_status = 1;
        DfuBegin begin_dfu = 2;      // Start update process
        DfuData write_data = 3;      // Send firmware chunk
        bool abort_dfu = 4;          // Cancel in-progress update
        bool reboot = 5;             // Reboot into new firmware
        bool enter_bootloader = 6;   // Reboot into UF2 bootloader
    }
}

message DfuBegin {
    uint32 image_size = 1;           // Total firmware size in bytes
    bytes checksum = 2;              // SHA256 of entire image
    string version = 3;              // Version string for logging
}

message DfuData {
    uint32 offset = 1;               // Byte offset in image
    bytes data = 2;                  // Chunk data (max 512 bytes recommended)
}

message Response {
    oneof response_type {
        DfuStatus get_dfu_status = 1;
        DfuStatus begin_dfu = 2;
        DfuStatus write_data = 3;
        bool abort_dfu = 4;
        bool reboot = 5;
        bool enter_bootloader = 6;
    }
}

message Notification {
    oneof notification_type {
        DfuStatus dfu_progress = 1;  // Progress updates during flash
    }
}
```

### Implementation Tasks

- [ ] **Firmware (C)**
  - [ ] Create `dfu_subsystem.c`
  - [ ] Implement DFU state machine
  - [ ] Use MCUboot or Zephyr flash APIs for image management
  - [ ] Implement chunked data reception with validation
  - [ ] Add SHA256 checksum verification
  - [ ] Implement `sys_reboot()` for reboot command
  - [ ] Add bootloader entry via `NRF_POWER->GPREGRET` (nRF52)

- [ ] **TypeScript Client**
  - [ ] Add `DfuManager` class for update orchestration
  - [ ] Implement chunked file upload with progress
  - [ ] Add retry logic for failed chunks
  - [ ] Expose progress callbacks

- [ ] **UI (page.tsx)**
  - [ ] "Check for Updates" button in Device Info
  - [ ] Firmware file picker (drag & drop .uf2)
  - [ ] Progress bar during upload
  - [ ] Confirmation dialogs (update, reboot)
  - [ ] Error handling with recovery options

### Security Considerations
- [ ] Image signing verification (optional but recommended)
- [ ] Rollback protection (version downgrade prevention)
- [ ] Secure boot chain integration

### Firmware References
```c
// Reboot into bootloader (nRF52)
#include <zephyr/sys/reboot.h>
#include <hal/nrf_power.h>

void enter_bootloader(void) {
    // Set magic value for bootloader
    nrf_power_gpregret_set(NRF_POWER, 0x57); // UF2 bootloader magic
    sys_reboot(SYS_REBOOT_COLD);
}

// MCUboot image management
#include <zephyr/dfu/mcuboot.h>
boot_request_upgrade(BOOT_UPGRADE_TEST);
```

### Alternative: Simple Bootloader Entry
For MVP, skip full DFU and just implement `enter_bootloader`:
```proto
message Request {
    oneof request_type {
        bool enter_bootloader = 1;  // Reboot to UF2 mode
    }
}
```
This allows the UI to trigger bootloader mode, then user drags .uf2 file.

---

## 4. Power Management Subsystem

**Priority:** High
**Status:** Not implemented
**Proto:** `power.proto` (new file)

### Purpose
Expose battery status, control power modes, and configure sleep/idle timeouts.

### Proposed Proto
```proto
syntax = "proto3";

package zmk.power;

// Battery status
message BatteryStatus {
    uint32 level = 1;                // 0-100 percent
    bool charging = 2;               // Currently charging
    bool usb_powered = 3;            // USB connected
    uint32 voltage_mv = 4;           // Raw voltage in millivolts
    BatteryState state = 5;          // State enum
}

enum BatteryState {
    BATTERY_STATE_UNKNOWN = 0;
    BATTERY_STATE_DISCHARGING = 1;
    BATTERY_STATE_CHARGING = 2;
    BATTERY_STATE_FULL = 3;
    BATTERY_STATE_NOT_PRESENT = 4;
}

// Power configuration (persisted settings)
message PowerConfig {
    uint32 idle_timeout_ms = 1;      // Time before idle (OLED off, etc.)
    uint32 sleep_timeout_ms = 2;     // Time before deep sleep (0 = disabled)
    bool ext_power_default = 3;      // External power on at boot
    bool rgb_auto_off_idle = 4;      // Turn off RGB on idle
    bool rgb_auto_off_usb = 5;       // Turn off RGB when USB disconnected
    bool backlight_auto_off_idle = 6;// Turn off backlight on idle
}

// Current power state
message PowerState {
    PowerMode mode = 1;
    bool ext_power_enabled = 2;      // External power rail state
    uint32 idle_time_ms = 3;         // Time since last activity
    BatteryStatus battery = 4;
}

enum PowerMode {
    POWER_MODE_ACTIVE = 0;
    POWER_MODE_IDLE = 1;
    POWER_MODE_SLEEP = 2;            // Deep sleep
}

message Request {
    oneof request_type {
        bool get_power_state = 1;
        bool get_power_config = 2;
        PowerConfig set_power_config = 3;
        bool get_battery_status = 4;

        // Immediate actions
        bool soft_off = 5;           // Enter deep sleep now
        bool wake = 6;               // Force wake from idle
        bool ext_power_enable = 7;   // Turn on external power
        bool ext_power_disable = 8;  // Turn off external power
        bool ext_power_toggle = 9;   // Toggle external power
    }
}

message Response {
    oneof response_type {
        PowerState get_power_state = 1;
        PowerConfig get_power_config = 2;
        bool set_power_config = 3;   // true = success
        BatteryStatus get_battery_status = 4;
        bool soft_off = 5;
        bool wake = 6;
        bool ext_power_enable = 7;
        bool ext_power_disable = 8;
        bool ext_power_toggle = 9;
    }
}

message Notification {
    oneof notification_type {
        BatteryStatus battery_changed = 1;   // Battery level/state change
        PowerMode power_mode_changed = 2;    // Idle/sleep state change
        bool ext_power_changed = 3;          // External power toggled
    }
}
```

### Implementation Tasks

- [ ] **Firmware (C)**
  - [ ] Create `power_subsystem.c`
  - [ ] Implement `get_battery_status` using `zmk_battery_state_of_charge()`
  - [ ] Implement `get_power_state` reading current mode and ext_power
  - [ ] Implement `get/set_power_config` with NVS persistence
  - [ ] Add `soft_off` using `sys_poweroff()` or `pm_state_force()`
  - [ ] Add `ext_power_enable/disable` using `ext_power_enable()`
  - [ ] Register for battery/power events for notifications

- [ ] **TypeScript Client**
  - [ ] Add `ZmkBatteryStatus`, `ZmkPowerConfig`, `ZmkPowerState` interfaces
  - [ ] Add power management methods to `ZMKStudioClient`
  - [ ] Handle power notifications (battery updates)

- [ ] **UI (page.tsx)**
  - [ ] Battery indicator in header (icon + percentage)
  - [ ] Power Management accordion:
    - Battery status with voltage
    - Charging indicator
    - Idle timeout slider (15s - 5min)
    - Sleep timeout slider (1min - 30min, or disabled)
    - External power toggle
    - RGB/backlight auto-off toggles
  - [ ] "Sleep Now" button
  - [ ] Low battery warning banner

### Firmware References
```c
// Battery level
#include <zmk/battery.h>
uint8_t level = zmk_battery_state_of_charge();

// External power control
#include <drivers/ext_power.h>
const struct device *ext_power = DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));
ext_power_enable(ext_power);
ext_power_disable(ext_power);
int state = ext_power_get(ext_power);

// Deep sleep / soft off
#include <zephyr/sys/poweroff.h>
sys_poweroff();  // Enter deep sleep

// Idle timeout (runtime modification would need custom implementation)
// Default: CONFIG_ZMK_IDLE_TIMEOUT (30000ms)
// Default: CONFIG_ZMK_IDLE_SLEEP_TIMEOUT (900000ms)

// Activity tracking
#include <zmk/activity.h>
// Hook zmk_activity_event for idle_time tracking
```

### Runtime Config Challenges
Note: ZMK's idle/sleep timeouts are compile-time Kconfig values. To make them runtime-configurable:
1. Add settings storage for timeout values
2. Modify `activity.c` to read from settings instead of Kconfig
3. Or: expose as "profiles" (e.g., "Performance", "Battery Saver")

---

## 5. Studio Proto Integration

### Updating `studio.proto`

Add new subsystems to the main studio proto:

```proto
syntax = "proto3";

package zmk.studio;

import "meta.proto";
import "core.proto";
import "behaviors.proto";
import "keymap.proto";
import "lighting.proto";
import "ble.proto";
import "stats.proto";
import "firmware.proto";  // NEW
import "dfu.proto";       // NEW
import "power.proto";     // NEW

message Request {
    uint32 request_id = 1;

    oneof subsystem {
        zmk.core.Request core = 3;
        zmk.behaviors.Request behaviors = 4;
        zmk.keymap.Request keymap = 5;
        zmk.lighting.Request lighting = 6;
        zmk.ble.Request ble = 7;
        zmk.stats.Request stats = 8;
        zmk.firmware.Request firmware = 9;   // NEW
        zmk.dfu.Request dfu = 10;            // NEW
        zmk.power.Request power = 11;        // NEW
    }
}

message RequestResponse {
    uint32 request_id = 1;
    oneof subsystem {
        zmk.meta.Response meta = 2;
        zmk.core.Response core = 3;
        zmk.behaviors.Response behaviors = 4;
        zmk.keymap.Response keymap = 5;
        zmk.lighting.Response lighting = 6;
        zmk.ble.Response ble = 7;
        zmk.stats.Response stats = 8;
        zmk.firmware.Response firmware = 9;  // NEW
        zmk.dfu.Response dfu = 10;           // NEW
        zmk.power.Response power = 11;       // NEW
    }
}

message Notification {
    oneof subsystem {
        zmk.core.Notification core = 2;
        zmk.keymap.Notification keymap = 5;
        zmk.ble.Notification ble = 7;
        zmk.dfu.Notification dfu = 10;       // NEW
        zmk.power.Notification power = 11;   // NEW
    }
}
```

---

## Implementation Priority

| Phase | Subsystem | Effort | Value | Notes |
|-------|-----------|--------|-------|-------|
| 1 | Power (battery only) | Low | High | Most requested feature |
| 1 | Firmware Info | Low | High | Essential for version checking |
| 2 | Stats Enhancement | Medium | Medium | Nice-to-have analytics |
| 2 | Power (full config) | Medium | High | Runtime timeout config is complex |
| 3 | DFU (bootloader entry) | Low | Medium | MVP: just enter_bootloader |
| 3 | DFU (full OTA) | High | Medium | Complex but eliminates UF2 drag |

---

## File Structure

```
firmware/zmk/modules/msgs/zmk-studio-messages/proto/zmk/
├── behaviors.proto      (official)
├── ble.proto           (custom - implemented)
├── core.proto          (official)
├── dfu.proto           (NEW - proposed)
├── firmware.proto      (NEW - proposed)
├── keymap.proto        (official)
├── lighting.proto      (custom - implemented)
├── meta.proto          (official)
├── power.proto         (NEW - proposed)
├── stats.proto         (custom - partial)
└── studio.proto        (custom - extended)

firmware/zmk/app/src/studio/
├── core_subsystem.c    (official)
├── keymap_subsystem.c  (official)
├── behaviors_subsystem.c (official)
├── lighting_subsystem.c (custom - implemented)
├── ble_subsystem.c     (custom - implemented)
├── stats_subsystem.c   (custom - partial)
├── firmware_subsystem.c (NEW - proposed)
├── dfu_subsystem.c     (NEW - proposed)
└── power_subsystem.c   (NEW - proposed)
```

---

## TypeScript Client Updates

The `@zmkfirmware/zmk-studio-ts-client` library will need updates for each subsystem. Since we're using a fork, we can modify `node_modules/@zmkfirmware/zmk-studio-ts-client/lib/studio.js` directly or create wrapper functions in `lib/zmkStudio.ts`.

For each new subsystem:
1. Add encoder in `Request.encode()`
2. Add decoder in `RequestResponse.decode()`
3. Add notification decoder if applicable
4. Add TypeScript interfaces in `lib/zmkStudio.ts`
5. Add methods to `ZMKStudioClient` class

---

## References

- [ZMK Power Management](https://zmk.dev/docs/config/power)
- [Zephyr Power Management](https://docs.zephyrproject.org/latest/services/pm/index.html)
- [MCUboot DFU](https://docs.mcuboot.com/)
- [nRF52 Bootloader](https://infocenter.nordicsemi.com/topic/sdk_nrf5_v17.1.0/lib_bootloader.html)
- [ZMK Studio Protocol](https://github.com/zmkfirmware/zmk-studio-ts-client)
