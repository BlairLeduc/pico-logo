# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to build tests instead of Pico target
option(BUILD_TESTING "Build native host tests instead of Pico target" OFF)

# If building tests, skip all Pico SDK setup
if(NOT BUILD_TESTING)
    # Initialise pico_sdk from installed location
    # (note this can come from environment, CMake cache etc)

    # == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
    if(WIN32)
        set(USERHOME $ENV{USERPROFILE})
    else()
        set(USERHOME $ENV{HOME})
    endif()
    set(sdkVersion 2.2.0)
    set(toolchainVersion 14_2_Rel1)
    set(picotoolVersion 2.2.0-a4)
    set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
    if (EXISTS ${picoVscode})
        include(${picoVscode})
    endif()
    # ====================================================================================
    set(PICO_BOARD pico2 CACHE STRING "Board type")

    # Pull in Raspberry Pi Pico SDK (must be before project)
    include(pico_sdk_import.cmake)

    project(pico-logo C CXX ASM)
else()
    # Native build for tests
    project(pico-logo-tests C)
endif()

if(NOT BUILD_TESTING)
    # Initialise the Raspberry Pi Pico SDK
    pico_sdk_init()
endif()

# ============================================================================
# Raspberry Pi Pico Build
# ============================================================================
if(NOT BUILD_TESTING)

# Core Logo interpreter library
add_library(logo_core
    core/lexer.c
    core/memory.c
    core/value.c
    core/variables.c
    core/procedures.c
    core/properties.c
    core/error.c
    core/eval.c
    core/primitives.c
    core/primitives_arithmetic.c
    core/primitives_control.c
    core/primitives_variables.c
    core/primitives_words_lists.c
    core/primitives_procedures.c
    core/primitives_workspace.c
    core/primitives_outside_world.c
    core/primitives_properties.c
    core/primitives_debug.c
    core/primitives_files.c
    core/primitives_hardware.c
    core/primitives_text.c
    core/primitives_turtle.c
    devices/console.c
    devices/hardware.c
    devices/io.c
    devices/storage.c
    devices/stream.c
)

target_include_directories(logo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(pico-logo
    devices/picocalc/clib.c
    devices/picocalc/fat32.c
    devices/picocalc/history.c
    devices/picocalc/input.c
    devices/picocalc/keyboard.c
    devices/picocalc/lcd.c
    devices/picocalc/picocalc_console.c
    devices/picocalc/picocalc_hardware.c
    devices/picocalc/picocalc_storage.c
    devices/picocalc/picocalc.c
    devices/picocalc/screen.c
    devices/picocalc/sdcard.c
    devices/picocalc/southbridge.c
    devices/picocalc/main.c
)

pico_set_program_name(pico-logo "pico-logo")
pico_set_program_version(pico-logo "0.1")


# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-logo 0)
pico_enable_stdio_usb(pico-logo 0)

# Turn on all warnings
# target_compile_options(pico-logo PRIVATE -Wall -Werror)


target_link_libraries(pico-logo
        logo_core
        pico_stdlib
        pico_printf
        pico_float
        pico_rand
        hardware_gpio
        hardware_i2c
        hardware_spi
        hardware_clocks
)

target_include_directories(pico-logo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

pico_add_extra_outputs(pico-logo)

endif() # NOT BUILD_TESTING

# ============================================================================
# Native Host Tests
# ============================================================================
if(BUILD_TESTING)
    # Coverage option
    option(ENABLE_COVERAGE "Enable code coverage" OFF)

    if(ENABLE_COVERAGE)
        message(STATUS "Code coverage enabled")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()

    enable_testing()

    # Host device library for tests (without Pico SDK dependencies)
    add_library(logo_devices_host
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/stream.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/console.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/hardware.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/storage.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/host/host_device.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/host/host_storage.c
    )

    target_include_directories(logo_devices_host PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Core library without device-specific code
    add_library(logo_core_host
        ${CMAKE_CURRENT_SOURCE_DIR}/core/lexer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/memory.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/value.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/variables.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/procedures.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/properties.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/error.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/eval.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_arithmetic.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_control.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_variables.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_words_lists.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_procedures.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_workspace.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_outside_world.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_properties.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_debug.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_files.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_hardware.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_text.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_turtle.c
    )

    target_include_directories(logo_core_host PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(logo_core_host logo_devices_host m)

    # Include all test subdirectory
    add_subdirectory(tests)
endif()
