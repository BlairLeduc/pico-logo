cmake_minimum_required(VERSION 3.13)

project(logo C)

# Use C11 for nice, modern-ish C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Coverage option
option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

enable_testing()

add_library(logo_devices_host
   devices/device.c
   devices/host/host_device.c
)

target_include_directories(logo_devices_host
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(logo_core
    core/lexer.c
    core/memory.c
    core/value.c
    core/variables.c
    core/procedures.c
    core/error.c
    core/eval.c
    core/primitives.c
    core/primitives_arithmetic.c
    core/primitives_control.c
    core/primitives_variables.c
    core/primitives_output.c
    core/primitives_words_lists.c
    core/primitives_procedures.c
    core/primitives_workspace.c
)

target_include_directories(logo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logo_core
    PUBLIC
        logo_devices_host
)

add_executable(logo
    main.c
)

target_include_directories(logo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logo
    PRIVATE
        logo_core
        logo_devices_host
)

# Test Executables
add_executable(test_lexer
    tests/test_lexer.c
    tests/unity.c
)

target_include_directories(test_lexer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_lexer PRIVATE logo_core)

add_test(NAME test_lexer COMMAND test_lexer)

add_executable(test_memory
    tests/test_memory.c
    tests/unity.c
)

target_include_directories(test_memory PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_memory PRIVATE logo_core)

add_test(NAME test_memory COMMAND test_memory)

add_executable(test_eval
    tests/test_eval.c
    tests/unity.c
)

target_include_directories(test_eval PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_eval PRIVATE logo_core)

add_test(NAME test_eval COMMAND test_eval)

# Primitive tests
add_executable(test_primitives_arithmetic
    tests/test_primitives_arithmetic.c
    tests/unity.c
)

target_include_directories(test_primitives_arithmetic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_primitives_arithmetic PRIVATE logo_core)

add_test(NAME test_primitives_arithmetic COMMAND test_primitives_arithmetic)

add_executable(test_primitives_control
    tests/test_primitives_control.c
    tests/unity.c
)

target_include_directories(test_primitives_control PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_primitives_control PRIVATE logo_core)

add_test(NAME test_primitives_control COMMAND test_primitives_control)

add_executable(test_primitives_variables
    tests/test_primitives_variables.c
    tests/unity.c
)

target_include_directories(test_primitives_variables PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_primitives_variables PRIVATE logo_core)

add_test(NAME test_primitives_variables COMMAND test_primitives_variables)

add_executable(test_primitives_words_lists
    tests/test_primitives_words_lists.c
    tests/unity.c
)

target_include_directories(test_primitives_words_lists PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_primitives_words_lists PRIVATE logo_core)

add_test(NAME test_primitives_words_lists COMMAND test_primitives_words_lists)

add_executable(test_primitives_procedures
    tests/test_primitives_procedures.c
    tests/unity.c
)

target_include_directories(test_primitives_procedures PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_primitives_procedures PRIVATE logo_core)

add_test(NAME test_primitives_procedures COMMAND test_primitives_procedures)

add_executable(test_primitives_workspace
    tests/test_primitives_workspace.c
    tests/unity.c
)

target_include_directories(test_primitives_workspace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_primitives_workspace PRIVATE logo_core)

add_test(NAME test_primitives_workspace COMMAND test_primitives_workspace)

# Coverage target
if(ENABLE_COVERAGE)
    find_program(LCOV lcov)
    
    if(LCOV)
        add_custom_target(coverage
            COMMAND find ${CMAKE_BINARY_DIR} -name '*.gcda' -delete
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${LCOV} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '*/tests/*' '/usr/*' --output-file coverage.info
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating coverage report"
            DEPENDS test_lexer test_memory
        )
        
        add_custom_target(coverage-clean
            COMMAND ${CMAKE_COMMAND} -E remove coverage.info
            COMMAND find ${CMAKE_BINARY_DIR} -name '*.gcda' -delete
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Cleaning coverage data"
        )
    else()
        message(WARNING "lcov not found. Coverage target will not be available.")
    endif()
endif()
