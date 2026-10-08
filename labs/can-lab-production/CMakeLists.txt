# SPDX-License-Identifier: Apache-2.0
#
# PSOC Control CAN Command & Telemetry lab - reference implementation.
#
# Build from the workspace root:
#
#   west build -b kit_psc3m6_evk -d build/lab -s <path-to-this-dir> -- \
#       -DEXTRA_CONF_FILE=conf/role_command.conf
#
# See ../../README.md and the lab guide for full instructions.

cmake_minimum_required(VERSION 3.20.0)

# On-board LED drive path select.
#
#   (default)        trigger-mux: route the real TCPWM PWM line to LED3
#                    (P8.4, alias led0) through the PSOC Control PERI
#                    trigger multiplexer. Hardware signal routing, no CPU
#                    in the path.
#   -DLED_SOFTPWM=y  fallback: bit-bang led0 from a dedicated thread.
#
# Both halves of the trigger-mux path (the source file selected by the
# Kconfig symbol and the devicetree overlay carrying the P8.4 pin-mux) are
# switched together here, so a single build flag is enough to change paths.
if(LED_SOFTPWM)
  list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/conf/led_softpwm.conf)
else()
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
