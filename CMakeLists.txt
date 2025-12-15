# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Coverage option
# option(ENABLE_COVERAGE "Enable code coverage" OFF)

# if(ENABLE_COVERAGE)
#     message(STATUS "Code coverage enabled")
#     add_compile_options(--coverage -O0 -g)
#     add_link_options(--coverage)
# endif()

# enable_testing()

add_executable(pico-logo
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
    main.c
)

pico_set_program_name(pico-logo "pico-logo")
pico_set_program_version(pico-logo "0.1")


# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-logo 0)
pico_enable_stdio_usb(pico-logo 0)

# Turn on all warnings
# target_compile_options(pico-logo PRIVATE -Wall -Werror)


target_link_libraries(pico-logo
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


# # Test Executables

# # Test utilities library (includes mock device)
# add_library(test_utils
#     tests/mock_device.c
#     tests/unity.c
# )

# target_include_directories(test_utils PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_utils PUBLIC logo_core m)

# add_executable(test_lexer
#     tests/test_lexer.c
# )

# target_include_directories(test_lexer PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_lexer PRIVATE test_utils)

# add_test(NAME test_lexer COMMAND test_lexer)

# add_executable(test_memory
#     tests/test_memory.c
# )

# target_include_directories(test_memory PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_memory PRIVATE test_utils)

# add_test(NAME test_memory COMMAND test_memory)

# add_executable(test_eval
#     tests/test_eval.c
# )

# target_include_directories(test_eval PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_eval PRIVATE test_utils)

# add_test(NAME test_eval COMMAND test_eval)

# # Primitive tests
# add_executable(test_primitives_arithmetic
#     tests/test_primitives_arithmetic.c
# )

# target_include_directories(test_primitives_arithmetic PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_arithmetic PRIVATE test_utils)

# add_test(NAME test_primitives_arithmetic COMMAND test_primitives_arithmetic)

# add_executable(test_primitives_control
#     tests/test_primitives_control.c
# )

# target_include_directories(test_primitives_control PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_control PRIVATE test_utils)

# add_test(NAME test_primitives_control COMMAND test_primitives_control)

# add_executable(test_primitives_variables
#     tests/test_primitives_variables.c
# )

# target_include_directories(test_primitives_variables PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_variables PRIVATE test_utils)

# add_test(NAME test_primitives_variables COMMAND test_primitives_variables)

# add_executable(test_primitives_words_lists
#     tests/test_primitives_words_lists.c
# )

# target_include_directories(test_primitives_words_lists PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_words_lists PRIVATE test_utils)

# add_test(NAME test_primitives_words_lists COMMAND test_primitives_words_lists)

# add_executable(test_primitives_procedures
#     tests/test_primitives_procedures.c
# )

# target_include_directories(test_primitives_procedures PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_procedures PRIVATE test_utils)

# add_test(NAME test_primitives_procedures COMMAND test_primitives_procedures)

# add_executable(test_primitives_workspace
#     tests/test_primitives_workspace.c
# )

# target_include_directories(test_primitives_workspace PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_workspace PRIVATE test_utils)

# add_test(NAME test_primitives_workspace COMMAND test_primitives_workspace)

# add_executable(test_primitives_outside_world
#     tests/test_primitives_outside_world.c
# )

# target_include_directories(test_primitives_outside_world PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_outside_world PRIVATE test_utils)

# add_test(NAME test_primitives_outside_world COMMAND test_primitives_outside_world)

# add_executable(test_primitives_properties
#     tests/test_primitives_properties.c
# )

# target_include_directories(test_primitives_properties PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_properties PRIVATE test_utils)

# add_test(NAME test_primitives_properties COMMAND test_primitives_properties)

# add_executable(test_primitives_debug
#     tests/test_primitives_debug.c
# )

# target_include_directories(test_primitives_debug PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_debug PRIVATE test_utils)

# add_test(NAME test_primitives_debug COMMAND test_primitives_debug)

# add_executable(test_primitives_files
#     tests/test_primitives_files.c
# )

# target_include_directories(test_primitives_files PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_files PRIVATE test_utils)

# add_test(NAME test_primitives_files COMMAND test_primitives_files)

# # Mock device test
# add_executable(test_mock_device
#     tests/test_mock_device.c
# )

# target_include_directories(test_mock_device PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_mock_device PRIVATE test_utils)

# add_test(NAME test_mock_device COMMAND test_mock_device)

# # Text primitives test
# add_executable(test_primitives_text
#     tests/test_primitives_text.c
# )

# target_include_directories(test_primitives_text PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_text PRIVATE test_utils)

# add_test(NAME test_primitives_text COMMAND test_primitives_text)

# # Turtle primitives test
# add_executable(test_primitives_turtle
#     tests/test_primitives_turtle.c
# )

# target_include_directories(test_primitives_turtle PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/tests
# )

# target_link_libraries(test_primitives_turtle PRIVATE test_utils)

# add_test(NAME test_primitives_turtle COMMAND test_primitives_turtle)


# # Coverage target
# if(ENABLE_COVERAGE)
#     find_program(LCOV lcov)
    
#     if(LCOV)
#         add_custom_target(coverage
#             COMMAND find ${CMAKE_BINARY_DIR} -name '*.gcda' -delete
#             COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
#             COMMAND ${LCOV} --capture --directory . --output-file coverage.info
#             COMMAND ${LCOV} --remove coverage.info '*/tests/*' '/usr/*' --output-file coverage.info
#             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#             COMMENT "Running tests and generating coverage report"
#             DEPENDS test_lexer test_memory
#         )
        
#         add_custom_target(coverage-clean
#             COMMAND ${CMAKE_COMMAND} -E remove coverage.info
#             COMMAND find ${CMAKE_BINARY_DIR} -name '*.gcda' -delete
#             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#             COMMENT "Cleaning coverage data"
#         )
#     else()
#         message(WARNING "lcov not found. Coverage target will not be available.")
#     endif()
# endif()
