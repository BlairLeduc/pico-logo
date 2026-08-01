# Graph Report - pico-logo  (2026-07-31)

## Corpus Check
- 285 files · ~474,617 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 6988 nodes · 22148 edges · 190 communities (184 shown, 6 thin omitted)
- Extraction: 56% EXTRACTED · 44% INFERRED · 0% AMBIGUOUS · INFERRED: 9795 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `08e15c68`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- run_string
- lfs.c
- eval_string
- test_value.c
- mem_is_nil
- reset_output
- mem_word_ptr
- result_none
- value_to_string
- test_trails.c
- iteration_callback
- io.c
- test_primitives_files_load_save.c
- picocalc_console.c
- format_buffer_init
- lexer_init
- test_frame_arena.c
- test_io.c
- syntax_highlight_line
- test_variables.c
- primitives_sound.c
- unity.c
- test_httpd.c
- error_format
- Turtle Graphics
- test_scaffold_setUp
- test_primitives_http.c
- fat32.c
- primitives_httpd.c
- mock_device.c
- test_primitives_json.c
- test_primitives_conditionals.c
- test_scaffold.h
- test_notation.c
- primitives_init
- keyboard.c
- primitives.h
- lexer_next_token
- eval_push_if
- test_cross_fs_move.c
- test_primitives_wifi.c
- test_primitives_files.c
- primitives_files_directory.c
- Managing Various Files
- test_time.c
- set_mock_input
- picocalc_editor_edit
- test_primitives_files_directory.c
- repository
- test_primitives_hardware.c
- Turtle Trails (design)
- httpd.c
- stream.c
- lcd.c
- ;
- Checkpoint Run — a maze-driving game (design)
- test_sound.c
- test_primitives_outside_world.c
- picocalc_hardware.c
- fat32_close
- test_primitives_network.c
- memory.c
- primitive_find
- primitives_json.c
- Conditionals and Control of Flow
- test_mock_fs.h
- demons_poll
- test_dirty_tiles.c
- Words and Lists
- op_stack_push
- test_primitives_editor.c
- step_proc_call
- lfs_storage.c
- mock_sdcard.c
- stdlib.h
- picocalc_storage.c
- proc_define_from_text
- lexer.c
- picocalc_flash.c
- test_storage_router.c
- test_frame.c
- host_storage.c
- test_costumes.c
- Introduction
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- host_console.c
- mock_device_get_state
- screen_set_mode
- logo_io_open
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- test_primitives_variables.c
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- test_token_source.c
- host_hardware.c
- sdcard.c
- Managing your Workspace
- proc_get_frame_stack
- test_help.c
- primitives_get_io
- test_tls_heap.c
- storage_router.c
- eval_primary
- primitives_http.c
- primitives_outside_world.c
- mem_atom
- primitives_workspace.c
- P9 — Tile maps and smooth scrolling (design)
- test_galaxian.c
- Galaxian in Pico Logo (design)
- File Management
- primitives_control_flow.c
- test_scaffold.c
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- ensure_wifi_initialized
- Design: `launch` background processes (P6)
- test_checkrun.c
- repl_evaluate_line
- mklfsimg_lib.c
- roadmap.md
- sound.c
- Using the Logo Editor
- HTTP Server
- mem_blob
- southbridge.c
- mem_atom_cstr
- WiFi Management
- prim_pause
- The pick of five: plans
- test_repl.c
- HTTP Operations
- logo_lfs_backup
- ms_to_datetime
- prim_error
- Text and Screen Commands
- The Outside World
- HTTP server (design)
- logo_io_close_all
- What to flag (in priority order)
- primitives_variables.c
- record_command_float
- test_lfs_backup.c
- prim_not
- Atom Garbage Collection: Implementation Plan
- Modifying Procedures Under Program Control
- prim_define
- primitives_properties.c
- as_httpd_conn
- Appendix A: Useful Tools
- prim_setdate
- logo_console_init
- Appendix B: Parsing
- logo_random_next
- Time Management
- JSON
- mock_device_get_dot
- Pico Logo
- value_number
- drain_tokens
- gen_ca_certs.py
- pandoc_slug
- primitives_bitwise.c
- dist.sh
- generate_help.sh
- run_e2e.sh
- VENDOR.md
- picocalc_network_tls_connect
- mock_device_set_editor_result
- List Processing

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 924 edges
2. `eval_string()` - 900 edges
3. `mem_word_ptr()` - 444 edges
4. `mem_is_nil()` - 238 edges
5. `mem_atom()` - 235 edges
6. `value_to_string()` - 201 edges
7. `result_error_arg()` - 195 edges
8. `result_none()` - 192 edges
9. `result_ok()` - 176 edges
10. `lexer_init()` - 173 edges

## Surprising Connections (you probably didn't know these)
- `test_nil_is_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_word()` --calls--> `mem_is_word()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_number_content()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_negative()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_type()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Communities (190 total, 6 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (201): mock_device_get_output(), mock_device_has_line_from_to(), mock_device_paint_canvas(), mock_device_set_canvas_point(), mock_device_verify_palette(), output_has(), stage_raster(), test_addressing_primitives_registered() (+193 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "eval_string"
Cohesion: 0.02
Nodes (201): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+193 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (169): Node, demons_print(), demons_set(), format_number(), prim_battery_level(), Node, Result, Value (+161 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (114): format_property_list(), mem_car(), mem_cdr(), mem_is_nil(), mem_is_word(), Evaluator, Result, Value (+106 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (176): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+168 more)

### Community 6 - "mem_word_ptr"
Cohesion: 0.02
Nodes (127): mem_word_ptr(), test_make_small_number_in_object_is_valid_json(), test_make_small_number_is_valid_json(), bind_long_blob_word(), exhaust_atom_table(), exhaust_node_pool(), test_ascii(), test_bar_list_literal_count() (+119 more)

### Community 7 - "result_none"
Cohesion: 0.08
Nodes (113): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator, Result, Value (+105 more)

### Community 8 - "value_to_string"
Cohesion: 0.13
Nodes (87): number_to_word(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_list_append(), mem_word_len(), prim_form(), prim_free(), Evaluator (+79 more)

### Community 9 - "test_trails.c"
Cohesion: 0.11
Nodes (65): mock_device_clear_graphics(), actor(), load_trails(), num(), numf(), put_actor(), read_map(), run() (+57 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.07
Nodes (84): demons_maybe_poll(), eval_instruction(), httpd_maybe_poll(), LogoIO, LogoStream, SyntaxCategory, highlight_write_span(), logo_io_check_freeze_request() (+76 more)

### Community 12 - "test_primitives_files_load_save.c"
Cohesion: 0.06
Nodes (55): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle() (+47 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.06
Nodes (52): LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_flush(), error_output_write(), heading_faces_left() (+44 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.06
Nodes (85): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_init(), format_buffer_output() (+77 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_io.c"
Cohesion: 0.05
Nodes (44): logo_io_parse_network_address(), logo_io_resolve_path(), logo_io_set_prefix(), normalize_path(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn() (+36 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.07
Nodes (70): Value, find_global(), var_bury(), var_bury_all(), var_declare_local(), var_erase(), var_erase_all(), var_erase_all_globals() (+62 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (72): httpd_listening(), httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled() (+64 more)

### Community 23 - "error_format"
Cohesion: 0.03
Nodes (85): append_caller_suffix(), Result, error_clear_caught(), error_format(), error_message(), error_set_caught(), test_error_format_cant_from_editor(), test_error_format_cant_open_network() (+77 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.04
Nodes (81): LogoIO, primitives_control_reset_test_state(), primitives_set_io(), procedures_init(), properties_init(), var_reset_test_state(), variables_init(), LogoHardware (+73 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.05
Nodes (76): logo_mem_set_aux_region(), mem_is_blob(), mock_device_get_last_resolve_hostname(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_resolve_result() (+68 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (52): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+44 more)

### Community 28 - "primitives_httpd.c"
Cohesion: 0.17
Nodes (30): httpd_body(), httpd_body_unread(), httpd_method(), httpd_path(), httpd_query(), httpd_remote(), mem_word(), Evaluator (+22 more)

### Community 29 - "mock_device.c"
Cohesion: 0.02
Nodes (59): MockCommandType, LogoPen, LogoStream, LogoTurtleRaster, SoundEvent, SoundStatus, WifiState, mock_device_add_wifi_scan_result() (+51 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (60): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+52 more)

### Community 31 - "test_primitives_conditionals.c"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (64): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), mock_fs_reset(), mock_fs_tearDown(), setUp() (+56 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.11
Nodes (34): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+26 more)

### Community 35 - "keyboard.c"
Cohesion: 0.10
Nodes (19): repeating_timer_t, keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_poll(), keyboard_set_background_poll(), keyboard_set_key_available_callback() (+11 more)

### Community 36 - "primitives.h"
Cohesion: 0.09
Nodes (27): Value, demons_clear(), demons_freeze(), demons_reset(), demons_running(), demons_thaw(), value_is_true(), count_bracket_balance() (+19 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "eval_push_if"
Cohesion: 0.48
Nodes (11): eval_push_if(), Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse() (+3 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.05
Nodes (58): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result(), mock_device_set_wifi_ssid() (+50 more)

### Community 41 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (46): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+38 more)

### Community 42 - "primitives_files_directory.c"
Cohesion: 0.16
Nodes (32): CatalogContext, CatalogEntry, Evaluator, LogoEntryType, LogoIO, Result, Value, catalog_callback() (+24 more)

### Community 43 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 44 - "test_time.c"
Cohesion: 0.07
Nodes (43): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+35 more)

### Community 45 - "set_mock_input"
Cohesion: 0.18
Nodes (37): LogoIO, repl_cleanup(), repl_init(), repl_run(), ReplFlags, test_repl_defines_proc_with_multiline_paren(), test_repl_error_clears_sync_refresh(), test_repl_error_restores_auto_refresh() (+29 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "test_primitives_files_directory.c"
Cohesion: 0.06
Nodes (41): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+33 more)

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
Cohesion: 0.16
Nodes (31): LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find(), httpd_buf_init() (+23 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (35): create_network_stream(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error(), logo_stream_copy(), logo_stream_flush() (+27 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (35): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+27 more)

### Community 54 - ";"
Cohesion: 0.04
Nodes (52): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, ashift, battery, bitand (+44 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.08
Nodes (35): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), tearDown(), test_env_default(), test_play_appends() (+27 more)

### Community 57 - "test_primitives_outside_world.c"
Cohesion: 0.05
Nodes (41): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+33 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.06
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.12
Nodes (45): repeating_timer_t, fat32_close(), fat32_create(), fat32_delete(), fat32_is_mounted(), fat32_mount(), fat32_open(), fat32_read() (+37 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.14
Nodes (27): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_set_ntp_result(), mock_device_set_ping_result(), tearDown(), test_network_ping_requires_one_argument(), test_network_ping_requires_word_argument() (+19 more)

### Community 61 - "memory.c"
Cohesion: 0.09
Nodes (48): BlobDesc, demons_gc_mark_all(), op_stack_gc_mark(), alloc_cell(), atom_chain_next(), atom_clear_marks(), atom_entry_is_free(), atom_entry_next() (+40 more)

### Community 62 - "primitive_find"
Cohesion: 0.18
Nodes (11): primitive_find(), primitive_get_by_index(), primitive_get_count(), primitive_register_alias(), Primitive, test_every_primitive_has_help_entry(), test_primitives_are_registered(), test_primitive_abbreviation_resolves_same_function() (+3 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.07
Nodes (53): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+45 more)

### Community 66 - "demons_poll"
Cohesion: 0.08
Nodes (42): Result, demons_frozen(), demons_poll(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle(), setUp(), tearDown() (+34 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.14
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "op_stack_push"
Cohesion: 0.09
Nodes (57): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, UserProcedure, Value (+49 more)

### Community 70 - "test_primitives_editor.c"
Cohesion: 0.09
Nodes (44): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, mock_file_can_read(), mock_file_close(), mock_file_flush() (+36 more)

### Community 71 - "step_proc_call"
Cohesion: 0.22
Nodes (31): eval_trampoline(), op_stack_pop(), EvalOp, EvalOpKind, Evaluator, Node, Result, UserProcedure (+23 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (19): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+11 more)

### Community 74 - "stdlib.h"
Cohesion: 0.07
Nodes (28): text_get_background(), text_get_foreground(), text_set_background(), turtle_canvas_point(), turtle_dot(), turtle_dot_at(), turtle_draw_text(), turtle_fill() (+20 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (27): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_file_open() (+19 more)

### Community 76 - "proc_define_from_text"
Cohesion: 0.03
Nodes (93): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), proc_define_from_text(), token_to_atom(), test_deep_nested_proc_in_repeat() (+85 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "picocalc_flash.c"
Cohesion: 0.12
Nodes (19): m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest(), writable_m1(), bd_erase() (+11 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "test_frame.c"
Cohesion: 0.08
Nodes (89): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+81 more)

### Community 81 - "host_storage.c"
Cohesion: 0.12
Nodes (17): LogoDirCallback, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos(), host_file_get_write_pos() (+9 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "Introduction"
Cohesion: 0.17
Nodes (12): A Further Note on Operations, Another Way to Talk about Procedures, Formal Logo, How to Think about Procedures You Define and their Inputs, How You Might Think about MAKE, How You Might Think about Quotes, Introduction, Logo Objects (+4 more)

### Community 84 - "test_lfs_storage.c"
Cohesion: 0.12
Nodes (18): Listing, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t, LogoEntryType (+10 more)

### Community 85 - "Code Review — 2026-07-02"
Cohesion: 0.09
Nodes (23): 1. Confirmed bug: `recycle` sweeps reachable data, 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate), 2.2 `find_atom` is a linear scan of the whole atom table, 2.3 Smaller items, 2. Hot-path efficiency, 3. Robustness: `mem_cons` failures are silently ignored, 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict, 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision (+15 more)

### Community 86 - "Contributing"
Cohesion: 0.08
Nodes (23): About Logo, Additional Features for the PicoCalc, Advanced Logo, Beginning Logo, Building and Running, Contributing, Credits, Dependencies (+15 more)

### Community 87 - "host_console.c"
Cohesion: 0.36
Nodes (10): LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write(), restore_mode() (+2 more)

### Community 88 - "mock_device_get_state"
Cohesion: 0.03
Nodes (118): MockCommand, MockLine, LogoConsole, MockDeviceState, mock_device_clear_commands(), mock_device_command_count(), mock_device_dot_count(), mock_device_get_command() (+110 more)

### Community 89 - "screen_set_mode"
Cohesion: 0.10
Nodes (30): picocalc_editor_get_ops(), keyboard_set_idle_callback(), lcd_get_palette_value(), lcd_restore_palette(), lcd_set_background(), lcd_set_palette_rgb(), lcd_set_palette_value(), LogoConsole (+22 more)

### Community 90 - "logo_io_open"
Cohesion: 0.25
Nodes (18): prim_editfile(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile(), prim_savepic() (+10 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.10
Nodes (30): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+22 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.08
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (implemented)"
Cohesion: 0.06
Nodes (31): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+23 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "test_primitives_variables.c"
Cohesion: 0.12
Nodes (15): setUp(), tearDown(), test_dots_variable(), test_error_no_value(), test_global_variable(), test_local_declaration(), test_local_with_list(), test_localmake_in_procedure() (+7 more)

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
Cohesion: 0.09
Nodes (72): Lexer, Node, Token, TokenType, classify_word(), is_comment_node(), is_delimiter_token(), is_number_word() (+64 more)

### Community 101 - "sdcard.c"
Cohesion: 0.21
Nodes (21): fat32_init(), logo_picocalc_mount_available(), sd_error_t, sd_card_init(), sd_card_present(), sd_cs_deselect(), sd_cs_select(), sd_error_string() (+13 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "proc_get_frame_stack"
Cohesion: 0.08
Nodes (39): frame_binding_count(), frame_pop(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_init(), frame_stack_is_empty(), frame_stack_used_bytes(), FrameStack (+31 more)

### Community 104 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 105 - "primitives_get_io"
Cohesion: 0.30
Nodes (26): Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble(), prim_filelen() (+18 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "eval_primary"
Cohesion: 0.21
Nodes (21): eval_at_end(), apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expr_bp() (+13 more)

### Community 109 - "primitives_http.c"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

### Community 110 - "primitives_outside_world.c"
Cohesion: 0.20
Nodes (22): Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Node, Result, Value, flush_writer() (+14 more)

### Community 111 - "mem_atom"
Cohesion: 0.05
Nodes (110): mem_atom(), mem_atom_unescape(), mem_cons(), mem_free_nodes(), mem_gc(), mem_set_cdr(), mem_word_eq(), Node (+102 more)

### Community 112 - "primitives_workspace.c"
Cohesion: 0.12
Nodes (51): format_variable(), Evaluator, Result, Value, prim_edall(), prim_edit(), prim_edn(), prim_edns() (+43 more)

### Community 113 - "P9 — Tile maps and smooth scrolling (design)"
Cohesion: 0.08
Nodes (24): 10. Checkpoint Run revamp, 11. Turtle Trails revamp (render-only, gameplay identical), 12. Budgets, 13. Milestones, 14. Tests, 15. Levers if M0 misses, 16. Rejected alternatives, 17. Roadmap gate questions, resolved (+16 more)

### Community 114 - "test_galaxian.c"
Cohesion: 0.17
Nodes (22): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), tearDown(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin() (+14 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (19): backup, cat, catalog, copyfile, createdir, dir? (dirp), directories, editfile (+11 more)

### Community 117 - "primitives_control_flow.c"
Cohesion: 0.37
Nodes (17): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_for(), prim_forever() (+9 more)

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

### Community 122 - "ensure_wifi_initialized"
Cohesion: 0.16
Nodes (15): WifiState, ensure_wifi_initialized(), mdns_start(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_wifi_connect(), picocalc_wifi_get_ip() (+7 more)

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (56): MockStamp, mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf(), player_at_junction() (+48 more)

### Community 125 - "repl_evaluate_line"
Cohesion: 0.23
Nodes (13): proc_reset_execution_state(), proc_restore_execution_state(), proc_save_execution_state(), Result, name_distance(), repl_evaluate_line(), repl_next_bracket_depth(), repl_restore_refresh() (+5 more)

### Community 126 - "mklfsimg_lib.c"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 127 - "roadmap.md"
Cohesion: 0.08
Nodes (24): Build and test, Code structure, Constraints, Graphify, Project, Unit testing, Working guidelines, Build & Test (+16 more)

### Community 128 - "sound.c"
Cohesion: 0.20
Nodes (13): SoundEvent, SoundStatus, is_noise_voice(), __not_in_flash_func(), queue_empty(), queue_free(), sound_gate(), sound_init() (+5 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "mem_blob"
Cohesion: 0.16
Nodes (19): blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_region_alloc(), mem_words_equal(), str_eq_nocase(), editor_pick_buffer() (+11 more)

### Community 132 - "southbridge.c"
Cohesion: 0.32
Nodes (14): picocalc_get_battery_level(), picocalc_power_off(), sb_is_power_off_supported(), sb_read(), sb_read_battery(), sb_read_keyboard(), sb_read_keyboard_backlight(), sb_read_keyboard_state() (+6 more)

### Community 133 - "mem_atom_cstr"
Cohesion: 0.23
Nodes (23): mem_atom_cstr(), Evaluator, Result, Value, prim_network_ping(), prim_network_resolve(), prim_ntp(), Evaluator (+15 more)

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "prim_pause"
Cohesion: 0.26
Nodes (13): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+5 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (22): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P1 — Host REPL stdin + CI, P2 — List utilities: `pick`, `reverse`, `shuffle` (+14 more)

### Community 137 - "test_repl.c"
Cohesion: 0.09
Nodes (29): repl_count_bracket_balance(), repl_extract_proc_name(), repl_find_end_token(), repl_line_is_end(), repl_line_starts_with_to(), repl_proc_def_append(), ProcDefStatus, load_fileserver() (+21 more)

### Community 138 - "HTTP Operations"
Cohesion: 0.25
Nodes (8): http.delete, http.get, http.header, HTTP Operations, http.patch, http.post, http.put, http.status

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.28
Nodes (13): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), ntp_dns_callback(), ntp_send_request() (+5 more)

### Community 141 - "prim_error"
Cohesion: 0.39
Nodes (9): CaughtError, error_get_caught(), Evaluator, Result, Value, prim_catch(), prim_error(), prim_throw() (+1 more)

### Community 142 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "logo_io_close_all"
Cohesion: 0.43
Nodes (8): Evaluator, Result, Value, prim_bootsel(), prim_goodbye(), prim_toot(), toot_gate_freq(), logo_io_close_all()

### Community 146 - "What to flag (in priority order)"
Cohesion: 0.20
Nodes (9): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on (+1 more)

### Community 147 - "primitives_variables.c"
Cohesion: 0.56
Nodes (8): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing()

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "test_lfs_backup.c"
Cohesion: 0.16
Nodes (23): bd_erase(), bd_prog(), bd_read(), blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read() (+15 more)

### Community 151 - "prim_not"
Cohesion: 0.61
Nodes (7): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or()

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 154 - "prim_define"
Cohesion: 0.54
Nodes (8): Evaluator, Result, Value, prim_copydef(), prim_define(), prim_definedp(), prim_primitivep(), prim_text()

### Community 155 - "primitives_properties.c"
Cohesion: 0.45
Nodes (10): Evaluator, Result, Value, prim_erprops(), prim_gprop(), prim_plist(), prim_pprop(), prim_pps() (+2 more)

### Community 156 - "as_httpd_conn"
Cohesion: 0.32
Nodes (8): MockHttpdConn, as_httpd_conn(), httpd_conn_read(), httpd_conn_write(), mock_network_tcp_can_read(), mock_network_tcp_close(), mock_network_tcp_read(), mock_network_tcp_write()

### Community 157 - "Appendix A: Useful Tools"
Cohesion: 0.25
Nodes (8): Appendix A: Useful Tools, arcr and arcl, circler and circlel, divisor?, Graphics Tools, Math Tools, Program Logic or Debugging Tools, sort

### Community 158 - "prim_setdate"
Cohesion: 0.54
Nodes (8): Evaluator, Result, Value, prim_date(), prim_setdate(), prim_settime(), prim_ticks(), prim_time()

### Community 159 - "logo_console_init"
Cohesion: 0.39
Nodes (7): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init()

### Community 160 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 161 - "logo_random_next"
Cohesion: 0.47
Nodes (5): LogoIO, logo_random_next(), logo_random_reset(), logo_random_seed(), pcg32_next()

### Community 162 - "Time Management"
Cohesion: 0.33
Nodes (6): date, setdate, settime, ticks, time, Time Management

### Community 163 - "JSON"
Cohesion: 0.33
Nodes (6): JSON, json.array, json.count, json.get, json.make, json.object

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "value_number"
Cohesion: 0.32
Nodes (25): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+17 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 177 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 194 - "picocalc_network_tls_connect"
Cohesion: 0.18
Nodes (15): ntp_recv_callback(), picocalc_dns_callback(), picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), ping_recv_callback(), poll_lwip_with_timeout(), tcp_connect_and_wait() (+7 more)

### Community 196 - "mock_device_set_editor_result"
Cohesion: 0.13
Nodes (33): LogoEditorResult, mock_device_set_editor_content(), mock_device_set_editor_result(), mock_editor_edit(), MockFile, mock_fs_create_file(), mock_fs_get_content(), mock_fs_get_file() (+25 more)

### Community 197 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

## Knowledge Gaps
- **727 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+722 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `eval_string`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `result_none`, `test_repl.c`, `test_trails.c`, `io.c`, `test_primitives_files_load_save.c`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `error_format`, `test_scaffold_setUp`, `primitives_httpd.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold.h`, `test_primitives_files.c`, `test_time.c`, `set_mock_input`, `test_primitives_files_directory.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_outside_world.c`, `demons_poll`, `mock_device_set_editor_result`, `op_stack_push`, `test_primitives_editor.c`, `proc_define_from_text`, `mock_device_get_state`, `test_primitives_variables.c`, `proc_get_frame_stack`, `eval_primary`, `mem_atom`, `test_galaxian.c`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.106) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `test_value.c`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `value_to_string`, `test_trails.c`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `test_httpd.c`, `error_format`, `test_primitives_http.c`, `primitives_httpd.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold.h`, `test_primitives_wifi.c`, `test_primitives_files.c`, `test_time.c`, `test_primitives_files_directory.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_outside_world.c`, `test_primitives_network.c`, `test_mock_fs.h`, `demons_poll`, `op_stack_push`, `proc_define_from_text`, `mock_device_get_state`, `test_primitives_variables.c`, `proc_get_frame_stack`, `eval_primary`, `mem_atom`, `test_galaxian.c`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Why does `mem_word_ptr()` connect `mem_word_ptr` to `run_string`, `eval_string`, `mem_blob`, `mem_is_nil`, `mem_atom_cstr`, `test_value.c`, `prim_pause`, `value_to_string`, `result_none`, `reset_output`, `format_buffer_init`, `test_variables.c`, `primitives_sound.c`, `test_httpd.c`, `error_format`, `prim_define`, `test_primitives_http.c`, `primitives_httpd.c`, `primitives.h`, `test_primitives_wifi.c`, `test_primitives_files.c`, `primitives_files_directory.c`, `test_time.c`, `set_mock_input`, `test_primitives_files_directory.c`, `test_primitives_hardware.c`, `httpd.c`, `test_sound.c`, `test_primitives_outside_world.c`, `test_primitives_network.c`, `memory.c`, `primitives_json.c`, `test_primitives_editor.c`, `step_proc_call`, `proc_define_from_text`, `logo_io_open`, `test_primitives_variables.c`, `test_token_source.c`, `primitives_get_io`, `eval_primary`, `primitives_http.c`, `primitives_outside_world.c`, `mem_atom`, `primitives_workspace.c`, `primitives_control_flow.c`?**
  _High betweenness centrality (0.063) - this node is a cross-community bridge._
- **Are the 922 inferred relationships involving `run_string()` (e.g. with `load_checkrun()` and `run()`) actually correct?**
  _`run_string()` has 922 INFERRED edges - model-reasoned connections that need verification._
- **Are the 898 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 898 INFERRED edges - model-reasoned connections that need verification._
- **Are the 438 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 438 INFERRED edges - model-reasoned connections that need verification._
- **Are the 235 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `parse_list()`) actually correct?**
  _`mem_is_nil()` has 235 INFERRED edges - model-reasoned connections that need verification._