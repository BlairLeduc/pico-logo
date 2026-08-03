# Graph Report - pico-logo  (2026-08-02)

## Corpus Check
- 293 files · ~513,553 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 7225 nodes · 22949 edges · 210 communities (198 shown, 12 thin omitted)
- Extraction: 56% EXTRACTED · 44% INFERRED · 0% AMBIGUOUS · INFERRED: 10185 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `4a3aadda`
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
- primitives_turtle.c
- result_none
- test_trails.c
- iteration_callback
- io.c
- test_primitives_files_directory.c
- picocalc_console.c
- test_format.c
- lexer_init
- test_frame_arena.c
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
- test_primitives_conditionals.c
- mock_device.c
- test_primitives_json.c
- mem_atom
- test_scaffold.h
- test_notation.c
- primitives_init
- error_format
- test_primitives_files.c
- lexer_next_token
- primitives_conditionals.c
- test_cross_fs_move.c
- test_primitives_wifi.c
- List Processing
- test_primitives_list_processing.c
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
- eval_primary
- primitives_json.c
- Conditionals and Control of Flow
- test_mock_fs.h
- demons_poll
- test_dirty_tiles.c
- Words and Lists
- test_fileserver.c
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
- logo_hardware_init
- host_storage.c
- test_costumes.c
- result_error_arg
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- host_console.c
- primitives_bitwise.c
- run_editor_and_process
- mem_atom_cstr
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- primitives_http.c
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- mem_cons
- host_hardware.c
- sdcard.c
- Managing your Workspace
- procedures.c
- read_map
- Property Lists
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
- mklfsimg_lib.c
- Design: `launch` background processes (P6)
- test_checkrun.c
- southbridge.c
- node_iter_next
- roadmap.md
- sound.c
- Using the Logo Editor
- HTTP Server
- mem_blob
- Appendix B: Parsing
- record_command
- WiFi Management
- P10 — Interpreter throughput (design)
- The pick of five: plans
- primitives.c
- prim_setdate
- LogoStream
- ms_to_datetime
- format.c
- logo_lfs_storage_init
- The Outside World
- HTTP server (design)
- prim_define
- PR Review Checklist (CRITICAL)
- Managing Various Files
- record_command_float
- test_lfs_backup.c
- format_buffer_init
- test_frame_sync.c
- Atom Garbage Collection: Implementation Plan
- logo_lfs_backup
- MockCommandType
- Introduction
- as_httpd_conn
- Appendix A: Useful Tools
- test_frame.c
- screen.c
- ensure_wifi_initialized
- primitives_text.c
- LogoEditorResult
- Text and Screen Commands
- Bitwise Operations
- result_goto
- Pico Logo
- value_number
- drain_tokens
- ip_addr_t
- gen_ca_certs.py
- test_bench_throughput.c
- pandoc_slug
- logo_random_next
- bd_prog
- test_pop_scope
- dist.sh
- mock_fs_tearDown
- generate_help.sh
- run_e2e.sh
- HTTP Operations
- VENDOR.md
- picocalc_wifi_status
- load_logo
- frame_stack_depth
- Variables
- mem_set_cdr
- eval_instruction
- mock_device_set_raster
- proc_save_execution_state
- mock_device_get_dot
- mock_sound_queue
- mock_sound_status
- mock_wifi_status
- prim_local
- mock_text_set_width
- mock_turtle_canvas_point

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 972 edges
2. `eval_string()` - 923 edges
3. `mem_word_ptr()` - 446 edges
4. `mem_atom()` - 242 edges
5. `mem_is_nil()` - 240 edges
6. `value_to_string()` - 208 edges
7. `result_error_arg()` - 201 edges
8. `result_none()` - 200 edges
9. `result_ok()` - 177 edges
10. `lexer_init()` - 173 edges

## Surprising Connections (you probably didn't know these)
- `test_format_buffer_init_sets_empty_buffer()` --calls--> `format_buffer_init()`  [INFERRED]
  tests/test_format.c → core/format.c
- `test_nil_is_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_list()` --calls--> `mem_is_list()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_word()` --calls--> `mem_is_word()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_number_content()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Communities (210 total, 12 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (228): MockDeviceState, mock_device_get_output(), mock_device_get_state(), mock_device_has_line_from_to(), test_cleartext_clears_screen(), test_cs_restores_auto_refresh(), test_cs_restores_from_sync(), test_ct_is_alias_for_cleartext() (+220 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "test_tilemap.c"
Cohesion: 0.10
Nodes (60): bake_ready(), bake_rect(), Evaluator, LogoConsoleTurtle, Result, Value, get_turtle_ops(), is_int_in() (+52 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (153): format_number(), result_error_in(), result_is_ok(), result_is_returnable(), result_output(), result_set_error_proc(), result_stop(), result_throw() (+145 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.05
Nodes (117): Node, mem_atom_unescape(), mem_car(), mem_cdr(), mem_is_list(), mem_is_nil(), mem_is_word(), mem_set_car() (+109 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (181): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+173 more)

### Community 6 - "eval_string"
Cohesion: 0.02
Nodes (168): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+160 more)

### Community 7 - "primitives_turtle.c"
Cohesion: 0.14
Nodes (76): arg_turtle(), Evaluator, LogoConsoleTurtle, LogoTurtleRaster, Result, Value, get_turtle_ops(), make_position_list() (+68 more)

### Community 8 - "result_none"
Cohesion: 0.12
Nodes (54): Node, demons_freeze(), demons_print(), demons_set(), demons_thaw(), Evaluator, Result, Value (+46 more)

### Community 9 - "test_trails.c"
Cohesion: 0.15
Nodes (47): actor(), num(), put_actor(), run(), runf(), test_a_bank_tile_carves_one_stroke_per_walkable_neighbour(), test_blocked_actor_discards_the_frame_step(), test_blocked_turn_stays_buffered() (+39 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.06
Nodes (87): LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span(), logo_io_check_user_interrupt(), logo_io_check_write_error(), logo_io_clear_freeze_request() (+79 more)

### Community 12 - "test_primitives_files_directory.c"
Cohesion: 0.06
Nodes (40): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+32 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.05
Nodes (61): picocalc_editor_get_ops(), LogoConsole, LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_write() (+53 more)

### Community 14 - "test_format.c"
Cohesion: 0.10
Nodes (27): format_body_element(), format_buffer_output(), format_variable(), format_variable_to_buffer(), test_format_body_element_nested_list(), test_format_body_element_nil_produces_no_output(), test_format_body_element_simple_list(), test_format_body_element_word() (+19 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_io.c"
Cohesion: 0.05
Nodes (33): logo_io_parse_network_address(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn(), mock_dir_callback(), mock_list_directory(), mock_open() (+25 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.08
Nodes (51): frame_stack_is_empty(), Value, find_global(), var_bury(), var_bury_all(), var_declare_local(), var_erase(), var_erase_all() (+43 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (72): httpd_listening(), httpd_request_pending(), httpd_reset(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex() (+64 more)

### Community 23 - "test_eval.c"
Cohesion: 0.03
Nodes (57): test_b10_bare_exponent_in_list_path_is_not_a_number(), test_b10_bare_n_exponent_word_is_not_a_number(), test_b10_valid_exponent_forms_still_numbers(), test_b7_grouped_call_inside_operand(), test_b7_grouped_multi_branch_call_inside_operand(), test_b7_grouping_paren_starting_with_call(), test_b7_grouping_paren_starting_with_call_output(), test_b7_nested_grouping_parens_around_call() (+49 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.06
Nodes (46): LogoIO, primitives_control_reset_test_state(), primitives_set_io(), properties_init(), LogoStorage, LogoStorageOps, logo_storage_init(), mock_device_init() (+38 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (67): logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), Result (+59 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (52): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+44 more)

### Community 28 - "test_primitives_conditionals.c"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 29 - "mock_device.c"
Cohesion: 0.03
Nodes (13): mock_device_add_wifi_scan_result(), mock_device_get_tcp_request_len(), mock_device_set_input(), mock_device_set_snap_result(), mock_device_set_tcp_close_after(), mock_device_set_tcp_read_chunk(), mock_device_set_tcp_write_chunk(), mock_device_verify_palette() (+5 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (62): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+54 more)

### Community 31 - "mem_atom"
Cohesion: 0.05
Nodes (80): mem_atom(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_total_atoms(), mem_word_eq(), mem_words_equal(), str_eq_nocase() (+72 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (56): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown() (+48 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.11
Nodes (35): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+27 more)

### Community 35 - "error_format"
Cohesion: 0.05
Nodes (51): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+43 more)

### Community 36 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (46): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+38 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "primitives_conditionals.c"
Cohesion: 0.51
Nodes (10): Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse(), prim_iftrue() (+2 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.05
Nodes (58): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result(), mock_device_set_wifi_ssid() (+50 more)

### Community 41 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

### Community 42 - "test_primitives_list_processing.c"
Cohesion: 0.06
Nodes (33): test_apply_unknown_procedure(), test_apply_with_lambda(), test_apply_with_multi_param_lambda(), test_apply_with_primitive_name(), test_apply_with_procedure_text(), test_apply_with_user_procedure(), test_apply_with_word_primitive(), test_crossmap_callback_throw_freed() (+25 more)

### Community 43 - "test_mock_device.c"
Cohesion: 0.04
Nodes (84): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), MockCommand (+76 more)

### Community 44 - "test_time.c"
Cohesion: 0.06
Nodes (48): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+40 more)

### Community 45 - "set_mock_input"
Cohesion: 0.08
Nodes (73): LogoIO, Result, repl_cleanup(), repl_count_bracket_balance(), repl_evaluate_line(), repl_extract_proc_name(), repl_find_end_token(), repl_init() (+65 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

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
Cohesion: 0.10
Nodes (53): demons_frozen(), LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find() (+45 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (37): LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error(), logo_stream_close() (+29 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (34): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+26 more)

### Community 54 - ";"
Cohesion: 0.03
Nodes (59): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Break (+51 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.08
Nodes (36): mock_sound_set_status(), assert_number_list(), assert_word(), MockDeviceState, Result, Value, snd(), test_env_default() (+28 more)

### Community 57 - "test_primitives_files_load_save.c"
Cohesion: 0.05
Nodes (63): proc_exists(), var_exists(), var_get(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result() (+55 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (41): fat32_close(), fat32_create(), fat32_delete(), fat32_is_mounted(), fat32_mount(), fat32_open(), fat32_read(), fat32_set_current_dir() (+33 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (35): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+27 more)

### Community 61 - "memory.c"
Cohesion: 0.10
Nodes (41): BlobDesc, remember_binding(), alloc_cell(), atom_chain_next(), atom_clear_marks(), atom_entry_is_free(), atom_entry_next(), atom_entry_set_next() (+33 more)

### Community 62 - "eval_primary"
Cohesion: 0.05
Nodes (127): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, UserProcedure, Value (+119 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.12
Nodes (28): LogoDirCallback, LogoStream, MockFile, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos() (+20 more)

### Community 66 - "demons_poll"
Cohesion: 0.10
Nodes (36): Result, demons_poll(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle(), test_action_does_not_reenter_poll(), test_cleardemons_disarms_all(), test_cleardemons_leaves_motion_and_freeze() (+28 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.13
Nodes (30): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+22 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "test_fileserver.c"
Cohesion: 0.19
Nodes (25): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+17 more)

### Community 70 - "primitives_outside_world.c"
Cohesion: 0.23
Nodes (20): Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Result, Value, flush_writer(), prim_keyp() (+12 more)

### Community 71 - "primitives.h"
Cohesion: 0.07
Nodes (47): demons_clear(), demons_gc_mark_all(), demons_reset(), demons_resume(), demons_suspend(), Evaluator, Result, Value (+39 more)

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
Nodes (110): proc_define_from_text(), FrameStack, proc_get_frame_stack(), test_deep_nested_proc_in_repeat(), test_many_iterations_proc_in_repeat_within_procedure(), test_proc_call_followed_by_commands_in_repeat(), test_proc_call_in_if_within_procedure(), test_proc_call_in_repeat_within_procedure() (+102 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "main"
Cohesion: 0.09
Nodes (25): proc_reset_execution_state(), logo_io_write_line(), main(), psram_verify(), m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program() (+17 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "logo_hardware_init"
Cohesion: 0.33
Nodes (4): LogoHardware, LogoHardwareOps, logo_hardware_init(), test_play_no_sound_engine_is_noop()

### Community 81 - "host_storage.c"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.23
Nodes (19): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), fill_pattern(), setUp() (+11 more)

### Community 83 - "result_error_arg"
Cohesion: 0.10
Nodes (76): CatalogContext, CatalogEntry, httpd_respondfile(), httpd_savebody(), path_is_safe(), prim_editfile(), Evaluator, Result (+68 more)

### Community 84 - "test_lfs_storage.c"
Cohesion: 0.12
Nodes (18): Listing, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t, LogoEntryType (+10 more)

### Community 85 - "Code Review — 2026-07-02"
Cohesion: 0.08
Nodes (23): 1. Confirmed bug: `recycle` sweeps reachable data, 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate), 2.2 `find_atom` is a linear scan of the whole atom table, 2.3 Smaller items, 2. Hot-path efficiency, 3. Robustness: `mem_cons` failures are silently ignored, 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict, 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision (+15 more)

### Community 86 - "Contributing"
Cohesion: 0.08
Nodes (23): About Logo, Additional Features for the PicoCalc, Advanced Logo, Beginning Logo, Building and Running, Contributing, Credits, Dependencies (+15 more)

### Community 87 - "host_console.c"
Cohesion: 0.26
Nodes (13): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+5 more)

### Community 88 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 89 - "run_editor_and_process"
Cohesion: 0.47
Nodes (10): Evaluator, Result, Value, count_bracket_balance(), prim_edall(), prim_edit(), prim_edn(), prim_edns() (+2 more)

### Community 90 - "mem_atom_cstr"
Cohesion: 0.13
Nodes (37): mem_atom_cstr(), Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or() (+29 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.16
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

### Community 95 - "primitives_http.c"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

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
Cohesion: 0.11
Nodes (72): mem_cons(), Lexer, Node, token_source_at_end(), token_source_consume_sublist(), token_source_copy(), token_source_gc_mark(), token_source_get_position() (+64 more)

### Community 100 - "host_hardware.c"
Cohesion: 0.07
Nodes (15): LogoHardware, host_network_tcp_connect(), init_winsock(), logo_host_hardware_create(), logo_host_hardware_destroy(), main(), LogoConsole, LogoHardware (+7 more)

### Community 101 - "sdcard.c"
Cohesion: 0.23
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "procedures.c"
Cohesion: 0.11
Nodes (27): atom_memo_bind_index(), atom_memo_bind_kind(), atom_memo_class(), atom_memo_set_binding(), atom_memo_set_class(), Token, resolve_word(), UserProcedure (+19 more)

### Community 104 - "read_map"
Cohesion: 0.17
Nodes (20): numf(), read_map(), read_slots(), step_tile(), test_calm_tiles_do_not_strand_bugs(), test_decoded_counts_match_the_encoded_words(), test_direction_deltas_and_opposites(), test_level_profiles_escalate() (+12 more)

### Community 105 - "Property Lists"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.14
Nodes (23): LogoDirCallback, LogoStorage, LogoStorageOps, LogoStream, cross_fs_move(), is_root(), logo_storage_router_init(), router_dir_create() (+15 more)

### Community 108 - "test_galaxian.c"
Cohesion: 0.17
Nodes (22): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), tearDown(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin() (+14 more)

### Community 109 - "mem_word_ptr"
Cohesion: 0.02
Nodes (124): Value, value_is_true(), mem_word_ptr(), bind_long_blob_word(), exhaust_atom_table(), exhaust_node_pool(), test_ascii(), test_bar_list_literal_count() (+116 more)

### Community 110 - "test_primitives_outside_world.c"
Cohesion: 0.05
Nodes (41): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+33 more)

### Community 111 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 112 - "value_to_string"
Cohesion: 0.11
Nodes (106): read_predicate_bool(), number_to_word(), mem_first_cell(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_list_append(), mem_next_cell(), mem_word() (+98 more)

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

### Community 122 - "mklfsimg_lib.c"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (58): MockStamp, mock_device_clear_graphics(), mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf() (+50 more)

### Community 125 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 126 - "node_iter_next"
Cohesion: 0.29
Nodes (8): Token, TokenType, compute_word_class(), is_delimiter_token(), is_number_word(), node_iter_next(), token_from_class(), NodeIterator

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

### Community 131 - "mem_blob"
Cohesion: 0.18
Nodes (18): httpd_buf_init(), blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_is_blob(), mem_region_alloc(), editor_pick_buffer() (+10 more)

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
Cohesion: 0.07
Nodes (28): 10. Relationship to P9, 11.1 First profile (2026-08-02, Pico Plus 2 W, 200 frames), 11. M5 — re-profile before choosing a lever (2026-08-02), 1. Goal, 2.1 The games miss their budgets, and it is not the screen, 2.2 The profile, 2. Evidence, 3.1 Every evaluation re-lexes the list (+20 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (23): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P10 — Interpreter throughput, P1 — Host REPL stdin + CI (+15 more)

### Community 137 - "primitives.c"
Cohesion: 0.17
Nodes (18): primitive_by_index(), primitive_find(), primitive_find_n(), primitive_get_by_index(), primitive_get_count(), primitive_index_of(), primitive_is_output(), primitive_name_compare() (+10 more)

### Community 138 - "prim_setdate"
Cohesion: 0.49
Nodes (9): Evaluator, Result, Value, prim_date(), prim_setdate(), prim_settime(), prim_ticks(), prim_time() (+1 more)

### Community 139 - "LogoStream"
Cohesion: 0.20
Nodes (10): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.40
Nodes (10): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+2 more)

### Community 141 - "format.c"
Cohesion: 0.14
Nodes (31): Node, Value, format_body_element_multiline(), format_body_indent(), format_buffer_pos(), format_list_contents(), format_list_with_newlines(), format_property() (+23 more)

### Community 142 - "logo_lfs_storage_init"
Cohesion: 0.40
Nodes (5): lfs_t, LogoStorage, logo_lfs_storage_init(), setUp(), setUp()

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "prim_define"
Cohesion: 0.56
Nodes (8): Evaluator, Result, Value, prim_copydef(), prim_define(), prim_definedp(), prim_primitivep(), prim_text()

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
Cohesion: 0.21
Nodes (17): blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read(), blob_set_read_pos(), blob_write_bytes(), lfs_t (+9 more)

### Community 150 - "format_buffer_init"
Cohesion: 0.20
Nodes (26): UserProcedure, format_buffer_init(), format_procedure_definition(), format_procedure_title(), format_procedure_to_buffer(), proc_erase(), proc_find(), test_format_procedure_definition_bracket_single_token_last_line() (+18 more)

### Community 151 - "test_frame_sync.c"
Cohesion: 0.22
Nodes (16): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), setUp(), test_boundary_is_fixed_regardless_of_work(), test_clock_wraparound() (+8 more)

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

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

### Community 158 - "test_frame.c"
Cohesion: 0.07
Nodes (91): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+83 more)

### Community 159 - "screen.c"
Cohesion: 0.06
Nodes (54): lcd_enable_cursor(), lcd_move_cursor(), lcd_restore_palette(), lcd_set_background(), lcd_set_cursor_char(), lcd_set_palette_rgb(), error_output_flush(), output_flush() (+46 more)

### Community 160 - "ensure_wifi_initialized"
Cohesion: 0.15
Nodes (21): ensure_wifi_initialized(), ntp_dns_callback(), ntp_send_request(), picocalc_network_ntp(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_tcp_connect(), picocalc_network_tcp_listen() (+13 more)

### Community 161 - "primitives_text.c"
Cohesion: 0.34
Nodes (19): Evaluator, Result, Value, get_screen_ops(), get_text_ops(), prim_cleartext(), prim_cursor(), prim_fullscreen() (+11 more)

### Community 163 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 164 - "Bitwise Operations"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

### Community 165 - "result_goto"
Cohesion: 0.13
Nodes (15): Result, result_get_error_code(), result_get_goto_label(), result_get_pause_proc(), result_get_throw_tag(), result_goto(), result_pause(), test_result_get_error_code() (+7 more)

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "value_number"
Cohesion: 0.31
Nodes (26): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+18 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 170 - "ip_addr_t"
Cohesion: 0.33
Nodes (6): ntp_recv_callback(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 173 - "test_bench_throughput.c"
Cohesion: 0.30
Nodes (13): calibrate_ns(), define_workspace(), load_game(), now_ms(), tearDown(), test_bench_checkrun_play_frame(), test_bench_proc_call_workspace_scaling(), test_bench_repeat_loop() (+5 more)

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 176 - "logo_random_next"
Cohesion: 0.38
Nodes (5): LogoIO, logo_random_next(), logo_random_reset(), logo_random_seed(), pcg32_next()

### Community 177 - "bd_prog"
Cohesion: 0.47
Nodes (6): bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t

### Community 178 - "test_pop_scope"
Cohesion: 0.10
Nodes (36): test_dots_variable(), test_global_variable(), test_local_declaration(), test_local_variable_not_visible_after_scope(), test_local_variable_shadowing(), test_local_with_list(), test_localmake_declares_and_sets(), test_localmake_in_procedure() (+28 more)

### Community 180 - "mock_fs_tearDown"
Cohesion: 0.40
Nodes (5): mock_fs_reset(), mock_fs_tearDown(), tearDown(), tearDown(), tearDown()

### Community 184 - "HTTP Operations"
Cohesion: 0.25
Nodes (8): http.delete, http.get, http.header, HTTP Operations, http.patch, http.post, http.put, http.status

### Community 194 - "picocalc_wifi_status"
Cohesion: 0.33
Nodes (6): WifiState, mdns_start(), picocalc_network_set_hostname(), picocalc_wifi_connect(), picocalc_wifi_status(), wifi_configure_link()

### Community 196 - "load_logo"
Cohesion: 0.50
Nodes (4): load_logo(), load_trails(), setUp(), test_p10prof_profiler_runs()

### Community 197 - "frame_stack_depth"
Cohesion: 0.12
Nodes (19): frame_pop(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_init(), var_local_count(), setUp(), test_pop_all_frames(), test_pop_empty_returns_none() (+11 more)

### Community 198 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 199 - "mem_set_cdr"
Cohesion: 0.20
Nodes (15): mem_set_cdr(), node_to_index(), append_to_list(), Lexer, Node, Token, parse_bracket_contents(), token_to_atom() (+7 more)

### Community 200 - "eval_instruction"
Cohesion: 0.19
Nodes (13): demons_maybe_poll(), demons_running(), eval_instruction(), httpd_maybe_poll(), logo_io_check_freeze_request(), logo_io_check_pause_request(), logo_io_clear_pause_request(), logo_io_has_ticks_ms() (+5 more)

### Community 201 - "mock_device_set_raster"
Cohesion: 0.67
Nodes (3): LogoTurtleRaster, mock_device_set_raster(), mock_turtle_get_raster()

### Community 207 - "prim_local"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_local(), prim_localmake(), prim_make(), prim_name(), prim_namep() (+1 more)

## Knowledge Gaps
- **759 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+754 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **12 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `mem_is_nil`, `reset_output`, `eval_string`, `result_none`, `test_trails.c`, `test_primitives_files_directory.c`, `lexer_init`, `format_buffer_init`, `test_eval.c`, `test_scaffold_setUp`, `test_primitives_conditionals.c`, `test_primitives_json.c`, `mem_atom`, `test_scaffold.h`, `error_format`, `test_primitives_files.c`, `test_primitives_list_processing.c`, `test_mock_device.c`, `test_time.c`, `test_bench_throughput.c`, `set_mock_input`, `test_primitives_tilemap.c`, `test_primitives_hardware.c`, `test_pop_scope`, `test_sound.c`, `test_primitives_files_load_save.c`, `eval_primary`, `demons_poll`, `load_logo`, `mem_set_cdr`, `eval_instruction`, `proc_define_from_text`, `logo_hardware_init`, `test_galaxian.c`, `mem_word_ptr`, `test_primitives_outside_world.c`, `value_to_string`, `test_primitives_editor.c`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.142) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `run_string`, `test_value.c`, `mem_is_nil`, `reset_output`, `test_trails.c`, `test_primitives_files_directory.c`, `lexer_init`, `test_httpd.c`, `test_eval.c`, `format_buffer_init`, `test_primitives_http.c`, `test_primitives_conditionals.c`, `test_primitives_json.c`, `mem_atom`, `test_scaffold.h`, `error_format`, `test_primitives_files.c`, `test_primitives_wifi.c`, `test_primitives_list_processing.c`, `test_time.c`, `test_primitives_tilemap.c`, `test_primitives_hardware.c`, `test_pop_scope`, `test_sound.c`, `test_primitives_network.c`, `eval_primary`, `test_fileserver.c`, `mem_set_cdr`, `proc_define_from_text`, `mem_atom_cstr`, `read_map`, `test_galaxian.c`, `mem_word_ptr`, `test_primitives_outside_world.c`, `value_to_string`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.097) - this node is a cross-community bridge._
- **Why does `result_none()` connect `result_none` to `run_string`, `test_tilemap.c`, `test_value.c`, `mem_is_nil`, `primitives_turtle.c`, `prim_setdate`, `prim_define`, `primitives_sound.c`, `primitives_text.c`, `primitives_conditionals.c`, `value_number`, `set_mock_input`, `httpd.c`, `eval_primary`, `demons_poll`, `primitives_outside_world.c`, `primitives.h`, `eval_instruction`, `prim_local`, `result_error_arg`, `run_editor_and_process`, `mem_atom_cstr`, `value_to_string`?**
  _High betweenness centrality (0.052) - this node is a cross-community bridge._
- **Are the 970 inferred relationships involving `run_string()` (e.g. with `define_workspace()` and `load_game()`) actually correct?**
  _`run_string()` has 970 INFERRED edges - model-reasoned connections that need verification._
- **Are the 921 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 921 INFERRED edges - model-reasoned connections that need verification._
- **Are the 440 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 440 INFERRED edges - model-reasoned connections that need verification._
- **Are the 232 inferred relationships involving `mem_atom()` (e.g. with `eval_primary()` and `parse_list()`) actually correct?**
  _`mem_atom()` has 232 INFERRED edges - model-reasoned connections that need verification._