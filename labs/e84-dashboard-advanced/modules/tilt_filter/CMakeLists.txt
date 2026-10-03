# SPDX-License-Identifier: Apache-2.0
#
# Everything here is guarded by CONFIG_TILT_FILTER. zephyr_library_*
# contributes to the Zephyr build only when the symbol is set, which is what
# makes the Kconfig switch meaningful rather than decorative.

if(CONFIG_TILT_FILTER)
  zephyr_library()
  zephyr_library_sources(src/tilt_filter.c)
  zephyr_include_directories(include)
endif()
