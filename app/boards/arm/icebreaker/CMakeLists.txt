# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_BLE)
  target_sources(app PRIVATE bt_indicator.c)
  target_sources(app PRIVATE icebreaker_ble_name.c)
endif()
