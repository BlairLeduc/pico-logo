# Graph Report - pico-logo  (2026-07-18)

## Corpus Check
- 276 files · ~428,732 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 6560 nodes · 20972 edges · 195 communities (185 shown, 10 thin omitted)
- Extraction: 55% EXTRACTED · 45% INFERRED · 0% AMBIGUOUS · INFERRED: 9488 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `c92e0b02`
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
- output_has
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
- primitives_properties.c
- logo_console_init
- logo_lfs_backup
- ms_to_datetime
- HTTP server (design)
- Text and Screen Commands
- The Outside World
- prim_recycle
- primitives_bitwise.c
- What to flag (in priority order)
- LogoStream
- primitives_variables.c
- Memory Reclamation: Design Notes (deferred)
- 3. Prior art
- List Processing
- CLAUDE.md
- ip_addr_t
- 3. Survey: multi-turtle and sprite Logos, 1981→now
- as_httpd_conn
- Appendix A: Useful Tools
- Modifying Procedures Under Program Control
- Managing Various Files
- HTTP Operations
- Bitwise Operations
- Variables
- parse_bracket_contents
- Pico Logo
- JSON
- drain_tokens
- repl_count_bracket_balance
- repl_line_starts_with_to
- gen_ca_certs.py
- logo_console_init
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
- Managing Various Files

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 893 edges
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
- `setUp()` --calls--> `logo_mem_init()`  [INFERRED]
  tests/test_token_source.c → core/memory.c
- `setUp()` --calls--> `logo_mem_init()`  [INFERRED]
  tests/test_value.c → core/memory.c
- `test_nil_is_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_word()` --calls--> `mem_is_word()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_list_empty()` --calls--> `value_list()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Communities (195 total, 10 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (220): MockLine, MockTurtleState, mock_device_clear_graphics(), mock_device_get_line(), mock_device_get_output(), mock_device_get_turtle(), mock_device_has_line_from_to(), mock_device_line_count() (+212 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "eval_string"
Cohesion: 0.02
Nodes (181): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+173 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (141): format_number(), json_format_number(), Result, Value, result_error_in(), result_get_error_code(), result_get_goto_label(), result_get_pause_proc() (+133 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (105): mem_car(), mem_cdr(), mem_is_nil(), Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Result (+97 more)

### Community 5 - "reset_output"
Cohesion: 0.04
Nodes (110): primitive_find(), proc_is_stepped(), proc_is_traced(), setUp(), tearDown(), test_step_and_trace_independent(), test_step_multiline_procedure(), test_step_nonexistent_gives_error() (+102 more)

### Community 6 - "mem_word_ptr"
Cohesion: 0.02
Nodes (161): mem_word_ptr(), mock_device_set_time(), test_readchar_from_file(), test_setread_to_file(), test_setreadpos(), test_setwrite_to_file(), test_write_and_read_file(), test_make_small_number_in_object_is_valid_json() (+153 more)

### Community 7 - "result_none"
Cohesion: 0.08
Nodes (115): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator, Result, Value (+107 more)

### Community 8 - "value_to_string"
Cohesion: 0.15
Nodes (81): number_to_word(), mem_list_append(), mem_word_len(), prim_form(), prim_wait(), prim_battery_level(), Evaluator, Result (+73 more)

### Community 9 - "mem_atom"
Cohesion: 0.05
Nodes (98): mem_atom(), mem_cons(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_is_list(), mem_total_atoms(), mem_word_eq() (+90 more)

### Community 10 - "test_frame.c"
Cohesion: 0.06
Nodes (106): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+98 more)

### Community 11 - "proc_define_from_text"
Cohesion: 0.03
Nodes (90): proc_define_from_text(), test_deep_nested_proc_in_repeat(), test_many_iterations_proc_in_repeat_within_procedure(), test_proc_call_followed_by_commands_in_repeat(), test_proc_call_in_if_within_procedure(), test_proc_call_in_repeat_within_procedure(), test_proc_expr_in_run_within_procedure(), test_co_at_toplevel() (+82 more)

### Community 12 - "error_format"
Cohesion: 0.03
Nodes (85): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+77 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.08
Nodes (41): LogoPen, LogoRotationStyle, LogoTurtleRaster, ScreenSprite, heading_faces_left(), raster_line(), refresh_shape_wearers(), text_clear() (+33 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.07
Nodes (83): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_buffer_init(), format_buffer_output(), format_buffer_pos() (+75 more)

### Community 15 - "lexer_init"
Cohesion: 0.07
Nodes (83): lexer_init(), assert_token(), test_alphanumeric_word(), test_binary_minus_after_colon_no_space(), test_binary_minus_after_right_bracket_no_space(), test_binary_minus_after_word_no_space(), test_binary_minus_spacing(), test_complex_expression() (+75 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_primitives_editor.c"
Cohesion: 0.07
Nodes (75): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_set_editor_content(), mock_device_set_editor_result(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, MockFile (+67 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.07
Nodes (74): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+66 more)

### Community 19 - "test_variables.c"
Cohesion: 0.05
Nodes (96): frame_stack_depth(), frame_stack_is_empty(), FrameStack, proc_get_frame_stack(), Value, find_global(), var_bury(), var_bury_all() (+88 more)

### Community 20 - "test_token_source.c"
Cohesion: 0.09
Nodes (73): Lexer, Node, Token, TokenType, classify_word(), is_comment_node(), is_delimiter_token(), is_number_word() (+65 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (72): httpd_listening(), httpd_request_pending(), httpd_reset(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex() (+64 more)

### Community 23 - "test_io.c"
Cohesion: 0.04
Nodes (47): logo_io_cleanup(), logo_io_parse_network_address(), logo_io_set_prefix(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn(), mock_dir_callback() (+39 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "io.c"
Cohesion: 0.09
Nodes (62): eval_instruction(), LogoDirCallback, LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span(), logo_io_check_freeze_request() (+54 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (64): mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), Result, get_body_of_size() (+56 more)

### Community 27 - "fat32.c"
Cohesion: 0.11
Nodes (61): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_delete(), fat32_dir_create() (+53 more)

### Community 28 - "mock_device_get_state"
Cohesion: 0.02
Nodes (137): MockCommand, LogoConsole, MockDeviceState, mock_device_clear_commands(), mock_device_command_count(), mock_device_dot_count(), mock_device_get_command(), mock_device_get_console() (+129 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (13): mock_device_add_wifi_scan_result(), mock_device_clear_wifi_scan_results(), mock_device_get_tcp_request_len(), mock_device_set_input(), mock_device_set_snap_result(), mock_device_set_tcp_close_after(), mock_device_set_tcp_read_chunk(), mock_device_set_tcp_write_chunk() (+5 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (61): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+53 more)

### Community 31 - "test_primitives_control_flow.c"
Cohesion: 0.03
Nodes (57): test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic(), test_do_until_invalid_predicate(), test_do_until_runs_once(), test_do_until_with_stop() (+49 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (61): tearDown(), tearDown(), tearDown(), tearDown(), setUp(), tearDown(), tearDown(), tearDown() (+53 more)

### Community 33 - "test_primitives_conditionals.c"
Cohesion: 0.12
Nodes (32): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+24 more)

### Community 34 - "test_primitives_files_load_save.c"
Cohesion: 0.08
Nodes (42): mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle(), tearDown_with_turtle() (+34 more)

### Community 35 - "stdlib.h"
Cohesion: 0.08
Nodes (28): keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_set_background_poll(), keyboard_set_idle_callback(), keyboard_set_key_available_callback(), lcd_get_palette_value() (+20 more)

### Community 36 - "memory.c"
Cohesion: 0.07
Nodes (53): BlobDesc, demons_gc_mark_all(), httpd_buf_init(), alloc_cell(), atom_entry_next(), atom_entry_set_next(), atom_hash(), blob_alloc() (+45 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (43): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+35 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.07
Nodes (43): mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_ssid(), test_hostname_default_is_picologo(), test_sethostname_accepts_digits_and_interior_hyphens() (+35 more)

### Community 41 - "test_primitives_hardware.c"
Cohesion: 0.06
Nodes (45): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+37 more)

### Community 42 - "mem_atom_cstr"
Cohesion: 0.18
Nodes (27): mem_atom_cstr(), Evaluator, Result, Value, prim_catch(), prim_error(), prim_throw(), prim_toplevel() (+19 more)

### Community 43 - "test_time.c"
Cohesion: 0.07
Nodes (31): mock_device_set_time_enabled(), test_date_error_when_not_available(), test_setdate_error_when_not_available(), test_setdate_preserves_time(), test_setdate_rejects_empty_list(), test_setdate_rejects_invalid_day_high(), test_setdate_rejects_invalid_day_low(), test_setdate_rejects_invalid_month_high() (+23 more)

### Community 44 - "test_scaffold_setUp"
Cohesion: 0.06
Nodes (53): logo_mem_init(), LogoIO, primitives_control_reset_test_state(), primitives_set_io(), properties_init(), variables_init(), LogoHardware, LogoHardwareOps (+45 more)

### Community 45 - "set_mock_input"
Cohesion: 0.14
Nodes (44): LogoIO, repl_cleanup(), repl_extract_proc_name(), repl_init(), repl_line_starts_with_to(), repl_run(), ReplFlags, test_repl_error_clears_sync_refresh() (+36 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "screen.c"
Cohesion: 0.09
Nodes (24): text_get_background(), text_get_foreground(), text_set_background(), turtle_canvas_point(), turtle_dot_at(), clip_and_round(), compose_row(), screen_gfx_blit_dirty() (+16 more)

### Community 48 - "repository"
Cohesion: 0.05
Nodes (42): name, name, 1, 2, match, name, match, name (+34 more)

### Community 49 - "test_primitives_outside_world.c"
Cohesion: 0.06
Nodes (33): test_apply_unknown_procedure(), test_apply_with_lambda(), test_apply_with_multi_param_lambda(), test_apply_with_primitive_name(), test_apply_with_procedure_text(), test_apply_with_user_procedure(), test_apply_with_word_primitive(), test_crossmap_callback_throw_freed() (+25 more)

### Community 50 - "primitives.h"
Cohesion: 0.10
Nodes (24): Value, demons_clear(), demons_freeze(), demons_reset(), demons_thaw(), value_is_true(), Evaluator, Result (+16 more)

### Community 51 - "httpd.c"
Cohesion: 0.12
Nodes (35): demons_frozen(), LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find() (+27 more)

### Community 52 - "stream.c"
Cohesion: 0.08
Nodes (42): logo_io_check_write_error(), logo_io_key_available(), logo_io_read_chars(), LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps (+34 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (41): picocalc_editor_get_ops(), repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_enabled() (+33 more)

### Community 54 - "Pico_Logo_Reference.md"
Cohesion: 0.04
Nodes (44): and, Appendix B: Parsing, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Brackets and Parentheses (+36 more)

### Community 55 - "primitives_workspace.c"
Cohesion: 0.10
Nodes (58): mem_is_word(), Evaluator, Result, Value, prim_step(), prim_trace(), prim_unstep(), prim_untrace() (+50 more)

### Community 56 - "test_mock_device.c"
Cohesion: 0.10
Nodes (31): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), test_env_default(), test_play_appends(), test_play_bad_notation_errors() (+23 more)

### Community 57 - "step_proc_call"
Cohesion: 0.13
Nodes (48): eval_at_end(), Evaluator, Node, Result, eval_expr_bp(), eval_expression(), eval_primary(), is_number_string() (+40 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.06
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (35): repeating_timer_t, fat32_close(), fat32_create(), fat32_is_mounted(), fat32_mount(), fat32_read(), fat32_unmount(), is_valid_fat32_boot_sector() (+27 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (34): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+26 more)

### Community 61 - "test_primitives_files_directory.c"
Cohesion: 0.08
Nodes (29): mock_fs_create_dir(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname(), test_catalog_with_invalid_input_error(), test_catalog_with_pathname_runs_without_error(), test_catalog_with_relative_pathname(), test_createdir(), test_directories_returns_list() (+21 more)

### Community 62 - "primitives_init"
Cohesion: 0.11
Nodes (30): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_init(), primitives_editor_init(), primitives_exceptions_init(), primitives_files_directory_init() (+22 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.18
Nodes (34): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+26 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (35): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+27 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.10
Nodes (33): LogoDirCallback, LogoStream, MockFile, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos() (+25 more)

### Community 66 - "demons_poll"
Cohesion: 0.12
Nodes (29): demons_poll(), mock_device_clear_output(), setUp(), tearDown(), test_action_does_not_reenter_poll(), test_cleardemons_disarms_all(), test_cleardemons_leaves_motion_and_freeze(), test_clearscreen_leaves_demons_armed() (+21 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.14
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "op_stack_push"
Cohesion: 0.13
Nodes (32): EvalOp, Value, mark_value(), op_stack_alloc_prim_args(), op_stack_depth(), op_stack_gc_mark(), op_stack_get_prim_args(), op_stack_init() (+24 more)

### Community 70 - "value_number"
Cohesion: 0.19
Nodes (29): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+21 more)

### Community 71 - "main"
Cohesion: 0.18
Nodes (15): m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest(), writable_m1(), bd_erase() (+7 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (19): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+11 more)

### Community 74 - "primitives_files.c"
Cohesion: 0.13
Nodes (22): save_output(), logo_io_flush(), logo_io_read_char(), logo_io_reader_is_keyboard(), logo_io_set_reader(), logo_io_set_writer(), logo_io_write(), logo_io_write_syntax_highlighted() (+14 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (27): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStream, file_context_stale(), logo_picocalc_dir_create(), logo_picocalc_dir_exists() (+19 more)

### Community 76 - "prim_savel"
Cohesion: 0.34
Nodes (17): httpd_savebody(), prim_editfile(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile() (+9 more)

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
Cohesion: 0.14
Nodes (53): demons_print(), prim_when(), Evaluator, Result, Value, Evaluator, LogoEntryType, Result (+45 more)

### Community 81 - "host_storage.c"
Cohesion: 0.12
Nodes (17): LogoDirCallback, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos(), host_file_get_write_pos() (+9 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "eval.c"
Cohesion: 0.23
Nodes (26): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, UserProcedure, Value (+18 more)

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
Cohesion: 0.16
Nodes (20): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+12 more)

### Community 88 - "primitives_httpd.c"
Cohesion: 0.25
Nodes (24): mem_word(), Evaluator, Result, Value, el_append(), el_append_cstr(), el_append_word(), no_request() (+16 more)

### Community 89 - "primitives_http.c"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

### Community 90 - "repl_evaluate_line"
Cohesion: 0.13
Nodes (21): proc_restore_execution_state(), proc_save_execution_state(), Result, repl_count_bracket_balance(), repl_evaluate_line(), repl_line_is_end(), repl_next_bracket_depth(), repl_restore_refresh() (+13 more)

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
Cohesion: 0.24
Nodes (16): mem_set_cdr(), Evaluator, Result, Value, prim_copydef(), prim_define(), prim_definedp(), prim_primitivep() (+8 more)

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
Cohesion: 0.16
Nodes (20): ensure_wifi_initialized(), mdns_start(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect() (+12 more)

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
Cohesion: 0.07
Nodes (24): proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), lfs_t, LogoStorage, logo_lfs_storage_init(), main(), lfs_t (+16 more)

### Community 112 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 113 - "primitives_control_flow.c"
Cohesion: 0.37
Nodes (17): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_for(), prim_forever() (+9 more)

### Community 114 - "primitives_outside_world.c"
Cohesion: 0.30
Nodes (18): Evaluator, Node, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar() (+10 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (18): backup, catalog, copyfile, createdir, dir? (dirp), directories, editfile, erasedir (+10 more)

### Community 117 - "test_primitives_properties.c"
Cohesion: 0.09
Nodes (21): tearDown(), test_erprops_clears_all_properties(), test_gprop_requires_word_for_name(), test_gprop_requires_word_for_property(), test_multiple_properties_on_same_name(), test_plist_requires_word(), test_pprop_and_gprop_number_value(), test_pprop_and_gprop_word_value() (+13 more)

### Community 118 - "logo_io_init"
Cohesion: 0.43
Nodes (8): Evaluator, Result, Value, prim_bootsel(), prim_goodbye(), prim_toot(), toot_gate_freq(), logo_io_close_all()

### Community 119 - "clib.c"
Cohesion: 0.22
Nodes (14): logo_host_rename(), fat32_error_t, _close(), fat32_error_to_errno(), _fstat(), init(), _lseek(), _open() (+6 more)

### Community 120 - "P8 — Sound: a stereo PSG synthesizer (design)"
Cohesion: 0.12
Nodes (16): 10. Rejected alternatives, 11. Resolved questions (user, 2026-07-10), 1. What limits sound today, 2. The output hardware, 4. The instrument, 5.1 `toot` — unchanged, 5.2 Immediate layer — `sound`, 5.3 Timbre layer — `setenv`, `setwave` (+8 more)

### Community 121 - "Input and Output to Files, Network Connections and Devices"
Cohesion: 0.12
Nodes (16): allopen, close, closeall, filelen, Input and Output to Files, Network Connections and Devices, open, reader, readpos (+8 more)

### Community 122 - "output_has"
Cohesion: 0.22
Nodes (18): mock_device_paint_canvas(), mock_device_set_canvas_point(), output_has(), stage_raster(), test_colourunder_reads_turtle_position(), test_colourunder_works_when_hidden(), test_distance_answers_for_first_active(), test_distance_is_euclidean() (+10 more)

### Community 123 - "record_command"
Cohesion: 0.12
Nodes (16): mock_screen_fullscreen(), mock_screen_refresh_now(), mock_screen_set_refresh_auto(), mock_screen_splitscreen(), mock_screen_textscreen(), mock_text_clear(), mock_turtle_clear(), mock_turtle_draw() (+8 more)

### Community 124 - "eval_primary"
Cohesion: 0.08
Nodes (49): Node, demons_set(), apply_binary_op(), TokenType, Value, Evaluator, Result, Value (+41 more)

### Community 125 - "frame_stack_is_empty"
Cohesion: 0.09
Nodes (30): lcd_enable_cursor(), lcd_restore_palette(), lcd_set_background(), lcd_set_palette_rgb(), LogoStream, error_output_flush(), error_output_write(), input_read_char() (+22 more)

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
Cohesion: 0.27
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

### Community 137 - "primitives_properties.c"
Cohesion: 0.39
Nodes (11): Evaluator, Result, Value, prim_erprops(), prim_gprop(), prim_plist(), prim_pprop(), prim_pps() (+3 more)

### Community 138 - "logo_console_init"
Cohesion: 0.38
Nodes (7): Result, demons_maybe_poll(), demons_running(), httpd_maybe_poll(), logo_io_has_ticks_ms(), logo_io_ticks_ms(), console_idle_poll()

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.40
Nodes (10): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+2 more)

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

### Community 145 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 146 - "What to flag (in priority order)"
Cohesion: 0.20
Nodes (9): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on (+1 more)

### Community 147 - "LogoStream"
Cohesion: 0.20
Nodes (10): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write() (+2 more)

### Community 148 - "primitives_variables.c"
Cohesion: 0.47
Nodes (9): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing() (+1 more)

### Community 149 - "Memory Reclamation: Design Notes (deferred)"
Cohesion: 0.22
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
Cohesion: 0.22
Nodes (9): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), picocalc_network_ntp(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t (+1 more)

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

### Community 162 - "Bitwise Operations"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

### Community 163 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 164 - "parse_bracket_contents"
Cohesion: 0.43
Nodes (7): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), primitives_procedures_init(), token_to_atom()

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 173 - "logo_console_init"
Cohesion: 0.39
Nodes (7): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init()

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

### Community 196 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

## Knowledge Gaps
- **618 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+613 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **10 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `eval_string`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `result_none`, `test_frame.c`, `proc_define_from_text`, `error_format`, `format_buffer_init`, `lexer_init`, `test_primitives_editor.c`, `test_variables.c`, `io.c`, `mock_device_get_state`, `test_primitives_json.c`, `test_primitives_control_flow.c`, `test_scaffold.h`, `test_primitives_files_load_save.c`, `test_primitives_files.c`, `test_primitives_hardware.c`, `test_time.c`, `set_mock_input`, `test_primitives_outside_world.c`, `test_mock_device.c`, `step_proc_call`, `test_primitives_files_directory.c`, `demons_poll`, `eval.c`, `primitives_httpd.c`, `run_editor_and_process`, `test_scaffold.c`, `test_galaxian.c`, `test_primitives_properties.c`, `output_has`, `eval_primary`?**
  _High betweenness centrality (0.138) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `run_string`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `value_to_string`, `proc_define_from_text`, `error_format`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `test_httpd.c`, `test_primitives_http.c`, `mock_device_get_state`, `test_primitives_json.c`, `test_primitives_control_flow.c`, `test_scaffold.h`, `test_primitives_files.c`, `test_primitives_wifi.c`, `test_primitives_hardware.c`, `mem_atom_cstr`, `test_time.c`, `test_primitives_outside_world.c`, `test_mock_device.c`, `step_proc_call`, `test_primitives_network.c`, `test_primitives_files_directory.c`, `demons_poll`, `eval.c`, `primitives_httpd.c`, `run_editor_and_process`, `test_scaffold.c`, `test_galaxian.c`, `test_primitives_properties.c`, `eval_primary`?**
  _High betweenness centrality (0.111) - this node is a cross-community bridge._
- **Why does `result_none()` connect `result_none` to `run_string`, `prim_pause`, `mem_is_nil`, `test_value.c`, `value_to_string`, `primitives_properties.c`, `logo_console_init`, `primitives_variables.c`, `io.c`, `memory.c`, `mem_atom_cstr`, `set_mock_input`, `primitives.h`, `httpd.c`, `primitives_workspace.c`, `step_proc_call`, `demons_poll`, `op_stack_push`, `value_number`, `prim_savel`, `primitives_get_io`, `eval.c`, `primitives_httpd.c`, `repl_evaluate_line`, `run_editor_and_process`, `eval_push_if`, `primitives_control_flow.c`, `primitives_outside_world.c`, `logo_io_init`, `eval_primary`, `procedures.c`?**
  _High betweenness centrality (0.056) - this node is a cross-community bridge._
- **Are the 891 inferred relationships involving `run_string()` (e.g. with `test_action_does_not_reenter_poll()` and `test_cleardemons_disarms_all()`) actually correct?**
  _`run_string()` has 891 INFERRED edges - model-reasoned connections that need verification._
- **Are the 865 inferred relationships involving `eval_string()` (e.g. with `test_deep_recursion_100_levels()` and `test_deep_recursion_addupto()`) actually correct?**
  _`eval_string()` has 865 INFERRED edges - model-reasoned connections that need verification._
- **Are the 416 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 416 INFERRED edges - model-reasoned connections that need verification._
- **Are the 234 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `skip_primary()`) actually correct?**
  _`mem_is_nil()` has 234 INFERRED edges - model-reasoned connections that need verification._