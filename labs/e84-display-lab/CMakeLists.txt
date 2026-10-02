# SPDX-License-Identifier: Apache-2.0
#
# PSOC Edge E84 display smoke-test / LVGL demo - DevCon 2026 training branch.
#
# Build against the devcon-training-2026 mainline-based Zephyr branch
# (ClarkJ-Infineon/zephyr, or the devcon-training-2026 west manifest repo
# once published):
#
#   west build -b kit_pse84_eval/pse846gps2dbzc4a/m55 -p always -s <path-to-this-dir>
#
# See ../../README.md for the lab layout.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(e84_display_lab)

target_sources(app PRIVATE src/main.c)
