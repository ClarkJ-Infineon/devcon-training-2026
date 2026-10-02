# SPDX-License-Identifier: Apache-2.0
#
# PSOC Control CAN Command & Telemetry lab - skeleton application.
#
# Build from a west workspace initialised with this manifest:
#
#   west build -b kit_psc3m5_evk -p always -s <path-to-this-dir> -- \
#       -DEXTRA_CONF_FILE=conf/role_command.conf
#
# See ../../README.md and the lab guide for full instructions.

cmake_minimum_required(VERSION 3.20.0)

# On-board LED drive path select.
#
#   (default)        soft-PWM: bit-bang LED0 from a thread. Known good.
#   -DLED_TRIGMUX=y  "Option A": route the real TCPWM PWM line to LED0
#                    through the PERI trigger mux. Not yet hardware
#                    validated - see src/lab_led_trigmux.c.
#
# Both halves of Option A (the Kconfig fragment and the devicetree overlay
# carrying the P8.5 pin-mux) are pulled in together here so that a single
# build flag is enough to switch paths.
if(LED_TRIGMUX)
  list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/conf/led_trigmux.conf)
  list(APPEND EXTRA_DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/led_trigmux.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(psoc_control_can_lab)

target_sources(app PRIVATE
  src/main.c
)

if(CONFIG_LAB_LED_TRIGMUX)
  target_sources(app PRIVATE src/lab_led_trigmux.c)
else()
  target_sources(app PRIVATE src/lab_led_softpwm.c)
endif()

if(CONFIG_LAB_ROLE_COMMAND)
  target_sources(app PRIVATE src/command_node.c)
elseif(CONFIG_LAB_ROLE_TELEMETRY)
  target_sources(app PRIVATE src/telemetry_node.c)
endif()
