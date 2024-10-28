# Copyright (c) 2022 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19)

set(MBED_OS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os CACHE INTERNAL "")

# Set path for mbed_app.json5
set(MBED_APP_JSON_PATH mbed_app.json5)

# Set path for custom_targets.json5
set(CUSTOM_TARGETS_PATH custom_targets) 
set(CUSTOM_TARGETS_JSON_PATH ${CUSTOM_TARGETS_PATH}/custom_targets.json5) 

# Load Mbed CE toolchain file and basic build system
include(${MBED_OS_PATH}/tools/cmake/app.cmake)

set(APP_PROJECT NUMAKER_MBED_CE_AZURE_IOT_CSDK_OTA_EXAMPLE)
set(APP_TARGET NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example)

# Set up project name
project(${APP_PROJECT})

# Add Mbed OS library
add_subdirectory(${MBED_OS_PATH})

# Add other libraries
add_subdirectory(mbed-ce-client-for-azure)
add_subdirectory(ntp-client)

# User-provided MBED_CONF_AZURE_CLIENT_OTA_ADUC_USER_CONFIG_FILE (aduc_user_config.h)
# for mbed-ce-client-for-azure (COMPONENT_AZIOT_OTA)
target_include_directories(mbed-ce-client-for-azure
    PRIVATE
        configs
)

add_executable(${APP_TARGET})

target_include_directories(${APP_TARGET}
    PRIVATE
        .
)

target_sources(${APP_TARGET}
    PRIVATE
        src/main.cpp
        src/mem_stats.cpp
)

# Use DPS
if("MBED_CONF_APP_USE_DPS=1" IN_LIST MBED_CONFIG_DEFINITIONS)
    target_sources(${APP_TARGET}
        PRIVATE
            hsm_custom/custom_hsm_example.c
    )

    target_compile_definitions(mbed-ce-client-for-azure
        PUBLIC
            USE_PROV_MODULE_FULL
            USE_PROV_MODULE
            HSM_AUTH_TYPE_CUSTOM
    )
endif()

target_link_libraries(${APP_TARGET}
    PRIVATE
        mbed-os
        mbed-ce-client-for-azure
        ntp-client
)

# Must call this for each target to set up bin file creation, code upload, etc
mbed_set_post_build(${APP_TARGET})

# Make sure this is the last line of the top-level build script
mbed_finalize_build()
