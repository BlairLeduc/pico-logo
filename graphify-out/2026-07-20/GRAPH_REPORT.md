# Graph Report - pico-logo  (2026-07-19)

## Corpus Check
- 276 files · ~430,576 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 6565 nodes · 20969 edges · 197 communities (188 shown, 9 thin omitted)
- Extraction: 55% EXTRACTED · 45% INFERRED · 0% AMBIGUOUS · INFERRED: 9492 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `3823b114`
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
- mem_atom
- test_frame.c
- proc_define_from_text
- error_format
- picocalc_console.c
- format_buffer_init
- lexer_init
- test_frame_arena.c
- test_primitives_editor.c
- syntax_highlight_line
- test_variables.c
- test_token_source.c
- unity.c
- test_httpd.c
- test_io.c
- Turtle Graphics
- io.c
- test_primitives_http.c
- fat32.c
- mock_device_get_state
- mock_device.c
- test_primitives_json.c
- test_primitives_control_flow.c
- test_scaffold.h
- test_primitives_conditionals.c
- test_primitives_files_load_save.c
- stdlib.h
- memory.c
- lexer_next_token
- test_primitives_files.c
- test_cross_fs_move.c
- test_primitives_wifi.c
- test_primitives_hardware.c
- mem_atom_cstr
- test_time.c
- test_scaffold_setUp
- set_mock_input
- picocalc_editor_edit
- screen.c
- repository
- test_primitives_outside_world.c
- primitives.h
- httpd.c
- stream.c
- lcd.c
- Pico_Logo_Reference.md
- primitives_workspace.c
- test_mock_device.c
- step_proc_call
- picocalc_hardware.c
- fat32_close
- test_primitives_network.c
- test_primitives_files_directory.c
- primitives_init
- primitives_json.c
- Conditionals and Control of Flow
- test_mock_fs.h
- demons_poll
- test_dirty_tiles.c
- Words and Lists
- op_stack_push
- value_number
- main
- lfs_storage.c
- mock_sdcard.c
- primitives_files.c
- picocalc_storage.c
- prim_savel
- lexer.c
- test_lfs_backup.c
- test_storage_router.c
- primitives_get_io
- host_storage.c
- test_costumes.c
- eval.c
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- main
- primitives_httpd.c
- primitives_http.c
- repl_evaluate_line
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (design)
- Arithmetic Operations
- run_editor_and_process
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- eval_push_if
- host_hardware.c
- sdcard.c
- Managing your Workspace
- test_scaffold.c
- test_help.c
- ensure_wifi_initialized
- test_tls_heap.c
- storage_router.c
- The pick of five: plans
- test_galaxian.c
- mklfsimg_lib.c
- logo_storage_init
- southbridge.c
- primitives_control_flow.c
- primitives_outside_world.c
- Galaxian in Pico Logo (design)
- File Management
- test_primitives_properties.c
- logo_io_init
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- record_command
- eval_primary
- frame_stack_is_empty
- procedures.c
- Design: `launch` background processes (P6)
- audio.c
- Using the Logo Editor
- HTTP Server
- prim_pause
- MockCommandType
- Introduction
- WiFi Management
- record_command_float
- logo_io_set_writer
- logo_console_init
- logo_lfs_backup
- ms_to_datetime
- HTTP server (design)
- Text and Screen Commands
- The Outside World
- prim_recycle
- What to flag (in priority order)
- LogoStream
- primitives_variables.c
- Memory Reclamation: Design Notes (deferred)
- 3. Prior art
- List Processing
- CLAUDE.md
- ip_addr_t
- improvements-roadmap.md
- 3. Survey: multi-turtle and sprite Logos, 1981→now
- as_httpd_conn
- Appendix A: Useful Tools
- Modifying Procedures Under Program Control
- Managing Various Files
- HTTP Operations
- Variables
- Pico Logo
- JSON
- drain_tokens
- repl_count_bracket_balance
- gen_ca_certs.py
- pandoc_slug
- iteration_callback
- lexer_set_preserve_comments
- mock_device_set_raster
- dist.sh
- mock_device_get_dot
- generate_help.sh
- run_e2e.sh
- LogoEditorResult
- VENDOR.md
- prim_not
- mem_set_cdr

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 894 edges
2. `eval_string()` - 867 edges
3. `mem_word_ptr()` - 421 edges
4. `mem_is_nil()` - 237 edges
5. `mem_atom()` - 229 edges
6. `value_to_string()` - 194 edges
7. `result_error_arg()` - 193 edges
8. `result_none()` - 190 edges
9. `result_ok()` - 175 edges
10. `value_number()` - 166 edges

## Surprising Connections (you probably didn't know these)
- `test_frame_at_none_returns_null()` --calls--> `frame_at()`  [INFERRED]
  tests/test_frame.c → core/frame.c
- `test_nil_is_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_list()` --calls--> `mem_is_list()`  [INFERRED]
  tests/test_memory.c → core/memory.c

## Import Cycles
- None detected.

## Communities (197 total, 9 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (205): MockTurtleState, mock_device_clear_graphics(), mock_device_get_output(), mock_device_get_turtle(), mock_device_has_line_from_to(), mock_device_paint_canvas(), mock_device_set_canvas_point(), test_cleardemons_leaves_motion_and_freeze() (+197 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "eval_string"
Cohesion: 0.02
Nodes (188): test_deep_recursion_100_levels(), test_deep_recursion_addupto(), test_deep_recursion_factorial(), test_deep_recursion_nested_in_expression(), test_deep_recursion_print_result(), test_error_infix_doesnt_like(), test_error_not_enough_inputs(), test_error_uses_alias_name_fd() (+180 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (162): format_number(), Node, Result, Value, extract_number_list(), Node, Result, Value (+154 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.06
Nodes (119): mem_car(), mem_cdr(), mem_is_list(), mem_is_nil(), mem_is_word(), prim_savel(), Evaluator, Result (+111 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (184): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+176 more)

### Community 6 - "mem_word_ptr"
Cohesion: 0.02
Nodes (168): Value, value_is_true(), mem_word_ptr(), test_make_small_number_in_object_is_valid_json(), test_make_small_number_is_valid_json(), test_apply_with_word_primitive(), test_filter_with_word(), test_find_basic() (+160 more)

### Community 7 - "result_none"
Cohesion: 0.07
Nodes (123): demons_freeze(), demons_thaw(), frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator (+115 more)

### Community 8 - "value_to_string"
Cohesion: 0.22
Nodes (53): number_to_word(), mem_list_append(), mem_word(), mem_word_len(), Evaluator, Node, Result, Value (+45 more)

### Community 9 - "mem_atom"
Cohesion: 0.07
Nodes (67): mem_atom(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_total_atoms(), mem_total_nodes(), mem_word_eq(), test_allocate_after_gc_recovery() (+59 more)

### Community 10 - "test_frame.c"
Cohesion: 0.08
Nodes (46): UserProcedure, frame_get_bindings(), frame_iterate(), frame_pop(), frame_reuse(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_init() (+38 more)

### Community 11 - "proc_define_from_text"
Cohesion: 0.03
Nodes (94): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), proc_define_from_text(), token_to_atom(), test_deep_nested_proc_in_repeat() (+86 more)

### Community 12 - "error_format"
Cohesion: 0.07
Nodes (42): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+34 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.06
Nodes (51): LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_write(), heading_faces_left(), input_read_char() (+43 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.06
Nodes (100): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_buffer_init(), format_buffer_output(), format_buffer_pos() (+92 more)

### Community 15 - "lexer_init"
Cohesion: 0.07
Nodes (83): lexer_init(), assert_token(), test_alphanumeric_word(), test_binary_minus_after_colon_no_space(), test_binary_minus_after_right_bracket_no_space(), test_binary_minus_after_word_no_space(), test_binary_minus_spacing(), test_complex_expression() (+75 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_primitives_editor.c"
Cohesion: 0.07
Nodes (76): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_set_editor_content(), mock_device_set_editor_result(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, MockFile (+68 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.07
Nodes (74): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+66 more)

### Community 19 - "test_variables.c"
Cohesion: 0.12
Nodes (47): value_number(), Value, find_global(), var_bury(), var_bury_all(), var_erase(), var_erase_all(), var_erase_all_globals() (+39 more)

### Community 20 - "test_token_source.c"
Cohesion: 0.09
Nodes (71): Lexer, Node, Token, TokenType, classify_word(), is_comment_node(), is_delimiter_token(), is_number_word() (+63 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (73): httpd_listening(), httpd_request_pending(), httpd_reset(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex() (+65 more)

### Community 23 - "test_io.c"
Cohesion: 0.05
Nodes (37): logo_io_parse_network_address(), logo_io_set_reader(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn(), mock_dir_callback(), mock_list_directory() (+29 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "io.c"
Cohesion: 0.08
Nodes (69): eval_instruction(), httpd_maybe_poll(), prim_editfile(), LogoDirCallback, LogoIO, LogoStream, SyntaxCategory, create_network_stream() (+61 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (67): logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), Result (+59 more)

### Community 27 - "fat32.c"
Cohesion: 0.11
Nodes (61): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_delete(), fat32_dir_create() (+53 more)

### Community 28 - "mock_device_get_state"
Cohesion: 0.03
Nodes (117): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), MockCommand (+109 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (11): mock_device_add_wifi_scan_result(), mock_device_get_tcp_request_len(), mock_device_set_input(), mock_device_set_snap_result(), mock_device_set_tcp_close_after(), mock_device_set_tcp_read_chunk(), mock_device_set_tcp_write_chunk(), mock_device_verify_palette() (+3 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (60): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+52 more)

### Community 31 - "test_primitives_control_flow.c"
Cohesion: 0.03
Nodes (59): test_false(), test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_operation_returns_value(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output() (+51 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (55): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown() (+47 more)

### Community 33 - "test_primitives_conditionals.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "test_primitives_files_load_save.c"
Cohesion: 0.03
Nodes (85): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), mock_fs_create_dir() (+77 more)

### Community 35 - "stdlib.h"
Cohesion: 0.08
Nodes (25): keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_set_background_poll(), keyboard_set_key_available_callback(), lcd_get_palette_value(), lcd_set_palette_value() (+17 more)

### Community 36 - "memory.c"
Cohesion: 0.11
Nodes (32): BlobDesc, demons_gc_mark_all(), Value, mark_value(), op_stack_gc_mark(), alloc_cell(), atom_entry_next(), atom_entry_set_next() (+24 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (46): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+38 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.06
Nodes (46): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result(), mock_device_set_wifi_ssid() (+38 more)

### Community 41 - "test_primitives_hardware.c"
Cohesion: 0.06
Nodes (45): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+37 more)

### Community 42 - "mem_atom_cstr"
Cohesion: 0.11
Nodes (59): Binding, FrameHeader, FrameStack, Value, word_offset_t, calc_frame_size(), frame_add_local(), frame_at() (+51 more)

### Community 43 - "test_time.c"
Cohesion: 0.06
Nodes (47): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+39 more)

### Community 44 - "test_scaffold_setUp"
Cohesion: 0.05
Nodes (57): blob_reset(), logo_mem_init(), LogoIO, primitives_control_reset_test_state(), primitives_set_io(), properties_init(), variables_init(), LogoHardware (+49 more)

### Community 45 - "set_mock_input"
Cohesion: 0.12
Nodes (48): LogoIO, repl_cleanup(), repl_extract_proc_name(), repl_init(), repl_line_is_end(), repl_line_starts_with_to(), repl_run(), ReplFlags (+40 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (41): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+33 more)

### Community 47 - "screen.c"
Cohesion: 0.09
Nodes (34): lcd_enable_cursor(), lcd_move_cursor(), lcd_set_cursor_char(), text_get_background(), text_get_foreground(), text_set_background(), text_set_cursor(), turtle_canvas_point() (+26 more)

### Community 48 - "repository"
Cohesion: 0.05
Nodes (42): name, name, 1, 2, match, name, match, name (+34 more)

### Community 49 - "test_primitives_outside_world.c"
Cohesion: 0.12
Nodes (22): picocalc_editor_get_ops(), keyboard_set_idle_callback(), lcd_restore_palette(), lcd_set_palette_rgb(), LogoConsole, error_output_flush(), logo_picocalc_console_create(), logo_picocalc_console_destroy() (+14 more)

### Community 50 - "primitives.h"
Cohesion: 0.08
Nodes (39): demons_running(), Evaluator, Result, Value, prim_step(), prim_trace(), prim_unstep(), prim_untrace() (+31 more)

### Community 51 - "httpd.c"
Cohesion: 0.10
Nodes (54): LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find(), httpd_body() (+46 more)

### Community 52 - "stream.c"
Cohesion: 0.15
Nodes (25): turtle_gfx_save(), LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error() (+17 more)

### Community 53 - "lcd.c"
Cohesion: 0.10
Nodes (40): dirty_tiles_clear(), repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink() (+32 more)

### Community 54 - "Pico_Logo_Reference.md"
Cohesion: 0.04
Nodes (44): and, Appendix B: Parsing, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Brackets and Parentheses (+36 more)

### Community 55 - "primitives_workspace.c"
Cohesion: 0.15
Nodes (40): Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add(), help_list_flush() (+32 more)

### Community 56 - "test_mock_device.c"
Cohesion: 0.09
Nodes (34): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), test_env_default(), test_play_appends(), test_play_bad_notation_errors() (+26 more)

### Community 57 - "step_proc_call"
Cohesion: 0.21
Nodes (29): op_stack_pop(), EvalOp, EvalOpKind, Evaluator, Node, Result, UserProcedure, eval_trace_entry() (+21 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (15): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_wifi_disconnect(), tcp_client_connected_cb() (+7 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (35): repeating_timer_t, fat32_close(), fat32_create(), fat32_is_mounted(), fat32_mount(), fat32_read(), fat32_unmount(), is_valid_fat32_boot_sector() (+27 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (35): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+27 more)

### Community 61 - "test_primitives_files_directory.c"
Cohesion: 0.11
Nodes (34): FrameStack, proc_get_frame_stack(), var_declare_local(), var_get_test(), var_local_count(), var_reset_test_state(), var_set_test(), var_test_is_valid() (+26 more)

### Community 62 - "primitives_init"
Cohesion: 0.12
Nodes (32): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+24 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (35): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+27 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.10
Nodes (33): LogoDirCallback, LogoStream, MockFile, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos() (+25 more)

### Community 66 - "demons_poll"
Cohesion: 0.13
Nodes (27): Result, demons_maybe_poll(), demons_poll(), mock_device_clear_output(), setUp(), test_action_does_not_reenter_poll(), test_cleardemons_disarms_all(), test_clearscreen_leaves_demons_armed() (+19 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.12
Nodes (31): dirty_tiles_any(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), console_idle_poll(), picocalc_sleep() (+23 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "op_stack_push"
Cohesion: 0.15
Nodes (29): EvalOp, op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init(), op_stack_is_empty(), op_stack_peek(), op_stack_push() (+21 more)

### Community 70 - "value_number"
Cohesion: 0.27
Nodes (25): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+17 more)

### Community 71 - "main"
Cohesion: 0.08
Nodes (26): Lexer, eval_init(), proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), main(), m1_capture(), m1_equal() (+18 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (19): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+11 more)

### Community 74 - "primitives_files.c"
Cohesion: 0.26
Nodes (24): EvalOpKind, Evaluator, FrameStack, Node, Result, UserProcedure, Value, eval_get_frames() (+16 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (27): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStream, file_context_stale(), logo_picocalc_dir_create(), logo_picocalc_dir_exists() (+19 more)

### Community 76 - "prim_savel"
Cohesion: 0.25
Nodes (19): httpd_savebody(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile(), prim_save() (+11 more)

### Community 77 - "lexer.c"
Cohesion: 0.18
Nodes (27): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+19 more)

### Community 78 - "test_lfs_backup.c"
Cohesion: 0.16
Nodes (23): bd_erase(), bd_prog(), bd_read(), blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read() (+15 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "primitives_get_io"
Cohesion: 0.09
Nodes (73): Node, demons_print(), demons_set(), mem_atom_cstr(), prim_when(), prim_error(), Evaluator, LogoEntryType (+65 more)

### Community 81 - "host_storage.c"
Cohesion: 0.12
Nodes (17): LogoDirCallback, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos(), host_file_get_write_pos() (+9 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "eval.c"
Cohesion: 0.14
Nodes (21): primitive_find(), primitive_get_by_index(), primitive_get_count(), primitive_register_alias(), Evaluator, Result, Value, prim_copydef() (+13 more)

### Community 84 - "test_lfs_storage.c"
Cohesion: 0.12
Nodes (18): Listing, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t, LogoEntryType (+10 more)

### Community 85 - "Code Review — 2026-07-02"
Cohesion: 0.12
Nodes (15): 1. Confirmed bug: `recycle` sweeps reachable data, 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate), 2.2 `find_atom` is a linear scan of the whole atom table, 2.3 Smaller items, 2. Hot-path efficiency, 3. Robustness: `mem_cons` failures are silently ignored, 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict, 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision (+7 more)

### Community 86 - "Contributing"
Cohesion: 0.08
Nodes (23): About Logo, Additional Features for the PicoCalc, Advanced Logo, Beginning Logo, Building and Running, Contributing, Credits, Dependencies (+15 more)

### Community 87 - "main"
Cohesion: 0.20
Nodes (17): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+9 more)

### Community 88 - "primitives_httpd.c"
Cohesion: 0.29
Nodes (23): Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble(), prim_filelen() (+15 more)

### Community 89 - "primitives_http.c"
Cohesion: 0.17
Nodes (19): httpd_buf_init(), blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_is_blob(), mem_region_alloc(), editor_pick_buffer() (+11 more)

### Community 90 - "repl_evaluate_line"
Cohesion: 0.14
Nodes (19): demons_clear(), demons_reset(), proc_restore_execution_state(), proc_save_execution_state(), Result, repl_count_bracket_balance(), repl_evaluate_line(), repl_next_bracket_depth() (+11 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.12
Nodes (26): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+18 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.09
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (design)"
Cohesion: 0.09
Nodes (23): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+15 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "run_editor_and_process"
Cohesion: 0.21
Nodes (11): logo_stream_write_line(), LogoStream, mock_write(), test_has_write_error_with_null_stream(), test_write_error_flag_persistence(), test_write_line_multiple_calls(), test_write_line_with_closed_stream(), test_write_line_with_empty_text() (+3 more)

### Community 96 - "Space Invaders in Pico Logo (design & implementation)"
Cohesion: 0.09
Nodes (22): 10. Why this is a good P5 acceptance test, 11. Deliverable, 1. The board, 2. Object representation — the central decision, 3. The alien formation on the canvas, 4. Collision routing — demons vs. the game loop, 5. Global events as demons, 6. Input (+14 more)

### Community 97 - "package.json"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 98 - "test_mklfsimg.c"
Cohesion: 0.14
Nodes (17): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+9 more)

### Community 99 - "eval_push_if"
Cohesion: 0.48
Nodes (11): eval_push_if(), Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse() (+3 more)

### Community 100 - "host_hardware.c"
Cohesion: 0.09
Nodes (5): LogoHardware, host_network_tcp_connect(), init_winsock(), logo_host_hardware_create(), logo_host_hardware_destroy()

### Community 101 - "sdcard.c"
Cohesion: 0.27
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "test_scaffold.c"
Cohesion: 0.12
Nodes (8): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write()

### Community 104 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 105 - "ensure_wifi_initialized"
Cohesion: 0.19
Nodes (17): ensure_wifi_initialized(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), picocalc_wifi_get_ip(), picocalc_wifi_get_mac() (+9 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "The pick of five: plans"
Cohesion: 0.10
Nodes (20): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Improvements Roadmap, Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P1 — Host REPL stdin + CI (+12 more)

### Community 109 - "test_galaxian.c"
Cohesion: 0.19
Nodes (20): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_file_loads_and_sets_globals() (+12 more)

### Community 110 - "mklfsimg_lib.c"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 111 - "logo_storage_init"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 112 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 113 - "primitives_control_flow.c"
Cohesion: 0.37
Nodes (17): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_for(), prim_forever() (+9 more)

### Community 114 - "primitives_outside_world.c"
Cohesion: 0.35
Nodes (16): Evaluator, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar(), prim_readchars() (+8 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (18): backup, catalog, copyfile, createdir, dir? (dirp), directories, editfile, erasedir (+10 more)

### Community 117 - "test_primitives_properties.c"
Cohesion: 0.22
Nodes (7): LogoHardware, LogoHardwareOps, logo_hardware_init(), LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), test_play_no_sound_engine_is_noop()

### Community 118 - "logo_io_init"
Cohesion: 0.18
Nodes (12): lfs_t, LogoStorage, logo_lfs_storage_init(), LogoStorage, LogoStorageOps, logo_storage_init(), LogoStorage, LogoStorageOps (+4 more)

### Community 119 - "clib.c"
Cohesion: 0.22
Nodes (14): logo_host_rename(), fat32_error_t, _close(), fat32_error_to_errno(), _fstat(), init(), _lseek(), _open() (+6 more)

### Community 120 - "P8 — Sound: a stereo PSG synthesizer (design)"
Cohesion: 0.10
Nodes (20): 10. Rejected alternatives, 11. Resolved questions (user, 2026-07-10), 12.1 DMA read ring-wrap (engine, 2026-07-18), 12.2 LCD driver no longer masks interrupts (2026-07-19), 12.3 Audio IRQ priority above default (2026-07-19), 12. Hardware bring-up findings (2026-07-18/19), 1. What limits sound today, 2. The output hardware (+12 more)

### Community 121 - "Input and Output to Files, Network Connections and Devices"
Cohesion: 0.12
Nodes (16): allopen, close, closeall, filelen, Input and Output to Files, Network Connections and Devices, open, reader, readpos (+8 more)

### Community 123 - "record_command"
Cohesion: 0.12
Nodes (16): mock_screen_fullscreen(), mock_screen_refresh_now(), mock_screen_set_refresh_auto(), mock_screen_splitscreen(), mock_screen_textscreen(), mock_text_clear(), mock_turtle_clear(), mock_turtle_draw() (+8 more)

### Community 124 - "eval_primary"
Cohesion: 0.21
Nodes (22): eval_at_end(), apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expr_bp() (+14 more)

### Community 125 - "frame_stack_is_empty"
Cohesion: 0.25
Nodes (8): 5.1 Duplicated infix-operator evaluation in `eval_expr.c`, 5.2 Four nearly identical loop steppers, 5.3 Repeated list-builder loop, 5.4 Repeated number→word coercion in element primitives, 5.5 Dead code, 5.6 Documentation drift in `memory.h`, 5.7 Small items, 5. Simplicity and maintainability

### Community 126 - "procedures.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 127 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 128 - "audio.c"
Cohesion: 0.18
Nodes (13): SoundEvent, SoundStatus, is_noise_voice(), __not_in_flash_func(), queue_empty(), queue_free(), sound_gate(), sound_init() (+5 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "prim_pause"
Cohesion: 0.31
Nodes (13): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+5 more)

### Community 132 - "MockCommandType"
Cohesion: 0.17
Nodes (12): MockCommandType, LogoPen, mock_turtle_dot(), mock_turtle_get_pen_state(), mock_turtle_set_bg_colour(), mock_turtle_set_pen_colour(), mock_turtle_set_pen_state(), mock_turtle_set_position() (+4 more)

### Community 133 - "Introduction"
Cohesion: 0.17
Nodes (12): A Further Note on Operations, Another Way to Talk about Procedures, Formal Logo, How to Think about Procedures You Define and their Inputs, How You Might Think about MAKE, How You Might Think about Quotes, Introduction, Logo Objects (+4 more)

### Community 134 - "WiFi Management"
Cohesion: 0.17
Nodes (12): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+4 more)

### Community 135 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 136 - "logo_io_set_writer"
Cohesion: 0.12
Nodes (26): logo_io_cleanup(), logo_io_close_all(), logo_io_is_dribbling(), logo_io_reader_is_keyboard(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble(), logo_io_writer_is_screen() (+18 more)

### Community 137 - "primitives_properties.c"
Cohesion: 0.48
Nodes (7): frame_get_test(), frame_set_test(), test_reuse_clears_test_state(), test_set_test_false(), test_set_test_true(), test_test_inherited_from_parent(), test_test_shadowed_by_child()

### Community 138 - "logo_console_init"
Cohesion: 0.67
Nodes (4): logo_io_get_timeout(), logo_io_set_timeout(), test_network_timeout(), test_set_negative_timeout()

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "HTTP server (design)"
Cohesion: 0.18
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 142 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing?, print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "prim_recycle"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 146 - "What to flag (in priority order)"
Cohesion: 0.20
Nodes (9): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on (+1 more)

### Community 147 - "LogoStream"
Cohesion: 0.20
Nodes (10): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write() (+2 more)

### Community 148 - "primitives_variables.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_local(), prim_localmake(), prim_make(), prim_name(), prim_namep() (+1 more)

### Community 149 - "Memory Reclamation: Design Notes (deferred)"
Cohesion: 0.25
Nodes (8): Background: the "atoms are never freed" simplification, Companion: let the node region shrink back, Design 1: `erall` soft reset (recommended first step, small), Design 2: atom mark-sweep with in-place reuse (larger, helps running programs), Memory Reclamation: Design Notes (deferred), Options considered and rejected, Suggested order, if ever needed, What PR #86 changed that makes reclamation feasible

### Community 150 - "3. Prior art"
Cohesion: 0.22
Nodes (9): 3.1 Pico Logo today, 3.2 Apple Logo II (the model dialect), 3.3 Atari Logo (1983, POKEY chip), 3.4 TI Logo II (1983, TMS9919/SN76489 — 3 tones + noise), 3.5 LogoWriter (LCSI, 1986), 3.6 Terrapin Logo (modern), 3.7 Adjacent non-Logo surfaces, 3.8 What this design takes (+1 more)

### Community 151 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

### Community 152 - "CLAUDE.md"
Cohesion: 0.25
Nodes (7): Build & Test, Code Structure, Constraints, graphify, How to work, Project, Unit Testing

### Community 153 - "ip_addr_t"
Cohesion: 0.25
Nodes (8): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 154 - "improvements-roadmap.md"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

### Community 155 - "3. Survey: multi-turtle and sprite Logos, 1981→now"
Cohesion: 0.25
Nodes (8): 3. Survey: multi-turtle and sprite Logos, 1981→now, Atari Logo (LCSI, 1983) — *events as first-class citizens*, BBC Logos (Acornsoft, Logotron; 1983–85), LogoWriter / MicroWorlds (LCSI, 1985 / 1993), StarLogo / NetLogo (MIT/Northwestern, 1994→) and Scratch (MIT, 2007→), Synthesis — what Pico Logo should steal, TI Logo / TI Logo II (TI-99/4A, 1981–82) — *sprites as autonomous objects*, TRS-80 Color Logo (Radio Shack, 1982) — *turtles as processes*

### Community 156 - "as_httpd_conn"
Cohesion: 0.32
Nodes (8): MockHttpdConn, as_httpd_conn(), httpd_conn_read(), httpd_conn_write(), mock_network_tcp_can_read(), mock_network_tcp_close(), mock_network_tcp_read(), mock_network_tcp_write()

### Community 157 - "Appendix A: Useful Tools"
Cohesion: 0.25
Nodes (8): Appendix A: Useful Tools, arcr and arcl, circler and circlel, divisor?, Graphics Tools, Math Tools, Program Logic or Debugging Tools, sort

### Community 158 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 159 - "Managing Various Files"
Cohesion: 0.33
Nodes (6): date, setdate, settime, ticks, time, Time Management

### Community 160 - "HTTP Operations"
Cohesion: 0.25
Nodes (8): http.delete, http.get, http.header, HTTP Operations, http.patch, http.post, http.put, http.status

### Community 161 - "run_editor_and_process"
Cohesion: 0.47
Nodes (5): LogoIO, logo_random_next(), logo_random_reset(), logo_random_seed(), pcg32_next()

### Community 163 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 176 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 177 - "lexer_set_preserve_comments"
Cohesion: 0.67
Nodes (3): lexer_set_preserve_comments(), test_semicolon_comment_preserved_when_enabled(), test_semicolon_comment_preserved_with_spaces()

### Community 178 - "mock_device_set_raster"
Cohesion: 0.67
Nodes (3): LogoTurtleRaster, mock_device_set_raster(), mock_turtle_get_raster()

### Community 197 - "prim_not"
Cohesion: 0.06
Nodes (58): Evaluator, Result, Value, prim_catch(), prim_throw(), prim_toplevel(), Evaluator, Result (+50 more)

### Community 198 - "on_sd_card_detect"
Cohesion: 0.50
Nodes (4): repeating_timer_t, on_sd_card_detect(), logo_picocalc_mount_available(), sd_card_present()

### Community 199 - "mem_set_cdr"
Cohesion: 0.07
Nodes (41): mem_cons(), mem_set_cdr(), Node, proc_define(), value_extract_rgb(), value_extract_xy(), Node, exhaust_node_pool() (+33 more)

## Knowledge Gaps
- **621 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+616 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **9 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `eval_string`, `test_value.c`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `result_none`, `test_frame.c`, `proc_define_from_text`, `error_format`, `format_buffer_init`, `lexer_init`, `test_primitives_editor.c`, `primitives_bitwise.c`, `io.c`, `mock_device_get_state`, `test_primitives_json.c`, `test_primitives_control_flow.c`, `test_scaffold.h`, `test_primitives_files_load_save.c`, `test_primitives_files.c`, `test_primitives_hardware.c`, `test_time.c`, `test_scaffold_setUp`, `set_mock_input`, `test_mock_device.c`, `test_primitives_files_directory.c`, `demons_poll`, `mem_set_cdr`, `main`, `primitives_files.c`, `test_scaffold.c`, `test_galaxian.c`, `test_primitives_properties.c`, `eval_primary`?**
  _High betweenness centrality (0.140) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `mem_is_nil`, `reset_output`, `mem_word_ptr`, `value_to_string`, `test_frame.c`, `proc_define_from_text`, `error_format`, `format_buffer_init`, `lexer_init`, `primitives_bitwise.c`, `test_httpd.c`, `test_primitives_http.c`, `mock_device_get_state`, `test_primitives_json.c`, `test_primitives_control_flow.c`, `test_scaffold.h`, `test_primitives_files_load_save.c`, `test_primitives_files.c`, `test_primitives_wifi.c`, `test_primitives_hardware.c`, `test_time.c`, `test_mock_device.c`, `test_primitives_network.c`, `test_primitives_files_directory.c`, `demons_poll`, `prim_not`, `mem_set_cdr`, `main`, `primitives_files.c`, `primitives_get_io`, `test_scaffold.c`, `test_galaxian.c`, `test_primitives_properties.c`, `eval_primary`?**
  _High betweenness centrality (0.109) - this node is a cross-community bridge._
- **Why does `result_none()` connect `result_none` to `run_string`, `prim_pause`, `mem_is_nil`, `test_value.c`, `value_to_string`, `primitives_variables.c`, `io.c`, `memory.c`, `set_mock_input`, `primitives.h`, `httpd.c`, `primitives_workspace.c`, `step_proc_call`, `demons_poll`, `op_stack_push`, `value_number`, `primitives_files.c`, `prim_savel`, `primitives_get_io`, `eval.c`, `primitives_httpd.c`, `repl_evaluate_line`, `eval_push_if`, `primitives_control_flow.c`, `primitives_outside_world.c`, `eval_primary`, `procedures.c`?**
  _High betweenness centrality (0.057) - this node is a cross-community bridge._
- **Are the 892 inferred relationships involving `run_string()` (e.g. with `test_action_does_not_reenter_poll()` and `test_cleardemons_disarms_all()`) actually correct?**
  _`run_string()` has 892 INFERRED edges - model-reasoned connections that need verification._
- **Are the 865 inferred relationships involving `eval_string()` (e.g. with `test_deep_recursion_100_levels()` and `test_deep_recursion_addupto()`) actually correct?**
  _`eval_string()` has 865 INFERRED edges - model-reasoned connections that need verification._
- **Are the 416 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 416 INFERRED edges - model-reasoned connections that need verification._
- **Are the 234 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `skip_primary()`) actually correct?**
  _`mem_is_nil()` has 234 INFERRED edges - model-reasoned connections that need verification._