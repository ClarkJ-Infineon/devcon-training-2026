# SPDX-License-Identifier: Apache-2.0
#
# Same shape as modules/tilt_filter: everything is guarded by CONFIG_BEEPER,
# so the module contributes nothing to the build unless the symbol is set.

if(CONFIG_BEEPER)
  zephyr_library()
  zephyr_library_sources(src/beeper.c)
  zephyr_include_directories(include)
endif()
