# Graph Report - pico-logo  (2026-07-31)

## Corpus Check
- 285 files · ~473,699 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 6975 nodes · 22105 edges · 187 communities (182 shown, 5 thin omitted)
- Extraction: 56% EXTRACTED · 44% INFERRED · 0% AMBIGUOUS · INFERRED: 9766 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `b33d0e71`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- run_string
- lfs.c
- eval_string
- test_value.c
- mem_word_ptr
- reset_output
- test_primitives_words_lists.c
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
- stdlib.h
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
- test_eval.c
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
- screen.c
- picocalc_storage.c
- proc_define_from_text
- lexer.c
- picocalc_flash.c
- test_storage_router.c
- value_number
- host_storage.c
- test_costumes.c
- Introduction
- test_lfs_storage.c
- Code Review — 2026-07-02
- Contributing
- main
- mock_device_get_state
- mem_is_word
- eval.c
- picocalc_read_line
- Design: LittleFS internal filesystem + `/sd` FAT32 mount
- P5 — Multi-sprite turtles and the display pipeline (implemented)
- Arithmetic Operations
- main
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
- result_error
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
- test_trails.c
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
- result_error_arg
- WiFi Management
- primitives_debug_control.c
- The pick of five: plans
- repl_line_starts_with_to
- HTTP Operations
- logo_lfs_backup
- ms_to_datetime
- Variables
- Text and Screen Commands
- The Outside World
- HTTP server (design)
- primitives_hardware.c
- What to flag (in priority order)
- logo_hardware_init
- record_command_float
- test_lfs_backup.c
- err_t
- Atom Garbage Collection: Implementation Plan
- Modifying Procedures Under Program Control
- properties.c
- as_httpd_conn
- Appendix A: Useful Tools
- mbedtls_ms_time
- Appendix B: Parsing
- Open
- Property Lists
- Pico Logo
- primitives_arithmetic.c
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
1. `run_string()` - 920 edges
2. `eval_string()` - 900 edges
3. `mem_word_ptr()` - 444 edges
4. `mem_is_nil()` - 238 edges
5. `mem_atom()` - 235 edges
6. `value_to_string()` - 201 edges
7. `result_error_arg()` - 195 edges
8. `result_none()` - 192 edges
9. `result_ok()` - 176 edges
10. `lexer_init()` - 172 edges

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

## Communities (187 total, 5 thin omitted)

### Community 0 - "run_string"
Cohesion: 0.02
Nodes (216): MockLine, MockTurtleState, mock_device_get_line(), mock_device_get_output(), mock_device_get_turtle(), mock_device_has_line_from_to(), mock_device_line_count(), mock_device_paint_canvas() (+208 more)

### Community 1 - "lfs.c"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "eval_string"
Cohesion: 0.02
Nodes (192): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+184 more)

### Community 3 - "test_value.c"
Cohesion: 0.02
Nodes (175): Node, demons_print(), demons_set(), format_number(), prim_for(), prim_free(), json_format_number(), parse_turtle_set() (+167 more)

### Community 4 - "mem_word_ptr"
Cohesion: 0.04
Nodes (179): number_to_word(), index_to_node(), mem_car(), mem_cdr(), mem_is_nil(), mem_list_append(), mem_word_ptr(), Lexer (+171 more)

### Community 5 - "reset_output"
Cohesion: 0.02
Nodes (175): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+167 more)

### Community 6 - "test_primitives_words_lists.c"
Cohesion: 0.02
Nodes (124): exhaust_atom_table(), exhaust_node_pool(), test_ascii(), test_bar_list_literal_count(), test_bar_list_literal_is_one_word(), test_bar_parse_first_is_whole_word(), test_bar_parse_of_word_containing_bars(), test_bar_protects_bracket_in_list_literal() (+116 more)

### Community 7 - "result_none"
Cohesion: 0.07
Nodes (122): demons_clear(), demons_freeze(), demons_reset(), demons_thaw(), frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set() (+114 more)

### Community 8 - "value_to_string"
Cohesion: 0.11
Nodes (26): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or(), value_to_string() (+18 more)

### Community 9 - "run"
Cohesion: 0.15
Nodes (41): actor(), num(), put_actor(), run(), runf(), test_blocked_actor_discards_the_frame_step(), test_blocked_turn_stays_buffered(), test_blossom_scores_pauses_and_turns_the_tables() (+33 more)

### Community 10 - "iteration_callback"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 11 - "io.c"
Cohesion: 0.08
Nodes (82): eval_instruction(), prim_editfile(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile() (+74 more)

### Community 12 - "test_primitives_files_load_save.c"
Cohesion: 0.08
Nodes (40): mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), setUp_with_turtle(), tearDown_with_turtle() (+32 more)

### Community 13 - "picocalc_console.c"
Cohesion: 0.04
Nodes (71): picocalc_editor_get_ops(), keyboard_set_idle_callback(), lcd_restore_palette(), lcd_set_palette_rgb(), LogoConsole, LogoPen, LogoRotationStyle, LogoStream (+63 more)

### Community 14 - "format_buffer_init"
Cohesion: 0.07
Nodes (84): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_body_indent(), format_buffer_init(), format_buffer_output() (+76 more)

### Community 15 - "lexer_init"
Cohesion: 0.06
Nodes (92): lexer_init(), assert_token(), test_alphanumeric_word(), test_bar_colon_variable(), test_bar_escaped_bar_inside(), test_bar_in_list_context(), test_bar_quoted_word(), test_bar_run_mid_quoted_word() (+84 more)

### Community 16 - "test_frame_arena.c"
Cohesion: 0.07
Nodes (77): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+69 more)

### Community 17 - "test_io.c"
Cohesion: 0.04
Nodes (64): logo_io_cleanup(), logo_io_flush(), logo_io_is_dribbling(), logo_io_parse_network_address(), logo_io_set_prefix(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble() (+56 more)

### Community 18 - "syntax_highlight_line"
Cohesion: 0.06
Nodes (80): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+72 more)

### Community 19 - "test_variables.c"
Cohesion: 0.06
Nodes (73): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing() (+65 more)

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
Cohesion: 0.07
Nodes (43): CaughtError, append_caller_suffix(), Result, error_clear_caught(), error_format(), error_get_caught(), error_message(), error_set_caught() (+35 more)

### Community 24 - "Turtle Graphics"
Cohesion: 0.03
Nodes (67): arc, ask, back (bk), background (bg), clean, cleardemons, clearscreen (cs), colourunder (colorunder) (+59 more)

### Community 25 - "test_scaffold_setUp"
Cohesion: 0.05
Nodes (56): blob_reset(), logo_mem_init(), primitives_control_reset_test_state(), primitives_set_io(), properties_init(), variables_init(), LogoConsole, LogoHardware (+48 more)

### Community 26 - "test_primitives_http.c"
Cohesion: 0.05
Nodes (74): logo_mem_set_aux_region(), mem_is_blob(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_get_tcp_request_len(), mock_device_set_tcp_close_after() (+66 more)

### Community 27 - "fat32.c"
Cohesion: 0.11
Nodes (61): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_delete(), fat32_dir_create() (+53 more)

### Community 28 - "primitives_httpd.c"
Cohesion: 0.19
Nodes (28): httpd_body(), httpd_body_unread(), httpd_method(), httpd_path(), httpd_query(), httpd_remote(), mem_word(), Evaluator (+20 more)

### Community 29 - "mock_device.c"
Cohesion: 0.02
Nodes (58): MockCommandType, MockDot, LogoPen, LogoStream, LogoTurtleRaster, SoundEvent, SoundStatus, WifiState (+50 more)

### Community 30 - "test_primitives_json.c"
Cohesion: 0.07
Nodes (61): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+53 more)

### Community 31 - "test_primitives_conditionals.c"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 32 - "test_scaffold.h"
Cohesion: 0.03
Nodes (67): tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), tearDown(), assert_fullscreen_canvas_hud(), assert_hud_write_positions() (+59 more)

### Community 33 - "test_notation.c"
Cohesion: 0.12
Nodes (33): NotationState, SoundEvent, duration_ms(), notation_parse_token(), notation_state_init(), note_freq(), parse_control(), pitch_class() (+25 more)

### Community 34 - "primitives_init"
Cohesion: 0.11
Nodes (34): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+26 more)

### Community 35 - "stdlib.h"
Cohesion: 0.07
Nodes (25): keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_set_background_poll(), keyboard_set_key_available_callback(), lcd_get_palette_value(), lcd_set_palette_value() (+17 more)

### Community 36 - "primitives.h"
Cohesion: 0.10
Nodes (27): Value, value_is_true(), Evaluator, Result, Value, prim_step(), prim_trace(), prim_unstep() (+19 more)

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
Cohesion: 0.03
Nodes (85): mock_fs_create_dir(), mock_fs_reset(), mock_fs_tearDown(), tearDown(), test_cat_lists_files(), test_cat_runs_without_error(), test_cat_with_invalid_input_error(), test_catalog_long_format_marks_directories() (+77 more)

### Community 42 - "primitives_files_directory.c"
Cohesion: 0.17
Nodes (31): CatalogContext, CatalogEntry, Evaluator, LogoEntryType, LogoIO, Result, Value, catalog_callback() (+23 more)

### Community 43 - "Managing Various Files"
Cohesion: 0.25
Nodes (8): dribble, load, loadpic, Managing Various Files, nodribble, save, savel, savepic

### Community 44 - "test_time.c"
Cohesion: 0.07
Nodes (45): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+37 more)

### Community 45 - "set_mock_input"
Cohesion: 0.13
Nodes (45): LogoIO, repl_cleanup(), repl_count_bracket_balance(), repl_extract_proc_name(), repl_init(), repl_run(), ReplFlags, test_repl_count_bracket_balance_basic() (+37 more)

### Community 46 - "picocalc_editor_edit"
Cohesion: 0.17
Nodes (41): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+33 more)

### Community 47 - "test_eval.c"
Cohesion: 0.05
Nodes (42): test_deep_recursion_100_levels(), test_deep_recursion_addupto(), test_deep_recursion_factorial(), test_deep_recursion_nested_in_expression(), test_deep_recursion_print_result(), test_error_infix_doesnt_like(), test_error_not_enough_inputs(), test_error_uses_alias_name_fd() (+34 more)

### Community 48 - "repository"
Cohesion: 0.04
Nodes (45): name, name, match, name, 1, 2, match, name (+37 more)

### Community 49 - "test_primitives_hardware.c"
Cohesion: 0.05
Nodes (46): tearDown(), test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable() (+38 more)

### Community 50 - "Turtle Trails (design)"
Cohesion: 0.06
Nodes (31): 10. Main loop and state order, 11. Memory and performance budget, 12. Design boundaries, 13. Tests, 14. Implementation milestones, 15. As built: divergences from this design, 1. Theme, 2. Display and board geometry (+23 more)

### Community 51 - "httpd.c"
Cohesion: 0.14
Nodes (34): demons_running(), LogoHardwareOps, Result, Value, check_response_headers(), ci_eq(), close_conn(), header_find() (+26 more)

### Community 52 - "stream.c"
Cohesion: 0.10
Nodes (37): logo_io_check_write_error(), LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error() (+29 more)

### Community 53 - "lcd.c"
Cohesion: 0.10
Nodes (40): dirty_tiles_clear(), repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_blink() (+32 more)

### Community 54 - ";"
Cohesion: 0.04
Nodes (50): ;, and, Appendix C: Useful Procedures, Appendix D: Error Messages, Appendix E: Colour Palette for Pico Logo, ashift, battery, bitand (+42 more)

### Community 55 - "Checkpoint Run — a maze-driving game (design)"
Cohesion: 0.05
Nodes (44): 10.1 Radar, 10.2 HUD, 10.3 Palette, 10.4 Shape slots, 10.5 Sound, 10. Radar, HUD, art, and sound, 11. State machine and frame order, 12. Logo coding constraints (+36 more)

### Community 56 - "test_sound.c"
Cohesion: 0.09
Nodes (34): mock_sound_set_status(), assert_word(), MockDeviceState, Result, snd(), test_env_default(), test_play_appends(), test_play_bad_notation_errors() (+26 more)

### Community 57 - "test_primitives_outside_world.c"
Cohesion: 0.08
Nodes (24): test_pr_abbreviation(), test_print_empty_list(), test_print_list_no_outer_brackets(), test_print_multiple_args(), test_print_nested_list(), test_print_number(), test_print_word(), test_readchar_multiple_calls() (+16 more)

### Community 58 - "picocalc_hardware.c"
Cohesion: 0.08
Nodes (19): cyw43_ev_scan_result_t, WifiState, ensure_wifi_initialized(), mdns_start(), mdns_stop(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname() (+11 more)

### Community 59 - "fat32_close"
Cohesion: 0.14
Nodes (35): repeating_timer_t, fat32_close(), fat32_create(), fat32_is_mounted(), fat32_mount(), fat32_read(), fat32_unmount(), is_valid_fat32_boot_sector() (+27 more)

### Community 60 - "test_primitives_network.c"
Cohesion: 0.10
Nodes (36): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+28 more)

### Community 61 - "memory.c"
Cohesion: 0.10
Nodes (42): demons_gc_mark_all(), Value, mark_value(), op_stack_gc_mark(), alloc_cell(), atom_chain_next(), atom_clear_marks(), atom_entry_is_free() (+34 more)

### Community 62 - "primitive_find"
Cohesion: 0.13
Nodes (16): primitive_find(), primitive_get_by_index(), primitive_get_count(), primitive_register_alias(), LogoIO, logo_random_next(), logo_random_reset(), logo_random_seed() (+8 more)

### Community 63 - "primitives_json.c"
Cohesion: 0.18
Nodes (34): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+26 more)

### Community 64 - "Conditionals and Control of Flow"
Cohesion: 0.06
Nodes (34): catch, co, Conditionals and Control of Flow, do.until, do.while, error, false, for (+26 more)

### Community 65 - "test_mock_fs.h"
Cohesion: 0.07
Nodes (53): assert_word(), LogoDirCallback, fs_list_children(), handle(), pump(), resp_str(), seed_tree(), status_is() (+45 more)

### Community 66 - "demons_poll"
Cohesion: 0.11
Nodes (32): Result, demons_frozen(), demons_maybe_poll(), demons_poll(), logo_io_ticks_ms(), mock_device_clear_output(), setUp(), test_action_does_not_reenter_poll() (+24 more)

### Community 67 - "test_dirty_tiles.c"
Cohesion: 0.12
Nodes (31): dirty_tiles_any(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), picocalc_sleep(), ScreenSprite (+23 more)

### Community 68 - "Words and Lists"
Cohesion: 0.06
Nodes (34): ascii, before? (beforep), butfirst (bf), butlast (bl), char, count, empty? (emptyp), equal? (equalp) (+26 more)

### Community 69 - "op_stack_push"
Cohesion: 0.15
Nodes (29): EvalOp, op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init(), op_stack_is_empty(), op_stack_peek(), op_stack_push() (+21 more)

### Community 70 - "test_primitives_editor.c"
Cohesion: 0.09
Nodes (45): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, mock_file_can_read(), mock_file_close(), mock_file_flush() (+37 more)

### Community 71 - "step_proc_call"
Cohesion: 0.21
Nodes (29): op_stack_pop(), EvalOp, EvalOpKind, Evaluator, Node, Result, UserProcedure, eval_trace_entry() (+21 more)

### Community 72 - "lfs_storage.c"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 73 - "mock_sdcard.c"
Cohesion: 0.12
Nodes (19): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+11 more)

### Community 74 - "screen.c"
Cohesion: 0.09
Nodes (35): lcd_enable_cursor(), lcd_move_cursor(), lcd_set_cursor_char(), text_get_background(), text_get_foreground(), text_set_background(), text_set_cursor(), turtle_dot() (+27 more)

### Community 75 - "picocalc_storage.c"
Cohesion: 0.14
Nodes (27): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStream, file_context_stale(), logo_picocalc_dir_create(), logo_picocalc_dir_exists() (+19 more)

### Community 76 - "proc_define_from_text"
Cohesion: 0.03
Nodes (93): append_to_list(), Lexer, Node, Token, parse_bracket_contents(), proc_define_from_text(), token_to_atom(), test_deep_nested_proc_in_repeat() (+85 more)

### Community 77 - "lexer.c"
Cohesion: 0.16
Nodes (31): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+23 more)

### Community 78 - "picocalc_flash.c"
Cohesion: 0.15
Nodes (15): m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest(), writable_m1(), bd_erase() (+7 more)

### Community 79 - "test_storage_router.c"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 80 - "value_number"
Cohesion: 0.07
Nodes (105): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+97 more)

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

### Community 87 - "main"
Cohesion: 0.17
Nodes (19): Lexer, eval_init(), LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line() (+11 more)

### Community 88 - "mock_device_get_state"
Cohesion: 0.03
Nodes (116): LogoConsole, LogoStreamOps, logo_console_has_editor(), logo_console_has_screen_modes(), logo_console_has_text(), logo_console_has_turtle(), logo_console_init(), MockCommand (+108 more)

### Community 89 - "mem_is_word"
Cohesion: 0.19
Nodes (33): BlobDesc, blob_desc(), Node, mem_atom_unescape(), mem_gc_roots_pop(), mem_gc_roots_push(), mem_is_word(), mem_word_len() (+25 more)

### Community 90 - "eval.c"
Cohesion: 0.26
Nodes (25): EvalOpKind, Evaluator, FrameStack, Node, Result, UserProcedure, Value, eval_call_primitive() (+17 more)

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

### Community 95 - "main"
Cohesion: 0.09
Nodes (24): proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), lfs_t, LogoStorage, logo_lfs_storage_init(), main(), psram_verify() (+16 more)

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
Cohesion: 0.10
Nodes (69): Lexer, Node, Token, TokenType, classify_word(), is_delimiter_token(), is_number_word(), token_source_at_end() (+61 more)

### Community 100 - "host_hardware.c"
Cohesion: 0.09
Nodes (5): LogoHardware, host_network_tcp_connect(), init_winsock(), logo_host_hardware_create(), logo_host_hardware_destroy()

### Community 101 - "sdcard.c"
Cohesion: 0.27
Nodes (19): fat32_init(), sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_init(), sd_read_block() (+11 more)

### Community 102 - "Managing your Workspace"
Cohesion: 0.10
Nodes (21): bury, buryall, buryname, erall, erase (er), ern, erns, erps (+13 more)

### Community 103 - "proc_get_frame_stack"
Cohesion: 0.10
Nodes (41): frame_binding_count(), frame_stack_depth(), frame_stack_used_bytes(), FrameStack, proc_get_frame_stack(), var_get_local_by_index(), var_local_count(), test_stack_init_used_zero() (+33 more)

### Community 104 - "test_help.c"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 105 - "primitives_get_io"
Cohesion: 0.32
Nodes (24): LogoIO, Evaluator, Result, Value, prim_allopen(), prim_close(), prim_closeall(), prim_dribble() (+16 more)

### Community 106 - "test_tls_heap.c"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 107 - "storage_router.c"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 108 - "eval_primary"
Cohesion: 0.19
Nodes (23): eval_at_end(), apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expr_bp() (+15 more)

### Community 109 - "primitives_http.c"
Cohesion: 0.25
Nodes (21): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+13 more)

### Community 110 - "result_error"
Cohesion: 0.32
Nodes (19): Evaluator, Node, Result, Value, flush_writer(), prim_keyp(), prim_print(), prim_readchar() (+11 more)

### Community 111 - "mem_atom"
Cohesion: 0.05
Nodes (102): mem_atom(), mem_cons(), mem_free_nodes(), mem_gc(), mem_is_list(), mem_set_cdr(), mem_word_eq(), Node (+94 more)

### Community 112 - "primitives_workspace.c"
Cohesion: 0.11
Nodes (55): format_property_list(), Evaluator, Result, Value, count_bracket_balance(), prim_edall(), prim_edit(), prim_edn() (+47 more)

### Community 113 - "P9 — Tile maps and smooth scrolling (design)"
Cohesion: 0.08
Nodes (24): 10. Checkpoint Run revamp, 11. Turtle Trails revamp (render-only, gameplay identical), 12. Budgets, 13. Milestones, 14. Tests, 15. Levers if M0 misses, 16. Rejected alternatives, 17. Roadmap gate questions, resolved (+16 more)

### Community 114 - "test_galaxian.c"
Cohesion: 0.20
Nodes (19): assert_num(), assert_true(), seed_convoy(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_diver_breaks_away_near_bottom(), test_file_loads_and_sets_globals(), test_find_flank_walks_inward() (+11 more)

### Community 115 - "Galaxian in Pico Logo (design)"
Cohesion: 0.11
Nodes (18): 10. Main loop, 11. Risks / tuning expectations, 1. What Galaxian is, mechanically, 2. The board, 3. Object representation, 4. The convoy, 5. Divers — the new mechanic, 6. Shot vs. convoy: `colourunder`, not `over?` (+10 more)

### Community 116 - "File Management"
Cohesion: 0.11
Nodes (19): backup, cat, catalog, copyfile, createdir, dir? (dirp), directories, editfile (+11 more)

### Community 117 - "primitives_control_flow.c"
Cohesion: 0.38
Nodes (16): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_forever(), prim_ignore() (+8 more)

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

### Community 122 - "test_trails.c"
Cohesion: 0.18
Nodes (24): mock_device_clear_graphics(), load_trails(), numf(), read_map(), setUp(), step_tile(), test_calm_tiles_do_not_strand_bugs(), test_decoded_counts_match_the_encoded_words() (+16 more)

### Community 123 - "Design: `launch` background processes (P6)"
Cohesion: 0.13
Nodes (15): 10. Milestones, 11. Risks, 12. Decisions (gate closed 2026-07-12), 13. Alternatives rejected, 1. Goals, 2. Prior art (survey in multi-sprite-design.md §3/§8), 3. The model, 4. Feasibility: what the evaluator already gives us, and the one gap (+7 more)

### Community 124 - "test_checkrun.c"
Cohesion: 0.10
Nodes (56): MockStamp, mock_device_get_stamp(), mock_device_stamp_count(), check_world_invariants(), load_checkrun(), num(), numf(), player_at_junction() (+48 more)

### Community 125 - "repl_evaluate_line"
Cohesion: 0.23
Nodes (14): proc_restore_execution_state(), proc_save_execution_state(), Result, name_distance(), repl_evaluate_line(), repl_next_bracket_depth(), repl_restore_refresh(), repl_suggest_name() (+6 more)

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

### Community 131 - "mem_blob"
Cohesion: 0.16
Nodes (19): blob_alloc(), mem_blob(), mem_blob_free_bytes(), mem_blob_used(), mem_region_alloc(), mem_words_equal(), str_eq_nocase(), editor_pick_buffer() (+11 more)

### Community 132 - "southbridge.c"
Cohesion: 0.23
Nodes (18): repeating_timer_t, keyboard_poll(), on_keyboard_timer(), picocalc_get_battery_level(), picocalc_power_off(), sb_available(), sb_is_power_off_supported(), sb_read() (+10 more)

### Community 133 - "result_error_arg"
Cohesion: 0.12
Nodes (47): mem_atom_cstr(), Evaluator, Result, Value, prim_network_ping(), prim_network_resolve(), prim_ntp(), Evaluator (+39 more)

### Community 134 - "WiFi Management"
Cohesion: 0.14
Nodes (14): Example, tls? (tlsp), wifi.connect, wifi.disconnect, wifi.hostname, wifi.ip, wifi.mac, WiFi Management (+6 more)

### Community 135 - "primitives_debug_control.c"
Cohesion: 0.31
Nodes (13): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+5 more)

### Community 136 - "The pick of five: plans"
Cohesion: 0.09
Nodes (22): Documentation, Done — `setpensize` / `pensize`, Implementation refinements (code-review leftovers), Language: big bets, Language: cheap wins (small primitives, high classroom value), Language: medium, P1 — Host REPL stdin + CI, P2 — List utilities: `pick`, `reverse`, `shuffle` (+14 more)

### Community 137 - "repl_line_starts_with_to"
Cohesion: 0.18
Nodes (11): repl_line_is_end(), repl_line_starts_with_to(), load_graphics_demo(), setUp(), test_repl_line_is_end_basic(), test_repl_line_is_end_false_cases(), test_repl_line_is_end_with_whitespace(), test_repl_line_starts_with_to_basic() (+3 more)

### Community 138 - "HTTP Operations"
Cohesion: 0.25
Nodes (8): http.delete, http.get, http.header, HTTP Operations, http.patch, http.post, http.put, http.status

### Community 139 - "logo_lfs_backup"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 140 - "ms_to_datetime"
Cohesion: 0.28
Nodes (13): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), ntp_dns_callback(), ntp_send_request() (+5 more)

### Community 141 - "Variables"
Cohesion: 0.29
Nodes (7): local, localmake, make, name, name? (namep), thing, Variables

### Community 142 - "Text and Screen Commands"
Cohesion: 0.18
Nodes (11): cleartext (ct), cursor, fullscreen (fs), refresh, refreshmode, setcursor, setrefresh, splitscreen (ss) (+3 more)

### Community 143 - "The Outside World"
Cohesion: 0.11
Nodes (19): env, key? (keyp), play, playing? (playingp), print (pr), readchar (rc), readchars (rcs), readlist (rl) (+11 more)

### Community 144 - "HTTP server (design)"
Cohesion: 0.17
Nodes (11): 10. Decisions (resolved with the user), 1. Goal, 2. What already exists, 3. Primitive surface, 4. Execution model: a poll-driven pump, 5. Device interface changes (`devices/hardware.h`), 6. Core structure, 7. mDNS naming (added 2026-07-12) (+3 more)

### Community 145 - "primitives_hardware.c"
Cohesion: 0.44
Nodes (9): Evaluator, Result, Value, prim_battery_level(), prim_bootsel(), prim_goodbye(), prim_toot(), toot_gate_freq() (+1 more)

### Community 146 - "What to flag (in priority order)"
Cohesion: 0.20
Nodes (9): 1. Floating point — single precision only, 2. Static memory footprint, 3. Error handling conventions, 4. Logo semantics, 5. Project conventions, GitHub Copilot Instructions, PR Review Checklist (CRITICAL), What NOT to comment on (+1 more)

### Community 147 - "logo_hardware_init"
Cohesion: 0.22
Nodes (7): LogoHardware, LogoHardwareOps, logo_hardware_init(), LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), test_play_no_sound_engine_is_noop()

### Community 148 - "record_command_float"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 149 - "test_lfs_backup.c"
Cohesion: 0.16
Nodes (23): bd_erase(), bd_prog(), bd_read(), blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read() (+15 more)

### Community 151 - "err_t"
Cohesion: 0.33
Nodes (6): tcp_client_connected_cb(), tcp_client_err_cb(), tcp_client_poll_cb(), tcp_client_recv_cb(), tcp_listener_accept_cb(), err_t

### Community 152 - "Atom Garbage Collection: Implementation Plan"
Cohesion: 0.12
Nodes (15): Alternatives not selected, Atom allocator and collector, Atom Garbage Collection: Implementation Plan, Background: the "atoms are never freed" simplification, Collection behaviour, Documentation updates during implementation, Existing groundwork and prerequisite, Implementation (+7 more)

### Community 153 - "Modifying Procedures Under Program Control"
Cohesion: 0.25
Nodes (8): copydef, define, defined? (definedp), help, Modifying Procedures Under Program Control, primitive? (primitivep), primitives, text

### Community 155 - "properties.c"
Cohesion: 0.21
Nodes (22): Evaluator, Result, Value, prim_erprops(), prim_gprop(), prim_plist(), prim_pprop(), prim_pps() (+14 more)

### Community 156 - "as_httpd_conn"
Cohesion: 0.32
Nodes (8): MockHttpdConn, as_httpd_conn(), httpd_conn_read(), httpd_conn_write(), mock_network_tcp_can_read(), mock_network_tcp_close(), mock_network_tcp_read(), mock_network_tcp_write()

### Community 157 - "Appendix A: Useful Tools"
Cohesion: 0.25
Nodes (8): Appendix A: Useful Tools, arcr and arcl, circler and circlel, divisor?, Graphics Tools, Math Tools, Program Logic or Debugging Tools, sort

### Community 160 - "Appendix B: Parsing"
Cohesion: 0.25
Nodes (8): Appendix B: Parsing, Brackets and Parentheses, Delimiters and Spacing, Infix Procedures, Quotation Marks and Delimiters, The Minus Sign, Vertical Bars, Words

### Community 161 - "Open"
Cohesion: 0.20
Nodes (10): B2 — Single-line `to … end` definitions are not supported, B3 — Demons fire during `load`, B4 — `parse_list` silently drops unknown tokens, B5 — `name_buf[64]` identifier truncation aliasing, B6 — `penreverse` ignores pen size (always 1 px), B7 — A user-procedure call as the left operand of a parenthesised expression corrupts the parse, Bugs, Fixed (+2 more)

### Community 164 - "Property Lists"
Cohesion: 0.29
Nodes (7): erprops, gprop, plist, pprop, pps, Property Lists, remprop

### Community 166 - "Pico Logo"
Cohesion: 0.33
Nodes (5): Building, Features, File Extensions, Installation, Pico Logo

### Community 167 - "primitives_arithmetic.c"
Cohesion: 0.27
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
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `eval_string()` connect `eval_string` to `test_value.c`, `mem_word_ptr`, `reset_output`, `result_error_arg`, `test_primitives_words_lists.c`, `value_to_string`, `run`, `format_buffer_init`, `lexer_init`, `test_httpd.c`, `error_format`, `test_primitives_http.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold.h`, `test_primitives_wifi.c`, `test_primitives_files.c`, `test_time.c`, `test_eval.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_outside_world.c`, `test_primitives_network.c`, `test_mock_fs.h`, `demons_poll`, `proc_define_from_text`, `main`, `mock_device_get_state`, `mem_is_word`, `eval.c`, `proc_get_frame_stack`, `eval_primary`, `mem_atom`, `test_galaxian.c`, `test_scaffold.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.118) - this node is a cross-community bridge._
- **Why does `run_string()` connect `run_string` to `eval_string`, `test_value.c`, `mem_word_ptr`, `reset_output`, `test_primitives_words_lists.c`, `result_none`, `repl_line_starts_with_to`, `run`, `io.c`, `test_primitives_files_load_save.c`, `format_buffer_init`, `lexer_init`, `test_variables.c`, `logo_hardware_init`, `error_format`, `test_scaffold_setUp`, `primitives_httpd.c`, `test_primitives_json.c`, `test_primitives_conditionals.c`, `test_scaffold.h`, `test_primitives_files.c`, `test_time.c`, `set_mock_input`, `test_eval.c`, `test_primitives_hardware.c`, `test_sound.c`, `test_primitives_outside_world.c`, `demons_poll`, `mock_device_set_editor_result`, `test_primitives_editor.c`, `proc_define_from_text`, `main`, `mock_device_get_state`, `eval.c`, `proc_get_frame_stack`, `eval_primary`, `mem_atom`, `test_galaxian.c`, `test_scaffold.c`, `test_trails.c`, `test_checkrun.c`?**
  _High betweenness centrality (0.115) - this node is a cross-community bridge._
- **Why does `mem_word_ptr()` connect `mem_word_ptr` to `run_string`, `eval_string`, `mem_blob`, `test_value.c`, `result_error_arg`, `reset_output`, `primitives_debug_control.c`, `result_none`, `value_to_string`, `test_primitives_words_lists.c`, `io.c`, `format_buffer_init`, `primitives_sound.c`, `test_httpd.c`, `test_primitives_http.c`, `properties.c`, `primitives_httpd.c`, `test_primitives_json.c`, `primitives.h`, `test_primitives_wifi.c`, `test_primitives_files.c`, `primitives_files_directory.c`, `test_time.c`, `set_mock_input`, `test_eval.c`, `test_primitives_hardware.c`, `httpd.c`, `test_sound.c`, `test_primitives_outside_world.c`, `test_primitives_network.c`, `memory.c`, `primitives_json.c`, `test_primitives_editor.c`, `step_proc_call`, `proc_define_from_text`, `mem_is_word`, `test_token_source.c`, `proc_get_frame_stack`, `primitives_get_io`, `eval_primary`, `primitives_http.c`, `result_error`, `mem_atom`, `primitives_workspace.c`?**
  _High betweenness centrality (0.054) - this node is a cross-community bridge._
- **Are the 918 inferred relationships involving `run_string()` (e.g. with `load_checkrun()` and `run()`) actually correct?**
  _`run_string()` has 918 INFERRED edges - model-reasoned connections that need verification._
- **Are the 898 inferred relationships involving `eval_string()` (e.g. with `num()` and `truth()`) actually correct?**
  _`eval_string()` has 898 INFERRED edges - model-reasoned connections that need verification._
- **Are the 438 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 438 INFERRED edges - model-reasoned connections that need verification._
- **Are the 235 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `parse_list()`) actually correct?**
  _`mem_is_nil()` has 235 INFERRED edges - model-reasoned connections that need verification._