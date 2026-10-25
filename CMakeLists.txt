add_subdirectory(drivers)

if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  add_subdirectory(src/display)
endif()
