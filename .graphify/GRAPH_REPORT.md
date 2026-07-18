# Graph Report - .  (2026-07-18)

## Corpus Check
- 275 files · ~418,839 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 5865 nodes · 20029 edges · 157 communities (151 shown, 6 thin omitted)
- Extraction: 53% EXTRACTED · 47% INFERRED · 0% AMBIGUOUS · INFERRED: 9339 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Turtle Graphics Tests
- LittleFS Block Filesystem
- Arithmetic/Bitwise/Logical Tests
- Workspace & Control-Flow Tests
- Value Type & Tests
- List/Word Memory Primitives
- Words/Lists Primitive Tests
- Turtle Primitives & Frame Sync
- Text Primitives & Mock Device
- Mock Device Command Recording
- Memory/GC Primitive Tests
- Frame Stack Management
- Error Handling
- Procedure & Debug-Control Tests
- Procedure Definition Formatting
- Lexer Tests
- Frame Arena Allocator
- Syntax Highlighting
- Device I/O Abstraction
- HTTP Daemon Tests
- HTTP Primitive Tests
- Token Source Abstraction
- File Read/Write Primitives
- Variable Get/Set
- Unity Test Framework
- File Load/Save & Directory Tests
- PicoCalc Console Rendering
- JSON Primitive Tests
- File & Directory Primitives
- Word/List Result Primitives
- I/O Stream Tests
- Editor Primitive Tests
- Conditional Primitive Tests
- Test Scaffold Setup
- FAT32 Filesystem Driver
- Lexer Fuzz Tests
- Procedure Core Headers
- REPL Tests
- Cross-Filesystem Move
- Primitive Registry Init
- LCD Driver
- Time/Date Primitives
- Hardware/Battery Primitives
- HTTP Daemon Core
- WiFi Primitive Tests
- File Info Primitive Tests
- Node Memory Allocator
- PicoCalc Text Editor
- Logo Syntax Grammar (TextMate)
- WiFi/Network Primitives
- FAT32 Mount Tests
- Stream Abstraction
- Screen Rendering & Dirty Tiles
- PicoCalc Device Main/Keyboard
- Frame Stack Introspection
- PicoCalc Hardware/TLS Init
- Network Primitive Tests (NTP/Ping)
- JSON Primitives
- Console History & Input
- Mock Filesystem Test Helper
- Eval Operand Stack
- Workspace Primitives
- Dirty-Tile Rectangle Tracking
- Primitive Help Coverage Tests
- Arithmetic & Random Primitives
- Variable Scope Tests
- Eval Steps (Procedure Calls)
- LittleFS Storage Device
- Mock SD Card & FAT32 Image
- Editor Storage Integration Tests
- Procedure/Time Primitive Definitions
- Host Storage Device
- PicoCalc Storage Device
- Demon Polling Tests
- Lexer Core
- HTTP Element Primitives
- Storage Router Tests
- Trampoline Evaluator Core
- Outside-World I/O Primitives
- I/O Writer/Dribble Management
- LittleFS Storage Tests
- Multi-Sprite Compositor Design
- Host Hardware Networking
- Sprite Costume System
- Expression Evaluator
- HTTP Request Primitives
- REPL Line Evaluation
- VS Code Extension Manifest
- Galaxian Game Tests
- LittleFS Backup/Restore Tests
- mklfsimg Round-Trip Tests
- SD Card Driver
- Property List Primitive Tests
- Test Scaffold Stream Mocks
- Device Main/Init Entry Points
- Help Lookup System
- WiFi/TLS Network Connect
- TLS Heap Allocator
- Storage Router
- LittleFS Image Build Tool
- File Load/Save Primitives
- Editor Invocation Primitives
- Screensaver
- Flash Self-Test
- Memory Reclamation Design Docs
- Control-Flow Primitives
- POSIX-Style clib Shims
- HTTP/Launch/Demon Design Docs
- Higher-Order List Primitives
- Host Console Device
- Southbridge (Keyboard/Battery) Driver
- Audio/PWM Playback
- Recursive Procedure Tests
- Debug-Control Primitives
- Property List Primitives
- Storage Subsystem Init
- Reference Manual Primitives (Misc)
- FAT32 Mount/Detect Tests
- Mock Turtle Command Recording
- Exception Handling Tests
- Project Documentation Index
- Conditional Primitives
- LittleFS Backup Device
- RTC Date/Time Conversion
- GC Mark-Sweep Roots
- Bitwise Primitives
- Build & CI Configuration Docs
- PSG Sound Synthesizer Design
- Variable Primitives
- NTP/DNS/Ping Callbacks
- Mock HTTP Connection
- Lexer Fuzz Edge Cases
- LittleFS Block-Device Helpers
- Galaxian Design & Primitives
- CA Cert Generator Script
- Reference Anchor Checker Script
- Frame Stack Iteration
- Comment-Preserving Lexer Tests
- Distribution Build Script
- Help Doc Generator Script
- End-to-End Test Runner
- Squiral Demo Image
- Color Palette Reference Image
- equal? Primitive Doc

## God Nodes (most connected - your core abstractions)
1. `eval_string()` - 858 edges
2. `run_string()` - 858 edges
3. `mem_word_ptr()` - 415 edges
4. `mem_is_nil()` - 233 edges
5. `mem_atom()` - 227 edges
6. `value_to_string()` - 187 edges
7. `result_error_arg()` - 184 edges
8. `result_none()` - 181 edges
9. `result_ok()` - 171 edges
10. `value_number()` - 165 edges

## Surprising Connections (you probably didn't know these)
- `setUp()` --calls--> `logo_mem_init()`  [INFERRED]
  tests/test_token_source.c → core/memory.c
- `setUp()` --calls--> `logo_mem_init()`  [INFERRED]
  tests/test_value.c → core/memory.c
- `test_nil_is_nil()` --calls--> `mem_is_nil()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_nil_is_not_word()` --calls--> `mem_is_word()`  [INFERRED]
  tests/test_memory.c → core/memory.c
- `test_value_number_content()` --calls--> `value_number()`  [INFERRED]
  tests/test_value.c → core/value.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Shared Demon Poll-Site Scheduling Across P5–P8** — concept_demon_system, docs_launch_design, docs_http_server_design, docs_sound_design, docs_multi_sprite_design [INFERRED 0.85]
- **Games Validating the P5 Sprite Stack and P8 Sound Engine** — docs_space_invaders_design, docs_galaxian_design, docs_multi_sprite_design, docs_sound_design [INFERRED 0.85]
- **Design-Gate-Before-Implementation Roadmap Pattern** — docs_multi_sprite_design, docs_launch_design, docs_http_server_design, docs_sound_design, docs_improvements_roadmap [INFERRED 0.75]

## Communities (157 total, 6 thin omitted)

### Community 0 - "Turtle Graphics Tests"
Cohesion: 0.02
Nodes (203): MockTurtleState, mock_device_clear_graphics(), mock_device_get_output(), mock_device_get_turtle(), mock_device_has_line_from_to(), mock_device_paint_canvas(), mock_device_set_canvas_point(), mock_device_verify_palette() (+195 more)

### Community 1 - "LittleFS Block Filesystem"
Cohesion: 0.06
Nodes (184): lfs1_dir_t, lfs1_entry_t, lfs_cache_t, lfs_dir_t, lfs_file_t, lfs_gstate_t, lfs_mdir_t, lfs_soff_t (+176 more)

### Community 2 - "Arithmetic/Bitwise/Logical Tests"
Cohesion: 0.02
Nodes (175): test_abs_decimal(), test_abs_negative(), test_abs_positive(), test_abs_zero(), test_arctan(), test_arctan_too_many_inputs(), test_arctan_two_input(), test_arctan_two_input_vertical() (+167 more)

### Community 3 - "Workspace & Control-Flow Tests"
Cohesion: 0.02
Nodes (172): proc_is_stepped(), proc_is_traced(), test_rerandom_affects_pick_and_shuffle(), test_comment_in_procedure(), test_comment_inline(), test_comment_with_list(), test_comment_with_word(), test_do_until_basic() (+164 more)

### Community 4 - "Value Type & Tests"
Cohesion: 0.03
Nodes (156): format_number(), json_format_number(), prim_settime(), Node, Result, Value, extract_number_list(), Result (+148 more)

### Community 5 - "List/Word Memory Primitives"
Cohesion: 0.05
Nodes (138): mem_car(), mem_cdr(), mem_is_nil(), mem_is_word(), mem_word_ptr(), prim_for(), Evaluator, Result (+130 more)

### Community 6 - "Words/Lists Primitive Tests"
Cohesion: 0.02
Nodes (116): exhaust_atom_table(), exhaust_node_pool(), test_ascii(), test_beforep_false(), test_beforep_stays_case_sensitive(), test_beforep_true(), test_butfirst_empty_list_error(), test_butfirst_empty_word_error() (+108 more)

### Community 7 - "Turtle Primitives & Frame Sync"
Cohesion: 0.08
Nodes (110): frame_sync_active(), frame_sync_period(), frame_sync_reset(), frame_sync_set(), frame_sync_wait_ms(), Evaluator, Result, Value (+102 more)

### Community 8 - "Text Primitives & Mock Device"
Cohesion: 0.03
Nodes (109): MockCommand, MockDeviceState, MockLine, LogoConsole, mock_device_clear_commands(), mock_device_command_count(), mock_device_dot_count(), mock_device_get_command() (+101 more)

### Community 9 - "Mock Device Command Recording"
Cohesion: 0.03
Nodes (52): MockCommandType, MockDot, LogoPen, LogoStream, LogoTurtleRaster, mock_device_add_wifi_scan_result(), mock_device_get_dot(), mock_device_set_input() (+44 more)

### Community 10 - "Memory/GC Primitive Tests"
Cohesion: 0.06
Nodes (95): mem_atom(), mem_cons(), mem_free_atoms(), mem_free_nodes(), mem_gc(), mem_is_list(), mem_total_atoms(), mem_word_eq() (+87 more)

### Community 11 - "Frame Stack Management"
Cohesion: 0.08
Nodes (88): Binding, FrameHeader, FrameStack, UserProcedure, Value, word_offset_t, calc_frame_size(), frame_add_local() (+80 more)

### Community 12 - "Error Handling"
Cohesion: 0.03
Nodes (84): append_caller_suffix(), Result, error_clear_caught(), error_format(), error_message(), error_set_caught(), test_error_format_cant_from_editor(), test_error_format_cant_open_network() (+76 more)

### Community 13 - "Procedure & Debug-Control Tests"
Cohesion: 0.03
Nodes (84): proc_define_from_text(), test_deep_nested_proc_in_repeat(), test_many_iterations_proc_in_repeat_within_procedure(), test_proc_call_followed_by_commands_in_repeat(), test_proc_call_in_if_within_procedure(), test_proc_call_in_repeat_within_procedure(), test_proc_expr_in_run_within_procedure(), test_co_at_toplevel() (+76 more)

### Community 14 - "Procedure Definition Formatting"
Cohesion: 0.07
Nodes (82): Node, UserProcedure, Value, format_body_element(), format_body_element_multiline(), format_buffer_init(), format_buffer_output(), format_buffer_pos() (+74 more)

### Community 15 - "Lexer Tests"
Cohesion: 0.07
Nodes (83): lexer_init(), assert_token(), test_alphanumeric_word(), test_binary_minus_after_colon_no_space(), test_binary_minus_after_right_bracket_no_space(), test_binary_minus_after_word_no_space(), test_binary_minus_spacing(), test_complex_expression() (+75 more)

### Community 16 - "Frame Arena Allocator"
Cohesion: 0.07
Nodes (76): arena_alloc_words(), arena_available(), arena_available_bytes(), arena_capacity(), arena_capacity_bytes(), arena_extend(), arena_free_to(), arena_init() (+68 more)

### Community 17 - "Syntax Highlighting"
Cohesion: 0.07
Nodes (74): bracket_category(), SyntaxCategory, ci_eq(), is_delimiter(), match_keyword(), read_word_span(), scan_comment(), scan_number() (+66 more)

### Community 18 - "Device I/O Abstraction"
Cohesion: 0.07
Nodes (77): eval_instruction(), logo_console_has_text(), LogoDirCallback, LogoIO, LogoStream, SyntaxCategory, create_network_stream(), highlight_write_span() (+69 more)

### Community 19 - "HTTP Daemon Tests"
Cohesion: 0.06
Nodes (72): httpd_listening(), httpd_request_pending(), httpd_reset(), mock_httpd_conn_response(), mock_httpd_is_listening(), mock_httpd_listen_port(), mock_httpd_queue_connection(), mock_httpd_queue_connection_ex() (+64 more)

### Community 20 - "HTTP Primitive Tests"
Cohesion: 0.05
Nodes (72): blob_reset(), logo_mem_set_aux_region(), mock_device_get_last_tcp_ip(), mock_device_get_last_tcp_port(), mock_device_get_last_tls_host(), mock_device_get_tcp_request(), mock_device_get_tcp_request_len(), mock_device_set_tcp_close_after() (+64 more)

### Community 21 - "Token Source Abstraction"
Cohesion: 0.09
Nodes (70): Lexer, Node, Token, TokenType, classify_word(), is_delimiter_token(), is_number_word(), token_source_at_end() (+62 more)

### Community 22 - "File Read/Write Primitives"
Cohesion: 0.04
Nodes (69): value_is_word(), value_to_string(), test_false(), test_if_false_operation_returns_value(), test_if_operation_nested(), test_if_true_operation_returns_value(), test_ifelse_false_operation_returns_value(), test_ifelse_nested() (+61 more)

### Community 23 - "Variable Get/Set"
Cohesion: 0.07
Nodes (66): Value, find_global(), var_bury(), var_bury_all(), var_declare_local(), var_erase(), var_erase_all(), var_erase_all_globals() (+58 more)

### Community 24 - "Unity Test Framework"
Cohesion: 0.12
Nodes (65): IsStringInBiggerString(), UnityAddMsgIfSpecified(), UnityAssertBits(), UnityAssertDoublesNotWithin(), UnityAssertDoubleSpecial(), UnityAssertDoublesWithin(), UnityAssertEqualIntArray(), UnityAssertEqualMemory() (+57 more)

### Community 25 - "File Load/Save & Directory Tests"
Cohesion: 0.05
Nodes (64): mock_device_get_gfx_load_call_count(), mock_device_get_gfx_save_call_count(), mock_device_get_last_gfx_load_filename(), mock_device_get_last_gfx_save_filename(), mock_device_set_gfx_load_result(), mock_device_set_gfx_save_result(), mock_fs_create_dir(), test_catalog_runs_without_error() (+56 more)

### Community 26 - "PicoCalc Console Rendering"
Cohesion: 0.07
Nodes (53): LogoPen, LogoRotationStyle, LogoStream, LogoTurtleRaster, ScreenSprite, error_output_flush(), error_output_write(), heading_faces_left() (+45 more)

### Community 27 - "JSON Primitive Tests"
Cohesion: 0.07
Nodes (61): assert_empty(), assert_number(), assert_word(), Result, make_doc(), test_array_index_is_one_based(), test_array_of_objects(), test_boolean_true() (+53 more)

### Community 28 - "File & Directory Primitives"
Cohesion: 0.11
Nodes (56): Evaluator, Result, Value, Evaluator, LogoEntryType, Result, Value, catalog_callback() (+48 more)

### Community 29 - "Word/List Result Primitives"
Cohesion: 0.20
Nodes (54): BlobDesc, number_to_word(), blob_desc(), mem_list_append(), mem_word_len(), Evaluator, Node, Result (+46 more)

### Community 30 - "I/O Stream Tests"
Cohesion: 0.05
Nodes (34): logo_io_parse_network_address(), LogoDirCallback, LogoEntryType, LogoStream, dribble_flush_fn(), mock_dir_callback(), mock_list_directory(), mock_open() (+26 more)

### Community 31 - "Editor Primitive Tests"
Cohesion: 0.08
Nodes (48): mock_device_clear_editor(), mock_device_get_editor_input(), mock_device_was_editor_called(), LogoDirCallback, LogoStream, mock_file_can_read(), mock_file_close(), mock_file_flush() (+40 more)

### Community 32 - "Conditional Primitive Tests"
Cohesion: 0.04
Nodes (51): test_if_false_case_insensitive(), test_if_false_one_list_command(), test_if_false_two_lists_command(), test_if_list_predicate_error(), test_if_list_with_empty_list_arg(), test_if_list_with_output(), test_if_list_with_print_empty_then_stop(), test_if_list_with_stop() (+43 more)

### Community 33 - "Test Scaffold Setup"
Cohesion: 0.06
Nodes (52): logo_mem_init(), LogoIO, primitives_control_reset_test_state(), primitives_set_io(), properties_init(), variables_init(), LogoHardware, LogoHardwareOps (+44 more)

### Community 34 - "FAT32 Filesystem Driver"
Cohesion: 0.14
Nodes (49): allocate_and_link_cluster(), fat32_error_t, clear_cluster(), cluster_to_sector(), delete_entry(), dir_offset_to_location(), fat32_dir_create(), fat32_dir_read() (+41 more)

### Community 35 - "Lexer Fuzz Tests"
Cohesion: 0.07
Nodes (50): lexer_next_token(), lexer_token_text(), assert_token_type(), TokenType, test_digit_starting_word(), test_fuzz_all_operators_consecutive(), test_fuzz_backslash_before_delimiter(), test_fuzz_binary_mixed_with_delimiters() (+42 more)

### Community 36 - "Procedure Core Headers"
Cohesion: 0.09
Nodes (28): CaughtError, Value, demons_freeze(), demons_thaw(), value_is_true(), error_get_caught(), Evaluator, Result (+20 more)

### Community 37 - "REPL Tests"
Cohesion: 0.14
Nodes (44): LogoIO, repl_cleanup(), repl_extract_proc_name(), repl_init(), repl_line_starts_with_to(), repl_run(), ReplFlags, test_repl_error_clears_sync_refresh() (+36 more)

### Community 38 - "Cross-Filesystem Move"
Cohesion: 0.08
Nodes (36): logo_io_copy_file(), MemFile, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t (+28 more)

### Community 39 - "Primitive Registry Init"
Cohesion: 0.07
Nodes (44): primitives_arithmetic_init(), primitives_bitwise_init(), primitives_conditionals_init(), primitives_control_flow_init(), primitives_debug_control_init(), primitives_debug_init(), primitives_editor_init(), primitives_events_init() (+36 more)

### Community 40 - "LCD Driver"
Cohesion: 0.10
Nodes (43): picocalc_editor_get_ops(), repeating_timer_t, decode_char(), lcd_blit(), lcd_blit_begin(), lcd_blit_end(), lcd_clear_screen(), lcd_cursor_enabled() (+35 more)

### Community 41 - "Time/Date Primitives"
Cohesion: 0.07
Nodes (45): mock_device_set_time(), mock_device_set_time_enabled(), test_date_and_setdate_roundtrip(), test_date_error_when_not_available(), test_date_outputs_correct_day(), test_date_outputs_correct_month(), test_date_outputs_correct_year(), test_date_outputs_different_values() (+37 more)

### Community 42 - "Hardware/Battery Primitives"
Cohesion: 0.06
Nodes (45): test_battery_charging(), test_battery_charging_in_procedure(), test_battery_in_procedure(), test_battery_level_empty(), test_battery_level_full(), test_battery_level_partial(), test_battery_level_unavailable(), test_battery_not_charging() (+37 more)

### Community 43 - "HTTP Daemon Core"
Cohesion: 0.09
Nodes (43): Result, demons_frozen(), demons_maybe_poll(), demons_reset(), demons_running(), LogoHardwareOps, Result, Value (+35 more)

### Community 44 - "WiFi Primitive Tests"
Cohesion: 0.07
Nodes (44): mock_device_clear_wifi_scan_results(), mock_device_get_hostname(), mock_device_set_wifi_connect_result(), mock_device_set_wifi_connected(), mock_device_set_wifi_ip(), mock_device_set_wifi_mac(), mock_device_set_wifi_scan_result(), mock_device_set_wifi_ssid() (+36 more)

### Community 45 - "File Info Primitive Tests"
Cohesion: 0.04
Nodes (43): test_append_to_file(), test_close_file(), test_close_invalid_input(), test_close_unopened_file_error(), test_dribble_starts(), test_filelen_empty_file(), test_filelen_invalid_input(), test_filelen_returns_size() (+35 more)

### Community 46 - "Node Memory Allocator"
Cohesion: 0.08
Nodes (41): httpd_buf_init(), alloc_cell(), atom_entry_next(), atom_entry_set_next(), atom_hash(), blob_alloc(), blob_free(), Node (+33 more)

### Community 47 - "PicoCalc Text Editor"
Cohesion: 0.16
Nodes (41): LogoEditorResult, editor_backspace(), editor_compute_depth_at_line(), editor_copy_line(), editor_copy_selection(), editor_count_lines(), editor_cut_line(), editor_decrease_indent() (+33 more)

### Community 48 - "Logo Syntax Grammar (TextMate)"
Cohesion: 0.05
Nodes (42): name, name, 1, 2, match, name, match, name (+34 more)

### Community 49 - "WiFi/Network Primitives"
Cohesion: 0.10
Nodes (40): Node, demons_print(), demons_set(), mem_atom_cstr(), prim_http_header(), Evaluator, Result, Value (+32 more)

### Community 50 - "FAT32 Mount Tests"
Cohesion: 0.14
Nodes (39): fat32_close(), fat32_create(), fat32_delete(), fat32_get_current_dir(), fat32_get_free_space(), fat32_is_ready(), fat32_mount(), fat32_open() (+31 more)

### Community 51 - "Stream Abstraction"
Cohesion: 0.10
Nodes (38): LogoStream, screen_gfx_load(), screen_gfx_save(), LogoStream, LogoStreamOps, logo_stream_can_read(), logo_stream_clear_write_error(), logo_stream_close() (+30 more)

### Community 52 - "Screen Rendering & Dirty Tiles"
Cohesion: 0.07
Nodes (36): lcd_set_background(), lcd_set_foreground(), screen_fullscreen(), screen_splitscreen(), screen_textscreen(), text_get_background(), text_get_foreground(), text_set_background() (+28 more)

### Community 53 - "PicoCalc Device Main/Keyboard"
Cohesion: 0.08
Nodes (24): audio_init(), repeating_timer_t, keyboard_get_key(), keyboard_init(), keyboard_key_available(), keyboard_peek_key(), keyboard_poll(), keyboard_set_background_poll() (+16 more)

### Community 54 - "Frame Stack Introspection"
Cohesion: 0.08
Nodes (38): frame_pop(), frame_stack_available_bytes(), frame_stack_depth(), frame_stack_init(), frame_stack_is_empty(), frame_stack_used_bytes(), FrameStack, proc_get_frame_stack() (+30 more)

### Community 55 - "PicoCalc Hardware/TLS Init"
Cohesion: 0.07
Nodes (15): cyw43_ev_scan_result_t, LogoHardware, logo_picocalc_hardware_create(), logo_picocalc_hardware_destroy(), mbedtls_ms_time(), mdns_stop(), picocalc_wifi_disconnect(), tcp_client_connected_cb() (+7 more)

### Community 56 - "Network Primitive Tests (NTP/Ping)"
Cohesion: 0.11
Nodes (34): mock_device_get_last_ntp_server(), mock_device_get_last_ntp_timezone(), mock_device_get_last_ping_ip(), mock_device_get_last_resolve_hostname(), mock_device_set_ntp_result(), mock_device_set_ping_result(), mock_device_set_resolve_result(), test_http_get_dns_failure_errors() (+26 more)

### Community 57 - "JSON Primitives"
Cohesion: 0.18
Nodes (34): Evaluator, Node, Result, Value, enter_array(), enter_object(), extract_value(), hex_val() (+26 more)

### Community 58 - "Console History & Input"
Cohesion: 0.11
Nodes (29): history_add(), history_get(), history_get_start_index(), history_is_empty(), history_is_end_index(), history_next_index(), history_next_matching(), history_prev_index() (+21 more)

### Community 59 - "Mock Filesystem Test Helper"
Cohesion: 0.10
Nodes (33): LogoDirCallback, LogoStream, MockFile, mock_file_can_read(), mock_file_close(), mock_file_flush(), mock_file_get_length(), mock_file_get_read_pos() (+25 more)

### Community 60 - "Eval Operand Stack"
Cohesion: 0.15
Nodes (29): EvalOp, op_stack_alloc_prim_args(), op_stack_depth(), op_stack_get_prim_args(), op_stack_init(), op_stack_is_empty(), op_stack_peek(), op_stack_push() (+21 more)

### Community 61 - "Workspace Primitives"
Cohesion: 0.18
Nodes (31): Evaluator, LogoIO, Node, Result, UserProcedure, Value, help_list_add(), help_list_flush() (+23 more)

### Community 62 - "Dirty-Tile Rectangle Tracking"
Cohesion: 0.14
Nodes (29): dirty_tiles_any(), dirty_tiles_clear(), dirty_tiles_mark_all(), dirty_tiles_mark_rect(), dirty_tiles_mark_rect_wrap(), dirty_tiles_next_span(), wrap_coord(), ScreenSprite (+21 more)

### Community 63 - "Primitive Help Coverage Tests"
Cohesion: 0.06
Nodes (29): tearDown(), tearDown(), tearDown(), tearDown(), setUp(), tearDown(), tearDown(), tearDown() (+21 more)

### Community 64 - "Arithmetic & Random Primitives"
Cohesion: 0.23
Nodes (29): Evaluator, Result, Value, prim_abs(), prim_arctan(), prim_cos(), prim_difference(), prim_exp() (+21 more)

### Community 65 - "Variable Scope Tests"
Cohesion: 0.11
Nodes (30): test_dots_variable(), test_global_variable(), test_local_declaration(), test_local_variable_not_visible_after_scope(), test_local_variable_shadowing(), test_local_with_list(), test_localmake_declares_and_sets(), test_localmake_in_procedure() (+22 more)

### Community 66 - "Eval Steps (Procedure Calls)"
Cohesion: 0.21
Nodes (29): op_stack_pop(), EvalOp, EvalOpKind, Evaluator, Node, Result, UserProcedure, eval_trace_entry() (+21 more)

### Community 67 - "LittleFS Storage Device"
Cohesion: 0.10
Nodes (19): LogoDirCallback, LogoStream, lfs_storage_fs_image_backup(), lfs_storage_fs_image_restore(), lfs_storage_list_directory(), lfs_storage_open(), lfs_stream_can_read(), lfs_stream_close() (+11 more)

### Community 68 - "Mock SD Card & FAT32 Image"
Cohesion: 0.12
Nodes (20): clear_root_cluster(), compute_fat_size(), fat32_image_format_mbr(), fat32_image_format_superfloppy(), write_boot_sector(), write_fsinfo(), write_initial_fat(), sd_error_t (+12 more)

### Community 69 - "Editor Storage Integration Tests"
Cohesion: 0.15
Nodes (30): LogoEditorResult, mock_device_set_editor_content(), mock_device_set_editor_result(), mock_editor_edit(), MockFile, mock_fs_create_file(), mock_fs_get_content(), mock_fs_get_file() (+22 more)

### Community 70 - "Procedure/Time Primitive Definitions"
Cohesion: 0.16
Nodes (23): Evaluator, Result, Value, get_bool_arg(), prim_and(), prim_not(), prim_or(), Evaluator (+15 more)

### Community 71 - "Host Storage Device"
Cohesion: 0.11
Nodes (20): LogoDirCallback, LogoStorage, LogoStream, host_file_can_read(), host_file_close(), host_file_flush(), host_file_get_length(), host_file_get_read_pos() (+12 more)

### Community 72 - "PicoCalc Storage Device"
Cohesion: 0.14
Nodes (27): fat32_get_cluster_size(), fat32_get_generation(), fat32_seek(), fat32_size(), LogoStorage, LogoStream, file_context_stale(), logo_picocalc_file_open() (+19 more)

### Community 73 - "Demon Polling Tests"
Cohesion: 0.14
Nodes (26): demons_poll(), mock_device_clear_output(), setUp(), tearDown(), test_action_does_not_reenter_poll(), test_clearscreen_clears_demons(), test_clearscreen_stops_motion(), test_freeze_halts_motion() (+18 more)

### Community 74 - "Lexer Core"
Cohesion: 0.18
Nodes (27): Lexer, Token, TokenType, is_delimiter(), is_digit(), is_number_char(), is_space(), is_valid_number() (+19 more)

### Community 75 - "HTTP Element Primitives"
Cohesion: 0.21
Nodes (27): mem_word(), Evaluator, Result, Value, el_append(), el_append_cstr(), el_append_word(), no_request() (+19 more)

### Community 76 - "Storage Router Tests"
Cohesion: 0.07
Nodes (6): LogoEntryType, LogoStream, collect_cb(), make_stream(), setUp(), spy_reset()

### Community 77 - "Trampoline Evaluator Core"
Cohesion: 0.24
Nodes (26): EvalOpKind, Evaluator, FrameStack, Node, Result, UserProcedure, Value, eval_get_frames() (+18 more)

### Community 78 - "Outside-World I/O Primitives"
Cohesion: 0.19
Nodes (23): Lexer, parse_list_body(), parse_list_from_string(), Evaluator, Node, Result, Value, flush_writer() (+15 more)

### Community 79 - "I/O Writer/Dribble Management"
Cohesion: 0.12
Nodes (25): logo_io_cleanup(), logo_io_close_all(), logo_io_dribble_input(), logo_io_flush(), logo_io_is_dribbling(), logo_io_set_writer(), logo_io_start_dribble(), logo_io_stop_dribble() (+17 more)

### Community 80 - "LittleFS Storage Tests"
Cohesion: 0.12
Nodes (18): Listing, bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t, LogoEntryType (+10 more)

### Community 81 - "Multi-Sprite Compositor Design"
Cohesion: 0.08
Nodes (24): Scanline Sprite Compositor, Tile-Based Dirty Tracking, P5 — Multi-sprite Turtles and the Display Pipeline (design), ask primitive, distance primitive, each primitive, freeze primitive, getsh primitive (+16 more)

### Community 82 - "Host Hardware Networking"
Cohesion: 0.09
Nodes (5): LogoHardware, host_network_tcp_connect(), init_winsock(), logo_host_hardware_create(), logo_host_hardware_destroy()

### Community 83 - "Sprite Costume System"
Cohesion: 0.21
Nodes (19): costume_delete(), costume_get(), costume_pool_free(), costume_put(), costumes_clear(), pool_release(), fill_pattern(), setUp() (+11 more)

### Community 84 - "Expression Evaluator"
Cohesion: 0.21
Nodes (22): eval_at_end(), apply_binary_op(), Evaluator, Node, Result, TokenType, Value, eval_expr_bp() (+14 more)

### Community 85 - "HTTP Request Primitives"
Cohesion: 0.26
Nodes (20): buf_appendf(), Evaluator, Result, Value, check_header_args(), ci_equal(), decode_chunked(), header_token_is_safe() (+12 more)

### Community 86 - "REPL Line Evaluation"
Cohesion: 0.13
Nodes (21): proc_restore_execution_state(), proc_save_execution_state(), Result, repl_count_bracket_balance(), repl_evaluate_line(), repl_line_is_end(), repl_next_bracket_depth(), repl_restore_refresh() (+13 more)

### Community 87 - "VS Code Extension Manifest"
Cohesion: 0.09
Nodes (21): categories, contributes, grammars, languages, description, devDependencies, @vscode/vsce, displayName (+13 more)

### Community 88 - "Galaxian Game Tests"
Cohesion: 0.19
Nodes (20): assert_num(), assert_true(), load_galaxian(), seed_convoy(), setUp(), test_convoy_kill_scores_and_shrinks(), test_dive_detach_and_rejoin(), test_file_loads_and_sets_globals() (+12 more)

### Community 89 - "LittleFS Backup/Restore Tests"
Cohesion: 0.21
Nodes (17): blob_flush(), blob_get_read_pos(), blob_read_chars(), blob_reset_for_write(), blob_rewind_for_read(), blob_set_read_pos(), blob_write_bytes(), lfs_t (+9 more)

### Community 90 - "mklfsimg Round-Trip Tests"
Cohesion: 0.14
Nodes (17): bd_erase(), bd_prog(), bd_read(), blob_get_read_pos(), blob_read_chars(), blob_set_read_pos(), lfs_block_t, lfs_off_t (+9 more)

### Community 91 - "SD Card Driver"
Cohesion: 0.27
Nodes (17): sd_error_t, sd_card_init(), sd_cs_deselect(), sd_cs_select(), sd_error_string(), sd_read_block(), sd_read_block_once(), sd_read_blocks() (+9 more)

### Community 92 - "Property List Primitive Tests"
Cohesion: 0.10
Nodes (19): test_erprops_clears_all_properties(), test_gprop_requires_word_for_name(), test_gprop_requires_word_for_property(), test_multiple_properties_on_same_name(), test_plist_requires_word(), test_pprop_and_gprop_number_value(), test_pprop_and_gprop_word_value(), test_pprop_overwrites_existing_property() (+11 more)

### Community 93 - "Test Scaffold Stream Mocks"
Cohesion: 0.12
Nodes (8): LogoStream, mock_stream_can_read(), mock_stream_close(), mock_stream_flush(), mock_stream_read_char(), mock_stream_read_chars(), mock_stream_read_line(), mock_stream_write()

### Community 94 - "Device Main/Init Entry Points"
Cohesion: 0.12
Nodes (11): Lexer, eval_init(), proc_clear_tail_call(), proc_reset_execution_state(), procedures_init(), main(), lfs_t, picocalc_lfs() (+3 more)

### Community 95 - "Help Lookup System"
Cohesion: 0.12
Nodes (12): help_check_sorted(), help_contains_nocase(), help_lookup(), test_help_contains_nocase(), test_help_lookup_is_case_insensitive(), test_help_lookup_returns_null_for_unknown(), test_help_lookup_returns_text_for_known_primitive(), test_help_table_is_sorted() (+4 more)

### Community 96 - "WiFi/TLS Network Connect"
Cohesion: 0.16
Nodes (20): ensure_wifi_initialized(), mdns_start(), picocalc_network_ping(), picocalc_network_resolve(), picocalc_network_set_hostname(), picocalc_network_tcp_connect(), picocalc_network_tcp_listen(), picocalc_network_tls_connect() (+12 more)

### Community 97 - "TLS Heap Allocator"
Cohesion: 0.19
Nodes (15): picocalc_tls_heap_setup(), tls_heap_calloc(), tls_heap_free(), tls_heap_init(), tls_heap_malloc(), setUp(), test_calloc_overflow_returns_null(), test_calloc_zeroes() (+7 more)

### Community 98 - "Storage Router"
Cohesion: 0.18
Nodes (19): LogoDirCallback, LogoStream, cross_fs_move(), is_root(), router_dir_create(), router_dir_delete(), router_dir_exists(), router_file_delete() (+11 more)

### Community 99 - "LittleFS Image Build Tool"
Cohesion: 0.17
Nodes (16): lfs_block_t, lfs_off_t, lfs_size_t, lfs_t, LogoStream, copy_file(), copy_tree(), file_flush() (+8 more)

### Community 100 - "File Load/Save Primitives"
Cohesion: 0.33
Nodes (18): httpd_savebody(), Evaluator, Result, Value, prim_load(), prim_loadpic(), prim_pofile(), prim_save() (+10 more)

### Community 101 - "Editor Invocation Primitives"
Cohesion: 0.21
Nodes (16): Evaluator, Result, Value, count_bracket_balance(), prim_edall(), prim_editfile(), prim_edns(), run_editor_and_process() (+8 more)

### Community 102 - "Screensaver"
Cohesion: 0.18
Nodes (14): lcd_get_palette_value(), lcd_restore_palette(), lcd_set_palette_value(), turtle_restore_palette(), turtle_set_bg_colour(), screen_get_mode(), screen_gfx_mark_all_dirty(), advance_cycle() (+6 more)

### Community 103 - "Flash Self-Test"
Cohesion: 0.18
Nodes (15): m1_capture(), m1_equal(), picocalc_flash_erase(), picocalc_flash_program(), picocalc_flash_read(), picocalc_flash_selftest(), writable_m1(), bd_erase() (+7 more)

### Community 104 - "Memory Reclamation Design Docs"
Cohesion: 0.15
Nodes (17): Atom Permanence (Atoms Are Never Freed), Atom Reclamation / Mark-Sweep Design, erall Soft Reset Design, find_atom Linear Scan Hot Path, primitive_find Linear Scan Hot Path, recycle GC Root-Set Bug, setx/sety Preferred Over setpos (GC pressure), Tail Call Optimization (TCO) (+9 more)

### Community 105 - "Control-Flow Primitives"
Cohesion: 0.38
Nodes (16): Evaluator, Result, Value, eval_to_number(), prim_do_until(), prim_do_while(), prim_forever(), prim_ignore() (+8 more)

### Community 106 - "POSIX-Style clib Shims"
Cohesion: 0.20
Nodes (14): logo_host_rename(), fat32_error_t, _close(), fat32_error_to_errno(), _fstat(), init(), _lseek(), _open() (+6 more)

### Community 107 - "HTTP/Launch/Demon Design Docs"
Cohesion: 0.14
Nodes (15): Demon System (when / poll-driven events), HTTP Server Poll-Driven Pump, launch Background Process Model, mDNS Naming (picologo.local), Process Yield Rule (outermost trampoline only), Trampoline Evaluator, HTTP Server (design), launch Background Processes Design (P6) (+7 more)

### Community 108 - "Higher-Order List Primitives"
Cohesion: 0.54
Nodes (14): Evaluator, Result, Value, invoke_proc_spec(), parse_proc_spec(), prim_apply(), prim_crossmap(), prim_filter() (+6 more)

### Community 109 - "Host Console Device"
Cohesion: 0.25
Nodes (14): LogoConsole, LogoStream, host_input_can_read(), host_input_read_char(), host_input_read_chars(), host_input_read_line(), host_output_flush(), host_output_write() (+6 more)

### Community 110 - "Southbridge (Keyboard/Battery) Driver"
Cohesion: 0.32
Nodes (14): picocalc_get_battery_level(), picocalc_power_off(), sb_is_power_off_supported(), sb_read(), sb_read_battery(), sb_read_keyboard(), sb_read_keyboard_backlight(), sb_read_keyboard_state() (+6 more)

### Community 111 - "Audio/PWM Playback"
Cohesion: 0.25
Nodes (13): alarm_id_t, audio_note_t, audio_song_t, audio_is_playing(), audio_play_note_blocking(), audio_play_song_blocking(), audio_play_sound(), audio_play_sound_blocking() (+5 more)

### Community 112 - "Recursive Procedure Tests"
Cohesion: 0.22
Nodes (14): mem_set_cdr(), append_to_list(), Lexer, Node, Token, parse_bracket_contents(), token_to_atom(), Node (+6 more)

### Community 113 - "Debug-Control Primitives"
Cohesion: 0.31
Nodes (13): Evaluator, Result, Value, pause_check_continue(), pause_request_continue(), pause_reset_state(), prim_co(), prim_go() (+5 more)

### Community 114 - "Property List Primitives"
Cohesion: 0.35
Nodes (11): Evaluator, Result, Value, prim_erprops(), prim_gprop(), prim_plist(), prim_pprop(), prim_pps() (+3 more)

### Community 115 - "Storage Subsystem Init"
Cohesion: 0.17
Nodes (12): lfs_t, LogoStorage, logo_lfs_storage_init(), LogoStorage, LogoStorageOps, logo_storage_init(), LogoStorage, LogoStorageOps (+4 more)

### Community 116 - "Reference Manual Primitives (Misc)"
Cohesion: 0.15
Nodes (13): Improvements Roadmap, arc primitive, arctan primitive, localmake primitive, modulo primitive, pick primitive, remdup primitive, remove primitive (+5 more)

### Community 117 - "FAT32 Mount/Detect Tests"
Cohesion: 0.18
Nodes (12): repeating_timer_t, fat32_init(), fat32_is_mounted(), fat32_unmount(), on_sd_card_detect(), logo_picocalc_mount_available(), sd_card_present(), sd_init() (+4 more)

### Community 118 - "Mock Turtle Command Recording"
Cohesion: 0.18
Nodes (12): LogoRotationStyle, heading_to_radians(), mock_turtle_move(), mock_turtle_select(), mock_turtle_set_heading(), mock_turtle_set_rotation_style(), mock_turtle_set_scale(), mock_turtle_set_shape() (+4 more)

### Community 119 - "Exception Handling Tests"
Cohesion: 0.17
Nodes (10): tearDown(), test_catch_basic(), test_catch_through_calls_catch(), test_catch_through_calls_good(), test_catch_throw_match(), test_catch_throw_nomatch(), test_throw_no_catch(), test_throw_toplevel() (+2 more)

### Community 120 - "Project Documentation Index"
Cohesion: 0.25
Nodes (11): Computer Science Logo Style, Mindstorms (Seymour Papert), Turtle Geometry: The Computer as a Medium for Exploring Mathematics, CLAUDE.md Project Instructions, Board Capability Tiers (LOGO_HAS_WIFI / LOGO_HAS_TLS), Single-Precision Float Discipline, SRAM Budget Constraint (~520 KB), GitHub Copilot Instructions (+3 more)

### Community 121 - "Conditional Primitives"
Cohesion: 0.51
Nodes (10): Evaluator, Result, Value, prim_false(), prim_if(), prim_ifelse(), prim_iffalse(), prim_iftrue() (+2 more)

### Community 122 - "LittleFS Backup Device"
Cohesion: 0.33
Nodes (10): lfs_block_t, lfs_t, LogoStream, get_u32(), logo_lfs_backup(), logo_lfs_restore(), mark_block(), put_u32() (+2 more)

### Community 123 - "RTC Date/Time Conversion"
Cohesion: 0.36
Nodes (11): datetime_to_ms(), days_in_month_of_year(), ensure_software_clock_initialized(), get_current_epoch_ms(), is_leap_year(), ms_to_datetime(), picocalc_get_date(), picocalc_get_time() (+3 more)

### Community 124 - "GC Mark-Sweep Roots"
Cohesion: 0.38
Nodes (10): demons_gc_mark_all(), Value, mark_value(), op_stack_gc_mark(), mem_gc_mark(), prim_recycle(), proc_gc_mark_all(), prop_gc_mark_all() (+2 more)

### Community 125 - "Bitwise Primitives"
Cohesion: 0.53
Nodes (9): Evaluator, Result, Value, prim_ashift(), prim_bitand(), prim_bitnot(), prim_bitor(), prim_bitxor() (+1 more)

### Community 126 - "Build & CI Configuration Docs"
Cohesion: 0.28
Nodes (9): Root CMakeLists.txt Build Configuration, LittleFS Flash Partition Layout, PSRAM/QMI-Safe Flash Write Recipe, VFS Storage Router (LittleFS root + /sd FAT32), LittleFS Internal Filesystem + /sd FAT32 Mount (design), CI Workflow (ci.yml), tests/CMakeLists.txt Test Configuration, littlefs LICENSE.md (+1 more)

### Community 127 - "PSG Sound Synthesizer Design"
Cohesion: 0.22
Nodes (9): Stereo PSG Synthesizer Engine, P8 — Sound: a Stereo PSG Synthesizer (design), play primitive, playing? primitive, setenv primitive, setwave primitive, sound primitive, stopsound primitive (+1 more)

### Community 128 - "Variable Primitives"
Cohesion: 0.56
Nodes (8): Evaluator, Result, Value, prim_localmake(), prim_make(), prim_name(), prim_namep(), prim_thing()

### Community 129 - "NTP/DNS/Ping Callbacks"
Cohesion: 0.25
Nodes (8): ntp_dns_callback(), ntp_recv_callback(), ntp_send_request(), picocalc_dns_callback(), ping_recv_callback(), tcp_dns_callback(), ip_addr_t, u16_t

### Community 130 - "Mock HTTP Connection"
Cohesion: 0.32
Nodes (8): MockHttpdConn, as_httpd_conn(), httpd_conn_read(), httpd_conn_write(), mock_network_tcp_can_read(), mock_network_tcp_close(), mock_network_tcp_read(), mock_network_tcp_write()

### Community 131 - "Lexer Fuzz Edge Cases"
Cohesion: 0.33
Nodes (6): Lexer, drain_tokens(), test_fuzz_deeply_nested_brackets(), test_fuzz_many_consecutive_minus(), test_fuzz_many_quoted_words(), test_fuzz_many_small_tokens()

### Community 132 - "LittleFS Block-Device Helpers"
Cohesion: 0.47
Nodes (6): bd_erase(), bd_prog(), bd_read(), lfs_block_t, lfs_off_t, lfs_size_t

### Community 133 - "Galaxian Design & Primitives"
Cohesion: 0.40
Nodes (5): Galaxian Diver Mechanic, Flat Liveness List Mutated via .setitem, Galaxian in Pico Logo (design), colourunder primitive, when primitive

### Community 136 - "CA Cert Generator Script"
Cohesion: 0.83
Nodes (3): main(), split_pem_blocks(), subject_cn()

### Community 137 - "Reference Anchor Checker Script"
Cohesion: 0.67
Nodes (3): main(), pandoc_slug(), Compute pandoc's auto_identifiers slug for a heading.

### Community 139 - "Frame Stack Iteration"
Cohesion: 0.67
Nodes (4): FrameHeader, FrameStack, iteration_callback(), stop_at_two()

### Community 140 - "Comment-Preserving Lexer Tests"
Cohesion: 0.67
Nodes (3): lexer_set_preserve_comments(), test_semicolon_comment_preserved_when_enabled(), test_semicolon_comment_preserved_with_spaces()

## Knowledge Gaps
- **115 isolated node(s):** `dist.sh script`, `flash.sh script`, `name`, `displayName`, `description` (+110 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `run_string()` connect `Turtle Graphics Tests` to `Arithmetic/Bitwise/Logical Tests`, `Workspace & Control-Flow Tests`, `Value Type & Tests`, `List/Word Memory Primitives`, `Words/Lists Primitive Tests`, `Turtle Primitives & Frame Sync`, `Text Primitives & Mock Device`, `Error Handling`, `Procedure & Debug-Control Tests`, `Procedure Definition Formatting`, `Lexer Tests`, `Device I/O Abstraction`, `File Read/Write Primitives`, `Variable Get/Set`, `File Load/Save & Directory Tests`, `JSON Primitive Tests`, `File & Directory Primitives`, `Editor Primitive Tests`, `Conditional Primitive Tests`, `REPL Tests`, `Time/Date Primitives`, `Hardware/Battery Primitives`, `File Info Primitive Tests`, `Frame Stack Introspection`, `Variable Scope Tests`, `Editor Storage Integration Tests`, `Demon Polling Tests`, `HTTP Element Primitives`, `Trampoline Evaluator Core`, `Expression Evaluator`, `Galaxian Game Tests`, `Property List Primitive Tests`, `Test Scaffold Stream Mocks`, `Device Main/Init Entry Points`, `Recursive Procedure Tests`, `Exception Handling Tests`?**
  _High betweenness centrality (0.161) - this node is a cross-community bridge._
- **Why does `eval_string()` connect `Arithmetic/Bitwise/Logical Tests` to `Workspace & Control-Flow Tests`, `List/Word Memory Primitives`, `Words/Lists Primitive Tests`, `Text Primitives & Mock Device`, `Error Handling`, `Procedure & Debug-Control Tests`, `Procedure Definition Formatting`, `Lexer Tests`, `HTTP Daemon Tests`, `HTTP Primitive Tests`, `File Read/Write Primitives`, `File Load/Save & Directory Tests`, `JSON Primitive Tests`, `File & Directory Primitives`, `Conditional Primitive Tests`, `Time/Date Primitives`, `Hardware/Battery Primitives`, `WiFi Primitive Tests`, `File Info Primitive Tests`, `WiFi/Network Primitives`, `Frame Stack Introspection`, `Network Primitive Tests (NTP/Ping)`, `Variable Scope Tests`, `Demon Polling Tests`, `HTTP Element Primitives`, `Trampoline Evaluator Core`, `Expression Evaluator`, `Galaxian Game Tests`, `Property List Primitive Tests`, `Test Scaffold Stream Mocks`, `Device Main/Init Entry Points`, `Recursive Procedure Tests`?**
  _High betweenness centrality (0.120) - this node is a cross-community bridge._
- **Why does `lexer_init()` connect `Lexer Tests` to `Turtle Graphics Tests`, `Arithmetic/Bitwise/Logical Tests`, `Lexer Fuzz Tests`, `File Load/Save Primitives`, `Editor Invocation Primitives`, `Lexer Fuzz Edge Cases`, `Demon Polling Tests`, `Lexer Core`, `Comment-Preserving Lexer Tests`, `Procedure & Debug-Control Tests`, `Outside-World I/O Primitives`, `Host Console Device`, `Token Source Abstraction`, `REPL Line Evaluation`, `Device Main/Init Entry Points`?**
  _High betweenness centrality (0.056) - this node is a cross-community bridge._
- **Are the 856 inferred relationships involving `eval_string()` (e.g. with `test_deep_recursion_100_levels()` and `test_deep_recursion_addupto()`) actually correct?**
  _`eval_string()` has 856 INFERRED edges - model-reasoned connections that need verification._
- **Are the 856 inferred relationships involving `run_string()` (e.g. with `test_action_does_not_reenter_poll()` and `test_clearscreen_clears_demons()`) actually correct?**
  _`run_string()` has 856 INFERRED edges - model-reasoned connections that need verification._
- **Are the 410 inferred relationships involving `mem_word_ptr()` (e.g. with `value_is_true()` and `eval_primary()`) actually correct?**
  _`mem_word_ptr()` has 410 INFERRED edges - model-reasoned connections that need verification._
- **Are the 230 inferred relationships involving `mem_is_nil()` (e.g. with `demons_set()` and `skip_primary()`) actually correct?**
  _`mem_is_nil()` has 230 INFERRED edges - model-reasoned connections that need verification._