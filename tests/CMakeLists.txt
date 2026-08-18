# Test configuration
# This file is included by the parent CMakeLists.txt when BUILD_TESTING=ON
# The parent already defines logo_core_host and logo_devices_host

# Test utilities library (includes mock device, scaffold, and unity)
add_library(test_utils
    mock_device.c
    test_scaffold.c
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
add_logo_test(test_eval_ops)
add_logo_test(test_frame)
add_logo_test(test_frame_arena)
add_logo_test(test_frame_sync)
add_logo_test(test_lexer)
add_logo_test(test_token_source)
add_logo_test(test_memory)
add_logo_test(test_value)
add_logo_test(test_variables)
add_logo_test(test_eval)
add_logo_test(test_error)
add_logo_test(test_mock_device)
add_logo_test(test_format)
add_logo_test(test_help)
add_logo_test(test_primitive_help_coverage)
# Primitive tests
add_logo_test(test_primitives_arithmetic)
add_logo_test(test_primitives_bitwise)
add_logo_test(test_primitives_conditionals)
add_logo_test(test_primitives_control_flow)
add_logo_test(test_primitives_debug_control)
add_logo_test(test_primitives_exceptions)
add_logo_test(test_primitives_editor)
add_logo_test(test_primitives_logical)
add_logo_test(test_primitives_variables)
add_logo_test(test_primitives_words_lists)
add_logo_test(test_primitives_procedures)
add_logo_test(test_primitives_workspace)
add_logo_test(test_primitives_outside_world)
add_logo_test(test_primitives_properties)
add_logo_test(test_primitives_json)
add_logo_test(test_primitives_debug)
add_logo_test(test_primitives_files)
add_logo_test(test_primitives_files_directory)
add_logo_test(test_primitives_files_load_save)
add_logo_test(test_primitives_text)
add_logo_test(test_primitives_turtle)
add_logo_test(test_tilemap)
add_logo_test(test_primitives_tilemap)
add_logo_test(test_demons)
add_logo_test(test_primitives_hardware)
add_logo_test(test_notation)
add_logo_test(test_sound)
add_logo_test(test_primitives_list_processing)
add_logo_test(test_primitives_wifi)
add_logo_test(test_primitives_network)
add_logo_test(test_httpd_device)
add_logo_test(test_httpd)
add_logo_test(test_primitives_http)
add_logo_test(test_time)
add_logo_test(test_io)
add_logo_test(test_stream)
add_logo_test(test_storage_router)
add_logo_test(test_lfs_storage)
add_logo_test(test_cross_fs_move)
add_logo_test(test_lfs_backup)
add_logo_test(test_repl)
add_logo_test(test_syntax_highlight)
# Graphics showcase test — loads logo/demos/graphics into the interpreter,
# then renders its static sprite and tilemap scenes on the mock device.
add_logo_test(test_graphics_demo)
target_compile_definitions(test_graphics_demo PRIVATE
    GRAPHICS_DEMO_SOURCE="${CMAKE_SOURCE_DIR}/logo/demos/graphics")

add_logo_test(test_xkcd2601)
target_compile_definitions(test_xkcd2601 PRIVATE
    XKCD2601_SOURCE="${CMAKE_SOURCE_DIR}/logo/samples/xkcd2601")

# Galaxian game test — loads logo/games/galaxian into the interpreter (proving
# it parses and its init path runs on the mock device) and checks the game's
# pure logic helpers. The source path is passed in as a compile definition.
add_logo_test(test_galaxian)
target_compile_definitions(test_galaxian PRIVATE
    GALAXIAN_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/galaxian"
    P10GAMES_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p10games")

# Space Invaders game test — the same treatment as Galaxian: the file loads and
# its init path runs on the mock device, the pure helpers are checked directly,
# and the frame is run for as long as a level lasts to prove it reclaims.
add_logo_test(test_invaders)
target_compile_definitions(test_invaders PRIVATE
    INVADERS_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/invaders"
    P10GAMES_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p10games")

# Asteroids (P11), M4: rocks, ship, shots, lives, the saucer and the sound. The
# game is pure Logo -- the file loads and its init path runs on the mock, and
# wrap, slot allocation, the split table and the outline walks are checked
# directly. The frame budget itself needs a board (logo/tests/p11m4).
add_logo_test(test_asteroids)
target_compile_definitions(test_asteroids PRIVATE
    ASTEROIDS_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/asteroids"
    P11M4_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p11m4")

# The P11 M0 timing harness, which decided how an Asteroids frame gets erased.
# A separate binary from test_asteroids because both Logo files define `place`
# and `draw.rock`, and the harness has to keep the shape it was measured in.
add_logo_test(test_p11rocks)
target_compile_definitions(test_p11rocks PRIVATE
    P11ROCKS_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p11rocks")

# P10 M0 interpreter throughput benchmark -- repeatable baseline plus a
# regression guard on relative numbers (interpreter-throughput-design.md §6).
add_logo_test(test_bench_throughput)
target_compile_definitions(test_bench_throughput PRIVATE
    CHECKRUN_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/checkrun"
    TRAILS_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/trails"
    GALAXIAN_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/galaxian"
    INVADERS_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/invaders"
    P10M0_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p10m0"
    # Every BENCH line is written here as well as to the terminal, so the
    # numbers can be pasted into a design doc instead of read off a screen.
    BENCH_REPORT="${CMAKE_BINARY_DIR}/bench-throughput.txt")

# Turtle Trails is a pure-Logo maze chase.  Its test loads the program and
# checks the encoded map, the deterministic 25 fps simulation, and the maze it
# carves from that same map.
add_logo_test(test_trails)
target_compile_definitions(test_trails PRIVATE
    TRAILS_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/trails"
    P9TRAILS_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p9trails"
    P10PROF_SOURCE="${CMAKE_SOURCE_DIR}/logo/tests/p10prof")

# Game HUD test — both games use the fullscreen graphics canvas and draw their
# score/lives/level text into the lower part of it.
add_logo_test(test_game_huds)
target_compile_definitions(test_game_huds PRIVATE
    INVADERS_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/invaders"
    GALAXIAN_SOURCE="${CMAKE_SOURCE_DIR}/logo/games/galaxian")

# File-server example test — loads logo/pfs into the interpreter and
# drives real HTTP requests through the mock connection. The source path is
# passed in as a compile definition.
add_logo_test(test_pfs)
target_compile_definitions(test_pfs PRIVATE
    PFS_PATH="${CMAKE_SOURCE_DIR}/logo/pfs")

# FAT32 driver test — compiles devices/picocalc/fat32.c on the host with a
# mock SD-card backend and tiny Pico SDK shim headers.  Independent of
# logo_core / test_utils because fat32.c has no Logo runtime dependencies.
add_executable(test_fat32
    test_fat32.c
    fat32_image.c
    mock_sdcard.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/fat32.c
    unity.c
)
target_include_directories(test_fat32 BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)
target_include_directories(test_fat32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_fat32 COMMAND test_fat32)

# Dirty-tile tracker test — compiles devices/picocalc/dirty_tiles.c on the
# host. Portable (no Pico SDK / Logo runtime dependencies).
add_executable(test_dirty_tiles
    test_dirty_tiles.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/dirty_tiles.c
    unity.c
)
target_include_directories(test_dirty_tiles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_dirty_tiles COMMAND test_dirty_tiles)

# Screen refresh policy test — compiles devices/picocalc/screen.c on the host
# against tests/fake_lcd.c, which records what reaches the panel.
add_executable(test_screen_refresh
    test_screen_refresh.c
    fake_lcd.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/screen.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/dirty_tiles.c
    ${CMAKE_SOURCE_DIR}/devices/stream.c
    unity.c
)
target_include_directories(test_screen_refresh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
# screen.c rounds with floorf; glibc keeps that in libm (macOS does not).
target_link_libraries(test_screen_refresh PRIVATE m)
add_test(NAME test_screen_refresh COMMAND test_screen_refresh)

# LCD scroll mapping test — the pure arithmetic behind the panel's vertical
# scroll, shared by the console, split mode and the editor.
add_executable(test_lcd_scroll
    test_lcd_scroll.c
    unity.c
)
target_include_directories(test_lcd_scroll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_lcd_scroll COMMAND test_lcd_scroll)

# Editor line lookup test — compiles devices/picocalc/editor_lines.c on the
# host. Portable (no Pico SDK / Logo runtime dependencies).
add_executable(test_editor_lines
    test_editor_lines.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_lines.c
    unity.c
)
target_include_directories(test_editor_lines PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_editor_lines COMMAND test_editor_lines)

# Editor incremental search test — compiles devices/picocalc/editor_search.c
# on the host. Portable (no Pico SDK / Logo runtime dependencies).
add_executable(test_editor_search
    test_editor_search.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_search.c
    unity.c
)
target_include_directories(test_editor_search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_editor_search COMMAND test_editor_search)

# Editor undo journal test — compiles devices/picocalc/editor_undo.c on the
# host. Portable (no Pico SDK / Logo runtime dependencies).
add_executable(test_editor_undo
    test_editor_undo.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_undo.c
    unity.c
)
target_include_directories(test_editor_undo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_editor_undo COMMAND test_editor_undo)

# Editor pattern matcher test — compiles devices/picocalc/editor_pattern.c on
# the host. It includes <regex.h> to run a differential against POSIX, which is
# host-only: no arm-none-eabi libc.a defines regcomp, so a device build could
# never follow it — which is the whole reason the matcher is vi's own (§16.1).
add_executable(test_editor_pattern
    test_editor_pattern.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_pattern.c
    unity.c
)
target_include_directories(test_editor_pattern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_editor_pattern COMMAND test_editor_pattern)

# Editor vi key layer test — compiles devices/picocalc/editor_vi.c (and the
# search and line-lookup modules it drives) on the host. editor.c itself has no
# host build, which is the whole reason the key layer is a separate file.
add_executable(test_editor_vi
    test_editor_vi.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_vi.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_pattern.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_search.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_lines.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/editor_undo.c
    unity.c
)
target_include_directories(test_editor_vi BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)
target_include_directories(test_editor_vi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_editor_vi COMMAND test_editor_vi)

# Colour costume pool test — compiles devices/picocalc/costumes.c on the
# host. Portable (no Pico SDK / Logo runtime dependencies).
add_executable(test_costumes
    test_costumes.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/costumes.c
    unity.c
)
target_include_directories(test_costumes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_costumes COMMAND test_costumes)

# Keyboard driver test — compiles devices/picocalc/keyboard.c on the host
# against a fake southbridge FIFO in the test file.
add_executable(test_keyboard
    test_keyboard.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/keyboard.c
    unity.c
)
target_include_directories(test_keyboard BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)
target_include_directories(test_keyboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_keyboard COMMAND test_keyboard)

# TLS heap allocator test — compiles devices/picocalc/tls_heap.c on the host.
# Portable (no Pico SDK / Logo runtime dependencies).
add_executable(test_tls_heap
    test_tls_heap.c
    ${CMAKE_SOURCE_DIR}/devices/picocalc/tls_heap.c
    unity.c
)
target_include_directories(test_tls_heap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/devices/picocalc
)
add_test(NAME test_tls_heap COMMAND test_tls_heap)

# mklfsimg round-trip test — builds an image from a host tree and restores it.
# Reuses the tool's core (tools/mklfsimg_lib.c) and the shared backup/restore +
# littlefs bundled in logo_devices.
add_executable(test_mklfsimg
    test_mklfsimg.c
    ${CMAKE_SOURCE_DIR}/tools/mklfsimg_lib.c
    unity.c
)
target_include_directories(test_mklfsimg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tools
)
target_link_libraries(test_mklfsimg logo_devices)
add_test(NAME test_mklfsimg COMMAND test_mklfsimg)
