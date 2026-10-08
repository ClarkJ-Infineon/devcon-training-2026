# SPDX-License-Identifier: Apache-2.0
#
# PSOC Control CAN Command & Telemetry lab - skeleton application.
#
# Build from the workspace set up in the
# Software General Session (Zephyr):
#
#   west build -b kit_psc3m6_evk -d build/lab -s <path-to-this-dir> -- \
#       -DEXTRA_CONF_FILE=conf/role_command.conf
#
# See ../../README.md and the lab guide for full instructions.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(psoc_control_can_lab)

target_sources(app PRIVATE
  src/main.c
  src/lab_led.c
)

if(CONFIG_LAB_ROLE_COMMAND)
  target_sources(app PRIVATE src/command_node.c)
elseif(CONFIG_LAB_ROLE_TELEMETRY)
  target_sources(app PRIVATE src/telemetry_node.c)
endif()
