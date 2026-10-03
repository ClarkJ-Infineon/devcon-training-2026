# SPDX-License-Identifier: Apache-2.0
#
# DevCon 2026 PSOC Edge lab - Device Dashboard, cheat tier.
#
# Steps 2 and 3 of the lab each switch on a local module. This app consumes
# LVGL as a west module, and additionally builds two small local modules
# (modules/tilt_filter, modules/beeper) to show what the other side of the
# boundary looks like. ZEPHYR_EXTRA_MODULES is the mechanism, and it must be
# set *before* find_package(Zephyr).

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/tilt_filter)
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/beeper)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(e84_dashboard)

target_sources(app PRIVATE
	src/main.c
	src/ui.c
)
