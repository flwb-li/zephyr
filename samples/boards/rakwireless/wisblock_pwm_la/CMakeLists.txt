# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 RAKwireless Technology Limited

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wisblock_pwm_la)

target_sources(app PRIVATE src/main.c)

if(CONFIG_BOARD_RAK3362_NRF54L15_CPUAPP OR CONFIG_BOARD_RAK3362_NRF54L15_CPUAPP_NS)
  target_include_directories(app PRIVATE
    ${ZEPHYR_BASE}/boards/rakwireless/rak3362)
endif()
