//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#pragma once

#include "unity.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "core/variables.h"
#include "core/properties.h"
#include "devices/stream.h"
#include "devices/console.h"
#include "devices/io.h"
#include "devices/hardware.h"
#include "devices/storage.h"
#include "mock_device.h"

// ============================================================================
// Global State (defined in test_scaffold.c)
// ============================================================================

// Buffer for capturing print output
extern char output_buffer[1024];
extern int output_pos;

// Buffer for simulated input
extern const char *mock_input_buffer;
extern size_t mock_input_pos;

// Mock battery state for testing
extern int mock_battery_level;
extern bool mock_battery_charging;

// User interrupt flag for testing
extern bool mock_user_interrupt;

// Pause request flag for testing (F9 key)
extern bool mock_pause_requested;

// Freeze request flag for testing (F4 key)
extern bool mock_freeze_requested;

// Mock power_off state for testing
extern bool mock_power_off_available;
extern bool mock_power_off_result;
extern bool mock_power_off_called;

// Flag to track if we're using mock_device for turtle/text testing
extern bool use_mock_device;

// Mock console (contains embedded streams)
extern LogoConsole mock_console;

// Mock hardware
extern LogoHardware mock_hardware;

// Mock I/O manager
extern LogoIO mock_io;

// Mock hardware ops (may need to modify power_off pointer)
extern LogoHardwareOps mock_hardware_ops;

// ============================================================================
// Test Setup/Teardown
// ============================================================================

// Standard setup - initializes core systems with mock streams
void test_scaffold_setUp(void);

// Setup with mock device - enables turtle/text/screen testing
void test_scaffold_setUp_with_device(void);

// Teardown (currently empty but available for extension)
void test_scaffold_tearDown(void);

// ============================================================================
// Input/Output Helpers
// ============================================================================

// Set mock input for testing input primitives
void set_mock_input(const char *input);

// Reset output buffer
void reset_output(void);

// ============================================================================
// Evaluation Helpers
// ============================================================================

// Evaluate an expression and return the result
Result eval_string(const char *input);

// Run instructions and return the result
Result run_string(const char *input);

// ============================================================================
// Procedure Definition Helper
// ============================================================================

// Define a procedure for testing
void define_proc(const char *name, const char **params, int param_count, const char *body);

// ============================================================================
// Mock Battery Helper
// ============================================================================

// Set mock battery level and charging state for testing
void set_mock_battery(int level, bool charging);

// ============================================================================
// Mock Power Off Helpers
// ============================================================================

// Configure mock power_off for testing
void set_mock_power_off(bool available, bool result);

// Check if mock_power_off was called
bool was_mock_power_off_called(void);

// ============================================================================
// Test Scope Helpers (for simulating procedure calls in tests)
// ============================================================================

// Push a test scope frame (simulates entering a procedure)
bool test_push_scope(void);

// Pop a test scope frame (simulates exiting a procedure)
void test_pop_scope(void);

// Get current test scope depth
int test_scope_depth(void);

// Set a local variable in the current test scope
void test_set_local(const char *name, Value value);
