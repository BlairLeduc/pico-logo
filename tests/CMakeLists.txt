# Test configuration
# This file is included by the parent CMakeLists.txt when BUILD_TESTING=ON
# The parent already defines logo_core_host and logo_devices_host

# Test utilities library (includes mock device and unity)
add_library(test_utils
    mock_device.c
    unity.c
)

target_include_directories(test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_utils PUBLIC logo_core)

# Macro to define a test executable
macro(add_logo_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.c)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${TEST_NAME} PRIVATE test_utils)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# Core tests
add_logo_test(test_lexer)
add_logo_test(test_memory)
add_logo_test(test_value)
add_logo_test(test_eval)
add_logo_test(test_error)
add_logo_test(test_mock_device)
add_logo_test(test_format)

# Primitive tests
add_logo_test(test_primitives_arithmetic)
add_logo_test(test_primitives_control)
add_logo_test(test_primitives_editor)
add_logo_test(test_primitives_logical)
add_logo_test(test_primitives_variables)
add_logo_test(test_primitives_words_lists)
add_logo_test(test_primitives_procedures)
add_logo_test(test_primitives_workspace)
add_logo_test(test_primitives_outside_world)
add_logo_test(test_primitives_properties)
add_logo_test(test_primitives_debug)
add_logo_test(test_primitives_files)
add_logo_test(test_primitives_text)
add_logo_test(test_primitives_turtle)
add_logo_test(test_primitives_hardware)
add_logo_test(test_io)
add_logo_test(test_repl)
