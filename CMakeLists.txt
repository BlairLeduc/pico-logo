# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to build tests instead of Pico target
option(BUILD_TESTING "Build native host tests instead of Pico target" OFF)
option(BUILD_FOR_HOST "Build for native host instead of Pico target" OFF)

# If building tests or building for host, skip all Pico SDK setup
if(NOT BUILD_TESTING AND NOT BUILD_FOR_HOST)
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
elseif(BUILD_FOR_HOST)
    # Native build for host
    project(logo C)
else()
    # Native build for tests
    project(pico-logo-tests C)
endif()

if(NOT BUILD_TESTING AND NOT BUILD_FOR_HOST)
    # Initialise the Raspberry Pi Pico SDK
    pico_sdk_init()
endif()

# ============================================================================
# Raspberry Pi Pico Build
# ============================================================================
if(NOT BUILD_TESTING AND NOT BUILD_FOR_HOST)

# Core Logo interpreter library
add_library(logo_core
    core/error.c
    core/eval.c
    core/format.c
    core/frame.c
    core/frame_arena.c
    core/lexer.c
    core/memory.c
    core/primitives_arithmetic.c
    core/primitives_conditionals.c
    core/primitives_control_flow.c
    core/primitives_control_reset.c
    core/primitives_debug_control.c
    core/primitives_debug.c
    core/primitives_editor.c
    core/primitives_exceptions.c
    core/primitives_files.c
    core/primitives_hardware.c
    core/primitives_logical.c
    core/primitives_bitwise.c
    core/primitives_outside_world.c
    core/primitives_procedures.c
    core/primitives_properties.c
    core/primitives_text.c
    core/primitives_turtle.c
    core/primitives_variables.c
    core/primitives_words_lists.c
    core/primitives_workspace.c
    core/primitives_list_processing.c
    core/primitives_wifi.c
    core/primitives.c
    core/procedures.c
    core/properties.c
    core/repl.c
    core/token_source.c
    core/value.c
    core/variables.c
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

# Allow LOGO_MEMORY_SIZE to be set via CMake cache variable
if(DEFINED LOGO_MEMORY_SIZE)
    target_compile_definitions(logo_core PUBLIC LOGO_MEMORY_SIZE=${LOGO_MEMORY_SIZE})
endif()

# Allow LOGO_EDITOR_BUFFER_SIZE to be set via CMake cache variable
if(DEFINED LOGO_EDITOR_BUFFER_SIZE)
    target_compile_definitions(logo_core PUBLIC LOGO_EDITOR_BUFFER_SIZE=${LOGO_EDITOR_BUFFER_SIZE})
endif()

# WiFi support (only for Pico W boards)
option(LOGO_HAS_WIFI "Enable WiFi support for Pico W boards" OFF)
if(LOGO_HAS_WIFI)
    message(STATUS "WiFi support enabled")
    target_compile_definitions(logo_core PUBLIC LOGO_HAS_WIFI=1)
endif()

add_executable(pico-logo
    devices/picocalc/clib.c
    devices/picocalc/audio.c
    devices/picocalc/editor.c
    devices/picocalc/fat32.c
    devices/picocalc/history.c
    devices/picocalc/input.c
    devices/picocalc/keyboard.c
    devices/picocalc/lcd.c
    devices/picocalc/main.c
    devices/picocalc/picocalc_console.c
    devices/picocalc/picocalc_hardware.c
    devices/picocalc/picocalc_storage.c
    devices/picocalc/picocalc.c
    devices/picocalc/screen.c
    devices/picocalc/screensaver.c
    devices/picocalc/sdcard.c
    devices/picocalc/southbridge.c
)

pico_set_program_name(pico-logo "pico-logo")
pico_set_program_version(pico-logo "0.1")

# Generate PIO header
pico_generate_pio_header(pico-logo ${CMAKE_CURRENT_LIST_DIR}/devices/picocalc/audio.pio)

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
        hardware_clocks
        hardware_gpio
        hardware_i2c
        hardware_pio
        hardware_spi
)

# Link WiFi libraries if WiFi support is enabled
if(LOGO_HAS_WIFI)
    # Add the directory containing lwipopts.h to include path
    target_include_directories(pico-logo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/picocalc
    )
    target_link_libraries(pico-logo
        pico_cyw43_arch_lwip_threadsafe_background
    )
endif()

target_include_directories(pico-logo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Allow LOGO_COPY_BUFFER_SIZE to be set via CMake cache variable
if(DEFINED LOGO_COPY_BUFFER_SIZE)
    target_compile_definitions(pico-logo PRIVATE LOGO_COPY_BUFFER_SIZE=${LOGO_COPY_BUFFER_SIZE})
endif()

# Set C stack size (default Pico SDK is 0x800 = 2KB)
# Default to 4KB.
# Note: this project uses a custom linker script that places the core 0 stack in main RAM,
# so the stack size is no longer constrained by the 4KB SCRATCH_Y region.
if(DEFINED PICO_STACK_SIZE)
    target_compile_definitions(pico-logo PRIVATE PICO_STACK_SIZE=${PICO_STACK_SIZE})
else()
    target_compile_definitions(pico-logo PRIVATE PICO_STACK_SIZE=0x1000)
endif()

target_link_options(pico-logo PRIVATE -Wl,--print-memory-usage)

# Use project-local memmap to place the core 0 stack in main RAM (not SCRATCH_Y).
# This frees SCRATCH_Y for other uses while keeping the default stack size policy.
if(PICO_RP2040)
    pico_set_linker_script(pico-logo ${CMAKE_CURRENT_LIST_DIR}/linker/rp2040/memmap_default_stack_in_ram.ld)
elseif(PICO_RP2350)
    pico_set_linker_script(pico-logo ${CMAKE_CURRENT_LIST_DIR}/linker/rp2350/memmap_default_stack_in_ram.ld)
endif()

pico_add_extra_outputs(pico-logo)

endif() # NOT BUILD_TESTING AND NOT BUILD_FOR_HOST

# ============================================================================
# Native Host Build
# ============================================================================

if(BUILD_FOR_HOST)

    # Host device library for tests (without Pico SDK dependencies)
    add_library(logo_devices
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/console.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/hardware.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/storage.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/stream.c
    )

    target_include_directories(logo_devices PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )


    # Core Logo interpreter library for host (with host device implementations)
    add_library(logo_core
        core/error.c
        core/eval.c
        core/format.c
        core/frame.c
        core/frame_arena.c
        core/lexer.c
        core/memory.c
        core/primitives_arithmetic.c
        core/primitives_conditionals.c
        core/primitives_control_flow.c
        core/primitives_control_reset.c
        core/primitives_debug_control.c
        core/primitives_debug.c
        core/primitives_editor.c
        core/primitives_exceptions.c
        core/primitives_files.c
        core/primitives_hardware.c
        core/primitives_logical.c
        core/primitives_bitwise.c
        core/primitives_outside_world.c
        core/primitives_procedures.c
        core/primitives_properties.c
        core/primitives_text.c
        core/primitives_turtle.c
        core/primitives_variables.c
        core/primitives_words_lists.c
        core/primitives_workspace.c
        core/primitives_list_processing.c
        core/primitives.c
        core/procedures.c
        core/properties.c
        core/repl.c
        core/token_source.c
        core/value.c
        core/variables.c
    )

    target_include_directories(logo_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_executable(logo
        devices/host/host_console.c
        devices/host/host_hardware.c
        devices/host/host_main.c
        devices/host/host_storage.c
    )

    target_link_libraries(logo
        logo_core
        logo_devices
        m
    )

    target_include_directories(logo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

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

        # Find required tools
        find_program(LCOV lcov REQUIRED)
        find_program(GENHTML genhtml REQUIRED)

        # Add coverage target
        add_custom_target(coverage
            # Clean previous coverage data
            COMMAND ${LCOV} --directory . --zerocounters
            # Run tests
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            # Capture coverage data
            COMMAND ${LCOV} --directory . --capture --output-file coverage.info
            # Remove system headers and test files from coverage
            COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
            # Generate HTML report
            COMMAND ${GENHTML} coverage.info --output-directory coverage-report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating coverage report..."
        )
    endif()

    enable_testing()

    # Host device library for tests (without Pico SDK dependencies)
    add_library(logo_devices
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/console.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/hardware.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/storage.c
        ${CMAKE_CURRENT_SOURCE_DIR}/devices/stream.c
    )

    target_include_directories(logo_devices PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Core library without device-specific code
    add_library(logo_core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/error.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/eval.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/format.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/frame.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/frame_arena.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/lexer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/memory.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_arithmetic.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_conditionals.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_control_flow.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_control_reset.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_debug_control.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_debug.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_editor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_exceptions.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_files.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_hardware.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_logical.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_bitwise.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_outside_world.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_procedures.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_properties.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_text.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_turtle.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_variables.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_words_lists.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_workspace.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_list_processing.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives_wifi.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/primitives.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/procedures.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/properties.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/repl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/token_source.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/value.c
        ${CMAKE_CURRENT_SOURCE_DIR}/core/variables.c
    )

    target_include_directories(logo_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(logo_core logo_devices m)

    # Include the tests subdirectory
    add_subdirectory(tests)
endif()
