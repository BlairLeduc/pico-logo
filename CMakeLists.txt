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

add_library(logo_core
    core/lexer.c
    core/memory.c
)

target_include_directories(logo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(logo_devices_host
   devices/device.c
   devices/host/host_device.c
)

target_include_directories(logo_devices_host
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
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
