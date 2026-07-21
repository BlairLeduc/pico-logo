# Graph Report - pico-logo  (2026-07-20)

## Corpus Check
- 274 files · ~431,639 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 6596 nodes · 21071 edges · 191 communities (185 shown, 6 thin omitted)
- Extraction: 55% EXTRACTED · 45% INFERRED · 0% AMBIGUOUS · INFERRED: 9539 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `f9e75866`
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
- logo_io_set_writer
- primitives_properties.c
- logo_console_init
- logo_lfs_backup
- ms_to_datetime
- HTTP server (design)
- Text and Screen Commands
- The Outside World
- prim_recycle
- test_primitives_exceptions.c
- What to flag (in priority order)
- LogoStream
- primitives_variables.c
- Memory Reclamation: Design Notes (deferred)
- 3. Prior art
- List Processing
- CLAUDE.md
- logo_io_close_all
- improvements-roadmap.md
- 3. Survey: multi-turtle and sprite Logos, 1981→now
- as_httpd_conn
- Appendix A: Useful Tools
- Modifying Procedures Under Program Control
- Managing Various Files
- repl_extract_proc_name
- repl_line_starts_with_to
- mock_text_set_width
- Variables
- Managing Various Files
- mock_fs_reset
- Pico Logo
- JSON
- drain_tokens
- mock_storage_list_directory
- gen_ca_certs.py
- pandoc_slug
- lexer_set_preserve_comments
- dist.sh
- generate_help.sh
- run_e2e.sh
- VENDOR.md
- Appendix B: Parsing
- on_sd_card_detect

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 896 edges
2. `eval_string()` - 874 edges
3. `mem_word_ptr()` - 429 edges
4. `mem_is_nil()` - 238 edges
5. `mem_atom()` - 228 edges
6. `value_to_string()` - 194 edges
7. `result_error_arg()` - 193 edges
8. `result_none()` - 190 edges
9. `result_ok()` - 175 edges
10. `lexer_init()` - 172 edges

## Surprising Connections (you probably didn't know these)
- `test_nil_is_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_list()` --calls--> `mem_is_list()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_word()` --calls--> `mem_is_word()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_number_content()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c
- `test_value_number_negative()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Communities (191 total, 6 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (188): mock_device_clear_graphics(), mock_device_get_output(), mock_device_has_line_from_to(), mock_device_verify_palette(), setUp(), test_addressing_primitives_registered(), test_arc_angle_clamped_to_full_circle(), test_arc_does_not_move_turtle() (+180 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "eval_string"
Cohesion: 0.03
Nodes (138): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+130 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (193): format_number(), Evaluator, Result, Value, prim_date(), prim_setdate(), prim_settime(), prim_ticks() (+185 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (175): BlobDesc, format_list_contents(), list_has_newlines(), number_to_word(), blob_desc(), Node, index_to_node(), mem_atom_unescape() (+167 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (170): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_deep_nested_proc_in_repeat() (+162 more)

### Community 6 - "mem_word_ptr"
Cohesion: 0.03
Nodes (121): mem_word_ptr(), bind_long_blob_word(), exhaust_atom_table(), exhaust_node_pool(), test_ascii(), test_bar_list_literal_count(), test_bar_list_literal_is_one_word(), test_bar_parse_first_is_whole_word() (+113 more)

### Community 7 - "result_none"
Cohesion: 0.07
Nodes (121): Node, Value, demons_clear(), demons_freeze(), demons_frozen(), demons_reset(), demons_running(), demons_set() (+113 more)

### Community 8 - "value_to_string"
Cohesion: 0.06
Nodes (51): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or(), value_to_string() (+43 more)

### Community 9 - "mem_atom"
Cohesion: 0.07
Nodes (72): mem_atom(), mem_cons(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_set_car(), mem_total_atoms(), mem_total_nodes() (+64 more)

### Community 10 - "test_frame.c"
Cohesion: 0.13
Nodes (19): frame_pop(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_used_bytes(), test_pop_all_frames(), test_pop_empty_returns_none(), test_pop_frees_memory(), test_pop_returns_previous() (+11 more)

### Community 11 - "proc_define_from_text"
Cohesion: 0.04
Nodes (76): proc_define_from_text(), test_butlast_removes_last_line(), test_copydef_copies_primitive(), test_copydef_copies_procedure(), test_copydef_error_dest_is_primitive(), test_copydef_error_dest_not_word(), test_copydef_error_source_not_found(), test_copydef_error_source_not_word() (+68 more)

### Community 12 - "error_format"
Cohesion: 0.03
Nodes (85): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+77 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.06
Nodes (52): LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_write(), heading_faces_left(), input_read_char() (+44 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.09
Nodes (50): UserProcedure, format_buffer_init(), format_buffer_output(), format_procedure_definition(), format_procedure_title(), format_procedure_to_buffer(), format_variable(), format_variable_to_buffer() (+42 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_primitives_editor.c"
Cohesion: 0.10
Nodes (58): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_set_editor_content(), mock_device_set_editor_result(), mock_device_was_editor_called(), MockFile, mock_fs_create_file(), mock_fs_get_content() (+50 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.07
Nodes (88): frame_stack_is_empty(), prim_pons(), FrameStack, proc_get_frame_stack(), value_number(), Value, find_global(), var_bury() (+80 more)

### Community 20 - "test_token_source.c"
Cohesion: 0.09
Nodes (70): Lexer, Node, Token, TokenType, classify_word(), is_comment_node(), is_delimiter_token(), is_number_word() (+62 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (71): httpd_listening(), httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled() (+63 more)

### Community 23 - "test_io.c"
Cohesion: 0.04
Nodes (64): prop_output(), ws_output(), logo_io_parse_network_address(), logo_io_set_reader(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble(), logo_io_write() (+56 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "io.c"
Cohesion: 0.08
Nodes (67): demons_maybe_poll(), eval_instruction(), LogoIO, SoundEvent, queue_event_waiting(), prim_sync(), LogoIO, LogoStream (+59 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.05
Nodes (73): logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_get_tcp_request_len(), mock_device_set_resolve_result(), mock_device_set_tcp_close_after() (+65 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (52): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+44 more)

### Community 28 - "mock_device_get_state"
Cohesion: 0.03
Nodes (117): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), MockCommand (+109 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (44): MockDot, LogoStream, LogoTurtleRaster, SoundEvent, SoundStatus, mock_device_add_wifi_scan_result(), mock_device_get_dot(), mock_device_set_input() (+36 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (62): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+54 more)

### Community 31 - "test_primitives_control_flow.c"
Cohesion: 0.03
Nodes (59): test_false(), test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_operation_returns_value(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output() (+51 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (58): tearDown(), tearDown(), tearDown(), tearDown(), setUp(), tearDown(), tearDown(), tearDown() (+50 more)

### Community 33 - "test_primitives_conditionals.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "test_primitives_files_load_save.c"
Cohesion: 0.04
Nodes (74): mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), mock_fs_create_dir(), test_catalog_runs_without_error() (+66 more)

### Community 35 - "stdlib.h"
Cohesion: 0.09
Nodes (22): repeating_timer_t, keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_poll(), keyboard_set_background_poll(), keyboard_set_idle_callback() (+14 more)

### Community 36 - "memory.c"
Cohesion: 0.11
Nodes (28): demons_gc_mark_all(), op_stack_gc_mark(), alloc_cell(), atom_entry_next(), atom_entry_set_next(), atom_hash(), blob_free(), blob_reset() (+20 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (46): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+38 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (35): MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t, LogoDirCallback (+27 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.06
Nodes (46): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result(), mock_device_set_wifi_ssid() (+38 more)

### Community 41 - "test_primitives_hardware.c"
Cohesion: 0.06
Nodes (45): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+37 more)

### Community 42 - "mem_atom_cstr"
Cohesion: 0.07
Nodes (92): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+84 more)

### Community 43 - "test_time.c"
Cohesion: 0.07
Nodes (43): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+35 more)

### Community 44 - "test_scaffold_setUp"
Cohesion: 0.06
Nodes (50): Lexer, eval_init(), LogoIO, primitives_control_reset_test_state(), primitives_set_io(), procedures_init(), properties_init(), variables_init() (+42 more)

### Community 45 - "set_mock_input"
Cohesion: 0.20
Nodes (33): LogoIO, repl_cleanup(), repl_init(), repl_run(), ReplFlags, test_repl_error_clears_sync_refresh(), test_repl_error_restores_auto_refresh(), test_repl_init_basic() (+25 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "screen.c"
Cohesion: 0.09
Nodes (24): text_get_background(), text_get_foreground(), text_set_background(), turtle_canvas_point(), turtle_dot_at(), clip_and_round(), compose_row(), screen_gfx_blit_dirty() (+16 more)

### Community 48 - "repository"
Cohesion: 0.04
Nodes (45): name, name, match, name, 1, 2, match, name (+37 more)

### Community 49 - "test_primitives_outside_world.c"
Cohesion: 0.06
Nodes (33): test_apply_unknown_procedure(), test_apply_with_lambda(), test_apply_with_multi_param_lambda(), test_apply_with_primitive_name(), test_apply_with_procedure_text(), test_apply_with_user_procedure(), test_apply_with_word_primitive(), test_crossmap_callback_throw_freed() (+25 more)

### Community 50 - "primitives.h"
Cohesion: 0.09
Nodes (39): Evaluator, Result, Value, prim_step(), prim_trace(), prim_unstep(), prim_untrace(), Evaluator (+31 more)

### Community 51 - "httpd.c"
Cohesion: 0.11
Nodes (38): LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find(), httpd_body() (+30 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (39): logo_io_copy_file(), logo_io_write_error_line(), LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read() (+31 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (35): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+27 more)

### Community 54 - "Pico_Logo_Reference.md"
Cohesion: 0.05
Nodes (39): and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Break, Contents (+31 more)

### Community 55 - "primitives_workspace.c"
Cohesion: 0.17
Nodes (37): Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add(), help_list_flush() (+29 more)

### Community 56 - "test_mock_device.c"
Cohesion: 0.09
Nodes (34): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), test_env_default(), test_play_appends(), test_play_bad_notation_errors() (+26 more)

### Community 57 - "step_proc_call"
Cohesion: 0.21
Nodes (29): op_stack_pop(), EvalOp, EvalOpKind, Evaluator, Node, Result, UserProcedure, eval_trace_entry() (+21 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (15): cyw43_ev_scan_result_t, mbedtls_ms_time(), mdns_stop(), ntp_dns_callback(), ntp_send_request(), picocalc_sleep(), picocalc_wifi_disconnect(), tcp_client_connected_cb() (+7 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (41): fat32_close(), fat32_create(), fat32_delete(), fat32_is_mounted(), fat32_mount(), fat32_open(), fat32_read(), fat32_set_current_dir() (+33 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.12
Nodes (31): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), test_network_ping_requires_one_argument(), test_network_ping_requires_word_argument() (+23 more)

### Community 61 - "test_primitives_files_directory.c"
Cohesion: 0.13
Nodes (31): mem_atom_cstr(), Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Result, Value, prim_catch() (+23 more)

### Community 62 - "primitives_init"
Cohesion: 0.11
Nodes (33): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+25 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (35): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+27 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.12
Nodes (28): LogoDirCallback, LogoStream, MockFile, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos() (+20 more)

### Community 66 - "demons_poll"
Cohesion: 0.11
Nodes (33): Result, demons_poll(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle(), setUp(), tearDown(), test_action_does_not_reenter_poll() (+25 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.14
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "op_stack_push"
Cohesion: 0.14
Nodes (31): EvalOp, Value, mark_value(), op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init(), op_stack_is_empty() (+23 more)

### Community 70 - "value_number"
Cohesion: 0.17
Nodes (40): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+32 more)

### Community 71 - "main"
Cohesion: 0.12
Nodes (19): m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest(), writable_m1(), bd_erase() (+11 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (20): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+12 more)

### Community 74 - "primitives_files.c"
Cohesion: 0.26
Nodes (24): EvalOpKind, Evaluator, FrameStack, Node, Result, UserProcedure, Value, eval_get_frames() (+16 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (28): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_dir_create() (+20 more)

### Community 76 - "prim_savel"
Cohesion: 0.22
Nodes (7): LogoHardware, LogoHardwareOps, logo_hardware_init(), LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), test_play_no_sound_engine_is_noop()

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "test_lfs_backup.c"
Cohesion: 0.12
Nodes (33): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+25 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "primitives_get_io"
Cohesion: 0.26
Nodes (29): demons_print(), Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble() (+21 more)

### Community 81 - "host_storage.c"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "eval.c"
Cohesion: 0.16
Nodes (23): mem_set_cdr(), append_to_list(), Evaluator, Lexer, Node, Result, Token, Value (+15 more)

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
Nodes (13): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+5 more)

### Community 88 - "primitives_httpd.c"
Cohesion: 0.35
Nodes (18): httpd_savebody(), prim_editfile(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile() (+10 more)

### Community 89 - "primitives_http.c"
Cohesion: 0.05
Nodes (41): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+33 more)

### Community 90 - "repl_evaluate_line"
Cohesion: 0.10
Nodes (21): lfs_t, LogoStorage, logo_lfs_storage_init(), LogoStorage, LogoStorageOps, logo_storage_init(), LogoStorage, LogoStorageOps (+13 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.11
Nodes (28): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+20 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.08
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (design)"
Cohesion: 0.09
Nodes (23): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+15 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "run_editor_and_process"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

### Community 96 - "Space Invaders in Pico Logo (design & implementation)"
Cohesion: 0.09
Nodes (22): 10. Why this is a good P5 acceptance test, 11. Deliverable, 1. The board, 2. Object representation — the central decision, 3. The alien formation on the canvas, 4. Collision routing — demons vs. the game loop, 5. Global events as demons, 6. Input (+14 more)

### Community 97 - "package.json"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 98 - "test_mklfsimg.c"
Cohesion: 0.08
Nodes (33): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+25 more)

### Community 99 - "eval_push_if"
Cohesion: 0.48
Nodes (11): eval_push_if(), Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse() (+3 more)

### Community 100 - "host_hardware.c"
Cohesion: 0.09
Nodes (5): LogoHardware, host_network_tcp_connect(), init_winsock(), logo_host_hardware_create(), logo_host_hardware_destroy()

### Community 101 - "sdcard.c"
Cohesion: 0.23
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
Cohesion: 0.11
Nodes (26): ensure_wifi_initialized(), mdns_start(), ntp_recv_callback(), picocalc_dns_callback(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_network_tcp_connect() (+18 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.13
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "The pick of five: plans"
Cohesion: 0.10
Nodes (20): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Improvements Roadmap, Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P1 — Host REPL stdin + CI (+12 more)

### Community 109 - "test_galaxian.c"
Cohesion: 0.18
Nodes (21): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom() (+13 more)

### Community 110 - "mklfsimg_lib.c"
Cohesion: 0.15
Nodes (29): Node, Value, format_body_element(), format_body_element_multiline(), format_buffer_pos(), format_list_with_newlines(), format_property(), format_property_list() (+21 more)

### Community 111 - "logo_storage_init"
Cohesion: 0.07
Nodes (25): test_co_at_toplevel(), test_freeze_request_break_stops_execution(), test_freeze_request_waits_for_key(), test_go_label_not_found_in_procedure(), test_go_no_label(), test_go_with_label(), test_label_basic(), test_pause_at_toplevel_error() (+17 more)

### Community 112 - "southbridge.c"
Cohesion: 0.32
Nodes (14): picocalc_get_battery_level(), picocalc_power_off(), sb_is_power_off_supported(), sb_read(), sb_read_battery(), sb_read_keyboard(), sb_read_keyboard_backlight(), sb_read_keyboard_state() (+6 more)

### Community 113 - "primitives_control_flow.c"
Cohesion: 0.37
Nodes (17): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_for(), prim_forever() (+9 more)

### Community 114 - "primitives_outside_world.c"
Cohesion: 0.32
Nodes (18): Evaluator, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar(), prim_readchars() (+10 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (18): backup, catalog, copyfile, createdir, dir? (dirp), directories, editfile, erasedir (+10 more)

### Community 117 - "test_primitives_properties.c"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

### Community 118 - "logo_io_init"
Cohesion: 0.20
Nodes (26): Evaluator, LogoEntryType, Result, Value, catalog_callback(), file_list_callback(), prim_backup(), prim_catalog() (+18 more)

### Community 119 - "clib.c"
Cohesion: 0.22
Nodes (14): logo_host_rename(), fat32_error_t, _close(), fat32_error_to_errno(), _fstat(), init(), _lseek(), _open() (+6 more)

### Community 120 - "P8 — Sound: a stereo PSG synthesizer (design)"
Cohesion: 0.10
Nodes (20): 10. Rejected alternatives, 11. Resolved questions (user, 2026-07-10), 12.1 DMA read ring-wrap (engine, 2026-07-18), 12.2 LCD driver no longer masks interrupts (2026-07-19), 12.3 Audio IRQ priority above default (2026-07-19), 12. Hardware bring-up findings (2026-07-18/19), 1. What limits sound today, 2. The output hardware (+12 more)

### Community 121 - "Input and Output to Files, Network Connections and Devices"
Cohesion: 0.12
Nodes (16): allopen, close, closeall, filelen, Input and Output to Files, Network Connections and Devices, open, reader, readpos (+8 more)

### Community 122 - "output_has"
Cohesion: 0.33
Nodes (6): date, setdate, settime, ticks, time, Time Management

### Community 123 - "record_command"
Cohesion: 0.09
Nodes (32): picocalc_editor_get_ops(), lcd_get_palette_value(), lcd_restore_palette(), lcd_set_background(), lcd_set_palette_rgb(), lcd_set_palette_value(), LogoConsole, error_output_flush() (+24 more)

### Community 124 - "eval_primary"
Cohesion: 0.25
Nodes (24): mem_word(), Evaluator, Result, Value, el_append(), el_append_cstr(), el_append_word(), no_request() (+16 more)

### Community 125 - "frame_stack_is_empty"
Cohesion: 0.25
Nodes (8): 5.1 Duplicated infix-operator evaluation in `eval_expr.c`, 5.2 Four nearly identical loop steppers, 5.3 Repeated list-builder loop, 5.4 Repeated number→word coercion in element primitives, 5.5 Dead code, 5.6 Documentation drift in `memory.h`, 5.7 Small items, 5. Simplicity and maintainability

### Community 126 - "procedures.c"
Cohesion: 0.26
Nodes (23): Evaluator, LogoHardwareOps, Node, Result, Value, is_noise_voice(), parse_voice_set(), play_one_voice() (+15 more)

### Community 127 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 128 - "audio.c"
Cohesion: 0.20
Nodes (13): SoundEvent, SoundStatus, is_noise_voice(), __not_in_flash_func(), queue_empty(), queue_free(), sound_gate(), sound_init() (+5 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "prim_pause"
Cohesion: 0.33
Nodes (6): JSON, json.array, json.count, json.get, json.make, json.object

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
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 136 - "logo_io_set_writer"
Cohesion: 0.12
Nodes (23): proc_clear_tail_call(), proc_reset_execution_state(), proc_restore_execution_state(), proc_save_execution_state(), Result, name_distance(), repl_count_bracket_balance(), repl_evaluate_line() (+15 more)

### Community 137 - "primitives_properties.c"
Cohesion: 0.20
Nodes (22): eval_at_end(), apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expr_bp() (+14 more)

### Community 138 - "logo_console_init"
Cohesion: 0.24
Nodes (16): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), setUp(), test_boundary_is_fixed_regardless_of_work(), test_clock_wraparound() (+8 more)

### Community 139 - "logo_lfs_backup"
Cohesion: 0.26
Nodes (16): mock_device_paint_canvas(), mock_device_set_canvas_point(), output_has(), stage_raster(), test_colourunder_reads_turtle_position(), test_colourunder_works_when_hidden(), test_over_answers_for_first_active(), test_over_false_for_other_colour() (+8 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "HTTP server (design)"
Cohesion: 0.17
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

### Community 145 - "test_primitives_exceptions.c"
Cohesion: 0.31
Nodes (13): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+5 more)

### Community 146 - "What to flag (in priority order)"
Cohesion: 0.20
Nodes (9): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on (+1 more)

### Community 147 - "LogoStream"
Cohesion: 0.15
Nodes (13): LogoStream, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos(), mock_file_get_write_pos(), mock_file_read_char() (+5 more)

### Community 148 - "primitives_variables.c"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

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
Cohesion: 0.19
Nodes (7): Build & Test, Code Structure, Constraints, graphify, How to work, Project, Unit Testing

### Community 153 - "logo_io_close_all"
Cohesion: 0.12
Nodes (15): setUp(), tearDown(), test_dots_variable(), test_error_no_value(), test_global_variable(), test_local_declaration(), test_local_with_list(), test_localmake_in_procedure() (+7 more)

### Community 154 - "improvements-roadmap.md"
Cohesion: 0.18
Nodes (18): blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_is_blob(), mem_region_alloc(), editor_pick_buffer(), enable_blob_region() (+10 more)

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
Cohesion: 0.44
Nodes (9): Evaluator, Result, Value, prim_battery_level(), prim_bootsel(), prim_goodbye(), prim_toot(), toot_gate_freq() (+1 more)

### Community 160 - "repl_extract_proc_name"
Cohesion: 0.33
Nodes (6): repl_extract_proc_name(), test_repl_extract_proc_name_basic(), test_repl_extract_proc_name_buffer_limit(), test_repl_extract_proc_name_no_name(), test_repl_extract_proc_name_with_inputs(), test_repl_extract_proc_name_with_whitespace()

### Community 161 - "repl_line_starts_with_to"
Cohesion: 0.40
Nodes (5): repl_line_starts_with_to(), test_repl_line_starts_with_to_basic(), test_repl_line_starts_with_to_false_cases(), test_repl_line_starts_with_to_just_to(), test_repl_line_starts_with_to_with_whitespace()

### Community 162 - "mock_text_set_width"
Cohesion: 0.14
Nodes (14): primitive_find(), primitive_get_by_index(), primitive_get_count(), primitive_register_alias(), logo_random_reset(), logo_random_seed(), pcg32_next(), Primitive (+6 more)

### Community 163 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 164 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 165 - "mock_fs_reset"
Cohesion: 0.67
Nodes (3): mock_fs_reset(), setUp(), tearDown()

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

### Community 177 - "lexer_set_preserve_comments"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 196 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 198 - "on_sd_card_detect"
Cohesion: 0.50
Nodes (4): repeating_timer_t, on_sd_card_detect(), logo_picocalc_mount_available(), sd_card_present()

## Knowledge Gaps
- **624 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+619 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `eval_string`, `test_value.c`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `result_none`, `primitives_properties.c`, `test_frame.c`, `proc_define_from_text`, `error_format`, `logo_lfs_backup`, `format_buffer_init`, `lexer_init`, `test_primitives_editor.c`, `test_variables.c`, `logo_io_close_all`, `io.c`, `mock_device_get_state`, `test_primitives_json.c`, `test_primitives_control_flow.c`, `test_scaffold.h`, `test_primitives_files_load_save.c`, `test_primitives_files.c`, `test_primitives_hardware.c`, `test_time.c`, `test_scaffold_setUp`, `set_mock_input`, `test_primitives_outside_world.c`, `test_mock_device.c`, `demons_poll`, `primitives_files.c`, `prim_savel`, `eval.c`, `primitives_http.c`, `test_scaffold.c`, `test_galaxian.c`, `logo_storage_init`, `eval_primary`?**
  _High betweenness centrality (0.132) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `mem_is_nil`, `reset_output`, `mem_word_ptr`, `value_to_string`, `mem_atom`, `test_frame.c`, `proc_define_from_text`, `error_format`, `primitives_properties.c`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `test_httpd.c`, `logo_io_close_all`, `test_primitives_http.c`, `mock_device_get_state`, `test_primitives_json.c`, `test_primitives_control_flow.c`, `test_scaffold.h`, `test_primitives_files_load_save.c`, `test_primitives_files.c`, `test_primitives_wifi.c`, `test_primitives_hardware.c`, `test_time.c`, `test_scaffold_setUp`, `test_primitives_outside_world.c`, `test_mock_device.c`, `test_primitives_network.c`, `test_primitives_files_directory.c`, `demons_poll`, `primitives_files.c`, `eval.c`, `primitives_http.c`, `test_scaffold.c`, `test_galaxian.c`, `eval_primary`?**
  _High betweenness centrality (0.086) - this node is a cross-community bridge._
- **Why does `result_none()` connect `result_none` to `run_string`, `test_value.c`, `mem_is_nil`, `logo_io_set_writer`, `primitives_properties.c`, `test_primitives_exceptions.c`, `test_variables.c`, `io.c`, `Managing Various Files`, `memory.c`, `set_mock_input`, `primitives.h`, `httpd.c`, `primitives_workspace.c`, `step_proc_call`, `test_primitives_files_directory.c`, `demons_poll`, `op_stack_push`, `value_number`, `primitives_files.c`, `primitives_get_io`, `eval.c`, `primitives_httpd.c`, `eval_push_if`, `primitives_control_flow.c`, `primitives_outside_world.c`, `logo_io_init`, `eval_primary`, `procedures.c`?**
  _High betweenness centrality (0.061) - this node is a cross-community bridge._
- **Are the 894 inferred relationships involving `run_string()` (e.g. with `test_action_does_not_reenter_poll()` and `test_cleardemons_disarms_all()`) actually correct?**
  _`run_string()` has 894 INFERRED edges - model-reasoned connections that need verification._
- **Are the 872 inferred relationships involving `eval_string()` (e.g. with `test_deep_recursion_100_levels()` and `test_deep_recursion_addupto()`) actually correct?**
  _`eval_string()` has 872 INFERRED edges - model-reasoned connections that need verification._
- **Are the 424 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 424 INFERRED edges - model-reasoned connections that need verification._
- **Are the 235 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `parse_list()`) actually correct?**
  _`mem_is_nil()` has 235 INFERRED edges - model-reasoned connections that need verification._