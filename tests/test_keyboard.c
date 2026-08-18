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

    // Clear latched modifier state, and release the keys the key-state tests
    // hold down, so every test starts from a keyboard with nothing pressed.
    static const uint8_t held[] = {
        KEY_MOD_CTRL, KEY_MOD_SHL,
        KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_SPACE, KEY_F5,
    };
    fifo_reset();
    for (size_t i = 0; i < sizeof(held); i++)
    {
        fifo_push(KEY_STATE_RELEASED, held[i]);
    }
    while (fake_fifo_next < fake_fifo_count)
    {
        keyboard_poll_keys();
    }
    keyboard_poll_keys(); // drop the hit latch the releases left behind

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

// Ctrl + Left/Right is a word move in the editor, so the two arrows have to
// reach the reader as codes of their own rather than as a plain arrow.  It is
// ctrl and not shift because the keyboard MCU emits nothing at all for a
// shifted Left/Right: it swaps a shifted key for its alternate character, and
// the two horizontal arrows have none.
static void test_ctrl_folds_into_the_arrow_keys(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_LEFT);
    fifo_push(KEY_STATE_PRESSED, KEY_MOD_CTRL);
    fifo_push(KEY_STATE_PRESSED, KEY_LEFT);
    fifo_push(KEY_STATE_PRESSED, KEY_RIGHT);
    fifo_push(KEY_STATE_RELEASED, KEY_MOD_CTRL);
    fifo_push(KEY_STATE_PRESSED, KEY_RIGHT);

    while (fake_fifo_next < fake_fifo_count)
    {
        keyboard_poll();
    }

    char keys[8];
    int n = drain_ring(keys, 8);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_HEX8(KEY_LEFT, (uint8_t)keys[0]);
    TEST_ASSERT_EQUAL_HEX8(KEY_WORD_LEFT, (uint8_t)keys[1]);
    TEST_ASSERT_EQUAL_HEX8(KEY_WORD_RIGHT, (uint8_t)keys[2]);
    TEST_ASSERT_EQUAL_HEX8(KEY_RIGHT, (uint8_t)keys[3]);
}

//
//  Key state (games)
//
//  `readchar` is a buffered character stream at the keyboard's typing cadence,
//  which reaches a frame loop too late and only one key at a time. These pin the
//  down/pressed view keyboard_poll_keys() builds from the same FIFO events.
//

// The point of the whole mechanism: a key that is down reads as down, and stops
// reading as down the moment the release arrives - no queue in between.
static void test_a_held_key_reads_as_down_until_it_is_released(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_LEFT);
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_down(KEY_LEFT));

    // Several frames pass with nothing new in the FIFO. The key is still down.
    keyboard_poll_keys();
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_down(KEY_LEFT));

    fifo_push(KEY_STATE_RELEASED, KEY_LEFT);
    keyboard_poll_keys();
    TEST_ASSERT_FALSE(keyboard_key_down(KEY_LEFT));
}

// What readchar could never do: thrust and fire on the same frame.
static void test_two_keys_can_be_held_at_once(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_UP);
    fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    keyboard_poll_keys();

    TEST_ASSERT_TRUE(keyboard_key_down(KEY_UP));
    TEST_ASSERT_TRUE(keyboard_key_down(KEY_SPACE));
    TEST_ASSERT_FALSE(keyboard_key_down(KEY_RIGHT));
}

// The southbridge repeats a held key as further PRESSED events every 100 ms.
// Those must not read as fresh presses, or a held fire button becomes auto-fire.
static void test_a_repeat_is_not_a_new_hit(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_SPACE));

    fifo_push(KEY_STATE_PRESSED, KEY_SPACE); // firmware repeat, still held
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_down(KEY_SPACE));
    TEST_ASSERT_FALSE(keyboard_key_hit(KEY_SPACE));

    // Released and pressed again is a new hit.
    fifo_push(KEY_STATE_RELEASED, KEY_SPACE);
    fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_SPACE));
}

// A tap that begins and ends between two polls is not down by the time anyone
// looks, so the level alone would lose it. The hit latch is what catches it.
static void test_a_tap_shorter_than_a_frame_still_registers(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    fifo_push(KEY_STATE_RELEASED, KEY_SPACE);

    keyboard_poll_keys();

    TEST_ASSERT_FALSE(keyboard_key_down(KEY_SPACE));
    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_SPACE));
}

// A hit is reported for one frame, however many times that frame asks - a game
// checks several controls and must get the same answer each time.
static void test_a_hit_is_reported_once_but_readable_twice(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    keyboard_poll_keys();

    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_SPACE));
    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_SPACE)); // same frame, same answer

    keyboard_poll_keys(); // next frame
    TEST_ASSERT_FALSE(keyboard_key_hit(KEY_SPACE));
}

// A press the background timer happened to drain between frames must survive to
// the game's next poll: the two share the FIFO, and whichever gets there first
// consumes the event.
static void test_a_press_drained_by_the_background_poll_is_not_lost(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    fifo_push(KEY_STATE_RELEASED, KEY_SPACE);
    keyboard_poll(); // the timer, not the game

    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_SPACE));
}

// Polling key state must leave nothing buffered for readchar. Otherwise the
// backlog this mechanism exists to avoid rebuilds itself, and the queued keys
// fire at whatever menu the game returns to.
static void test_polling_key_state_discards_the_character_backlog(void)
{
    for (int i = 0; i < 12; i++)
    {
        fifo_push(KEY_STATE_PRESSED, KEY_SPACE);
    }
    while (fake_fifo_next < fake_fifo_count)
    {
        keyboard_poll_keys();
    }

    TEST_ASSERT_FALSE(keyboard_key_available());
}

// The southbridge sends a bare HOLD for the keys it refuses to auto-repeat
// (F-keys, ESC, Home/End/PgUp/PgDn). Those still have to read as held.
static void test_a_hold_event_counts_as_down(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_F5);
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_down(KEY_F5));
    TEST_ASSERT_TRUE(keyboard_key_hit(KEY_F5)); // the press

    fifo_push(KEY_STATE_HOLD, KEY_F5);
    keyboard_poll_keys();
    TEST_ASSERT_TRUE(keyboard_key_down(KEY_F5));
    TEST_ASSERT_FALSE(keyboard_key_hit(KEY_F5)); // still held, not pressed again
}

// The character path folds the firmware's LF into CR, so key state has to agree
// or `keydown? 13` names a key that can never be pressed.
static void test_enter_reads_as_carriage_return(void)
{
    fifo_push(KEY_STATE_PRESSED, KEY_ENTER);
    keyboard_poll_keys();

    TEST_ASSERT_TRUE(keyboard_key_down(KEY_RETURN));
    TEST_ASSERT_FALSE(keyboard_key_down(KEY_ENTER));
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
    RUN_TEST(test_ctrl_folds_into_the_arrow_keys);
    RUN_TEST(test_a_held_key_reads_as_down_until_it_is_released);
    RUN_TEST(test_two_keys_can_be_held_at_once);
    RUN_TEST(test_a_repeat_is_not_a_new_hit);
    RUN_TEST(test_a_tap_shorter_than_a_frame_still_registers);
    RUN_TEST(test_a_hit_is_reported_once_but_readable_twice);
    RUN_TEST(test_a_press_drained_by_the_background_poll_is_not_lost);
    RUN_TEST(test_polling_key_state_discards_the_character_backlog);
    RUN_TEST(test_a_hold_event_counts_as_down);
    RUN_TEST(test_enter_reads_as_carriage_return);
    return UNITY_END();
}
