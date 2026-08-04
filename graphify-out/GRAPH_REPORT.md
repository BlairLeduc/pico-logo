# Graph Report - pico-logo  (2026-08-04)

## Corpus Check
- 294 files · ~519,396 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 7211 nodes · 22312 edges · 201 communities (190 shown, 11 thin omitted)
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
- mem_atom
- mem_is_nil
- reset_output
- eval_string
- result_none
- primitives_workspace.c
- test_trails.c
- error_format
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
- test_scaffold.h
- test_notation.c
- primitives_init
- mem_atom_cstr
- test_frame.c
- lexer_next_token
- primitives_conditionals.c
- test_cross_fs_move.c
- test_primitives_wifi.c
- primitives_files.c
- prim_savel
- mock_device_get_state
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
- demons_poll
- test_dirty_tiles.c
- Words and Lists
- test_primitives_list_processing.c
- primitives_outside_world.c
- primitives.h
- lfs_storage.c
- mock_sdcard.c
- stdlib.h
- picocalc_storage.c
- proc_define_from_text
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
- test_pop_scope
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- eval.c
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- mem_cons
- host_hardware.c
- sdcard.c
- Managing your Workspace
- procedures.c
- primitives_control_flow.c
- primitives_text.c
- test_tls_heap.c
- storage_router.c
- test_galaxian.c
- mem_word_ptr
- test_primitives_outside_world.c
- test_help.c
- result_ok
- P9 — Tile maps and smooth scrolling (design)
- on_sd_card_detect
- Galaxian in Pico Logo (design)
- File Management
- test_primitives_editor.c
- test_scaffold.c
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- test_frame_sync.c
- Design: `launch` background processes (P6)
- test_checkrun.c
- southbridge.c
- read_map
- roadmap.md
- mklfsimg_lib.c
- Using the Logo Editor
- HTTP Server
- primitives_tilemap.c
- Appendix B: Parsing
- record_command
- WiFi Management
- P10 — Interpreter throughput (design)
- The pick of five: plans
- primitives.c
- LOGO_HOT
- Modifying Procedures Under Program Control
- ms_to_datetime
- format_buffer_init
- test_bench_throughput.c
- The Outside World
- HTTP server (design)
- value_to_string
- PR Review Checklist (CRITICAL)
- Managing Various Files
- record_command_float
- test_lfs_backup.c
- primitives_variables.c
- HTTP Operations
- Atom Garbage Collection: Implementation Plan
- Tile Maps
- MockCommandType
- Introduction
- as_httpd_conn
- Appendix A: Useful Tools
- frame.c
- screen.c
- ensure_wifi_initialized
- Bitwise Operations
- primitives_wifi.c
- Text and Screen Commands
- ip_addr_t
- LogoStream
- Pico Logo
- value_number
- drain_tokens
- random.c
- load_logo
- gen_ca_certs.py
- mock_text_set_width
- pandoc_slug
- Property Lists
- dist.sh
- picocalc_network_tls_connect
- generate_help.sh
- run_e2e.sh
- Variables
- VENDOR.md
- mock_device_set_raster
- mock_device_get_dot
- List Processing
- LogoEditorResult
- mock_sound_queue
- mock_sound_status
- repl_evaluate_line
- mock_wifi_status

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
- `test_value_pop_empty_returns_none()` --calls--> `frame_pop_value()`  [INFERRED]
  tests/test_frame.c → core/frame.c
- `test_atom_interning()` --calls--> `mem_atom()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_atom_interning_case_insensitive()` --calls--> `mem_atom()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_different_atoms()` --calls--> `mem_atom()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_car_of_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c

## Import Cycles
- None detected.

## Communities (201 total, 11 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (235): mock_device_get_output(), mock_device_has_line_from_to(), mock_device_verify_palette(), test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts() (+227 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "test_tilemap.c"
Cohesion: 0.09
Nodes (56): ensure_pool(), tilemap_bank_slots(), tilemap_cell(), tilemap_cols(), tilemap_fill_row(), tilemap_get_scroll(), tilemap_get_viewport(), tilemap_map_capacity() (+48 more)

### Community 3 - "mem_atom"
Cohesion: 0.03
Nodes (185): Node, demons_set(), format_number(), mem_atom(), json_format_number(), Node, Result, Value (+177 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (81): mem_is_nil(), mem_set_cdr(), Value, prim_edit(), prim_edn(), Evaluator, Result, Value (+73 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (169): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+161 more)

### Community 6 - "eval_string"
Cohesion: 0.02
Nodes (153): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+145 more)

### Community 7 - "result_none"
Cohesion: 0.14
Nodes (79): arg_turtle(), Evaluator, LogoConsoleTurtle, LogoTurtleRaster, Result, Value, get_turtle_ops(), make_position_list() (+71 more)

### Community 8 - "primitives_workspace.c"
Cohesion: 0.16
Nodes (39): Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add(), help_list_flush() (+31 more)

### Community 9 - "test_trails.c"
Cohesion: 0.15
Nodes (47): actor(), num(), put_actor(), run(), runf(), test_a_bank_tile_carves_one_stroke_per_walkable_neighbour(), test_blocked_actor_discards_the_frame_step(), test_blocked_turn_stays_buffered() (+39 more)

### Community 10 - "error_format"
Cohesion: 0.06
Nodes (50): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+42 more)

### Community 11 - "io.c"
Cohesion: 0.08
Nodes (70): LogoDirCallback, LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span(), logo_io_check_freeze_request(), logo_io_check_pause_request() (+62 more)

### Community 12 - "test_primitives_files_directory.c"
Cohesion: 0.05
Nodes (43): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+35 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.05
Nodes (71): lcd_set_palette_value(), LogoConsole, LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_flush() (+63 more)

### Community 14 - "LOGO_HOT"
Cohesion: 0.16
Nodes (35): UserProcedure, eval_at_end(), advance(), get_infix_bp(), Evaluator, Token, TokenType, peek() (+27 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_io.c"
Cohesion: 0.04
Nodes (47): LogoConsole, LogoHardware, LogoStorage, logo_io_cleanup(), logo_io_init(), logo_io_parse_network_address(), LogoDirCallback, LogoEntryType (+39 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.07
Nodes (43): Value, find_global(), LOGO_HOT(), var_bury(), var_bury_all(), var_erase(), var_erase_all(), var_erase_all_globals() (+35 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (71): httpd_listening(), httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled() (+63 more)

### Community 23 - "mem_is_word"
Cohesion: 0.04
Nodes (96): blob_alloc(), mem_atom_unescape(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_free_atoms(), mem_free_nodes(), mem_gc() (+88 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.04
Nodes (63): blob_reset(), logo_mem_init(), LogoIO, primitives_control_reset_test_state(), primitives_set_io(), properties_init(), repl_line_is_end(), repl_line_starts_with_to() (+55 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (67): logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), Result (+59 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (52): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+44 more)

### Community 28 - "test_primitives_conditionals.c"
Cohesion: 0.03
Nodes (59): test_false(), test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_operation_returns_value(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output() (+51 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (13): mock_device_get_tcp_request_len(), mock_device_set_input(), mock_device_set_snap_result(), mock_device_set_tcp_close_after(), mock_device_set_tcp_read_chunk(), mock_device_set_tcp_write_chunk(), mock_device_was_restore_palette_called(), mock_httpd_set_read_hook() (+5 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (60): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+52 more)

### Community 31 - "test_eval.c"
Cohesion: 0.03
Nodes (57): test_b10_bare_exponent_in_list_path_is_not_a_number(), test_b10_bare_n_exponent_word_is_not_a_number(), test_b10_valid_exponent_forms_still_numbers(), test_b7_grouped_call_inside_operand(), test_b7_grouped_multi_branch_call_inside_operand(), test_b7_grouping_paren_starting_with_call(), test_b7_grouping_paren_starting_with_call_output(), test_b7_nested_grouping_parens_around_call() (+49 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (59): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown() (+51 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.11
Nodes (35): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+27 more)

### Community 35 - "mem_atom_cstr"
Cohesion: 0.15
Nodes (19): mem_atom_cstr(), Lexer, parse_list_body(), Evaluator, Result, Value, prim_catch(), prim_error() (+11 more)

### Community 36 - "test_frame.c"
Cohesion: 0.09
Nodes (45): frame_clear_continuation(), frame_current(), frame_get_bindings(), frame_get_test(), frame_has_pending_continuation(), frame_save_continuation(), frame_set_test(), FrameHeader (+37 more)

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
Nodes (60): mock_device_add_wifi_scan_result(), mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result() (+52 more)

### Community 41 - "primitives_files.c"
Cohesion: 0.22
Nodes (29): Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble(), prim_filelen() (+21 more)

### Community 42 - "prim_savel"
Cohesion: 0.31
Nodes (19): prim_editfile(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile(), prim_save() (+11 more)

### Community 43 - "mock_device_get_state"
Cohesion: 0.03
Nodes (134): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), MockCommand (+126 more)

### Community 44 - "test_time.c"
Cohesion: 0.07
Nodes (45): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+37 more)

### Community 45 - "set_mock_input"
Cohesion: 0.15
Nodes (44): prim_pause(), LogoIO, repl_cleanup(), repl_extract_proc_name(), repl_init(), repl_run(), ReplFlags, test_repl_defines_proc_with_multiline_paren() (+36 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.15
Nodes (44): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+36 more)

### Community 47 - "test_primitives_tilemap.c"
Cohesion: 0.09
Nodes (40): mock_device_get_canvas_point(), mock_device_paint_canvas(), mock_device_set_canvas_point(), assert_baked_tile(), assert_canvas_block(), capture_tile(), tearDown(), test_bake_paints_empty_cells_in_the_background_colour() (+32 more)

### Community 48 - "repository"
Cohesion: 0.04
Nodes (45): name, name, match, name, 1, 2, match, name (+37 more)

### Community 49 - "test_primitives_hardware.c"
Cohesion: 0.05
Nodes (47): tearDown(), test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable() (+39 more)

### Community 50 - "Turtle Trails (design)"
Cohesion: 0.06
Nodes (32): 10. Main loop and state order, 11.1 The procedure table, 11. Memory and performance budget, 12. Design boundaries, 13. Tests, 14. Implementation milestones, 15. As built: divergences from this design, 1. Theme (+24 more)

### Community 51 - "httpd.c"
Cohesion: 0.09
Nodes (60): demons_running(), LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find() (+52 more)

### Community 52 - "stream.c"
Cohesion: 0.11
Nodes (35): screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error(), logo_stream_close(), logo_stream_copy(), logo_stream_flush() (+27 more)

### Community 53 - "lcd.c"
Cohesion: 0.10
Nodes (38): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+30 more)

### Community 54 - ";"
Cohesion: 0.05
Nodes (43): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Break (+35 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.08
Nodes (36): mock_sound_set_status(), assert_number_list(), assert_word(), MockDeviceState, Result, Value, snd(), test_env_default() (+28 more)

### Community 57 - "test_primitives_files_load_save.c"
Cohesion: 0.05
Nodes (59): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle() (+51 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (41): fat32_close(), fat32_create(), fat32_delete(), fat32_is_mounted(), fat32_mount(), fat32_open(), fat32_read(), fat32_set_current_dir() (+33 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (34): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+26 more)

### Community 61 - "memory.c"
Cohesion: 0.09
Nodes (52): BlobDesc, demons_gc_mark_all(), remember_binding(), op_stack_gc_mark(), alloc_cell(), atom_chain_next(), atom_clear_marks(), atom_entry_is_free() (+44 more)

### Community 62 - "logo_io_set_writer"
Cohesion: 0.18
Nodes (17): logo_io_check_write_error(), logo_io_is_dribbling(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble(), test_check_write_error(), test_check_write_error_with_dribble(), test_console_write() (+9 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.18
Nodes (34): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+26 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.06
Nodes (62): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+54 more)

### Community 66 - "demons_poll"
Cohesion: 0.11
Nodes (36): Result, demons_frozen(), demons_maybe_poll(), demons_poll(), logo_io_ticks_ms(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle() (+28 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.13
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "test_primitives_list_processing.c"
Cohesion: 0.05
Nodes (41): test_apply_unknown_procedure(), test_apply_with_lambda(), test_apply_with_multi_param_lambda(), test_apply_with_primitive_name(), test_apply_with_procedure_text(), test_apply_with_user_procedure(), test_apply_with_word_primitive(), test_crossmap_basic() (+33 more)

### Community 70 - "primitives_outside_world.c"
Cohesion: 0.30
Nodes (18): Evaluator, Node, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar() (+10 more)

### Community 71 - "primitives.h"
Cohesion: 0.08
Nodes (38): Value, demons_clear(), demons_freeze(), demons_print(), demons_reset(), demons_resume(), demons_suspend(), demons_thaw() (+30 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (20): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+12 more)

### Community 74 - "stdlib.h"
Cohesion: 0.08
Nodes (26): keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_set_background_poll(), keyboard_set_idle_callback(), keyboard_set_key_available_callback(), lcd_get_palette_value() (+18 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (28): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_dir_create() (+20 more)

### Community 76 - "proc_define_from_text"
Cohesion: 0.03
Nodes (113): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), proc_define_from_text(), token_to_atom(), test_deep_nested_proc_in_repeat() (+105 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "main"
Cohesion: 0.09
Nodes (24): proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), main(), psram_verify(), m1_capture(), m1_equal(), picocalc_flash_erase() (+16 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "proc_get_frame_stack"
Cohesion: 0.09
Nodes (38): frame_binding_count(), frame_pop(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_init(), frame_stack_is_empty(), frame_stack_used_bytes(), FrameStack (+30 more)

### Community 81 - "host_storage.c"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.21
Nodes (19): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), fill_pattern(), setUp() (+11 more)

### Community 83 - "result_error_arg"
Cohesion: 0.12
Nodes (52): CatalogContext, CatalogEntry, Evaluator, LogoEntryType, LogoIO, Result, Value, catalog_callback() (+44 more)

### Community 84 - "test_lfs_storage.c"
Cohesion: 0.10
Nodes (23): lfs_t, LogoStorage, logo_lfs_storage_init(), Listing, setUp(), bd_erase(), bd_prog(), bd_read() (+15 more)

### Community 85 - "Code Review — 2026-07-02"
Cohesion: 0.08
Nodes (23): 1. Confirmed bug: `recycle` sweeps reachable data, 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate), 2.2 `find_atom` is a linear scan of the whole atom table, 2.3 Smaller items, 2. Hot-path efficiency, 3. Robustness: `mem_cons` failures are silently ignored, 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict, 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision (+15 more)

### Community 86 - "Contributing"
Cohesion: 0.08
Nodes (23): About Logo, Additional Features for the PicoCalc, Advanced Logo, Beginning Logo, Building and Running, Contributing, Credits, Dependencies (+15 more)

### Community 87 - "main"
Cohesion: 0.12
Nodes (23): Lexer, eval_init(), LogoHardware, LogoHardwareOps, logo_hardware_init(), LogoConsole, LogoStream, host_input_can_read() (+15 more)

### Community 88 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 89 - "op_stack_depth"
Cohesion: 0.11
Nodes (30): EvalOp, Value, LOGO_HOT(), mark_value(), op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init() (+22 more)

### Community 90 - "test_pop_scope"
Cohesion: 0.17
Nodes (27): var_declare_local(), var_set_local(), test_local_variable_not_visible_after_scope(), test_local_variable_shadowing(), test_localmake_declares_and_sets(), test_localmake_shadows_global(), test_make_creates_global_when_no_local(), test_make_updates_local_in_scope() (+19 more)

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

### Community 95 - "eval.c"
Cohesion: 0.28
Nodes (19): EvalOpKind, Evaluator, FrameStack, Node, Result, Value, eval_get_frames(), eval_in_procedure() (+11 more)

### Community 96 - "Space Invaders in Pico Logo (design & implementation)"
Cohesion: 0.09
Nodes (22): 10. Why this is a good P5 acceptance test, 11. Deliverable, 1. The board, 2. Object representation — the central decision, 3. The alien formation on the canvas, 4. Collision routing — demons vs. the game loop, 5. Global events as demons, 6. Input (+14 more)

### Community 97 - "package.json"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 98 - "test_mklfsimg.c"
Cohesion: 0.14
Nodes (17): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+9 more)

### Community 99 - "mem_cons"
Cohesion: 0.08
Nodes (82): atom_memo_class(), atom_memo_set_class(), mem_cons(), Lexer, Node, Token, TokenType, compute_word_class() (+74 more)

### Community 101 - "sdcard.c"
Cohesion: 0.23
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "procedures.c"
Cohesion: 0.16
Nodes (23): Evaluator, Result, Value, prim_step(), prim_trace(), prim_unstep(), prim_untrace(), UserProcedure (+15 more)

### Community 104 - "primitives_control_flow.c"
Cohesion: 0.37
Nodes (17): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_for(), prim_forever() (+9 more)

### Community 105 - "primitives_text.c"
Cohesion: 0.34
Nodes (19): Evaluator, Result, Value, get_screen_ops(), get_text_ops(), prim_cleartext(), prim_cursor(), prim_fullscreen() (+11 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.11
Nodes (23): LogoDirCallback, LogoStorage, LogoStorageOps, LogoStream, cross_fs_move(), is_root(), logo_storage_router_init(), router_dir_create() (+15 more)

### Community 108 - "test_galaxian.c"
Cohesion: 0.19
Nodes (20): assert_num(), assert_true(), seed_convoy(), tearDown(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom(), test_file_loads_and_sets_globals() (+12 more)

### Community 109 - "mem_word_ptr"
Cohesion: 0.02
Nodes (129): mem_word_ptr(), parse_list_from_string(), ParseListResult, test_make_small_number_in_object_is_valid_json(), test_make_small_number_is_valid_json(), bind_long_blob_word(), exhaust_atom_table(), exhaust_node_pool() (+121 more)

### Community 110 - "test_primitives_outside_world.c"
Cohesion: 0.05
Nodes (41): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+33 more)

### Community 111 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 112 - "result_ok"
Cohesion: 0.16
Nodes (76): number_to_word(), mem_first_cell(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_list_append(), mem_next_cell(), mem_word(), mem_word_len() (+68 more)

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

### Community 122 - "test_frame_sync.c"
Cohesion: 0.22
Nodes (16): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), setUp(), test_boundary_is_fixed_regardless_of_work(), test_clock_wraparound() (+8 more)

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (58): MockStamp, mock_device_clear_graphics(), mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf() (+50 more)

### Community 125 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 126 - "read_map"
Cohesion: 0.17
Nodes (20): numf(), read_map(), read_slots(), step_tile(), test_calm_tiles_do_not_strand_bugs(), test_decoded_counts_match_the_encoded_words(), test_direction_deltas_and_opposites(), test_level_profiles_escalate() (+12 more)

### Community 127 - "roadmap.md"
Cohesion: 0.10
Nodes (19): Build and test, Code structure, Constraints, Graphify, Project, Unit testing, Working guidelines, Build & Test (+11 more)

### Community 128 - "mklfsimg_lib.c"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "primitives_tilemap.c"
Cohesion: 0.32
Nodes (17): bake_ready(), bake_rect(), Evaluator, LogoConsoleTurtle, Result, Value, get_turtle_ops(), is_int_in() (+9 more)

### Community 132 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 133 - "record_command"
Cohesion: 0.12
Nodes (16): mock_screen_fullscreen(), mock_screen_refresh_now(), mock_screen_set_refresh_auto(), mock_screen_splitscreen(), mock_screen_textscreen(), mock_text_clear(), mock_turtle_clear(), mock_turtle_draw() (+8 more)

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "P10 — Interpreter throughput (design)"
Cohesion: 0.06
Nodes (33): 10. Relationship to P9, 11.1 First profile (2026-08-02, Pico Plus 2 W, 200 frames), 11.2 The instruction-fetch experiment, built and ready to measure, 11.3 Measured: instruction fetch confirmed (2026-08-04, Plus 2 W), 11.4 Tier 2 measured: 53.2 ms, and every moved function shows (Plus 2 W), 11.5 Tier 3 measured, and the pattern is now the finding (Plus 2 W), 11.6 Tier 4, and where the tiering stops (Plus 2 W), 11. M5 — re-profile before choosing a lever (2026-08-02) (+25 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (23): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P10 — Interpreter throughput, P1 — Host REPL stdin + CI (+15 more)

### Community 137 - "primitives.c"
Cohesion: 0.12
Nodes (24): atom_memo_bind_index(), atom_memo_bind_kind(), atom_memo_set_binding(), Token, resolve_word(), primitive_by_index(), primitive_find(), primitive_find_n() (+16 more)

### Community 138 - "LOGO_HOT"
Cohesion: 0.21
Nodes (13): apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expression(), is_number_string() (+5 more)

### Community 139 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "format_buffer_init"
Cohesion: 0.05
Nodes (112): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_init(), format_buffer_output() (+104 more)

### Community 142 - "test_bench_throughput.c"
Cohesion: 0.28
Nodes (14): calibrate_ns(), define_workspace(), load_game(), now_ms(), tearDown(), test_bench_checkrun_play_frame(), test_bench_expr_shapes(), test_bench_proc_call_workspace_scaling() (+6 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "value_to_string"
Cohesion: 0.07
Nodes (49): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or(), value_bool() (+41 more)

### Community 146 - "PR Review Checklist (CRITICAL)"
Cohesion: 0.22
Nodes (8): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on

### Community 147 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "test_lfs_backup.c"
Cohesion: 0.12
Nodes (33): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+25 more)

### Community 150 - "primitives_variables.c"
Cohesion: 0.56
Nodes (8): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing()

### Community 151 - "HTTP Operations"
Cohesion: 0.25
Nodes (8): http.delete, http.get, http.header, HTTP Operations, http.patch, http.post, http.put, http.status

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
Cohesion: 0.12
Nodes (39): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_at() (+31 more)

### Community 159 - "screen.c"
Cohesion: 0.07
Nodes (41): lcd_enable_cursor(), lcd_move_cursor(), lcd_set_background(), lcd_set_cursor_char(), text_get_background(), text_get_foreground(), text_set_background(), text_set_cursor() (+33 more)

### Community 160 - "ensure_wifi_initialized"
Cohesion: 0.16
Nodes (15): WifiState, ensure_wifi_initialized(), mdns_start(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_wifi_connect(), picocalc_wifi_get_ip() (+7 more)

### Community 161 - "Bitwise Operations"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

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

### Community 167 - "value_number"
Cohesion: 0.18
Nodes (36): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+28 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 169 - "random.c"
Cohesion: 0.50
Nodes (3): logo_random_reset(), logo_random_seed(), pcg32_next()

### Community 170 - "load_logo"
Cohesion: 0.50
Nodes (4): load_logo(), load_trails(), setUp(), test_p10prof_profiler_runs()

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 177 - "Property Lists"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 180 - "picocalc_network_tls_connect"
Cohesion: 0.33
Nodes (9): picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), poll_lwip_with_timeout(), tcp_connect_and_wait(), tcp_resolve_addr(), tcp_state_alloc(), tls_client_config() (+1 more)

### Community 184 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 196 - "mock_device_set_raster"
Cohesion: 0.67
Nodes (3): LogoTurtleRaster, mock_device_set_raster(), mock_turtle_get_raster()

### Community 198 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

### Community 202 - "repl_evaluate_line"
Cohesion: 0.10
Nodes (25): proc_restore_execution_state(), proc_save_execution_state(), Result, repl_count_bracket_balance(), repl_evaluate_line(), repl_find_end_token(), repl_next_bracket_depth(), repl_proc_def_append() (+17 more)

## Knowledge Gaps
- **764 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+759 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **11 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `mem_atom`, `mem_is_nil`, `reset_output`, `eval_string`, `result_none`, `test_trails.c`, `error_format`, `test_primitives_files_directory.c`, `format_buffer_init`, `test_bench_throughput.c`, `LOGO_HOT`, `lexer_init`, `test_scaffold_setUp`, `test_primitives_conditionals.c`, `test_primitives_json.c`, `test_eval.c`, `test_scaffold.h`, `load_logo`, `mock_device_get_state`, `test_time.c`, `set_mock_input`, `test_primitives_tilemap.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_files_load_save.c`, `demons_poll`, `test_primitives_list_processing.c`, `proc_define_from_text`, `proc_get_frame_stack`, `main`, `test_pop_scope`, `eval.c`, `test_galaxian.c`, `mem_word_ptr`, `test_primitives_outside_world.c`, `result_ok`, `test_primitives_editor.c`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.130) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `run_string`, `mem_is_nil`, `reset_output`, `test_trails.c`, `error_format`, `LOGO_HOT`, `test_primitives_files_directory.c`, `format_buffer_init`, `lexer_init`, `value_to_string`, `test_httpd.c`, `mem_is_word`, `test_primitives_http.c`, `test_primitives_conditionals.c`, `test_primitives_json.c`, `test_eval.c`, `test_scaffold.h`, `mem_atom_cstr`, `test_primitives_wifi.c`, `mock_device_get_state`, `test_time.c`, `test_primitives_tilemap.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_network.c`, `test_mock_fs.h`, `demons_poll`, `test_primitives_list_processing.c`, `proc_define_from_text`, `proc_get_frame_stack`, `main`, `test_pop_scope`, `eval.c`, `test_galaxian.c`, `mem_word_ptr`, `test_primitives_outside_world.c`, `result_ok`, `test_scaffold.c`, `test_checkrun.c`, `read_map`?**
  _High betweenness centrality (0.090) - this node is a cross-community bridge._
- **Why does `result_none()` connect `result_none` to `run_string`, `mem_atom`, `mem_is_nil`, `primitives_tilemap.c`, `primitives_workspace.c`, `LOGO_HOT`, `format_buffer_init`, `LOGO_HOT`, `primitives_sound.c`, `primitives_variables.c`, `primitives_wifi.c`, `primitives_conditionals.c`, `value_number`, `primitives_files.c`, `prim_savel`, `set_mock_input`, `httpd.c`, `memory.c`, `demons_poll`, `primitives_outside_world.c`, `primitives.h`, `repl_evaluate_line`, `result_error_arg`, `op_stack_depth`, `eval.c`, `procedures.c`, `primitives_control_flow.c`, `primitives_text.c`, `result_ok`?**
  _High betweenness centrality (0.047) - this node is a cross-community bridge._
- **Are the 970 inferred relationships involving `run_string()` (e.g. with `define_workspace()` and `load_game()`) actually correct?**
  _`run_string()` has 970 INFERRED edges - model-reasoned connections that need verification._
- **Are the 921 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 921 INFERRED edges - model-reasoned connections that need verification._
- **Are the 440 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `LOGO_HOT()`) actually correct?**
  _`mem_word_ptr()` has 440 INFERRED edges - model-reasoned connections that need verification._
- **Are the 232 inferred relationships involving `mem_atom()` (e.g. with `LOGO_HOT()` and `parse_list()`) actually correct?**
  _`mem_atom()` has 232 INFERRED edges - model-reasoned connections that need verification._