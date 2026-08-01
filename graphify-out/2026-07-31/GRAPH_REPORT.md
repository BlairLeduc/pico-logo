# Graph Report - pico-logo  (2026-07-31)

## Corpus Check
- 285 files · ~474,980 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 6991 nodes · 22166 edges · 195 communities (189 shown, 6 thin omitted)
- Extraction: 56% EXTRACTED · 44% INFERRED · 0% AMBIGUOUS · INFERRED: 9809 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `d83de2a4`
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
- run
- iteration_callback
- io.c
- test_primitives_files_load_save.c
- picocalc_console.c
- format_buffer_init
- lexer_init
- test_frame_arena.c
- test_io.c
- syntax_highlight_line
- value_number
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
- primitives_conditionals.c
- test_cross_fs_move.c
- test_primitives_wifi.c
- test_primitives_files.c
- result_error_arg
- test_eval.c
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
- eval_primary
- test_primitives_editor.c
- demons.c
- lfs_storage.c
- mock_sdcard.c
- stdlib.h
- picocalc_storage.c
- proc_define_from_text
- lexer.c
- main
- test_storage_router.c
- test_frame.c
- host_storage.c
- test_costumes.c
- Introduction
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- main
- mock_device_get_state
- screensaver.c
- prim_savel
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- test_trails.c
- Space Invaders in Pico Logo (design & implementation)
- package.json
- test_mklfsimg.c
- test_token_source.c
- host_hardware.c
- sdcard.c
- Managing your Workspace
- format.c
- test_help.c
- primitives_files.c
- test_tls_heap.c
- storage_router.c
- httpd_request_pending
- primitives_http.c
- result_error
- mem_atom
- primitives_workspace.c
- P9 — Tile maps and smooth scrolling (design)
- test_galaxian.c
- Galaxian in Pico Logo (design)
- File Management
- logo_io_set_writer
- test_scaffold.c
- clib.c
- P8 — Sound: a stereo PSG synthesizer (design)
- Input and Output to Files, Network Connections and Devices
- ensure_wifi_initialized
- Design: `launch` background processes (P6)
- test_checkrun.c
- procedures.c
- mklfsimg_lib.c
- roadmap.md
- sound.c
- Using the Logo Editor
- HTTP Server
- run_editor_and_process
- southbridge.c
- mem_atom_cstr
- WiFi Management
- primitives_debug_control.c
- The pick of five: plans
- repl_evaluate_line
- HTTP Operations
- logo_lfs_backup
- ms_to_datetime
- logo_storage_init
- Text and Screen Commands
- The Outside World
- HTTP server (design)
- httpd_listening
- PR Review Checklist (CRITICAL)
- primitives_variables.c
- record_command_float
- test_lfs_backup.c
- fat32_unmount
- Atom Garbage Collection: Implementation Plan
- Modifying Procedures Under Program Control
- MockCommandType
- test_httpd_device.c
- as_httpd_conn
- Appendix A: Useful Tools
- logo_io_init
- Open
- ip_addr_t
- repl.h
- Time Management
- JSON
- Bitwise Operations
- Property Lists
- Pico Logo
- result_ok
- drain_tokens
- Variables
- repl_extract_proc_name
- gen_ca_certs.py
- bd_prog
- pandoc_slug
- repl_line_starts_with_to
- primitives_bitwise.c
- mock_sound_status
- dist.sh
- generate_help.sh
- run_e2e.sh
- VENDOR.md
- picocalc_network_tls_connect
- List Processing

## God Nodes (most connected - your core abstractions)
1. `run_string()` - 927 edges
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

## Communities (195 total, 6 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (209): MockLine, mock_device_get_line(), mock_device_get_output(), mock_device_has_line_from_to(), mock_device_line_count(), mock_device_paint_canvas(), mock_device_set_canvas_point(), mock_device_verify_palette() (+201 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "eval_string"
Cohesion: 0.02
Nodes (183): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+175 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (144): format_number(), Result, result_get_error_code(), result_get_goto_label(), result_get_pause_proc(), result_get_throw_tag(), result_goto(), result_is_ok() (+136 more)

### Community 4 - "mem_is_nil"
Cohesion: 0.04
Nodes (152): BlobDesc, blob_desc(), Node, gc_mark_index(), get_node_ptr(), index_to_node(), mem_car(), mem_cdr() (+144 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (184): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+176 more)

### Community 6 - "mem_word_ptr"
Cohesion: 0.02
Nodes (130): mem_atom_unescape(), mem_word_ptr(), test_atom_unescape_bar_and_backslash_agree(), test_atom_unescape_bar_with_delimiters(), test_atom_unescape_escaped_bar_inside_bars(), test_atom_unescape_strips_backslash(), test_atom_unescape_strips_bars(), test_atom_unescape_too_long_is_error() (+122 more)

### Community 7 - "result_none"
Cohesion: 0.07
Nodes (124): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator, Result, Value (+116 more)

### Community 8 - "value_to_string"
Cohesion: 0.12
Nodes (87): demons_print(), number_to_word(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_list_append(), mem_word_len(), Evaluator, Result (+79 more)

### Community 9 - "run"
Cohesion: 0.15
Nodes (41): actor(), num(), put_actor(), run(), runf(), test_blocked_actor_discards_the_frame_step(), test_blocked_turn_stays_buffered(), test_blossom_scores_pauses_and_turns_the_tables() (+33 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.08
Nodes (73): eval_instruction(), prim_editfile(), LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span(), logo_io_check_freeze_request() (+65 more)

### Community 12 - "test_primitives_files_load_save.c"
Cohesion: 0.06
Nodes (51): var_exists(), mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle() (+43 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.06
Nodes (51): lcd_set_palette_rgb(), LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_flush(), error_output_write() (+43 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.09
Nodes (52): UserProcedure, format_buffer_init(), format_buffer_output(), format_procedure_definition(), format_procedure_title(), format_procedure_to_buffer(), format_variable(), format_variable_to_buffer() (+44 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "test_io.c"
Cohesion: 0.04
Nodes (45): logo_io_parse_network_address(), logo_io_set_prefix(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn(), mock_dir_callback(), mock_list_directory() (+37 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "value_number"
Cohesion: 0.07
Nodes (82): frame_stack_used_bytes(), FrameStack, proc_get_frame_stack(), value_number(), Value, find_global(), var_bury(), var_bury_all() (+74 more)

### Community 20 - "primitives_sound.c"
Cohesion: 0.23
Nodes (25): Evaluator, LogoHardwareOps, LogoIO, Node, Result, SoundEvent, Value, is_noise_voice() (+17 more)

### Community 21 - "unity.c"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 22 - "test_httpd.c"
Cohesion: 0.12
Nodes (30): resp_str(), serve(), test_accessors_error_when_no_request(), test_accessors_report_request_fields(), test_body_is_reported_for_post(), test_client_get_does_not_disturb_pending_request(), test_element_escaped_attribute_value(), test_element_list_content_formats_like_print() (+22 more)

### Community 23 - "error_format"
Cohesion: 0.07
Nodes (43): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+35 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.06
Nodes (46): LogoIO, primitives_control_reset_test_state(), primitives_set_io(), properties_init(), var_erase_all(), variables_init(), LogoHardware, LogoHardwareOps (+38 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.05
Nodes (71): mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_get_tcp_request_len(), mock_device_set_tcp_close_after(), mock_device_set_tcp_connect_result(), mock_device_set_tcp_read_chunk() (+63 more)

### Community 27 - "fat32.c"
Cohesion: 0.14
Nodes (49): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+41 more)

### Community 28 - "primitives_httpd.c"
Cohesion: 0.25
Nodes (24): mem_word(), Evaluator, Result, Value, el_append(), el_append_cstr(), el_append_word(), no_request() (+16 more)

### Community 29 - "mock_device.c"
Cohesion: 0.02
Nodes (45): MockDot, LogoEditorResult, LogoStream, LogoTurtleRaster, SoundEvent, WifiState, mock_device_add_wifi_scan_result(), mock_device_get_dot() (+37 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (62): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+54 more)

### Community 31 - "test_primitives_conditionals.c"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (54): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown() (+46 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.11
Nodes (33): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_events_init(), primitives_exceptions_init() (+25 more)

### Community 35 - "keyboard.c"
Cohesion: 0.10
Nodes (21): repeating_timer_t, keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_poll(), keyboard_set_background_poll(), keyboard_set_key_available_callback() (+13 more)

### Community 36 - "primitives.h"
Cohesion: 0.10
Nodes (28): Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Result, Value, prim_catch(), prim_error() (+20 more)

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
Nodes (59): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result(), mock_device_set_wifi_ssid() (+51 more)

### Community 41 - "test_primitives_files.c"
Cohesion: 0.04
Nodes (46): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+38 more)

### Community 42 - "result_error_arg"
Cohesion: 0.11
Nodes (54): CatalogContext, CatalogEntry, httpd_savebody(), Evaluator, LogoEntryType, LogoIO, Result, Value (+46 more)

### Community 43 - "test_eval.c"
Cohesion: 0.05
Nodes (42): test_deep_recursion_100_levels(), test_deep_recursion_addupto(), test_deep_recursion_factorial(), test_deep_recursion_nested_in_expression(), test_deep_recursion_print_result(), test_error_divide_by_zero_in_procedure(), test_error_dont_know_how(), test_error_not_enough_inputs() (+34 more)

### Community 44 - "test_time.c"
Cohesion: 0.07
Nodes (43): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+35 more)

### Community 45 - "set_mock_input"
Cohesion: 0.19
Nodes (37): LogoIO, repl_cleanup(), repl_init(), repl_run(), ReplFlags, test_repl_defines_proc_with_multiline_paren(), test_repl_error_clears_sync_refresh(), test_repl_error_restores_auto_refresh() (+29 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (42): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+34 more)

### Community 47 - "test_primitives_files_directory.c"
Cohesion: 0.06
Nodes (40): mock_fs_create_dir(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories(), test_catalog_long_format_shows_size(), test_catalog_runs_without_error(), test_catalog_with_absolute_pathname() (+32 more)

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
Cohesion: 0.12
Nodes (35): LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find(), httpd_body() (+27 more)

### Community 52 - "stream.c"
Cohesion: 0.09
Nodes (39): logo_io_check_write_error(), LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error() (+31 more)

### Community 53 - "lcd.c"
Cohesion: 0.11
Nodes (35): repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink(), lcd_cursor_enabled() (+27 more)

### Community 54 - ";"
Cohesion: 0.04
Nodes (47): ;, and, Appendix B: Parsing, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, battery, .bootsel (+39 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.08
Nodes (35): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), tearDown(), test_env_default(), test_play_appends() (+27 more)

### Community 57 - "test_primitives_outside_world.c"
Cohesion: 0.04
Nodes (43): test_keyp_no_input_returns_false(), test_keyp_with_input_returns_true(), test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number() (+35 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.07
Nodes (16): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_sleep(), picocalc_wifi_disconnect() (+8 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (39): fat32_close(), fat32_create(), fat32_delete(), fat32_get_current_dir(), fat32_get_free_space(), fat32_is_ready(), fat32_mount(), fat32_open() (+31 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.11
Nodes (35): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+27 more)

### Community 61 - "memory.c"
Cohesion: 0.07
Nodes (53): alloc_cell(), atom_chain_next(), atom_clear_marks(), atom_entry_is_free(), atom_entry_next(), atom_entry_set_next(), atom_entry_size(), atom_free_add() (+45 more)

### Community 62 - "primitive_find"
Cohesion: 0.22
Nodes (11): primitive_find(), primitive_get_by_index(), primitive_get_count(), primitive_register_alias(), Primitive, test_every_primitive_has_help_entry(), test_primitives_are_registered(), test_primitive_abbreviation_resolves_same_function() (+3 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.17
Nodes (35): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+27 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.06
Nodes (62): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+54 more)

### Community 66 - "demons_poll"
Cohesion: 0.09
Nodes (39): demons_poll(), MockTurtleState, mock_device_clear_output(), mock_device_get_turtle(), tearDown(), test_action_does_not_reenter_poll(), test_cleardemons_disarms_all(), test_cleardemons_leaves_motion_and_freeze() (+31 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.14
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "eval_primary"
Cohesion: 0.05
Nodes (131): EvalOpKind, Evaluator, FrameStack, Lexer, Node, Result, UserProcedure, Value (+123 more)

### Community 70 - "test_primitives_editor.c"
Cohesion: 0.07
Nodes (76): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_set_editor_content(), mock_device_set_editor_result(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, MockFile (+68 more)

### Community 71 - "demons.c"
Cohesion: 0.11
Nodes (27): Node, Result, Value, demons_clear(), demons_freeze(), demons_frozen(), demons_gc_mark_all(), demons_maybe_poll() (+19 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (20): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+12 more)

### Community 74 - "stdlib.h"
Cohesion: 0.06
Nodes (35): lcd_set_background(), screen_fullscreen(), screen_splitscreen(), screen_textscreen(), text_get_background(), text_get_foreground(), text_set_background(), turtle_canvas_point() (+27 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (27): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_file_open() (+19 more)

### Community 76 - "proc_define_from_text"
Cohesion: 0.03
Nodes (100): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), proc_define_from_text(), token_to_atom(), test_deep_nested_proc_in_repeat() (+92 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "main"
Cohesion: 0.10
Nodes (21): main(), psram_verify(), m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest() (+13 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "test_frame.c"
Cohesion: 0.06
Nodes (107): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+99 more)

### Community 81 - "host_storage.c"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

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

### Community 87 - "main"
Cohesion: 0.20
Nodes (17): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+9 more)

### Community 88 - "mock_device_get_state"
Cohesion: 0.03
Nodes (116): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), MockCommand (+108 more)

### Community 89 - "screensaver.c"
Cohesion: 0.12
Nodes (21): picocalc_editor_get_ops(), keyboard_set_idle_callback(), lcd_get_palette_value(), lcd_restore_palette(), lcd_set_palette_value(), LogoConsole, logo_picocalc_console_create(), logo_picocalc_console_destroy() (+13 more)

### Community 90 - "prim_savel"
Cohesion: 0.41
Nodes (15): Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile(), prim_save(), prim_savel() (+7 more)

### Community 91 - "picocalc_read_line"
Cohesion: 0.11
Nodes (29): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+21 more)

### Community 92 - "Design: LittleFS internal filesystem + `/sd` FAT32 mount"
Cohesion: 0.08
Nodes (23): 10. Testing strategy, 11. Phased plan, 12. Decisions (resolved), 1. Goals, 2. Current architecture (baseline), 3. Flash layout — surviving flash-and-debug, 4. The PSRAM / QMI-safe flash-write path (do this FIRST), 5. LittleFS block device + configuration (+15 more)

### Community 93 - "P5 — Multi-sprite turtles and the display pipeline (implemented)"
Cohesion: 0.06
Nodes (31): 10. Budgets, 11. Phasing, 12. Risks and open questions, 13. Rejected alternatives (summary), 1. Where the time goes today, 2.1 Tile-based dirty tracking, 2.2 DMA blit with a pipelined palette-expansion line buffer, 2.3 Refresh policy: automatic by default, manual on request (+23 more)

### Community 94 - "Arithmetic Operations"
Cohesion: 0.09
Nodes (23): abs, arctan, Arithmetic Operations, cos, difference, exp, form, int (+15 more)

### Community 95 - "test_trails.c"
Cohesion: 0.18
Nodes (24): mock_device_clear_graphics(), load_trails(), numf(), read_map(), setUp(), step_tile(), test_calm_tiles_do_not_strand_bugs(), test_decoded_counts_match_the_encoded_words() (+16 more)

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
Nodes (70): Lexer, Node, Token, TokenType, classify_word(), is_comment_node(), is_delimiter_token(), is_number_word() (+62 more)

### Community 101 - "sdcard.c"
Cohesion: 0.27
Nodes (17): sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_read_block(), sd_read_block_once(), sd_read_blocks() (+9 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "format.c"
Cohesion: 0.27
Nodes (22): Node, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_pos(), format_list_contents(), format_list_with_newlines() (+14 more)

### Community 104 - "test_help.c"
Cohesion: 0.13
Nodes (10): help_check_sorted(), help_lookup(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted(), test_help_lookup_alias(), test_help_lookup_case_insensitive() (+2 more)

### Community 105 - "primitives_files.c"
Cohesion: 0.31
Nodes (22): Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble(), prim_filelen() (+14 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "httpd_request_pending"
Cohesion: 0.20
Nodes (23): httpd_request_pending(), mock_httpd_conn_response(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex(), mock_httpd_queue_connection_stalled(), mock_httpd_set_read_hook(), test_accept_returns_connection_and_remote_ip(), pump() (+15 more)

### Community 109 - "primitives_http.c"
Cohesion: 0.24
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

### Community 110 - "result_error"
Cohesion: 0.18
Nodes (27): Evaluator, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar(), prim_readchars() (+19 more)

### Community 111 - "mem_atom"
Cohesion: 0.04
Nodes (116): mem_atom(), mem_cons(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_set_car(), mem_set_cdr(), mem_word_eq() (+108 more)

### Community 112 - "primitives_workspace.c"
Cohesion: 0.16
Nodes (36): help_contains_nocase(), Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add() (+28 more)

### Community 113 - "P9 — Tile maps and smooth scrolling (design)"
Cohesion: 0.08
Nodes (24): 10. Checkpoint Run revamp, 11. Turtle Trails revamp (render-only, gameplay identical), 12. Budgets, 13. Milestones, 14. Tests, 15. Levers if M0 misses, 16. Rejected alternatives, 17. Roadmap gate questions, resolved (+16 more)

### Community 114 - "test_galaxian.c"
Cohesion: 0.18
Nodes (21): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom() (+13 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (19): backup, cat, catalog, copyfile, createdir, dir? (dirp), directories, editfile (+11 more)

### Community 117 - "logo_io_set_writer"
Cohesion: 0.19
Nodes (16): logo_io_is_dribbling(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble(), test_check_write_error(), test_check_write_error_with_dribble(), test_console_write(), test_dribble() (+8 more)

### Community 118 - "test_scaffold.c"
Cohesion: 0.12
Nodes (8): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write()

### Community 119 - "clib.c"
Cohesion: 0.20
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

### Community 125 - "procedures.c"
Cohesion: 0.17
Nodes (17): find_procedure_index(), proc_bury(), proc_bury_all(), proc_clear_tail_call(), proc_push_current(), proc_reset_execution_state(), proc_restore_execution_state(), proc_save_execution_state() (+9 more)

### Community 126 - "mklfsimg_lib.c"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 127 - "roadmap.md"
Cohesion: 0.12
Nodes (14): Build and test, Code structure, Constraints, Graphify, Project, Unit testing, Working guidelines, Build & Test (+6 more)

### Community 128 - "sound.c"
Cohesion: 0.20
Nodes (13): SoundEvent, SoundStatus, is_noise_voice(), __not_in_flash_func(), queue_empty(), queue_free(), sound_gate(), sound_init() (+5 more)

### Community 129 - "Using the Logo Editor"
Cohesion: 0.14
Nodes (14): Block editing, Cursor motion, edall, edit (ed), Editing actions, edn, edns, end (+6 more)

### Community 130 - "HTTP Server"
Cohesion: 0.14
Nodes (14): http.body, http.element, http.listen, http.method, http.path, http.query, http.remote, http.reqheader (+6 more)

### Community 131 - "run_editor_and_process"
Cohesion: 0.31
Nodes (14): Evaluator, Result, Value, count_bracket_balance(), editor_pick_buffer(), prim_edall(), prim_edit(), prim_edn() (+6 more)

### Community 132 - "southbridge.c"
Cohesion: 0.32
Nodes (14): picocalc_get_battery_level(), picocalc_power_off(), sb_is_power_off_supported(), sb_read(), sb_read_battery(), sb_read_keyboard(), sb_read_keyboard_backlight(), sb_read_keyboard_state() (+6 more)

### Community 133 - "mem_atom_cstr"
Cohesion: 0.25
Nodes (22): mem_atom_cstr(), Evaluator, Result, Value, hostname_is_valid(), prim_tls_supported(), prim_wifi_connect(), prim_wifi_connected() (+14 more)

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "primitives_debug_control.c"
Cohesion: 0.31
Nodes (13): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+5 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (22): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P1 — Host REPL stdin + CI, P2 — List utilities: `pick`, `reverse`, `shuffle` (+14 more)

### Community 137 - "repl_evaluate_line"
Cohesion: 0.10
Nodes (26): Result, name_distance(), repl_count_bracket_balance(), repl_evaluate_line(), repl_find_end_token(), repl_line_is_end(), repl_next_bracket_depth(), repl_proc_def_append() (+18 more)

### Community 138 - "HTTP Operations"
Cohesion: 0.25
Nodes (8): http.delete, http.get, http.header, HTTP Operations, http.patch, http.post, http.put, http.status

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 141 - "logo_storage_init"
Cohesion: 0.17
Nodes (13): lfs_t, LogoStorage, logo_lfs_storage_init(), LogoStorage, LogoStorageOps, logo_storage_init(), LogoStorage, LogoStorageOps (+5 more)

### Community 142 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "httpd_listening"
Cohesion: 0.26
Nodes (12): httpd_listening(), mock_httpd_is_listening(), mock_httpd_listen_port(), test_listen_returns_handle_and_records_port(), test_unlisten_drops_pending_connection(), test_clearscreen_leaves_the_listener_running(), test_listen_errors_when_wifi_not_connected(), test_listen_starts_the_server() (+4 more)

### Community 146 - "PR Review Checklist (CRITICAL)"
Cohesion: 0.22
Nodes (8): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on

### Community 147 - "primitives_variables.c"
Cohesion: 0.56
Nodes (8): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing()

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "test_lfs_backup.c"
Cohesion: 0.21
Nodes (17): blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read(), blob_set_read_pos(), blob_write_bytes(), lfs_t (+9 more)

### Community 151 - "fat32_unmount"
Cohesion: 0.18
Nodes (12): repeating_timer_t, fat32_init(), fat32_is_mounted(), fat32_unmount(), on_sd_card_detect(), logo_picocalc_mount_available(), sd_card_present(), sd_init() (+4 more)

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 154 - "MockCommandType"
Cohesion: 0.17
Nodes (12): MockCommandType, LogoPen, mock_turtle_dot(), mock_turtle_get_pen_state(), mock_turtle_set_bg_colour(), mock_turtle_set_pen_colour(), mock_turtle_set_pen_state(), mock_turtle_set_position() (+4 more)

### Community 155 - "test_httpd_device.c"
Cohesion: 0.25
Nodes (7): drain(), setUp(), tearDown(), test_can_read_reflects_available_bytes(), test_dribbled_read_delivers_in_chunks(), test_full_connection_round_trip(), test_two_connections_served_serially()

### Community 156 - "as_httpd_conn"
Cohesion: 0.32
Nodes (8): MockHttpdConn, as_httpd_conn(), httpd_conn_read(), httpd_conn_write(), mock_network_tcp_can_read(), mock_network_tcp_close(), mock_network_tcp_read(), mock_network_tcp_write()

### Community 157 - "Appendix A: Useful Tools"
Cohesion: 0.25
Nodes (8): Appendix A: Useful Tools, arcr and arcl, circler and circlel, divisor?, Graphics Tools, Math Tools, Program Logic or Debugging Tools, sort

### Community 158 - "logo_io_init"
Cohesion: 0.28
Nodes (9): LogoConsole, LogoHardware, LogoStorage, logo_io_cleanup(), logo_io_init(), tearDown(), test_init_no_console(), test_open_no_storage() (+1 more)

### Community 159 - "Open"
Cohesion: 0.22
Nodes (9): B4 — `parse_list` silently drops unknown tokens, B5 — `name_buf[64]` identifier truncation aliasing, B6 — `penreverse` ignores pen size (always 1 px), B7 — A user-procedure call as the left operand of a parenthesised expression corrupts the parse, B9 — `text` returns newline markers as ordinary list elements, Bugs, Fixed, Open (+1 more)

### Community 160 - "ip_addr_t"
Cohesion: 0.25
Nodes (8): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 161 - "repl.h"
Cohesion: 0.33
Nodes (6): Value, result_get_value(), value_get_number(), test_result_get_value_ok(), test_result_get_value_output(), test_value_get_number_returns_payload()

### Community 162 - "Time Management"
Cohesion: 0.33
Nodes (6): date, setdate, settime, ticks, time, Time Management

### Community 163 - "JSON"
Cohesion: 0.33
Nodes (6): JSON, json.array, json.count, json.get, json.make, json.object

### Community 164 - "Bitwise Operations"
Cohesion: 0.29
Nodes (7): ashift, bitand, bitnot, bitor, Bitwise Operations, bitxor, lshift

### Community 165 - "Property Lists"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "result_ok"
Cohesion: 0.20
Nodes (32): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+24 more)

### Community 168 - "drain_tokens"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 169 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 170 - "repl_extract_proc_name"
Cohesion: 0.33
Nodes (6): repl_extract_proc_name(), test_repl_extract_proc_name_basic(), test_repl_extract_proc_name_buffer_limit(), test_repl_extract_proc_name_no_name(), test_repl_extract_proc_name_with_inputs(), test_repl_extract_proc_name_with_whitespace()

### Community 172 - "gen_ca_certs.py"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 173 - "bd_prog"
Cohesion: 0.47
Nodes (6): bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t

### Community 174 - "pandoc_slug"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 176 - "repl_line_starts_with_to"
Cohesion: 0.40
Nodes (5): repl_line_starts_with_to(), test_repl_line_starts_with_to_basic(), test_repl_line_starts_with_to_false_cases(), test_repl_line_starts_with_to_just_to(), test_repl_line_starts_with_to_with_whitespace()

### Community 177 - "primitives_bitwise.c"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 194 - "picocalc_network_tls_connect"
Cohesion: 0.33
Nodes (9): picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect(), poll_lwip_with_timeout(), tcp_connect_and_wait(), tcp_resolve_addr(), tcp_state_alloc(), tls_client_config() (+1 more)

### Community 197 - "List Processing"
Cohesion: 0.22
Nodes (9): apply, crossmap, filter, find, foreach, List Processing, map, map.se (+1 more)

## Knowledge Gaps
- **725 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+720 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `run_string` to `eval_string`, `mem_is_nil`, `reset_output`, `mem_word_ptr`, `result_none`, `run`, `io.c`, `test_primitives_files_load_save.c`, `format_buffer_init`, `lexer_init`, `value_number`, `error_format`, `test_scaffold_setUp`, `primitives_httpd.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold.h`, `test_primitives_files.c`, `test_eval.c`, `test_time.c`, `set_mock_input`, `test_primitives_files_directory.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_outside_world.c`, `demons_poll`, `eval_primary`, `test_primitives_editor.c`, `proc_define_from_text`, `mock_device_get_state`, `test_trails.c`, `mem_atom`, `test_galaxian.c`, `test_scaffold.c`, `test_checkrun.c`, `procedures.c`?**
  _High betweenness centrality (0.117) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `eval_string` to `mem_is_nil`, `reset_output`, `mem_word_ptr`, `value_to_string`, `run`, `format_buffer_init`, `lexer_init`, `httpd_listening`, `value_number`, `test_httpd.c`, `error_format`, `test_primitives_http.c`, `primitives_httpd.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold.h`, `test_primitives_wifi.c`, `test_primitives_files.c`, `test_eval.c`, `test_time.c`, `test_primitives_files_directory.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_outside_world.c`, `test_primitives_network.c`, `memory.c`, `test_mock_fs.h`, `demons_poll`, `eval_primary`, `proc_define_from_text`, `mock_device_get_state`, `httpd_request_pending`, `mem_atom`, `test_galaxian.c`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.111) - this node is a cross-community bridge._
- **Why does `mem_word_ptr()` connect `mem_word_ptr` to `run_string`, `eval_string`, `run_editor_and_process`, `mem_is_nil`, `mem_atom_cstr`, `test_value.c`, `primitives_debug_control.c`, `value_to_string`, `result_none`, `reset_output`, `io.c`, `format_buffer_init`, `value_number`, `primitives_sound.c`, `test_httpd.c`, `test_primitives_http.c`, `primitives_httpd.c`, `test_primitives_json.c`, `test_primitives_wifi.c`, `test_primitives_files.c`, `result_error_arg`, `test_eval.c`, `test_time.c`, `set_mock_input`, `test_primitives_files_directory.c`, `test_primitives_hardware.c`, `httpd.c`, `test_sound.c`, `test_primitives_outside_world.c`, `test_primitives_network.c`, `memory.c`, `primitives_json.c`, `eval_primary`, `test_primitives_editor.c`, `demons.c`, `proc_define_from_text`, `prim_savel`, `test_token_source.c`, `format.c`, `primitives_files.c`, `httpd_request_pending`, `primitives_http.c`, `result_error`, `mem_atom`, `primitives_workspace.c`?**
  _High betweenness centrality (0.052) - this node is a cross-community bridge._
- **Are the 925 inferred relationships involving `run_string()` (e.g. with `load_checkrun()` and `run()`) actually correct?**
  _`run_string()` has 925 INFERRED edges - model-reasoned connections that need verification._
- **Are the 898 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 898 INFERRED edges - model-reasoned connections that need verification._
- **Are the 438 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 438 INFERRED edges - model-reasoned connections that need verification._
- **Are the 235 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `parse_list()`) actually correct?**
  _`mem_is_nil()` has 235 INFERRED edges - model-reasoned connections that need verification._