# Built-in widget headers (output_status.h, layer_status.h) live in the ZMK
# app include dir, which is not on the module include path by default.
# CMAKE_SOURCE_DIR points at zmk/app during the build.
zephyr_library()
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
zephyr_library_sources(custom_status_screen.c)
zephyr_library_sources(widgets/dual_battery.c)
