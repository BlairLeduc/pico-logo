# Graph Report - pico-logo  (2026-08-01)

## Corpus Check
- 288 files · ~495,052 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 7102 nodes · 22546 edges · 188 communities (182 shown, 6 thin omitted)
- Extraction: 56% EXTRACTED · 44% INFERRED · 0% AMBIGUOUS · INFERRED: 10009 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `807b2829`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- run_string
- lfs.c
- test_primitives_properties.c
- test_value.c
- mem_cdr
- reset_output
- eval_string
- primitives_turtle.c
- result_none
- test_trails.c
- iteration_callback
- io.c
- test_primitives_files_directory.c
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
- test_primitives_control_flow.c
- mock_device.c
- test_primitives_json.c
- output_has
- test_scaffold.h
- test_notation.c
- primitives_init
- keyboard.c
- prim_local
- lexer_next_token
- primitives_conditionals.c
- test_cross_fs_move.c
- test_primitives_wifi.c
- List Processing
- result_error_arg
- test_primitives_outside_world.c
- test_time.c
- set_mock_input
- picocalc_editor_edit
- prim_savel
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
- test_frame_sync.c
- primitives_json.c
- Conditionals and Control of Flow
- test_mock_fs.h
- demons_poll
- test_dirty_tiles.c
- Words and Lists
- eval_primary
- primitives_text.c
- stdlib.h
- lfs_storage.c
- mock_sdcard.c
- screen.c
- picocalc_storage.c
- proc_define_from_text
- lexer.c
- picocalc_flash.c
- test_storage_router.c
- frame.c
- host_storage.c
- test_costumes.c
- mem_set_cdr
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- main
- mock_device_get_state
- Appendix B: Parsing
- logo_random_next
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- err_t
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- mem_cons
- host_hardware.c
- sdcard.c
- Managing your Workspace
- repl_evaluate_line
- test_help.c
- Property Lists
- test_tls_heap.c
- storage_router.c
- test_galaxian.c
- mem_word_ptr
- JSON
- mem_atom
- mem_is_nil
- P9 — Tile maps and smooth scrolling (design)
- on_sd_card_detect
- Galaxian in Pico Logo (design)
- File Management
- primitives_http.c
- test_scaffold.c
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- Time Management
- Design: `launch` background processes (P6)
- test_checkrun.c
- test_bench_throughput.c
- mock_device_get_dot
- roadmap.md
- sound.c
- Using the Logo Editor
- HTTP Server
- read_map
- proc_get_frame_stack
- Modifying Procedures Under Program Control
- WiFi Management
- P10 — Interpreter throughput (design)
- The pick of five: plans
- primitives.c
- Managing Various Files
- logo_lfs_backup
- ms_to_datetime
- primitives_outside_world.c
- The Outside World
- HTTP server (design)
- PR Review Checklist (CRITICAL)
- test_primitives_editor.c
- record_command_float
- test_lfs_backup.c
- test_frame.c
- Atom Garbage Collection: Implementation Plan
- mem_blob
- MockCommandType
- Introduction
- as_httpd_conn
- Appendix A: Useful Tools
- southbridge.c
- test_primitives_variables.c
- ensure_wifi_initialized
- Result
- mock_device_set_editor_result
- Text and Screen Commands
- Pico Logo
- value_number
- drain_tokens
- value_extract_xy
- ip_addr_t
- gen_ca_certs.py
- pandoc_slug
- primitives_bitwise.c
- dist.sh
- generate_help.sh
- run_e2e.sh
- VENDOR.md
- value_bool
- logo_console_init
- Variables

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 951 edges
2. `eval_string()` - 918 edges
3. `mem_word_ptr()` - 446 edges
4. `mem_atom()` - 242 edges
5. `mem_is_nil()` - 240 edges
6. `value_to_string()` - 201 edges
7. `result_error_arg()` - 195 edges
8. `result_none()` - 194 edges
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

## Communities (188 total, 6 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (251): MockLine, mock_device_get_line(), mock_device_get_output(), mock_device_line_count(), mock_device_verify_palette(), test_turtle_no_line_when_pen_up(), test_if_false_case_insensitive(), test_if_false_one_list_command() (+243 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "test_primitives_properties.c"
Cohesion: 0.09
Nodes (20): tearDown(), test_erprops_clears_all_properties(), test_gprop_requires_word_for_name(), test_gprop_requires_word_for_property(), test_multiple_properties_on_same_name(), test_plist_requires_word(), test_pprop_and_gprop_number_value(), test_pprop_and_gprop_word_value() (+12 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (163): format_number(), Result, Value, result_get_error_code(), result_get_goto_label(), result_get_pause_proc(), result_get_throw_tag(), result_get_value() (+155 more)

### Community 4 - "mem_cdr"
Cohesion: 0.03
Nodes (67): mem_cdr(), test_allopen_multiple_files(), test_readlist_from_file(), test_apply_unknown_procedure(), test_apply_with_lambda(), test_apply_with_multi_param_lambda(), test_apply_with_primitive_name(), test_apply_with_procedure_text() (+59 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (154): proc_is_stepped(), proc_is_traced(), test_b10_bare_exponent_in_list_path_is_not_a_number(), test_b10_bare_n_exponent_word_is_not_a_number(), test_b10_valid_exponent_forms_still_numbers(), test_b7_grouped_call_inside_operand(), test_b7_grouped_multi_branch_call_inside_operand(), test_b7_grouping_paren_starting_with_call() (+146 more)

### Community 6 - "eval_string"
Cohesion: 0.02
Nodes (164): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+156 more)

### Community 7 - "primitives_turtle.c"
Cohesion: 0.11
Nodes (85): demons_clear(), demons_freeze(), demons_reset(), demons_thaw(), Evaluator, Result, Value, prim_cleardemons() (+77 more)

### Community 8 - "result_none"
Cohesion: 0.09
Nodes (70): mem_atom_unescape(), mem_is_word(), Evaluator, Result, Value, prim_step(), prim_trace(), prim_unstep() (+62 more)

### Community 9 - "test_trails.c"
Cohesion: 0.16
Nodes (45): actor(), load_trails(), num(), put_actor(), run(), runf(), setUp(), test_blocked_actor_discards_the_frame_step() (+37 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.08
Nodes (73): eval_instruction(), prop_output(), LogoDirCallback, LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span() (+65 more)

### Community 12 - "test_primitives_files_directory.c"
Cohesion: 0.06
Nodes (37): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+29 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.05
Nodes (57): LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_flush(), heading_faces_left(), input_read_char() (+49 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.07
Nodes (85): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_init(), format_buffer_output() (+77 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_io.c"
Cohesion: 0.04
Nodes (62): logo_io_cleanup(), logo_io_flush(), logo_io_is_dribbling(), logo_io_parse_network_address(), logo_io_set_reader(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble() (+54 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.06
Nodes (70): Value, find_global(), var_bury(), var_bury_all(), var_declare_local(), var_erase(), var_erase_all(), var_erase_all_globals() (+62 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.06
Nodes (71): httpd_listening(), httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled() (+63 more)

### Community 23 - "error_format"
Cohesion: 0.06
Nodes (48): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+40 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.04
Nodes (76): LogoIO, primitives_control_reset_test_state(), primitives_set_io(), proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), properties_init(), variables_init() (+68 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.06
Nodes (65): logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_response(), Result (+57 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (52): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+44 more)

### Community 28 - "test_primitives_control_flow.c"
Cohesion: 0.03
Nodes (57): test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic(), test_do_until_invalid_predicate(), test_do_until_runs_once(), test_do_until_with_stop() (+49 more)

### Community 29 - "mock_device.c"
Cohesion: 0.02
Nodes (49): LogoStream, LogoTurtleRaster, SoundEvent, SoundStatus, WifiState, mock_device_add_wifi_scan_result(), mock_device_get_tcp_request_len(), mock_device_set_input() (+41 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (62): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+54 more)

### Community 31 - "output_has"
Cohesion: 0.22
Nodes (18): mock_device_paint_canvas(), mock_device_set_canvas_point(), output_has(), stage_raster(), test_colourunder_reads_turtle_position(), test_colourunder_works_when_hidden(), test_distance_answers_for_first_active(), test_distance_is_euclidean() (+10 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (60): MockCommand, mock_device_get_command(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), assert_fullscreen_canvas_hud() (+52 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.11
Nodes (34): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+26 more)

### Community 35 - "keyboard.c"
Cohesion: 0.08
Nodes (25): keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_set_background_poll(), keyboard_set_idle_callback(), keyboard_set_key_available_callback(), lcd_get_palette_value() (+17 more)

### Community 36 - "prim_local"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_local(), prim_localmake(), prim_make(), prim_name(), prim_namep() (+1 more)

### Community 37 - "lexer_next_token"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 38 - "primitives_conditionals.c"
Cohesion: 0.22
Nodes (19): Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse(), prim_iftrue() (+11 more)

### Community 39 - "test_cross_fs_move.c"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 40 - "test_primitives_wifi.c"
Cohesion: 0.05
Nodes (58): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_ssid(), mock_device_set_wifi_status() (+50 more)

### Community 41 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

### Community 42 - "result_error_arg"
Cohesion: 0.07
Nodes (98): CatalogContext, CatalogEntry, mem_atom_cstr(), Evaluator, Result, Value, Evaluator, LogoEntryType (+90 more)

### Community 43 - "test_primitives_outside_world.c"
Cohesion: 0.05
Nodes (42): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+34 more)

### Community 44 - "test_time.c"
Cohesion: 0.07
Nodes (45): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+37 more)

### Community 45 - "set_mock_input"
Cohesion: 0.09
Nodes (65): prim_pause(), proc_get_current(), LogoIO, repl_cleanup(), repl_count_bracket_balance(), repl_extract_proc_name(), repl_find_end_token(), repl_init() (+57 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "prim_savel"
Cohesion: 0.10
Nodes (52): demons_resume(), demons_suspend(), Evaluator, Result, Value, count_bracket_balance(), prim_edall(), prim_edit() (+44 more)

### Community 48 - "repository"
Cohesion: 0.04
Nodes (45): name, name, match, name, 1, 2, match, name (+37 more)

### Community 49 - "test_primitives_hardware.c"
Cohesion: 0.05
Nodes (46): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+38 more)

### Community 50 - "Turtle Trails (design)"
Cohesion: 0.06
Nodes (31): 10. Main loop and state order, 11. Memory and performance budget, 12. Design boundaries, 13. Tests, 14. Implementation milestones, 15. As built: divergences from this design, 1. Theme, 2. Display and board geometry (+23 more)

### Community 51 - "httpd.c"
Cohesion: 0.09
Nodes (62): demons_frozen(), demons_running(), LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn() (+54 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (36): screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error(), logo_stream_close(), logo_stream_copy(), logo_stream_flush() (+28 more)

### Community 53 - "lcd.c"
Cohesion: 0.10
Nodes (36): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+28 more)

### Community 54 - ";"
Cohesion: 0.05
Nodes (39): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel, Break (+31 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.10
Nodes (30): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), test_play_appends(), test_play_bad_notation_errors(), test_play_fans_out() (+22 more)

### Community 57 - "test_primitives_files_load_save.c"
Cohesion: 0.06
Nodes (51): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle() (+43 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (15): cyw43_ev_scan_result_t, LogoHardware, WifiState, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_start(), mdns_stop() (+7 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (41): fat32_close(), fat32_create(), fat32_delete(), fat32_mount(), fat32_open(), fat32_read(), fat32_set_current_dir(), fat32_unmount() (+33 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (34): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+26 more)

### Community 61 - "memory.c"
Cohesion: 0.07
Nodes (63): BlobDesc, atom_memo_bind_index(), atom_memo_bind_kind(), atom_memo_class(), atom_memo_set_binding(), atom_memo_set_class(), demons_gc_mark_all(), Token (+55 more)

### Community 62 - "test_frame_sync.c"
Cohesion: 0.24
Nodes (16): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), setUp(), test_boundary_is_fixed_regardless_of_work(), test_clock_wraparound() (+8 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.07
Nodes (58): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+50 more)

### Community 66 - "demons_poll"
Cohesion: 0.08
Nodes (43): Result, demons_maybe_poll(), demons_poll(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle(), setUp(), test_action_does_not_reenter_poll() (+35 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.13
Nodes (29): dirty_tiles_any(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite, screen_gfx_mark_dirty() (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "eval_primary"
Cohesion: 0.05
Nodes (127): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, UserProcedure, Value (+119 more)

### Community 70 - "primitives_text.c"
Cohesion: 0.34
Nodes (19): Evaluator, Result, Value, get_screen_ops(), get_text_ops(), prim_cleartext(), prim_cursor(), prim_fullscreen() (+11 more)

### Community 71 - "stdlib.h"
Cohesion: 0.10
Nodes (24): Node, demons_print(), demons_set(), Evaluator, Result, Value, pause_check_continue(), pause_request_continue() (+16 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (19): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+11 more)

### Community 74 - "screen.c"
Cohesion: 0.06
Nodes (59): dirty_tiles_clear(), picocalc_editor_get_ops(), redraw_input_line(), lcd_enable_cursor(), lcd_move_cursor(), lcd_set_cursor_char(), LogoConsole, error_output_write() (+51 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.16
Nodes (25): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStream, file_context_stale(), logo_picocalc_dir_create(), logo_picocalc_file_open() (+17 more)

### Community 76 - "proc_define_from_text"
Cohesion: 0.03
Nodes (92): proc_define_from_text(), test_deep_nested_proc_in_repeat(), test_many_iterations_proc_in_repeat_within_procedure(), test_proc_call_followed_by_commands_in_repeat(), test_proc_call_in_if_within_procedure(), test_proc_call_in_repeat_within_procedure(), test_proc_expr_in_run_within_procedure(), test_co_at_toplevel() (+84 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "picocalc_flash.c"
Cohesion: 0.12
Nodes (19): m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest(), writable_m1(), bd_erase() (+11 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "frame.c"
Cohesion: 0.11
Nodes (39): Binding, FrameHeader, FrameStack, Value, word_offset_t, calc_frame_size(), frame_add_local(), frame_at() (+31 more)

### Community 81 - "host_storage.c"
Cohesion: 0.12
Nodes (17): LogoDirCallback, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos(), host_file_get_write_pos() (+9 more)

### Community 82 - "test_costumes.c"
Cohesion: 0.19
Nodes (21): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), turtle_put_shape_data(), turtles_init() (+13 more)

### Community 83 - "mem_set_cdr"
Cohesion: 0.20
Nodes (15): mem_set_cdr(), append_to_list(), Lexer, Node, Token, parse_bracket_contents(), token_to_atom(), Node (+7 more)

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

### Community 88 - "mock_device_get_state"
Cohesion: 0.02
Nodes (140): LogoConsole, MockDeviceState, mock_device_clear_commands(), mock_device_command_count(), mock_device_dot_count(), mock_device_get_console(), mock_device_get_state(), mock_device_has_dot_at() (+132 more)

### Community 89 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 90 - "logo_random_next"
Cohesion: 0.38
Nodes (5): LogoIO, logo_random_next(), logo_random_reset(), logo_random_seed(), pcg32_next()

### Community 91 - "picocalc_read_line"
Cohesion: 0.18
Nodes (14): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+6 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.08
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (implemented)"
Cohesion: 0.06
Nodes (31): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+23 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "err_t"
Cohesion: 0.33
Nodes (6): tcp_client_connected_cb(), tcp_client_err_cb(), tcp_client_poll_cb(), tcp_client_recv_cb(), tcp_listener_accept_cb(), err_t

### Community 96 - "Space Invaders in Pico Logo (design & implementation)"
Cohesion: 0.09
Nodes (22): 10. Why this is a good P5 acceptance test, 11. Deliverable, 1. The board, 2. Object representation — the central decision, 3. The alien formation on the canvas, 4. Collision routing — demons vs. the game loop, 5. Global events as demons, 6. Input (+14 more)

### Community 97 - "package.json"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 98 - "test_mklfsimg.c"
Cohesion: 0.08
Nodes (33): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+25 more)

### Community 99 - "mem_cons"
Cohesion: 0.09
Nodes (79): mem_cons(), Lexer, Node, Token, TokenType, compute_word_class(), is_delimiter_token(), is_number_word() (+71 more)

### Community 101 - "sdcard.c"
Cohesion: 0.25
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "repl_evaluate_line"
Cohesion: 0.15
Nodes (19): proc_restore_execution_state(), proc_save_execution_state(), Result, name_distance(), repl_evaluate_line(), repl_line_is_end(), repl_restore_refresh(), repl_suggest_name() (+11 more)

### Community 104 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 105 - "Property Lists"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.13
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "test_galaxian.c"
Cohesion: 0.19
Nodes (20): assert_num(), assert_true(), seed_convoy(), tearDown(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom(), test_file_loads_and_sets_globals() (+12 more)

### Community 109 - "mem_word_ptr"
Cohesion: 0.02
Nodes (159): Value, value_is_true(), mem_is_blob(), mem_word_ptr(), test_eval_quoted_word(), test_infix_equal_empty_lists(), test_infix_equal_lists(), test_infix_equal_lists_different_length() (+151 more)

### Community 110 - "JSON"
Cohesion: 0.33
Nodes (6): JSON, json.array, json.count, json.get, json.make, json.object

### Community 111 - "mem_atom"
Cohesion: 0.07
Nodes (71): mem_atom(), mem_free_nodes(), mem_gc(), mem_is_list(), mem_word_eq(), test_allocate_after_gc_recovery(), test_atom_chain_end_is_not_an_allocatable_offset(), test_atom_content() (+63 more)

### Community 112 - "mem_is_nil"
Cohesion: 0.09
Nodes (136): read_predicate_bool(), number_to_word(), mem_car(), mem_first_cell(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_is_nil(), mem_list_append() (+128 more)

### Community 113 - "P9 — Tile maps and smooth scrolling (design)"
Cohesion: 0.07
Nodes (28): 10. Checkpoint Run revamp, 11. Turtle Trails revamp (render-only, gameplay identical), 12. Budgets, 13. Milestones, 14. Tests, 15. Levers if M0 misses, 16. Rejected alternatives, 17. Roadmap gate questions, resolved (+20 more)

### Community 114 - "on_sd_card_detect"
Cohesion: 0.40
Nodes (5): repeating_timer_t, fat32_is_mounted(), on_sd_card_detect(), logo_picocalc_mount_available(), sd_card_present()

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (19): backup, cat, catalog, copyfile, createdir, dir? (dirp), directories, editfile (+11 more)

### Community 117 - "primitives_http.c"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

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

### Community 122 - "Time Management"
Cohesion: 0.33
Nodes (6): date, setdate, settime, ticks, time, Time Management

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (57): MockStamp, mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf(), player_at_junction() (+49 more)

### Community 125 - "test_bench_throughput.c"
Cohesion: 0.26
Nodes (14): calibrate_ns(), define_workspace(), load_game(), now_ms(), setUp(), tearDown(), test_bench_checkrun_play_frame(), test_bench_proc_call_workspace_scaling() (+6 more)

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

### Community 131 - "read_map"
Cohesion: 0.15
Nodes (21): mock_device_clear_graphics(), numf(), read_map(), step_tile(), test_calm_tiles_do_not_strand_bugs(), test_decoded_counts_match_the_encoded_words(), test_direction_deltas_and_opposites(), test_drawn_corridors_match_the_map() (+13 more)

### Community 132 - "proc_get_frame_stack"
Cohesion: 0.17
Nodes (20): frame_stack_depth(), frame_stack_used_bytes(), FrameStack, proc_get_frame_stack(), var_is_shadowed_by_local(), var_local_count(), test_stack_init_used_zero(), test_tco_10000_recursions_constant_memory() (+12 more)

### Community 133 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "P10 — Interpreter throughput (design)"
Cohesion: 0.08
Nodes (26): 10. Relationship to P9, 1. Goal, 2.1 The games miss their budgets, and it is not the screen, 2.2 The profile, 2. Evidence, 3.1 Every evaluation re-lexes the list, 3.2 Every call resolves its name by string compare, 3.3 Cons-cell access is indirect (+18 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (23): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P10 — Interpreter throughput, P1 — Host REPL stdin + CI (+15 more)

### Community 137 - "primitives.c"
Cohesion: 0.18
Nodes (16): primitive_by_index(), primitive_find(), primitive_find_n(), primitive_get_by_index(), primitive_get_count(), primitive_index_of(), primitive_is_output(), primitive_name_compare() (+8 more)

### Community 138 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "primitives_outside_world.c"
Cohesion: 0.19
Nodes (24): Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Node, Result, Value, flush_writer() (+16 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 146 - "PR Review Checklist (CRITICAL)"
Cohesion: 0.22
Nodes (8): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on

### Community 147 - "test_primitives_editor.c"
Cohesion: 0.08
Nodes (48): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, mock_file_can_read(), mock_file_close(), mock_file_flush() (+40 more)

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "test_lfs_backup.c"
Cohesion: 0.16
Nodes (23): bd_erase(), bd_prog(), bd_read(), blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read() (+15 more)

### Community 151 - "test_frame.c"
Cohesion: 0.08
Nodes (67): UserProcedure, frame_current(), frame_find_binding(), frame_get_bindings(), frame_get_test(), frame_pop(), frame_push(), frame_push_value() (+59 more)

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "mem_blob"
Cohesion: 0.16
Nodes (19): blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_region_alloc(), mem_words_equal(), str_eq_nocase(), editor_pick_buffer() (+11 more)

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

### Community 158 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 159 - "test_primitives_variables.c"
Cohesion: 0.12
Nodes (15): setUp(), tearDown(), test_dots_variable(), test_error_no_value(), test_global_variable(), test_local_declaration(), test_local_with_list(), test_localmake_in_procedure() (+7 more)

### Community 160 - "ensure_wifi_initialized"
Cohesion: 0.18
Nodes (18): ensure_wifi_initialized(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), picocalc_wifi_get_ip(), picocalc_wifi_get_mac() (+10 more)

### Community 161 - "Result"
Cohesion: 0.20
Nodes (10): Result, result_eof(), result_error_in(), result_set_error_proc(), test_result_error_in_non_error_unchanged(), test_result_error_in_preserves_existing_caller(), test_result_error_in_sets_caller(), test_result_set_error_proc_noop_for_non_error() (+2 more)

### Community 162 - "mock_device_set_editor_result"
Cohesion: 0.15
Nodes (30): LogoEditorResult, mock_device_set_editor_content(), mock_device_set_editor_result(), mock_editor_edit(), MockFile, mock_fs_create_file(), mock_fs_get_content(), mock_fs_get_file() (+22 more)

### Community 163 - "Text and Screen Commands"
Cohesion: 0.10
Nodes (18): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, cleartext (ct), cursor (+10 more)

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "value_number"
Cohesion: 0.31
Nodes (26): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+18 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 169 - "value_extract_xy"
Cohesion: 0.25
Nodes (8): value_extract_xy(), test_value_extract_xy_decimal_values(), test_value_extract_xy_fails_on_empty_list(), test_value_extract_xy_fails_on_non_numeric(), test_value_extract_xy_fails_on_single_element_list(), test_value_extract_xy_fails_on_word(), test_value_extract_xy_negative_values(), test_value_extract_xy_success()

### Community 170 - "ip_addr_t"
Cohesion: 0.25
Nodes (8): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 177 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 198 - "value_bool"
Cohesion: 0.27
Nodes (16): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or(), Evaluator (+8 more)

### Community 204 - "logo_console_init"
Cohesion: 0.27
Nodes (9): LogoConsole, LogoStreamOps, logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), test_console_has_screen_modes(), test_console_has_text() (+1 more)

### Community 209 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

## Knowledge Gaps
- **747 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+742 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `test_primitives_properties.c`, `mem_cdr`, `reset_output`, `eval_string`, `proc_get_frame_stack`, `result_none`, `test_trails.c`, `io.c`, `test_primitives_files_directory.c`, `format_buffer_init`, `lexer_init`, `test_primitives_editor.c`, `test_variables.c`, `error_format`, `test_scaffold_setUp`, `test_primitives_control_flow.c`, `test_primitives_json.c`, `output_has`, `test_scaffold.h`, `test_primitives_variables.c`, `mock_device_set_editor_result`, `test_primitives_outside_world.c`, `test_time.c`, `set_mock_input`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_files_load_save.c`, `demons_poll`, `eval_primary`, `proc_define_from_text`, `mem_set_cdr`, `mock_device_get_state`, `repl_evaluate_line`, `test_galaxian.c`, `mem_word_ptr`, `mem_is_nil`, `test_scaffold.c`, `test_checkrun.c`, `test_bench_throughput.c`?**
  _High betweenness centrality (0.114) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `run_string`, `test_primitives_properties.c`, `mem_cdr`, `reset_output`, `proc_get_frame_stack`, `test_trails.c`, `test_primitives_files_directory.c`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `test_httpd.c`, `error_format`, `test_primitives_http.c`, `test_primitives_control_flow.c`, `test_primitives_json.c`, `test_primitives_variables.c`, `test_primitives_wifi.c`, `result_error_arg`, `test_primitives_outside_world.c`, `test_time.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_network.c`, `test_mock_fs.h`, `demons_poll`, `eval_primary`, `proc_define_from_text`, `mem_set_cdr`, `mock_device_get_state`, `test_galaxian.c`, `mem_word_ptr`, `mem_is_nil`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.114) - this node is a cross-community bridge._
- **Why does `mem_word_ptr()` connect `mem_word_ptr` to `run_string`, `test_value.c`, `mem_cdr`, `reset_output`, `eval_string`, `primitives_turtle.c`, `result_none`, `test_primitives_files_directory.c`, `primitives_outside_world.c`, `format_buffer_init`, `test_primitives_editor.c`, `primitives_sound.c`, `test_variables.c`, `test_httpd.c`, `mem_blob`, `test_primitives_http.c`, `test_primitives_json.c`, `test_primitives_variables.c`, `prim_local`, `test_primitives_wifi.c`, `result_error_arg`, `test_primitives_outside_world.c`, `test_time.c`, `set_mock_input`, `prim_savel`, `test_primitives_hardware.c`, `httpd.c`, `test_sound.c`, `test_primitives_network.c`, `memory.c`, `primitives_json.c`, `eval_primary`, `value_bool`, `stdlib.h`, `proc_define_from_text`, `mem_set_cdr`, `mem_cons`, `mem_atom`, `mem_is_nil`, `primitives_http.c`?**
  _High betweenness centrality (0.060) - this node is a cross-community bridge._
- **Are the 949 inferred relationships involving `run_string()` (e.g. with `define_workspace()` and `load_game()`) actually correct?**
  _`run_string()` has 949 INFERRED edges - model-reasoned connections that need verification._
- **Are the 916 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 916 INFERRED edges - model-reasoned connections that need verification._
- **Are the 440 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 440 INFERRED edges - model-reasoned connections that need verification._
- **Are the 232 inferred relationships involving `mem_atom()` (e.g. with `eval_primary()` and `parse_list()`) actually correct?**
  _`mem_atom()` has 232 INFERRED edges - model-reasoned connections that need verification._