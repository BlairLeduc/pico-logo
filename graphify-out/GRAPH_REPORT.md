# Graph Report - pico-logo  (2026-08-01)

## Corpus Check
- 287 files · ~489,289 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 7075 nodes · 22457 edges · 214 communities (204 shown, 10 thin omitted)
- Extraction: 56% EXTRACTED · 44% INFERRED · 0% AMBIGUOUS · INFERRED: 9968 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `02c4c29f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- run_string
- lfs.c
- primitives_httpd.c
- test_value.c
- mem_is_nil
- reset_output
- eval_string
- result_none
- value_to_string
- test_trails.c
- iteration_callback
- io.c
- test_primitives_files_directory.c
- picocalc_console.c
- format_buffer_init
- lexer_init
- arena_alloc_words
- test_io.c
- syntax_highlight_line
- test_variables.c
- primitives_sound.c
- unity.c
- test_httpd.c
- test_eval.c
- Turtle Graphics
- test_scaffold_setUp
- test_primitives_http.c
- fat32.c
- southbridge.c
- mock_device.c
- test_primitives_json.c
- test_primitives_conditionals.c
- test_scaffold_tearDown
- test_notation.c
- primitives_init
- stdlib.h
- mem_atom_cstr
- lexer_next_token
- eval_push_if
- test_cross_fs_move.c
- test_primitives_wifi.c
- test_mock_device.c
- primitives_get_io
- test_primitives_outside_world.c
- test_time.c
- set_mock_input
- picocalc_editor_edit
- primitives_properties.c
- repository
- test_primitives_hardware.c
- Turtle Trails (design)
- httpd.c
- stream.c
- lcd.c
- ;
- Checkpoint Run — a maze-driving game (design)
- test_sound.c
- test_primitives_files_load_save.c
- picocalc_hardware.c
- fat32_close
- test_primitives_network.c
- memory.c
- test_fileserver.c
- primitives_json.c
- Conditionals and Control of Flow
- test_mock_fs.h
- demons_poll
- test_dirty_tiles.c
- Words and Lists
- op_stack_push
- mem_is_word
- primitives.h
- lfs_storage.c
- mock_sdcard.c
- screen.c
- picocalc_storage.c
- proc_define_from_text
- lexer.c
- main
- test_storage_router.c
- test_frame.c
- host_storage.c
- test_costumes.c
- step_proc_call
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- main
- mock_device_get_state
- test_frame_arena.c
- prim_savel
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- primitives_files.c
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- test_token_source.c
- host_hardware.c
- sdcard.c
- Managing your Workspace
- repl_evaluate_line
- unity.h
- eval.c
- test_tls_heap.c
- storage_router.c
- test_galaxian.c
- mem_word_ptr
- prim_print
- mem_atom
- primitives_workspace.c
- P9 — Tile maps and smooth scrolling (design)
- eval_primary
- Galaxian in Pico Logo (design)
- File Management
- primitives_http.c
- test_scaffold.c
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- test_primitives_properties.c
- Design: `launch` background processes (P6)
- test_checkrun.c
- test_bench_throughput.c
- mklfsimg_lib.c
- roadmap.md
- sound.c
- Using the Logo Editor
- HTTP Server
- prim_define
- arena_offset_to_ptr
- Modifying Procedures Under Program Control
- WiFi Management
- P10 — Interpreter throughput (design)
- The pick of five: plans
- primitives.c
- Managing Various Files
- logo_lfs_backup
- ms_to_datetime
- prim_not
- Text and Screen Commands
- The Outside World
- HTTP server (design)
- record_command
- PR Review Checklist (CRITICAL)
- test_primitives_editor.c
- record_command_float
- test_lfs_backup.c
- frame_stack_depth
- Atom Garbage Collection: Implementation Plan
- mem_blob
- MockCommandType
- Introduction
- as_httpd_conn
- Appendix A: Useful Tools
- primitives_control_flow.c
- test_primitives_variables.c
- picocalc_network_tls_connect
- procedures.c
- LogoEditorResult
- mock_device_get_dot
- Bitwise Operations
- primitives_debug_control.c
- Pico Logo
- value_number
- drain_tokens
- mock_sound_queue
- LogoStream
- gen_ca_certs.py
- logo_io_close_all
- pandoc_slug
- mock_sound_status
- primitives_bitwise.c
- logo_lfs_storage_init
- dist.sh
- logo_io_copy_file
- generate_help.sh
- run_e2e.sh
- mock_wifi_status
- VENDOR.md
- ensure_wifi_initialized
- prim_setdate
- List Processing
- primitives_variables.c
- mock_device_set_raster
- JSON
- bd_prog
- test_httpd_device.c
- httpd_listening
- logo_console_init
- arena_used
- run_editor_and_process
- ip_addr_t
- Appendix B: Parsing
- Variables
- bd_prog
- word_of
- bd_prog
- logo_storage_router_init

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 945 edges
2. `eval_string()` - 914 edges
3. `mem_word_ptr()` - 445 edges
4. `mem_atom()` - 241 edges
5. `mem_is_nil()` - 239 edges
6. `value_to_string()` - 201 edges
7. `result_error_arg()` - 195 edges
8. `result_none()` - 194 edges
9. `result_ok()` - 176 edges
10. `lexer_init()` - 173 edges

## Surprising Connections (you probably didn't know these)
- `test_nil_is_not_word()` --calls--> `mem_is_word()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_number_content()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_negative()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_type()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_zero()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Communities (214 total, 10 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (266): MockLine, mock_device_get_line(), mock_device_get_output(), mock_device_has_line_from_to(), mock_device_line_count(), mock_device_paint_canvas(), mock_device_set_canvas_point(), test_turtle_draw_line_when_pen_down() (+258 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "primitives_httpd.c"
Cohesion: 0.25
Nodes (24): mem_word(), Evaluator, Result, Value, el_append(), el_append_cstr(), el_append_word(), no_request() (+16 more)

### Community 3 - "test_value.c"
Cohesion: 0.03
Nodes (156): format_number(), Node, Result, Value, extract_number_list(), Result, Value, result_eof() (+148 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (116): mem_car(), mem_cdr(), mem_is_nil(), Lexer, parse_list_body(), parse_list_from_string(), Node, Value (+108 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (176): proc_is_stepped(), proc_is_traced(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic(), test_do_until_invalid_predicate() (+168 more)

### Community 6 - "eval_string"
Cohesion: 0.02
Nodes (166): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+158 more)

### Community 7 - "result_none"
Cohesion: 0.08
Nodes (112): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator, Result, Value (+104 more)

### Community 8 - "value_to_string"
Cohesion: 0.11
Nodes (91): Node, demons_print(), demons_set(), number_to_word(), mem_list_append(), mem_word_len(), prim_form(), prim_wait() (+83 more)

### Community 9 - "test_trails.c"
Cohesion: 0.11
Nodes (66): mock_device_clear_graphics(), actor(), load_trails(), num(), numf(), put_actor(), read_map(), run() (+58 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.09
Nodes (62): Result, demons_maybe_poll(), demons_running(), eval_instruction(), httpd_maybe_poll(), LogoIO, SyntaxCategory, highlight_write_span() (+54 more)

### Community 12 - "test_primitives_files_directory.c"
Cohesion: 0.07
Nodes (35): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+27 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.05
Nodes (69): picocalc_editor_get_ops(), keyboard_set_idle_callback(), lcd_restore_palette(), lcd_set_palette_rgb(), LogoConsole, LogoPen, LogoRotationStyle, LogoStream (+61 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.07
Nodes (85): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_init(), format_buffer_output() (+77 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "arena_alloc_words"
Cohesion: 0.15
Nodes (27): arena_alloc_words(), arena_free_to(), arena_is_empty(), arena_is_top_allocation(), arena_top(), FrameArena, word_offset_t, test_alloc_sequential_offsets() (+19 more)

### Community 17 - "test_io.c"
Cohesion: 0.04
Nodes (62): logo_io_cleanup(), logo_io_is_dribbling(), logo_io_parse_network_address(), logo_io_set_reader(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble(), LogoDirCallback (+54 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.06
Nodes (81): FrameStack, proc_get_frame_stack(), Value, find_global(), var_bury(), var_bury_all(), var_declare_local(), var_erase() (+73 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.12
Nodes (41): httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_queue_connection(), mock_httpd_set_read_hook(), pump(), resp_str(), responded(), serve() (+33 more)

### Community 23 - "test_eval.c"
Cohesion: 0.02
Nodes (107): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+99 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.06
Nodes (54): primitives_control_reset_test_state(), primitives_set_io(), properties_init(), variables_init(), LogoConsole, LogoHardware, LogoStorage, logo_io_init() (+46 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (68): logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), test_client_get_does_not_disturb_pending_request() (+60 more)

### Community 27 - "fat32.c"
Cohesion: 0.13
Nodes (54): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+46 more)

### Community 28 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (16): mock_device_add_wifi_scan_result(), mock_device_clear_wifi_scan_results(), mock_device_get_tcp_request_len(), mock_device_set_input(), mock_device_set_snap_result(), mock_device_set_tcp_close_after(), mock_device_set_tcp_read_chunk(), mock_device_set_tcp_write_chunk() (+8 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (62): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+54 more)

### Community 31 - "test_primitives_conditionals.c"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 32 - "test_scaffold_tearDown"
Cohesion: 0.05
Nodes (39): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown() (+31 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.12
Nodes (31): demons_clear(), demons_reset(), primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_init(), primitives_editor_init() (+23 more)

### Community 35 - "stdlib.h"
Cohesion: 0.07
Nodes (25): keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_set_background_poll(), keyboard_set_key_available_callback(), lcd_get_palette_value(), lcd_set_palette_value() (+17 more)

### Community 36 - "mem_atom_cstr"
Cohesion: 0.14
Nodes (34): mem_atom_cstr(), Evaluator, Result, Value, prim_catch(), prim_error(), prim_throw(), prim_toplevel() (+26 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "eval_push_if"
Cohesion: 0.48
Nodes (11): eval_push_if(), Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse() (+3 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.11
Nodes (15): MemFile, LogoDirCallback, LogoStream, mem_close(), mem_create(), mem_file_delete(), mem_file_exists(), mem_file_size() (+7 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.05
Nodes (56): mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_ssid(), mock_device_set_wifi_status(), test_listen_errors_when_wifi_not_connected() (+48 more)

### Community 41 - "test_mock_device.c"
Cohesion: 0.10
Nodes (35): LogoConsole, mock_device_dot_count(), mock_device_get_console(), mock_device_has_dot_at(), mock_device_verify_heading(), mock_device_verify_position(), test_background_colour(), test_dot_at_query() (+27 more)

### Community 42 - "primitives_get_io"
Cohesion: 0.16
Nodes (35): CatalogContext, CatalogEntry, LogoIO, Evaluator, LogoEntryType, LogoIO, Result, Value (+27 more)

### Community 43 - "test_primitives_outside_world.c"
Cohesion: 0.08
Nodes (24): tearDown(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number(), test_print_word() (+16 more)

### Community 44 - "test_time.c"
Cohesion: 0.06
Nodes (48): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+40 more)

### Community 45 - "set_mock_input"
Cohesion: 0.15
Nodes (44): prim_pause(), LogoIO, repl_cleanup(), repl_extract_proc_name(), repl_init(), repl_run(), ReplFlags, test_repl_defines_proc_with_multiline_paren() (+36 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "primitives_properties.c"
Cohesion: 0.39
Nodes (11): Evaluator, Result, Value, prim_erprops(), prim_gprop(), prim_plist(), prim_pprop(), prim_pps() (+3 more)

### Community 48 - "repository"
Cohesion: 0.04
Nodes (45): name, name, match, name, 1, 2, match, name (+37 more)

### Community 49 - "test_primitives_hardware.c"
Cohesion: 0.06
Nodes (45): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+37 more)

### Community 50 - "Turtle Trails (design)"
Cohesion: 0.06
Nodes (31): 10. Main loop and state order, 11. Memory and performance budget, 12. Design boundaries, 13. Tests, 14. Implementation milestones, 15. As built: divergences from this design, 1. Theme, 2. Display and board geometry (+23 more)

### Community 51 - "httpd.c"
Cohesion: 0.12
Nodes (36): LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find(), httpd_body() (+28 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (37): LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error(), logo_stream_close() (+29 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (35): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+27 more)

### Community 54 - ";"
Cohesion: 0.04
Nodes (52): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Break (+44 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.10
Nodes (31): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), test_env_default(), test_play_appends(), test_play_bad_notation_errors() (+23 more)

### Community 57 - "test_primitives_files_load_save.c"
Cohesion: 0.05
Nodes (61): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle() (+53 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.13
Nodes (43): repeating_timer_t, fat32_close(), fat32_create(), fat32_delete(), fat32_is_mounted(), fat32_mount(), fat32_open(), fat32_read() (+35 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (34): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+26 more)

### Community 61 - "memory.c"
Cohesion: 0.09
Nodes (49): BlobDesc, demons_gc_mark_all(), op_stack_gc_mark(), alloc_cell(), atom_chain_next(), atom_clear_marks(), atom_entry_is_free(), atom_entry_next() (+41 more)

### Community 62 - "test_fileserver.c"
Cohesion: 0.19
Nodes (25): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+17 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.10
Nodes (33): LogoDirCallback, LogoStream, MockFile, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos() (+25 more)

### Community 66 - "demons_poll"
Cohesion: 0.15
Nodes (27): demons_frozen(), demons_poll(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle(), test_action_does_not_reenter_poll(), test_cleardemons_disarms_all(), test_cleardemons_leaves_motion_and_freeze() (+19 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.12
Nodes (34): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+26 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "op_stack_push"
Cohesion: 0.14
Nodes (31): EvalOp, Value, mark_value(), op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init(), op_stack_insert() (+23 more)

### Community 70 - "mem_is_word"
Cohesion: 0.18
Nodes (40): mem_atom_unescape(), mem_first_cell(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_is_word(), mem_next_cell(), prim_for(), Evaluator (+32 more)

### Community 71 - "primitives.h"
Cohesion: 0.11
Nodes (20): Value, demons_freeze(), demons_resume(), demons_suspend(), demons_thaw(), value_is_true(), Evaluator, Result (+12 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (19): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+11 more)

### Community 74 - "screen.c"
Cohesion: 0.08
Nodes (37): lcd_enable_cursor(), lcd_move_cursor(), lcd_set_background(), lcd_set_cursor_char(), text_get_background(), text_get_foreground(), text_set_background(), text_set_cursor() (+29 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.13
Nodes (29): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_dir_create() (+21 more)

### Community 76 - "proc_define_from_text"
Cohesion: 0.03
Nodes (94): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), proc_define_from_text(), token_to_atom(), test_deep_nested_proc_in_repeat() (+86 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "main"
Cohesion: 0.09
Nodes (24): proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), main(), psram_verify(), m1_capture(), m1_equal(), picocalc_flash_erase() (+16 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "test_frame.c"
Cohesion: 0.07
Nodes (92): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+84 more)

### Community 81 - "host_storage.c"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "step_proc_call"
Cohesion: 0.21
Nodes (30): op_stack_pop(), EvalOp, EvalOpKind, Evaluator, Node, Result, UserProcedure, eval_trace_entry() (+22 more)

### Community 84 - "test_lfs_storage.c"
Cohesion: 0.16
Nodes (12): Listing, LogoEntryType, list_cb(), listing_has(), test_delete_and_rename(), test_dir_delete_nonempty_fails(), test_file_exists_and_size(), test_list_directory_and_filter() (+4 more)

### Community 85 - "Code Review — 2026-07-02"
Cohesion: 0.08
Nodes (23): 1. Confirmed bug: `recycle` sweeps reachable data, 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate), 2.2 `find_atom` is a linear scan of the whole atom table, 2.3 Smaller items, 2. Hot-path efficiency, 3. Robustness: `mem_cons` failures are silently ignored, 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict, 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision (+15 more)

### Community 86 - "Contributing"
Cohesion: 0.08
Nodes (23): About Logo, Additional Features for the PicoCalc, Advanced Logo, Beginning Logo, Building and Running, Contributing, Credits, Dependencies (+15 more)

### Community 87 - "main"
Cohesion: 0.13
Nodes (21): LogoHardware, LogoHardwareOps, logo_hardware_init(), LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars() (+13 more)

### Community 88 - "mock_device_get_state"
Cohesion: 0.04
Nodes (78): MockCommand, MockDeviceState, mock_device_clear_commands(), mock_device_command_count(), mock_device_get_command(), mock_device_get_state(), mock_device_last_command(), assert_fullscreen_canvas_hud() (+70 more)

### Community 89 - "test_frame_arena.c"
Cohesion: 0.18
Nodes (20): arena_available(), arena_capacity(), arena_init(), setUp(), test_alloc_exact_capacity(), test_alloc_exhausts_arena(), test_alloc_just_over_available_fails(), test_alloc_too_large_returns_none() (+12 more)

### Community 90 - "prim_savel"
Cohesion: 0.34
Nodes (18): httpd_savebody(), prim_editfile(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile() (+10 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.17
Nodes (17): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+9 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.08
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (implemented)"
Cohesion: 0.06
Nodes (31): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+23 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "primitives_files.c"
Cohesion: 0.17
Nodes (33): Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble(), prim_filelen() (+25 more)

### Community 96 - "Space Invaders in Pico Logo (design & implementation)"
Cohesion: 0.09
Nodes (22): 10. Why this is a good P5 acceptance test, 11. Deliverable, 1. The board, 2. Object representation — the central decision, 3. The alien formation on the canvas, 4. Collision routing — demons vs. the game loop, 5. Global events as demons, 6. Input (+14 more)

### Community 97 - "package.json"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 98 - "test_mklfsimg.c"
Cohesion: 0.14
Nodes (17): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+9 more)

### Community 99 - "test_token_source.c"
Cohesion: 0.08
Nodes (79): Lexer, Node, Token, TokenType, compute_word_class(), is_delimiter_token(), is_number_word(), node_iter_next() (+71 more)

### Community 101 - "sdcard.c"
Cohesion: 0.25
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "repl_evaluate_line"
Cohesion: 0.10
Nodes (25): proc_restore_execution_state(), proc_save_execution_state(), Result, repl_count_bracket_balance(), repl_evaluate_line(), repl_find_end_token(), repl_next_bracket_depth(), repl_proc_def_append() (+17 more)

### Community 104 - "unity.h"
Cohesion: 0.09
Nodes (15): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+7 more)

### Community 105 - "eval.c"
Cohesion: 0.23
Nodes (27): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, UserProcedure, Value (+19 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "test_galaxian.c"
Cohesion: 0.18
Nodes (21): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom() (+13 more)

### Community 109 - "mem_word_ptr"
Cohesion: 0.02
Nodes (171): mem_word_ptr(), test_body_is_reported_for_post(), test_path_is_percent_decoded(), test_query_empty_when_absent(), test_readchar_from_file(), test_setread_to_file(), test_setreadpos(), test_setwrite_to_file() (+163 more)

### Community 110 - "prim_print"
Cohesion: 0.42
Nodes (11): Evaluator, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_show(), prim_standout() (+3 more)

### Community 111 - "mem_atom"
Cohesion: 0.05
Nodes (105): mem_atom(), mem_cons(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_is_list(), mem_set_cdr(), mem_word_eq() (+97 more)

### Community 112 - "primitives_workspace.c"
Cohesion: 0.14
Nodes (42): primitive_get_count(), Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add() (+34 more)

### Community 113 - "P9 — Tile maps and smooth scrolling (design)"
Cohesion: 0.07
Nodes (28): 10. Checkpoint Run revamp, 11. Turtle Trails revamp (render-only, gameplay identical), 12. Budgets, 13. Milestones, 14. Tests, 15. Levers if M0 misses, 16. Rejected alternatives, 17. Roadmap gate questions, resolved (+20 more)

### Community 114 - "eval_primary"
Cohesion: 0.21
Nodes (22): eval_at_end(), apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expr_bp() (+14 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (19): backup, cat, catalog, copyfile, createdir, dir? (dirp), directories, editfile (+11 more)

### Community 117 - "primitives_http.c"
Cohesion: 0.26
Nodes (19): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+11 more)

### Community 118 - "test_scaffold.c"
Cohesion: 0.12
Nodes (8): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write()

### Community 119 - "clib.c"
Cohesion: 0.22
Nodes (14): logo_host_rename(), fat32_error_t, _close(), fat32_error_to_errno(), _fstat(), init(), _lseek(), _open() (+6 more)

### Community 120 - "P8 — Sound: a stereo PSG synthesizer (design)"
Cohesion: 0.07
Nodes (29): 10. Rejected alternatives, 11. Resolved questions (user, 2026-07-10), 12.1 DMA read ring-wrap (engine, 2026-07-18), 12.2 LCD driver no longer masks interrupts (2026-07-19), 12.3 Audio IRQ priority above default (2026-07-19), 12. Hardware bring-up findings (2026-07-18/19), 1. What limits sound today, 2. The output hardware (+21 more)

### Community 121 - "Input and Output to Files, Network Connections and Devices"
Cohesion: 0.12
Nodes (16): allopen, close, closeall, filelen, Input and Output to Files, Network Connections and Devices, open, reader, readpos (+8 more)

### Community 122 - "test_primitives_properties.c"
Cohesion: 0.09
Nodes (21): tearDown(), test_erprops_clears_all_properties(), test_gprop_requires_word_for_name(), test_gprop_requires_word_for_property(), test_multiple_properties_on_same_name(), test_plist_requires_word(), test_pprop_and_gprop_number_value(), test_pprop_and_gprop_word_value() (+13 more)

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (57): MockStamp, mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf(), player_at_junction() (+49 more)

### Community 125 - "test_bench_throughput.c"
Cohesion: 0.14
Nodes (23): repl_line_is_end(), repl_line_starts_with_to(), calibrate_ns(), define_workspace(), load_game(), now_ms(), tearDown(), test_bench_checkrun_play_frame() (+15 more)

### Community 126 - "mklfsimg_lib.c"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 127 - "roadmap.md"
Cohesion: 0.10
Nodes (19): Build and test, Code structure, Constraints, Graphify, Project, Unit testing, Working guidelines, Build & Test (+11 more)

### Community 128 - "sound.c"
Cohesion: 0.20
Nodes (13): SoundEvent, SoundStatus, is_noise_voice(), __not_in_flash_func(), queue_empty(), queue_free(), sound_gate(), sound_init() (+5 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "prim_define"
Cohesion: 0.54
Nodes (8): Evaluator, Result, Value, prim_copydef(), prim_define(), prim_definedp(), prim_primitivep(), prim_text()

### Community 132 - "arena_offset_to_ptr"
Cohesion: 0.13
Nodes (19): arena_available_bytes(), arena_capacity_bytes(), arena_offset_to_ptr(), arena_ptr_to_offset(), arena_used_bytes(), FrameArena, word_offset_t, test_available_bytes() (+11 more)

### Community 133 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "P10 — Interpreter throughput (design)"
Cohesion: 0.08
Nodes (24): 10. Relationship to P9, 1. Goal, 2.1 The games miss their budgets, and it is not the screen, 2.2 The profile, 2. Evidence, 3.1 Every evaluation re-lexes the list, 3.2 Every call resolves its name by string compare, 3.3 Cons-cell access is indirect (+16 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (23): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P10 — Interpreter throughput, P1 — Host REPL stdin + CI (+15 more)

### Community 137 - "primitives.c"
Cohesion: 0.21
Nodes (11): primitive_find(), primitive_find_n(), primitive_get_by_index(), primitive_name_compare(), primitive_register_alias(), Primitive, test_primitives_are_registered(), test_primitive_abbreviation_resolves_same_function() (+3 more)

### Community 138 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "prim_not"
Cohesion: 0.61
Nodes (7): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or()

### Community 142 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "record_command"
Cohesion: 0.12
Nodes (16): mock_screen_fullscreen(), mock_screen_refresh_now(), mock_screen_set_refresh_auto(), mock_screen_splitscreen(), mock_screen_textscreen(), mock_text_clear(), mock_turtle_clear(), mock_turtle_draw() (+8 more)

### Community 146 - "PR Review Checklist (CRITICAL)"
Cohesion: 0.22
Nodes (8): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on

### Community 147 - "test_primitives_editor.c"
Cohesion: 0.07
Nodes (77): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_set_editor_content(), mock_device_set_editor_result(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, MockFile (+69 more)

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "test_lfs_backup.c"
Cohesion: 0.21
Nodes (17): blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read(), blob_set_read_pos(), blob_write_bytes(), lfs_t (+9 more)

### Community 151 - "frame_stack_depth"
Cohesion: 0.15
Nodes (20): frame_pop(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_is_empty(), frame_stack_used_bytes(), test_pop_all_frames(), test_pop_empty_returns_none(), test_pop_frees_memory() (+12 more)

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "mem_blob"
Cohesion: 0.19
Nodes (17): blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_is_blob(), mem_region_alloc(), editor_pick_buffer(), enable_blob_region() (+9 more)

### Community 154 - "MockCommandType"
Cohesion: 0.17
Nodes (12): MockCommandType, LogoPen, mock_turtle_dot(), mock_turtle_get_pen_state(), mock_turtle_set_bg_colour(), mock_turtle_set_pen_colour(), mock_turtle_set_pen_state(), mock_turtle_set_position() (+4 more)

### Community 155 - "Introduction"
Cohesion: 0.17
Nodes (12): A Further Note on Operations, Another Way to Talk about Procedures, Formal Logo, How to Think about Procedures You Define and their Inputs, How You Might Think about MAKE, How You Might Think about Quotes, Introduction, Logo Objects (+4 more)

### Community 156 - "as_httpd_conn"
Cohesion: 0.32
Nodes (8): MockHttpdConn, as_httpd_conn(), httpd_conn_read(), httpd_conn_write(), mock_network_tcp_can_read(), mock_network_tcp_close(), mock_network_tcp_read(), mock_network_tcp_write()

### Community 157 - "Appendix A: Useful Tools"
Cohesion: 0.25
Nodes (8): Appendix A: Useful Tools, arcr and arcl, circler and circlel, divisor?, Graphics Tools, Math Tools, Program Logic or Debugging Tools, sort

### Community 158 - "primitives_control_flow.c"
Cohesion: 0.38
Nodes (16): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_forever(), prim_ignore() (+8 more)

### Community 159 - "test_primitives_variables.c"
Cohesion: 0.12
Nodes (15): setUp(), tearDown(), test_dots_variable(), test_error_no_value(), test_global_variable(), test_local_declaration(), test_local_with_list(), test_localmake_in_procedure() (+7 more)

### Community 160 - "picocalc_network_tls_connect"
Cohesion: 0.33
Nodes (9): picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), poll_lwip_with_timeout(), tcp_connect_and_wait(), tcp_resolve_addr(), tcp_state_alloc(), tls_client_config() (+1 more)

### Community 161 - "procedures.c"
Cohesion: 0.19
Nodes (14): UserProcedure, find_procedure_index(), find_procedure_index_n(), proc_bury(), proc_bury_all(), proc_find_n(), proc_get_current(), proc_push_current() (+6 more)

### Community 164 - "Bitwise Operations"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

### Community 165 - "primitives_debug_control.c"
Cohesion: 0.27
Nodes (11): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+3 more)

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "value_number"
Cohesion: 0.24
Nodes (28): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+20 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 170 - "LogoStream"
Cohesion: 0.20
Nodes (10): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write() (+2 more)

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 173 - "logo_io_close_all"
Cohesion: 0.43
Nodes (8): Evaluator, Result, Value, prim_bootsel(), prim_goodbye(), prim_toot(), toot_gate_freq(), logo_io_close_all()

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 177 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 178 - "logo_lfs_storage_init"
Cohesion: 0.40
Nodes (5): lfs_t, LogoStorage, logo_lfs_storage_init(), setUp(), setUp()

### Community 180 - "logo_io_copy_file"
Cohesion: 0.25
Nodes (15): logo_io_copy_file(), copy_setup(), get(), put(), test_copy_directory_source_rejected(), test_copy_missing_source_fails(), test_copy_onto_self_is_noop(), test_copy_overwrites_dest() (+7 more)

### Community 194 - "ensure_wifi_initialized"
Cohesion: 0.16
Nodes (15): WifiState, ensure_wifi_initialized(), mdns_start(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_wifi_connect(), picocalc_wifi_get_ip() (+7 more)

### Community 196 - "prim_setdate"
Cohesion: 0.54
Nodes (8): Evaluator, Result, Value, prim_date(), prim_setdate(), prim_settime(), prim_ticks(), prim_time()

### Community 197 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

### Community 198 - "primitives_variables.c"
Cohesion: 0.47
Nodes (9): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing() (+1 more)

### Community 199 - "mock_device_set_raster"
Cohesion: 0.67
Nodes (3): LogoTurtleRaster, mock_device_set_raster(), mock_turtle_get_raster()

### Community 200 - "JSON"
Cohesion: 0.33
Nodes (6): JSON, json.array, json.count, json.get, json.make, json.object

### Community 201 - "bd_prog"
Cohesion: 0.47
Nodes (6): bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t

### Community 202 - "test_httpd_device.c"
Cohesion: 0.22
Nodes (9): mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled(), drain(), tearDown(), test_accept_returns_connection_and_remote_ip(), test_can_read_reflects_available_bytes(), test_dribbled_read_delivers_in_chunks(), test_full_connection_round_trip() (+1 more)

### Community 203 - "httpd_listening"
Cohesion: 0.29
Nodes (11): httpd_listening(), mock_httpd_is_listening(), mock_httpd_listen_port(), test_listen_returns_handle_and_records_port(), test_unlisten_drops_pending_connection(), test_clearscreen_leaves_the_listener_running(), test_listen_starts_the_server(), test_relisten_new_port_moves_listener() (+3 more)

### Community 204 - "logo_console_init"
Cohesion: 0.25
Nodes (10): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), test_console_has_screen_modes() (+2 more)

### Community 205 - "arena_used"
Cohesion: 0.27
Nodes (10): arena_extend(), arena_used(), test_alloc_updates_used(), test_extend_at_capacity_fails(), test_extend_decreases_available(), test_extend_exact_available_succeeds(), test_extend_increases_used(), test_extend_too_large_fails() (+2 more)

### Community 206 - "run_editor_and_process"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, count_bracket_balance(), prim_edall(), prim_edit(), prim_edn(), prim_edns() (+1 more)

### Community 207 - "ip_addr_t"
Cohesion: 0.25
Nodes (8): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 208 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 209 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 210 - "bd_prog"
Cohesion: 0.47
Nodes (6): bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t

### Community 211 - "word_of"
Cohesion: 0.33
Nodes (6): test_element_escaped_attribute_value(), test_element_list_content_formats_like_print(), test_element_nests(), test_element_with_attributes(), test_element_wraps_content(), word_of()

### Community 212 - "bd_prog"
Cohesion: 0.47
Nodes (6): bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t

### Community 213 - "logo_storage_router_init"
Cohesion: 0.50
Nodes (4): LogoStorage, LogoStorageOps, logo_storage_router_init(), LogoMountAvailableFn

## Knowledge Gaps
- **745 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+740 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **10 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `primitives_httpd.c`, `mem_is_nil`, `reset_output`, `eval_string`, `result_none`, `test_trails.c`, `io.c`, `test_primitives_files_directory.c`, `format_buffer_init`, `lexer_init`, `test_primitives_editor.c`, `test_variables.c`, `test_eval.c`, `frame_stack_depth`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold_tearDown`, `test_primitives_variables.c`, `test_primitives_outside_world.c`, `test_time.c`, `set_mock_input`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_files_load_save.c`, `demons_poll`, `proc_define_from_text`, `main`, `mock_device_get_state`, `eval.c`, `test_galaxian.c`, `mem_word_ptr`, `mem_atom`, `eval_primary`, `test_scaffold.c`, `test_primitives_properties.c`, `test_checkrun.c`, `test_bench_throughput.c`?**
  _High betweenness centrality (0.130) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `run_string`, `primitives_httpd.c`, `test_value.c`, `mem_is_nil`, `reset_output`, `value_to_string`, `test_trails.c`, `test_primitives_files_directory.c`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `test_httpd.c`, `test_eval.c`, `frame_stack_depth`, `test_primitives_http.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_primitives_variables.c`, `test_primitives_wifi.c`, `test_primitives_outside_world.c`, `test_time.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_network.c`, `test_fileserver.c`, `httpd_listening`, `proc_define_from_text`, `word_of`, `mock_device_get_state`, `eval.c`, `test_galaxian.c`, `mem_word_ptr`, `mem_atom`, `eval_primary`, `test_scaffold.c`, `test_primitives_properties.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.121) - this node is a cross-community bridge._
- **Why does `mem_word_ptr()` connect `mem_word_ptr` to `run_string`, `primitives_httpd.c`, `prim_define`, `mem_is_nil`, `test_value.c`, `eval_string`, `result_none`, `value_to_string`, `reset_output`, `test_primitives_files_directory.c`, `format_buffer_init`, `test_primitives_editor.c`, `primitives_sound.c`, `test_variables.c`, `test_httpd.c`, `test_eval.c`, `mem_blob`, `test_primitives_http.c`, `test_primitives_json.c`, `test_primitives_variables.c`, `mem_atom_cstr`, `primitives_debug_control.c`, `test_primitives_wifi.c`, `primitives_get_io`, `test_time.c`, `set_mock_input`, `test_primitives_hardware.c`, `httpd.c`, `test_sound.c`, `test_primitives_network.c`, `memory.c`, `primitives_json.c`, `mem_is_word`, `primitives.h`, `proc_define_from_text`, `run_editor_and_process`, `step_proc_call`, `word_of`, `prim_savel`, `primitives_files.c`, `test_token_source.c`, `mem_atom`, `primitives_workspace.c`, `eval_primary`, `primitives_http.c`?**
  _High betweenness centrality (0.053) - this node is a cross-community bridge._
- **Are the 943 inferred relationships involving `run_string()` (e.g. with `define_workspace()` and `load_game()`) actually correct?**
  _`run_string()` has 943 INFERRED edges - model-reasoned connections that need verification._
- **Are the 912 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 912 INFERRED edges - model-reasoned connections that need verification._
- **Are the 439 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 439 INFERRED edges - model-reasoned connections that need verification._
- **Are the 231 inferred relationships involving `mem_atom()` (e.g. with `eval_primary()` and `parse_list()`) actually correct?**
  _`mem_atom()` has 231 INFERRED edges - model-reasoned connections that need verification._