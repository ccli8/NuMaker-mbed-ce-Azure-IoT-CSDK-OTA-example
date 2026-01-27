# Copyright (c) 2022 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19)

# Set default path for Mbed OS library (no force-write for override)
set(MBED_OS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os CACHE INTERNAL "")

# Set mbed_app.json5 as default, no cache force write for argument passing
set(MBED_APP_JSON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed_app.json5 CACHE INTERNAL "")
if(NOT EXISTS ${MBED_APP_JSON_PATH})
    unset(MBED_APP_JSON_PATH CACHE)
endif()

# Set custom_targets/custom_targets.json5 as default, no cache force write for argument passing
set(CUSTOM_TARGETS_JSON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/custom_targets/custom_targets.json5 CACHE INTERNAL "")
if(NOT EXISTS ${CUSTOM_TARGETS_JSON_PATH})
    unset(CUSTOM_TARGETS_JSON_PATH CACHE)
endif()

# Include Mbed toolchain setup file
include(mbed-os/tools/cmake/mbed_toolchain_setup.cmake)

set(APP_PROJECT NUMAKER_MBED_CE_AZURE_IOT_CSDK_OTA_EXAMPLE)
set(APP_TARGET NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example)

# Set up project name
project(${APP_PROJECT})

# Include Mbed project setup file
include(mbed_project_setup)

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
    # DPS NOT SUPPORT
    message(FATAL_ERROR "DPS NOT SUPPORT")

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

# Change flash image name for upload target, where <app>_merged.hex is the
# combined image in post-build process
#
# "load" -> "load <app>_merged.hex"
function(mbed_generate_upload_target target)
    # add upload target
    #
    # Ignore OUTPUT_EXT and fix to Intel Hex format
    gen_upload_target(${target} ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_BASE_NAME:${target}>_merged.hex)

    # Make sure building the upload target causes the target to be built first
    if(TARGET flash-${target})
        add_dependencies(flash-${target} ${target})
    endif()

endfunction()

# Change flash image name for GDB debug, where <app>_merged.hex is the
# combined image in post-build process
#
# "load" -> "load <app>_merged.hex"
function(mbed_get_upload_launch_commands_for target result_var)

    set(${result_var} ${MBED_UPLOAD_LAUNCH_COMMANDS})

    # <app>_merged.hex for debug launch load
    set(HEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_BASE_NAME:${target}>_merged.hex)

    # GDB load command in MBED_UPLOAD_LAUNCH_COMMANDS?
    list(FIND MBED_UPLOAD_LAUNCH_COMMANDS "load" LOAD_INDEX)
    if(${LOAD_INDEX} GREATER_EQUAL 0)
        # "load" -> "load <app>.hex"
        #
        # GDB load command doesn't support binary format. Ignore OUTPUT_EXT
        # and fix to Intel Hex format.
        #
        # NOTE: The <app>.hex file name needs to be quoted (\") to pass along
        #       to gdb correctly.
        list(TRANSFORM ${result_var} APPEND " \"${HEX_FILE}\"" AT ${LOAD_INDEX})
    endif()

    # 'monitor program' command in MBED_UPLOAD_LAUNCH_COMMANDS?
    list(FIND MBED_UPLOAD_LAUNCH_COMMANDS "monitor program" MON_PROG_INDEX)
    if(${MON_PROG_INDEX} GREATER_EQUAL 0)
        list(TRANSFORM ${result_var} APPEND " \"${HEX_FILE}\"" AT ${MON_PROG_INDEX})
    endif()

    set(${result_var} ${${result_var}} PARENT_SCOPE)
endfunction(mbed_get_upload_launch_commands_for)

# Must call this for each target to set up bin file creation, code upload, etc
mbed_set_post_build(${APP_TARGET})
