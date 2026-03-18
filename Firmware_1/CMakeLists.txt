# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(mcuboot_CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256 y)
set(mcuboot_CONFIG_BOOT_SIGNATURE_KEY_FILE \"${CMAKE_CURRENT_SOURCE_DIR}/priv.pem\")
#set(PM_STATIC_YML_FILE \"${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml\")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Firmware_1)

target_sources(app PRIVATE src/main.c)