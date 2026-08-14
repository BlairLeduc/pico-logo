//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Keyboard driver tests - compiles devices/picocalc/keyboard.c on the host
//  against a fake southbridge FIFO.
//
//  These pin the drain behaviour of keyboard_poll().  The southbridge repeats
//  a held key every 100 ms into a 31-entry FIFO; the driver's background timer
//  also runs at 100 ms, so draining a single entry per tick left the consumer
//  with exactly the producer's rate and no headroom (bug B28), and the ring
//  push let the head lap the tail when it filled (bug B29).
//

#include <string.h>

#include "unity.h"
#include "keyboard.h"

//
//  Globals the driver expects main.c to own
//

volatile bool user_interrupt = false;
volatile bool pause_requested = false;
volatile bool freeze_requested = false;
volatile bool input_active = false;
volatile bool screensaver_dismissed = false;

//
//  Fake southbridge
//

#define FAKE_FIFO_SIZE (128)

static uint16_t fake_fifo[FAKE_FIFO_SIZE];
static int fake_fifo_count = 0;
static int fake_fifo_next = 0;
static int sb_read_count = 0; // how many times the driver hit the bus

static void fifo_push(uint8_t state, uint8_t code)
{
    TEST_ASSERT_LESS_THAN_INT(FAKE_FIFO_SIZE, fake_fifo_count);
    fake_fifo[fake_fifo_count++] = (uint16_t)(state << 8 | code);
}

static void fifo_reset(void)
{
    fake_fifo_count = 0;
    fake_fifo_next = 0;
    sb_read_count = 0;
}

uint16_t sb_read_keyboard(void)
{
    sb_read_count++;
    if (fake_fifo_next >= fake_fifo_count)
    {
        return 0; // KEY_STATE_IDLE - FIFO empty
    }
    return fake_fifo[fake_fifo_next++];
}

bool sb_available(void) { return true; }
void sb_init(void) {}

//
//  Fake clock - the idle loop rate-limits its polling against this
//

static uint64_t fake_now_us = 0;
uint64_t time_us_64(void) { return fake_now_us; }

//
//  Fakes for the rest of the driver's collaborators
//

void lcd_cursor_blink(void) {}
void screensaver_init(void) {}
void screensaver_update(void) {}
bool screensaver_on_key_press(void) { return false; }
void screen_gfx_flush(void) {}

static int mode_key_switches = 0;
bool screen_handle_mode_key(int key_code) { (void)key_code; mode_key_switches++; return true; }

//
//  Helpers
//

// Read everything currently buffered into `out`, returning the count.
static int drain_ring(char *out, int max)
{
    int n = 0;
    while (keyboard_key_available() && n < max)
    {
        out[n++] = keyboard_get_key();
    }
    return n;
}

void setUp(void)
{
    // Empty the ring left over from the previous test.
    char scratch[KBD_BUFFER_SIZE];
    drain_ring(scratch, KBD_BUFFER_SIZE);

    // Clear any latched modifier state.
    fifo_reset();
    fifo_push(KEY_STATE_RELEASED, KEY_MOD_CTRL);
    fifo_push(KEY_STATE_RELEASED, KEY_MOD_SHL);
    keyboard_poll();
    keyboard_poll();

    fifo_reset();
    mode_key_switches = 0;
    user_interrupt = false;
    input_active = false;
    fake_now_us = 0;
}

void tearDown(void) {}

//
//  Tests
//

// The regression that started this: one poll must clear more than one event.
// A press/release pair per keystroke means a single-event poll could only ever
// deliver five characters a second.
static void test_a_single_poll_delivers_more_than_one_keystroke(void)
{
    fifo_push(KEY_STATE_PRESSED, 'a');
    fifo_push(KEY_STATE_RELEASED, 'a');
    fifo_push(KEY_STATE_PRESSED, 'b');
    fifo_push(KEY_STATE_RELEASED, 'b');

    keyboard_poll();

    char keys[8];
    int n = drain_ring(keys, 8);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_CHAR('a', keys[0]);
    TEST_ASSERT_EQUAL_CHAR('b', keys[1]);
}

// The drain is bounded so a FIFO that never empties cannot hold the timer IRQ.
static void test_a_poll_stops_at_the_drain_cap(void)
{
    for (int i = 0; i < KEYBOARD_DRAIN_MAX + 4; i++)
    {
        fifo_push(KEY_STATE_PRESSED, (uint8_t)('a' + i));
    }

    keyboard_poll();

    TEST_ASSERT_EQUAL_INT(KEYBOARD_DRAIN_MAX, sb_read_count);

    char keys[16];
    int n = drain_ring(keys, 16);
    TEST_ASSERT_EQUAL_INT(KEYBOARD_DRAIN_MAX, n);
}

// An idle keyboard must cost exactly one bus read, not KEYBOARD_DRAIN_MAX of
// them - the poll runs from a timer IRQ and the bus is 10 kHz.
static void test_an_idle_keyboard_costs_one_bus_read(void)
{
    keyboard_poll();

    TEST_ASSERT_EQUAL_INT(1, sb_read_count);
    TEST_ASSERT_FALSE(keyboard_key_available());
}

// A bare KEY_STATE_HOLD (the southbridge sends these for keys that must not
// auto-repeat) is discarded, but it must not consume the whole poll: before
// the fix it fell through both branches and cost a full 100 ms tick.
static void test_a_hold_event_does_not_stall_the_drain(void)
{
    fifo_push(KEY_STATE_HOLD, KEY_F5);
    fifo_push(KEY_STATE_PRESSED, 'x');

    keyboard_poll();

    char keys[4];
    int n = drain_ring(keys, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_CHAR('x', keys[0]);
}

// When the ring fills, the key that does not fit is dropped.  The old push let
// the head lap the tail, which discarded the entire buffered backlog instead.
static void test_a_full_ring_drops_the_newest_key_not_the_backlog(void)
{
    // KBD_BUFFER_SIZE - 1 usable slots; queue more than that.
    for (int i = 0; i < KBD_BUFFER_SIZE + 8; i++)
    {
        fifo_push(KEY_STATE_PRESSED, (uint8_t)('a' + (i % 26)));
    }
    while (fake_fifo_next < fake_fifo_count)
    {
        keyboard_poll();
    }

    char keys[KBD_BUFFER_SIZE * 2];
    int n = drain_ring(keys, KBD_BUFFER_SIZE * 2);

    TEST_ASSERT_EQUAL_INT(KBD_BUFFER_SIZE - 1, n);
    // The oldest keys survived, in order.
    TEST_ASSERT_EQUAL_CHAR('a', keys[0]);
    TEST_ASSERT_EQUAL_CHAR('b', keys[1]);
    TEST_ASSERT_EQUAL_CHAR('c', keys[2]);
}

// A held key as the southbridge actually reports it: one press, then a repeat
// every 100 ms delivered as further KEY_STATE_PRESSED events, then a release.
static void test_a_repeat_burst_arrives_whole_and_in_order(void)
{
    const int repeats = 10;
    fifo_push(KEY_STATE_PRESSED, 'z');
    for (int i = 0; i < repeats; i++)
    {
        fifo_push(KEY_STATE_PRESSED, 'z');
    }
    fifo_push(KEY_STATE_RELEASED, 'z');

    while (fake_fifo_next < fake_fifo_count)
    {
        keyboard_poll();
    }

    char keys[32];
    int n = drain_ring(keys, 32);
    TEST_ASSERT_EQUAL_INT(repeats + 1, n);
    for (int i = 0; i < n; i++)
    {
        TEST_ASSERT_EQUAL_CHAR('z', keys[i]);
    }
}

// keyboard_get_key() polls the bus itself rather than waiting on the 100 ms
// timer, so a key already sitting in the southbridge FIFO comes back promptly.
static void test_get_key_polls_instead_of_waiting_for_the_timer(void)
{
    fifo_push(KEY_STATE_PRESSED, 'q');

    TEST_ASSERT_FALSE(keyboard_key_available()); // nothing buffered yet
    TEST_ASSERT_EQUAL_CHAR('q', keyboard_get_key());
}

// Modifier decoding still works across the restructured poll.
static void test_shift_and_ctrl_still_decode(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_MOD_SHL);
    fifo_push(KEY_STATE_PRESSED, 'a');
    fifo_push(KEY_STATE_RELEASED, KEY_MOD_SHL);
    fifo_push(KEY_STATE_PRESSED, KEY_MOD_CTRL);
    fifo_push(KEY_STATE_PRESSED, 'c');

    while (fake_fifo_next < fake_fifo_count)
    {
        keyboard_poll();
    }

    char keys[8];
    int n = drain_ring(keys, 8);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_CHAR('A', keys[0]);
    TEST_ASSERT_EQUAL_CHAR(0x03, keys[1]); // Ctrl-C
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_single_poll_delivers_more_than_one_keystroke);
    RUN_TEST(test_a_poll_stops_at_the_drain_cap);
    RUN_TEST(test_an_idle_keyboard_costs_one_bus_read);
    RUN_TEST(test_a_hold_event_does_not_stall_the_drain);
    RUN_TEST(test_a_full_ring_drops_the_newest_key_not_the_backlog);
    RUN_TEST(test_a_repeat_burst_arrives_whole_and_in_order);
    RUN_TEST(test_get_key_polls_instead_of_waiting_for_the_timer);
    RUN_TEST(test_shift_and_ctrl_still_decode);
    return UNITY_END();
}
