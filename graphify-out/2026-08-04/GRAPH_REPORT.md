# Graph Report - pico-logo  (2026-08-04)

## Corpus Check
- 294 files · ~518,692 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 7210 nodes · 22311 edges · 207 communities (197 shown, 10 thin omitted)
- Extraction: 57% EXTRACTED · 43% INFERRED · 0% AMBIGUOUS · INFERRED: 9627 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `7edba751`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- run_string
- lfs.c
- test_tilemap.c
- test_value.c
- mem_is_nil
- reset_output
- eval_string
- result_none
- primitives_workspace.c
- test_trails.c
- iteration_callback
- io.c
- test_primitives_files_directory.c
- picocalc_console.c
- LOGO_HOT
- lexer_init
- test_frame_arena.c
- test_io.c
- syntax_highlight_line
- test_variables.c
- primitives_sound.c
- unity.c
- test_httpd.c
- mem_is_word
- Turtle Graphics
- test_scaffold_setUp
- test_primitives_http.c
- fat32.c
- test_primitives_conditionals.c
- mock_device.c
- test_primitives_json.c
- test_eval.c
- test_scaffold_tearDown
- test_notation.c
- primitives_init
- prim_error
- value_number
- lexer_next_token
- primitives_conditionals.c
- test_cross_fs_move.c
- test_primitives_wifi.c
- primitives_properties.c
- prim_savel
- test_mock_device.c
- test_time.c
- set_mock_input
- picocalc_editor_edit
- test_primitives_tilemap.c
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
- logo_io_set_writer
- primitives_json.c
- Conditionals and Control of Flow
- test_mock_fs.h
- test_scaffold.h
- test_dirty_tiles.c
- Words and Lists
- test_primitives_list_processing.c
- result_error
- primitives.h
- lfs_storage.c
- mock_sdcard.c
- keyboard.c
- picocalc_storage.c
- test_primitives_control_flow.c
- lexer.c
- main
- test_storage_router.c
- proc_get_frame_stack
- host_storage.c
- test_costumes.c
- result_error_arg
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- main
- primitives_bitwise.c
- op_stack_depth
- test_primitives_files.c
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- eval.c
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- mem_atom
- host_hardware.c
- sdcard.c
- Managing your Workspace
- procedures.c
- primitives_control_flow.c
- logo_io_init
- test_tls_heap.c
- storage_router.c
- test_galaxian.c
- mem_word_ptr
- test_primitives_outside_world.c
- test_help.c
- value_to_string
- P9 — Tile maps and smooth scrolling (design)
- on_sd_card_detect
- Galaxian in Pico Logo (design)
- File Management
- test_primitives_editor.c
- test_scaffold.c
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- primitives_text.c
- Design: `launch` background processes (P6)
- test_checkrun.c
- southbridge.c
- test_scaffold_setUp_with_device
- roadmap.md
- sound.c
- Using the Logo Editor
- HTTP Server
- test_primitives_variables.c
- Appendix B: Parsing
- token_source.c
- WiFi Management
- P10 — Interpreter throughput (design)
- The pick of five: plans
- primitives.c
- test_primitives_properties.c
- Modifying Procedures Under Program Control
- ms_to_datetime
- format_buffer_init
- test_bench_throughput.c
- The Outside World
- HTTP server (design)
- prim_not
- PR Review Checklist (CRITICAL)
- Managing Various Files
- record_command_float
- unity.h
- screen_set_mode
- repl_line_starts_with_to
- Atom Garbage Collection: Implementation Plan
- Tile Maps
- MockCommandType
- Introduction
- as_httpd_conn
- Appendix A: Useful Tools
- frame.c
- stdlib.h
- ensure_wifi_initialized
- primitives_http.c
- primitives_wifi.c
- Text and Screen Commands
- ip_addr_t
- LogoStream
- Pico Logo
- primitives_arithmetic.c
- drain_tokens
- frame_get_test
- logo_storage_init
- gen_ca_certs.py
- logo_console_init
- pandoc_slug
- run_editor_and_process
- Property Lists
- JSON
- dist.sh
- picocalc_network_tls_connect
- generate_help.sh
- run_e2e.sh
- Variables
- VENDOR.md
- prim_go
- mock_device_set_raster
- mock_device_get_dot
- List Processing
- LogoEditorResult
- mock_sound_queue
- mock_sound_status
- repl_evaluate_line
- mock_wifi_status
- Time Management
- logo_io_list_directory

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 972 edges
2. `eval_string()` - 923 edges
3. `mem_word_ptr()` - 446 edges
4. `mem_atom()` - 242 edges
5. `mem_is_nil()` - 240 edges
6. `value_to_string()` - 207 edges
7. `result_error_arg()` - 199 edges
8. `result_none()` - 193 edges
9. `result_ok()` - 177 edges
10. `lexer_init()` - 173 edges

## Surprising Connections (you probably didn't know these)
- `test_atom_interning()` --calls--> `mem_atom()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_atom_interning_case_insensitive()` --calls--> `mem_atom()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_different_atoms()` --calls--> `mem_atom()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_number_content()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_negative()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Communities (207 total, 10 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (247): MockLine, MockDeviceState, mock_device_dot_count(), mock_device_get_line(), mock_device_get_output(), mock_device_get_state(), mock_device_has_line_from_to(), mock_device_line_count() (+239 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "test_tilemap.c"
Cohesion: 0.11
Nodes (60): bake_ready(), bake_rect(), Evaluator, LogoConsoleTurtle, Result, Value, get_turtle_ops(), is_int_in() (+52 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (175): Node, demons_print(), demons_set(), apply_binary_op(), TokenType, Value, format_number(), prim_when() (+167 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (72): mem_atom_cstr(), mem_is_list(), mem_is_nil(), mem_set_cdr(), Lexer, parse_list_body(), parse_list_from_string(), LogoEntryType (+64 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (219): proc_define_from_text(), proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_co_at_toplevel(), test_freeze_request_break_stops_execution(), test_freeze_request_waits_for_key(), test_go_label_not_found_in_procedure() (+211 more)

### Community 6 - "eval_string"
Cohesion: 0.02
Nodes (168): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+160 more)

### Community 7 - "result_none"
Cohesion: 0.14
Nodes (79): arg_turtle(), Evaluator, LogoConsoleTurtle, LogoTurtleRaster, Result, Value, get_turtle_ops(), make_position_list() (+71 more)

### Community 8 - "primitives_workspace.c"
Cohesion: 0.16
Nodes (39): Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add(), help_list_flush() (+31 more)

### Community 9 - "test_trails.c"
Cohesion: 0.10
Nodes (72): mock_device_get_canvas_point(), actor(), load_logo(), load_trails(), num(), numf(), put_actor(), read_map() (+64 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.09
Nodes (69): prim_editfile(), prim_pofile(), LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span(), logo_io_check_freeze_request() (+61 more)

### Community 12 - "test_primitives_files_directory.c"
Cohesion: 0.06
Nodes (41): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+33 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.06
Nodes (52): LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_flush(), error_output_write(), heading_faces_left() (+44 more)

### Community 14 - "LOGO_HOT"
Cohesion: 0.11
Nodes (48): UserProcedure, Value, eval_at_end(), Evaluator, Node, Result, eval_expression(), is_number_string() (+40 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_io.c"
Cohesion: 0.04
Nodes (47): logo_io_parse_network_address(), logo_io_set_prefix(), logo_io_set_reader(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn(), mock_dir_callback() (+39 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.06
Nodes (60): Value, find_global(), LOGO_HOT(), var_bury(), var_bury_all(), var_declare_local(), var_erase(), var_erase_all() (+52 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (73): httpd_listening(), httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled() (+65 more)

### Community 23 - "mem_is_word"
Cohesion: 0.05
Nodes (78): blob_alloc(), mem_atom_unescape(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_free_atoms(), mem_free_nodes(), mem_gc() (+70 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.06
Nodes (40): LogoIO, primitives_control_reset_test_state(), primitives_set_io(), procedures_init(), properties_init(), variables_init(), LogoHardware, LogoHardwareOps (+32 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (65): mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), Result, get_body_of_size() (+57 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (52): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+44 more)

### Community 28 - "test_primitives_conditionals.c"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (30): mock_device_get_tcp_request_len(), mock_device_set_input(), mock_device_set_snap_result(), mock_device_set_tcp_close_after(), mock_device_set_tcp_read_chunk(), mock_device_set_tcp_write_chunk(), mock_device_was_restore_palette_called(), mock_screen_fullscreen() (+22 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (62): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+54 more)

### Community 31 - "test_eval.c"
Cohesion: 0.02
Nodes (105): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+97 more)

### Community 32 - "test_scaffold_tearDown"
Cohesion: 0.05
Nodes (40): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown() (+32 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.10
Nodes (36): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), editor_pick_buffer(), primitives_editor_init() (+28 more)

### Community 35 - "prim_error"
Cohesion: 0.57
Nodes (7): Evaluator, Result, Value, prim_catch(), prim_error(), prim_throw(), prim_toplevel()

### Community 36 - "value_number"
Cohesion: 0.10
Nodes (48): frame_current(), frame_get_bindings(), frame_pop(), frame_stack_available_bytes(), frame_stack_init(), frame_stack_is_empty(), value_number(), init_reuse_procs() (+40 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "primitives_conditionals.c"
Cohesion: 0.48
Nodes (11): eval_push_if(), Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse() (+3 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.05
Nodes (61): mock_device_add_wifi_scan_result(), mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result() (+53 more)

### Community 41 - "primitives_properties.c"
Cohesion: 0.39
Nodes (11): Evaluator, Result, Value, prim_erprops(), prim_gprop(), prim_plist(), prim_pprop(), prim_pps() (+3 more)

### Community 42 - "prim_savel"
Cohesion: 0.27
Nodes (15): Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_save(), prim_savel(), prim_savepic() (+7 more)

### Community 43 - "test_mock_device.c"
Cohesion: 0.10
Nodes (35): LogoConsole, mock_device_get_console(), mock_device_has_dot_at(), mock_device_verify_heading(), mock_device_verify_position(), test_background_colour(), test_dot_at_query(), test_draw_dot() (+27 more)

### Community 44 - "test_time.c"
Cohesion: 0.07
Nodes (45): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+37 more)

### Community 45 - "set_mock_input"
Cohesion: 0.15
Nodes (44): prim_pause(), LogoIO, repl_cleanup(), repl_extract_proc_name(), repl_init(), repl_run(), ReplFlags, test_repl_defines_proc_with_multiline_paren() (+36 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "test_primitives_tilemap.c"
Cohesion: 0.09
Nodes (39): mock_device_paint_canvas(), mock_device_set_canvas_point(), assert_baked_tile(), assert_canvas_block(), capture_tile(), tearDown(), test_bake_paints_empty_cells_in_the_background_colour(), test_bake_repeats_a_world_smaller_than_the_screen() (+31 more)

### Community 48 - "repository"
Cohesion: 0.04
Nodes (45): name, name, match, name, 1, 2, match, name (+37 more)

### Community 49 - "test_primitives_hardware.c"
Cohesion: 0.05
Nodes (45): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+37 more)

### Community 50 - "Turtle Trails (design)"
Cohesion: 0.06
Nodes (32): 10. Main loop and state order, 11.1 The procedure table, 11. Memory and performance budget, 12. Design boundaries, 13. Tests, 14. Implementation milestones, 15. As built: divergences from this design, 1. Theme (+24 more)

### Community 51 - "httpd.c"
Cohesion: 0.09
Nodes (60): demons_running(), LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find() (+52 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (39): logo_io_write_error_line(), LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error() (+31 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (35): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+27 more)

### Community 54 - ";"
Cohesion: 0.04
Nodes (46): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, ashift, battery, bitand (+38 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.08
Nodes (36): mock_sound_set_status(), assert_number_list(), assert_word(), MockDeviceState, Result, Value, snd(), test_env_default() (+28 more)

### Community 57 - "test_primitives_files_load_save.c"
Cohesion: 0.05
Nodes (58): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle() (+50 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (41): fat32_close(), fat32_create(), fat32_delete(), fat32_is_mounted(), fat32_mount(), fat32_open(), fat32_read(), fat32_set_current_dir() (+33 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.10
Nodes (36): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+28 more)

### Community 61 - "memory.c"
Cohesion: 0.07
Nodes (60): BlobDesc, demons_gc_mark_all(), remember_binding(), Value, mark_value(), op_stack_gc_mark(), alloc_cell(), atom_chain_next() (+52 more)

### Community 62 - "logo_io_set_writer"
Cohesion: 0.18
Nodes (18): logo_io_check_write_error(), logo_io_flush(), logo_io_is_dribbling(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble(), logo_io_write_line(), test_check_write_error() (+10 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.18
Nodes (34): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+26 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.06
Nodes (62): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+54 more)

### Community 66 - "test_scaffold.h"
Cohesion: 0.09
Nodes (41): Result, demons_frozen(), demons_maybe_poll(), demons_poll(), logo_io_ticks_ms(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle() (+33 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.14
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "test_primitives_list_processing.c"
Cohesion: 0.04
Nodes (50): test_apply_unknown_procedure(), test_apply_with_lambda(), test_apply_with_multi_param_lambda(), test_apply_with_primitive_name(), test_apply_with_procedure_text(), test_apply_with_word_primitive(), test_crossmap_basic(), test_crossmap_callback_throw_freed() (+42 more)

### Community 70 - "result_error"
Cohesion: 0.17
Nodes (31): Evaluator, Node, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar() (+23 more)

### Community 71 - "primitives.h"
Cohesion: 0.11
Nodes (18): Value, demons_clear(), demons_freeze(), demons_reset(), demons_resume(), demons_suspend(), demons_thaw(), value_is_true() (+10 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (20): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+12 more)

### Community 74 - "keyboard.c"
Cohesion: 0.09
Nodes (22): repeating_timer_t, keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_poll(), keyboard_set_background_poll(), keyboard_set_idle_callback() (+14 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (28): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_dir_create() (+20 more)

### Community 76 - "test_primitives_control_flow.c"
Cohesion: 0.03
Nodes (63): test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_deep_nested_proc_in_repeat(), test_do_until_basic(), test_do_until_invalid_predicate(), test_do_until_runs_once() (+55 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "main"
Cohesion: 0.10
Nodes (22): proc_reset_execution_state(), main(), psram_verify(), m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read() (+14 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "proc_get_frame_stack"
Cohesion: 0.15
Nodes (23): frame_binding_count(), frame_stack_depth(), frame_stack_used_bytes(), FrameStack, proc_get_frame_stack(), var_get_local_by_index(), var_is_shadowed_by_local(), var_local_count() (+15 more)

### Community 81 - "host_storage.c"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "result_error_arg"
Cohesion: 0.10
Nodes (71): CatalogContext, CatalogEntry, Evaluator, Result, Value, Evaluator, LogoIO, Result (+63 more)

### Community 84 - "test_lfs_storage.c"
Cohesion: 0.12
Nodes (18): Listing, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t, LogoEntryType (+10 more)

### Community 85 - "Code Review — 2026-07-02"
Cohesion: 0.08
Nodes (23): 1. Confirmed bug: `recycle` sweeps reachable data, 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate), 2.2 `find_atom` is a linear scan of the whole atom table, 2.3 Smaller items, 2. Hot-path efficiency, 3. Robustness: `mem_cons` failures are silently ignored, 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict, 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision (+15 more)

### Community 86 - "Contributing"
Cohesion: 0.08
Nodes (23): About Logo, Additional Features for the PicoCalc, Advanced Logo, Beginning Logo, Building and Running, Contributing, Credits, Dependencies (+15 more)

### Community 87 - "main"
Cohesion: 0.20
Nodes (17): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+9 more)

### Community 88 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 89 - "op_stack_depth"
Cohesion: 0.12
Nodes (28): EvalOp, LOGO_HOT(), op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init(), op_stack_insert(), op_stack_is_empty() (+20 more)

### Community 90 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (54): test_allopen_empty(), test_allopen_multiple_files(), test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_closeall(), test_dribble_starts() (+46 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.11
Nodes (27): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+19 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.08
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (implemented)"
Cohesion: 0.06
Nodes (31): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+23 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "eval.c"
Cohesion: 0.27
Nodes (20): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, eval_get_frames(), eval_in_procedure() (+12 more)

### Community 96 - "Space Invaders in Pico Logo (design & implementation)"
Cohesion: 0.09
Nodes (22): 10. Why this is a good P5 acceptance test, 11. Deliverable, 1. The board, 2. Object representation — the central decision, 3. The alien formation on the canvas, 4. Collision routing — demons vs. the game loop, 5. Global events as demons, 6. Input (+14 more)

### Community 97 - "package.json"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 98 - "test_mklfsimg.c"
Cohesion: 0.08
Nodes (33): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+25 more)

### Community 99 - "mem_atom"
Cohesion: 0.07
Nodes (95): mem_atom(), mem_cons(), Lexer, token_source_consume_sublist(), token_source_get_sublist(), token_source_init_lexer(), token_source_init_list(), value_extract_rgb() (+87 more)

### Community 101 - "sdcard.c"
Cohesion: 0.23
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "procedures.c"
Cohesion: 0.21
Nodes (14): UserProcedure, find_procedure_index(), find_procedure_index_n(), proc_bury(), proc_bury_all(), proc_by_index(), proc_find_n(), proc_index_of() (+6 more)

### Community 104 - "primitives_control_flow.c"
Cohesion: 0.37
Nodes (17): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_for(), prim_forever() (+9 more)

### Community 105 - "logo_io_init"
Cohesion: 0.28
Nodes (9): LogoConsole, LogoHardware, LogoStorage, logo_io_cleanup(), logo_io_init(), tearDown(), test_init_no_console(), test_open_no_storage() (+1 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "test_galaxian.c"
Cohesion: 0.20
Nodes (19): assert_num(), assert_true(), seed_convoy(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom(), test_file_loads_and_sets_globals(), test_find_flank_walks_inward() (+11 more)

### Community 109 - "mem_word_ptr"
Cohesion: 0.02
Nodes (131): logo_mem_set_aux_region(), mem_word_ptr(), bind_long_blob_word(), exhaust_atom_table(), exhaust_node_pool(), test_ascii(), test_bar_list_literal_count(), test_bar_list_literal_is_one_word() (+123 more)

### Community 110 - "test_primitives_outside_world.c"
Cohesion: 0.04
Nodes (46): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+38 more)

### Community 111 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 112 - "value_to_string"
Cohesion: 0.11
Nodes (103): number_to_word(), mem_first_cell(), mem_gc_roots_pop(), mem_list_append(), mem_next_cell(), mem_word(), mem_word_len(), Evaluator (+95 more)

### Community 113 - "P9 — Tile maps and smooth scrolling (design)"
Cohesion: 0.06
Nodes (33): 10. Checkpoint Run revamp, 11. Turtle Trails revamp (render-only, gameplay identical), 12. Budgets, 13.1 M1+M2 as built (2026-08-02), 13.2 M1+M2 on hardware (2026-08-02, Pico Plus 2 W), 13.3 M3 as built (2026-08-02), 13.4 M3 on hardware (2026-08-02) — accepted, but on the wrong board, 13.5 The board mismatch, and how to settle it (+25 more)

### Community 114 - "on_sd_card_detect"
Cohesion: 0.50
Nodes (4): repeating_timer_t, on_sd_card_detect(), logo_picocalc_mount_available(), sd_card_present()

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (19): backup, cat, catalog, copyfile, createdir, dir? (dirp), directories, editfile (+11 more)

### Community 117 - "test_primitives_editor.c"
Cohesion: 0.07
Nodes (77): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_set_editor_content(), mock_device_set_editor_result(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, MockFile (+69 more)

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

### Community 122 - "primitives_text.c"
Cohesion: 0.14
Nodes (35): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator, Result, Value (+27 more)

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (58): MockStamp, mock_device_clear_graphics(), mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf() (+50 more)

### Community 125 - "southbridge.c"
Cohesion: 0.32
Nodes (14): picocalc_get_battery_level(), picocalc_power_off(), sb_is_power_off_supported(), sb_read(), sb_read_battery(), sb_read_keyboard(), sb_read_keyboard_backlight(), sb_read_keyboard_state() (+6 more)

### Community 126 - "test_scaffold_setUp_with_device"
Cohesion: 0.11
Nodes (28): MockCommand, mock_device_clear_commands(), mock_device_command_count(), mock_device_get_command(), mock_device_last_command(), assert_fullscreen_canvas_hud(), assert_hud_write_positions(), load_game() (+20 more)

### Community 127 - "roadmap.md"
Cohesion: 0.10
Nodes (19): Build and test, Code structure, Constraints, Graphify, Project, Unit testing, Working guidelines, Build & Test (+11 more)

### Community 128 - "sound.c"
Cohesion: 0.18
Nodes (13): SoundEvent, SoundStatus, is_noise_voice(), __not_in_flash_func(), queue_empty(), queue_free(), sound_gate(), sound_init() (+5 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "test_primitives_variables.c"
Cohesion: 0.12
Nodes (15): setUp(), tearDown(), test_dots_variable(), test_error_no_value(), test_global_variable(), test_local_declaration(), test_local_with_list(), test_localmake_in_procedure() (+7 more)

### Community 132 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 133 - "token_source.c"
Cohesion: 0.13
Nodes (26): atom_memo_bind_index(), atom_memo_bind_kind(), atom_memo_class(), atom_memo_set_binding(), atom_memo_set_class(), Token, resolve_word(), Node (+18 more)

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "P10 — Interpreter throughput (design)"
Cohesion: 0.06
Nodes (32): 10. Relationship to P9, 11.1 First profile (2026-08-02, Pico Plus 2 W, 200 frames), 11.2 The instruction-fetch experiment, built and ready to measure, 11.3 Measured: instruction fetch confirmed (2026-08-04, Plus 2 W), 11.4 Tier 2 measured: 53.2 ms, and every moved function shows (Plus 2 W), 11.5 Tier 3 measured, and the pattern is now the finding (Plus 2 W), 11. M5 — re-profile before choosing a lever (2026-08-02), 1. Goal (+24 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (23): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P10 — Interpreter throughput, P1 — Host REPL stdin + CI (+15 more)

### Community 137 - "primitives.c"
Cohesion: 0.13
Nodes (18): primitive_by_index(), primitive_find(), primitive_find_n(), primitive_get_by_index(), primitive_get_count(), primitive_index_of(), primitive_is_output(), primitive_name_compare() (+10 more)

### Community 138 - "test_primitives_properties.c"
Cohesion: 0.08
Nodes (23): test_erprops_clears_all_properties(), test_gprop_requires_word_for_name(), test_gprop_requires_word_for_property(), test_gprop_returns_empty_list_for_unknown_name(), test_gprop_returns_empty_list_for_unknown_property(), test_multiple_properties_on_same_name(), test_plist_requires_word(), test_plist_returns_empty_list_for_unknown_name() (+15 more)

### Community 139 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "format_buffer_init"
Cohesion: 0.07
Nodes (86): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_init(), format_buffer_output() (+78 more)

### Community 142 - "test_bench_throughput.c"
Cohesion: 0.30
Nodes (13): calibrate_ns(), define_workspace(), load_game(), now_ms(), test_bench_checkrun_play_frame(), test_bench_expr_shapes(), test_bench_proc_call_workspace_scaling(), test_bench_repeat_loop() (+5 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "prim_not"
Cohesion: 0.61
Nodes (7): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or()

### Community 146 - "PR Review Checklist (CRITICAL)"
Cohesion: 0.22
Nodes (8): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on

### Community 147 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "unity.h"
Cohesion: 0.11
Nodes (33): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+25 more)

### Community 150 - "screen_set_mode"
Cohesion: 0.10
Nodes (30): picocalc_editor_get_ops(), lcd_enable_cursor(), lcd_get_palette_value(), lcd_restore_palette(), lcd_set_background(), lcd_set_palette_rgb(), lcd_set_palette_value(), LogoConsole (+22 more)

### Community 151 - "repl_line_starts_with_to"
Cohesion: 0.17
Nodes (13): repl_line_is_end(), repl_line_starts_with_to(), load_galaxian(), setUp(), load_graphics_demo(), setUp(), test_repl_line_is_end_basic(), test_repl_line_is_end_false_cases() (+5 more)

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "Tile Maps"
Cohesion: 0.25
Nodes (8): newmap, newtiles, settile, snaptile, stampmap, stamptile, tile, Tile Maps

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

### Community 158 - "frame.c"
Cohesion: 0.10
Nodes (45): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_at() (+37 more)

### Community 159 - "stdlib.h"
Cohesion: 0.07
Nodes (29): text_get_background(), text_get_foreground(), text_set_background(), turtle_canvas_point(), turtle_canvas_write_row(), turtle_dot_at(), turtle_draw_text(), turtle_fill() (+21 more)

### Community 160 - "ensure_wifi_initialized"
Cohesion: 0.16
Nodes (15): WifiState, ensure_wifi_initialized(), mdns_start(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_wifi_connect(), picocalc_wifi_get_ip() (+7 more)

### Community 161 - "primitives_http.c"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

### Community 162 - "primitives_wifi.c"
Cohesion: 0.37
Nodes (16): Evaluator, Result, Value, hostname_is_valid(), prim_tls_supported(), prim_wifi_connect(), prim_wifi_connected(), prim_wifi_disconnect() (+8 more)

### Community 163 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 164 - "ip_addr_t"
Cohesion: 0.25
Nodes (8): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 165 - "LogoStream"
Cohesion: 0.20
Nodes (10): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write() (+2 more)

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "primitives_arithmetic.c"
Cohesion: 0.20
Nodes (28): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+20 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 169 - "frame_get_test"
Cohesion: 0.19
Nodes (15): frame_get_test(), frame_set_test(), var_get_test(), var_reset_test_state(), var_set_test(), var_test_is_valid(), test_set_test_false(), test_set_test_true() (+7 more)

### Community 170 - "logo_storage_init"
Cohesion: 0.17
Nodes (13): lfs_t, LogoStorage, logo_lfs_storage_init(), LogoStorage, LogoStorageOps, logo_storage_init(), LogoStorage, LogoStorageOps (+5 more)

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 173 - "logo_console_init"
Cohesion: 0.25
Nodes (10): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), test_console_has_screen_modes() (+2 more)

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 176 - "run_editor_and_process"
Cohesion: 0.40
Nodes (10): Evaluator, Result, Value, count_bracket_balance(), prim_edall(), prim_edit(), prim_edns(), run_editor_and_process() (+2 more)

### Community 177 - "Property Lists"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 178 - "JSON"
Cohesion: 0.33
Nodes (6): JSON, json.array, json.count, json.get, json.make, json.object

### Community 180 - "picocalc_network_tls_connect"
Cohesion: 0.33
Nodes (9): picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), poll_lwip_with_timeout(), tcp_connect_and_wait(), tcp_resolve_addr(), tcp_state_alloc(), tls_client_config() (+1 more)

### Community 184 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 194 - "prim_go"
Cohesion: 0.60
Nodes (6): Evaluator, Result, Value, prim_co(), prim_go(), prim_label()

### Community 196 - "mock_device_set_raster"
Cohesion: 0.67
Nodes (3): LogoTurtleRaster, mock_device_set_raster(), mock_turtle_get_raster()

### Community 198 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

### Community 202 - "repl_evaluate_line"
Cohesion: 0.10
Nodes (25): proc_restore_execution_state(), proc_save_execution_state(), Result, name_distance(), repl_count_bracket_balance(), repl_evaluate_line(), repl_find_end_token(), repl_next_bracket_depth() (+17 more)

### Community 205 - "Time Management"
Cohesion: 0.33
Nodes (6): date, setdate, settime, ticks, time, Time Management

### Community 206 - "logo_io_list_directory"
Cohesion: 0.67
Nodes (3): LogoDirCallback, logo_io_list_directory(), test_list_directory()

## Knowledge Gaps
- **763 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+758 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **10 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `test_primitives_variables.c`, `mem_is_nil`, `reset_output`, `eval_string`, `result_none`, `test_trails.c`, `test_primitives_properties.c`, `test_primitives_files_directory.c`, `format_buffer_init`, `test_bench_throughput.c`, `LOGO_HOT`, `lexer_init`, `test_variables.c`, `mem_is_word`, `repl_line_starts_with_to`, `test_scaffold_setUp`, `test_primitives_conditionals.c`, `test_primitives_json.c`, `test_eval.c`, `test_scaffold_tearDown`, `test_time.c`, `set_mock_input`, `test_primitives_tilemap.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_files_load_save.c`, `test_scaffold.h`, `test_primitives_list_processing.c`, `test_primitives_control_flow.c`, `proc_get_frame_stack`, `test_primitives_files.c`, `eval.c`, `test_galaxian.c`, `mem_word_ptr`, `test_primitives_outside_world.c`, `value_to_string`, `test_primitives_editor.c`, `test_scaffold.c`, `test_checkrun.c`, `test_scaffold_setUp_with_device`?**
  _High betweenness centrality (0.130) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `run_string`, `test_value.c`, `mem_is_nil`, `reset_output`, `test_primitives_variables.c`, `test_trails.c`, `test_primitives_properties.c`, `test_primitives_files_directory.c`, `format_buffer_init`, `LOGO_HOT`, `lexer_init`, `test_variables.c`, `test_httpd.c`, `mem_is_word`, `test_primitives_http.c`, `test_primitives_conditionals.c`, `test_primitives_json.c`, `test_eval.c`, `test_scaffold_tearDown`, `test_primitives_wifi.c`, `test_time.c`, `test_primitives_tilemap.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_network.c`, `test_mock_fs.h`, `test_scaffold.h`, `test_primitives_list_processing.c`, `test_primitives_control_flow.c`, `proc_get_frame_stack`, `test_primitives_files.c`, `eval.c`, `mem_atom`, `test_galaxian.c`, `mem_word_ptr`, `test_primitives_outside_world.c`, `value_to_string`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.090) - this node is a cross-community bridge._
- **Why does `result_none()` connect `result_none` to `run_string`, `test_tilemap.c`, `test_value.c`, `mem_is_nil`, `primitives_workspace.c`, `io.c`, `LOGO_HOT`, `primitives_sound.c`, `primitives_wifi.c`, `primitives_conditionals.c`, `primitives_arithmetic.c`, `primitives_properties.c`, `prim_savel`, `set_mock_input`, `run_editor_and_process`, `httpd.c`, `memory.c`, `test_scaffold.h`, `prim_go`, `result_error`, `primitives.h`, `repl_evaluate_line`, `result_error_arg`, `op_stack_depth`, `eval.c`, `primitives_control_flow.c`, `value_to_string`, `primitives_text.c`?**
  _High betweenness centrality (0.047) - this node is a cross-community bridge._
- **Are the 970 inferred relationships involving `run_string()` (e.g. with `define_workspace()` and `load_game()`) actually correct?**
  _`run_string()` has 970 INFERRED edges - model-reasoned connections that need verification._
- **Are the 921 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 921 INFERRED edges - model-reasoned connections that need verification._
- **Are the 440 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `LOGO_HOT()`) actually correct?**
  _`mem_word_ptr()` has 440 INFERRED edges - model-reasoned connections that need verification._
- **Are the 232 inferred relationships involving `mem_atom()` (e.g. with `LOGO_HOT()` and `parse_list()`) actually correct?**
  _`mem_atom()` has 232 INFERRED edges - model-reasoned connections that need verification._