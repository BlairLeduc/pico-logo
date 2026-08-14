//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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

// Buffer for capturing print output.
//
// Clear it with reset_output(), never with `output_buffer[0] = '\0'`: writes
// land at output_pos, which that leaves untouched, so a second print in the
// same test writes past the terminator and every later assertion reads an
// empty buffer — which reads as the interpreter having silently stopped
// working, and has produced at least one false bug report.
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

// Mock reboot_bootloader state for testing
extern bool mock_bootsel_available;
extern bool mock_bootsel_called;

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

// Setup with mock device AND mock hardware - adds a controllable clock
// (ticks_ms) so demon polling and autonomous turtle motion can be tested.
void test_scaffold_setUp_with_device_and_hardware(void);

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
// Mock Clock Helper
// ============================================================================

// The controllable monotonic clock backing mock_hardware_ops.ticks_ms.
extern uint32_t mock_ticks_value;
uint32_t mock_ticks_ms(void);

// Set the mock monotonic clock (milliseconds) for demon/motion tests.
void set_mock_ticks(uint32_t ms);

// ============================================================================
// Mock Key State Helpers
// ============================================================================
//
// Drives the pollkeys/keydown?/keyhit? ops. The real driver derives these from
// the southbridge's press/release events (devices/picocalc/keyboard.c); here a
// test just says which keys are down and which were tapped.

// Hold a key down (or let it up) from now until changed. Pressing a key that
// was up also records a hit for the next pollkeys, as the driver does.
void set_mock_key_down(int key_code, bool down);

// Record a press-and-release too quick to still be down at the next pollkeys.
void set_mock_key_tap(int key_code);

// How many times pollkeys has reached the hardware.
int mock_poll_keys_count(void);

// Release every key and drop the hit latch. Called by every setUp variant.
void reset_mock_key_state(void);

// ============================================================================
// Mock Power Off Helpers
// ============================================================================

// Configure mock power_off for testing
void set_mock_power_off(bool available, bool result);

// Check if mock_power_off was called
bool was_mock_power_off_called(void);

// Configure mock reboot_bootloader for testing
void set_mock_bootsel(bool available);

// Check if mock reboot_bootloader was called
bool was_mock_bootsel_called(void);

// ============================================================================
// Test Scope Helpers (for simulating procedure calls in tests)
// These use the frame system to create proper scoping for local variables
// ============================================================================

// Push a test scope frame (simulates entering a procedure)
bool test_push_scope(void);

// Pop a test scope frame (simulates exiting a procedure)
void test_pop_scope(void);

// Get current test scope depth
int test_scope_depth(void);

// Set a local variable in the current test scope
void test_set_local(const char *name, Value value);
