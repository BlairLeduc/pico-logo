//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc keyboard driver
//
//  This driver implements a simple keyboard interface for the PicoCalc
//  using the I2C bus. It handles key presses and releases, modifier keys,
//  and user interrupts.
//
//  The PicoCalc only allows for polling the keyboard, and the API is
//  limited. To support user interrupts, we need to poll the keyboard and
//  buffer the key events for when needed, except for the user interrupt
//  where we process it immediately. We use a semaphore to protect access
//  to the I2C bus and a repeating timer to poll for the key events.
//
//  We also provide functions to interact with other features in the system,
//  such as reading the battery level.
//
//  What the keyboard MCU does before we ever see a key - its register map, its
//  repeat cadence, and which modifier chords it silently swallows - is written
//  up in docs/keyboard-firmware-notes.md. Read that before adding a binding.
//

#include "pico/stdlib.h"

#include "keyboard.h"
#include "lcd.h"
#include "southbridge.h"
#include "screensaver.h"
#include "screen.h"

keyboard_key_available_callback_t keyboard_key_available_callback = NULL;
static keyboard_idle_callback_t keyboard_idle_callback = NULL;

static bool keyboard_initialised = false; // flag to indicate if the keyboard is initialised

// Modifier key states
static bool key_control = false; // control key state
static bool key_shift = false;   // shift key state
static bool key_alt = false;     // alt key state

static volatile char rx_buffer[KBD_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static repeating_timer_t key_timer;
static volatile bool poll_busy = false; // guards keyboard_poll re-entry

// Key-state view of the keyboard, maintained alongside the character ring.
//
// `readchar` hands a game a buffered character STREAM at the southbridge's
// TYPING cadence: nothing for 300 ms after a press, then one repeat per 100 ms,
// queued.  A frame loop reading one character a frame consumes slower than that
// produces, so the backlog grows and the ship keeps turning after the player let
// go; and one character a frame means two keys can never be held at once.
//
// The FIFO already carries what a game actually wants.  Every entry names a key
// code and a state, so a press can set a bit and a release clear it, and the game
// asks "is this key down NOW" instead of replaying history.  No repeat cadence is
// involved, because nothing is counted - only a level is read.
static volatile uint32_t key_down[KEY_STATE_WORDS];        // held right now
static volatile uint32_t key_hit_pending[KEY_STATE_WORDS]; // presses since the last visit
static uint32_t key_hit_latched[KEY_STATE_WORDS];          // ... handed to the reader

static inline void key_bit_set(volatile uint32_t *bits, uint8_t code)
{
    bits[code >> 5] |= 1u << (code & 31);
}

static inline void key_bit_clear(volatile uint32_t *bits, uint8_t code)
{
    bits[code >> 5] &= ~(1u << (code & 31));
}

static inline bool key_bit_test(const volatile uint32_t *bits, uint8_t code)
{
    return (bits[code >> 5] & (1u << (code & 31))) != 0;
}

//
//  Keyboard Driver
//
//  This section implements the keyboard driver, which polls the
//  keyboard for key events and buffers them for processing. It uses
//  a repeating timer to poll the keyboard at regular intervals.
//

// Push a decoded character into the ring.  Drops the key when the ring is
// full: letting the head lap the tail would throw away the whole buffered
// backlog rather than the one key we cannot fit.
static void kbd_push(char ch)
{
    uint16_t next_head = (rx_head + 1) & (KBD_BUFFER_SIZE - 1);
    if (next_head == rx_tail)
    {
        return; // ring full - drop the key
    }
    rx_buffer[rx_head] = ch;
    rx_head = next_head;
}

// Read and decode one entry from the southbridge FIFO.
// Returns false when the FIFO is empty.
static bool keyboard_poll_once(void)
{
    uint16_t key = sb_read_keyboard();
    uint8_t key_state = (key >> 8) & 0xFF;
    uint8_t key_code = key & 0xFF;

    if (key_state == KEY_STATE_IDLE)
    {
        return false; // FIFO empty
    }

    // Maintain the key-state view first: it wants the key code, before the
    // modifier decoding below folds it into a character, and it has to see every
    // event rather than only the ones that produce a character.  A press latches
    // `hit` only when it finds the key up, so the firmware's 100 ms repeats of a
    // held key do not read as a stream of fresh presses.
    //
    // Enter is the one code that is translated here too: the character path
    // below turns the firmware's LF into CR, so a game asking `keydown? 13`
    // would otherwise find a key nothing can ever press.
    uint8_t state_code = (key_code == KEY_ENTER) ? KEY_RETURN : key_code;
    if (key_state == KEY_STATE_PRESSED || key_state == KEY_STATE_HOLD)
    {
        if (!key_bit_test(key_down, state_code))
        {
            key_bit_set(key_hit_pending, state_code);
        }
        key_bit_set(key_down, state_code);
    }
    else if (key_state == KEY_STATE_RELEASED)
    {
        key_bit_clear(key_down, state_code);
    }

    if (key_state == KEY_STATE_PRESSED)
    {
        if (key_code == KEY_MOD_CTRL)
        {
            key_control = true;
        }
        else if (key_code == KEY_MOD_SHL || key_code == KEY_MOD_SHR)
        {
            key_shift = true;
        }
        else if (key_code == KEY_MOD_ALT)
        {
            key_alt = true;
        }
        else if (key_code == KEY_BREAK)
        {
            user_interrupt = true; // set user interrupt flag
            // Don't add to buffer - keyboard_get_key() will synthesize KEY_BREAK
            // when it sees user_interrupt is set
        }
        else if (key_code == KEY_F9)
        {
            // F9 requests pause during execution (not during input)
            if (!input_active)
            {
                pause_requested = true;
            }
            // Don't buffer F9 - it's handled via the flag
        }
        else if (key_code == KEY_F4)
        {
            // F4 requests freeze during execution (not during input)
            if (!input_active)
            {
                freeze_requested = true;
            }
            // Don't buffer F4 - it's handled via the flag
        }
        else if (key_code == KEY_F1 || key_code == KEY_F2 || key_code == KEY_F3)
        {
            // During execution (input_active=false), switch screen mode immediately
            // When input is active (editor or line input), just buffer the key
            // and let the input handler decide what to do
            if (!input_active)
            {
                screen_handle_mode_key(key_code);
            }
            // Always buffer the key so input handlers can respond
            kbd_push(key_code);
        }
        else if (key_code == KEY_CAPS_LOCK)
        {
            // do nothing, processed in the south bridge
        }
        else
        {
            // An ordinary key: decode it against the latched modifiers and
            // buffer it for the reader.
            uint8_t ch = key_code;
            if ((ch >= 'a' && ch <= 'z') || ch == ',' || ch == '.') // Ctrl and Shift handling
            {
                if (key_control)
                {
                    ch &= 0x1F; // convert to control character
                }
                if (key_shift)
                {
                    ch &= ~0x20;
                }
            }
            else if (ch == KEY_ENTER) // enter key is returned as LF
            {
                ch = KEY_RETURN; // convert LF to CR
            }
            else if (key_control && (ch == KEY_LEFT || ch == KEY_RIGHT))
            {
                // Ctrl + arrow: a word move.  The southbridge sends the ctrl
                // and the arrow as separate events, so without folding them
                // here the reader cannot tell ctrl + left from left.
                ch = (ch == KEY_LEFT) ? KEY_WORD_LEFT : KEY_WORD_RIGHT;
            }

            kbd_push(ch);

            // Notify that characters are available
            if (keyboard_key_available_callback)
            {
                keyboard_key_available_callback();
            }
        }
    }
    else if (key_state == KEY_STATE_RELEASED)
    {
        if (key_code == KEY_MOD_CTRL)
        {
            key_control = false;
        }
        else if (key_code == KEY_MOD_SHL || key_code == KEY_MOD_SHR)
        {
            key_shift = false;
        }
    }
    // KEY_STATE_HOLD is deliberately ignored.  The southbridge already turns a
    // held key into repeated KEY_STATE_PRESSED events for printable ASCII,
    // enter, tab, del, backspace and the arrows; it sends a bare HOLD only for
    // the keys that must not auto-repeat (F-keys, ESC, BREAK, Home/End/PgUp/
    // PgDn).  We consume the event so it cannot back the FIFO up.

    return true;
}

// Drain the southbridge FIFO.
//
// The southbridge repeats a held key every 100 ms (KEY_HOLD_TIME is 300 ms,
// then one repeat per 100 ms) into a 31-entry FIFO.  Taking a single entry per
// KEYBOARD_POLL_MS tick gave the consumer exactly the producer's rate and no
// headroom: every press/release pair, every ignored HOLD and every tick skipped
// because the I2C bus was busy added to a backlog that could never be worked
// off, and the two free-running 100 ms clocks beat against each other.  Drain
// what is actually waiting instead, bounded so a stuck FIFO cannot hold the
// timer IRQ for long.
static void keyboard_drain(void)
{
    for (int i = 0; i < KEYBOARD_DRAIN_MAX && keyboard_poll_once(); i++)
    {
        // keyboard_poll_once() does the work; the loop just bounds the drain.
    }
}

// The background timer's and the idle loop's way in: drain into the character
// ring. Games use keyboard_poll_keys() below instead.
void keyboard_poll(void)
{
    if (poll_busy)
    {
        return; // the timer IRQ landed inside the idle-loop poll
    }
    poll_busy = true;

    keyboard_drain();

    poll_busy = false;
}

static bool on_keyboard_timer(repeating_timer_t *rt)
{
    if (!sb_available())
    {
        return true; // if southbridge is not available, skip this timer tick
    }

    keyboard_poll();

    return true; // continue the timer
}

//
// Keyboard API
//

bool keyboard_key_available()
{
    return rx_head != rx_tail;
}

char keyboard_peek_key()
{
    if (rx_head == rx_tail)
    {
        return 0;  // No key available
    }
    return rx_buffer[rx_tail];
}

char keyboard_get_key()
{
    // Flush any pending graphics before blocking on keyboard input
    screen_gfx_flush();

    // Wait for a key, running the screen saver while idle
    uint64_t next_poll = 0;
    while (!keyboard_key_available())
    {
        // Poll here as well as from the timer.  The timer runs at
        // KEYBOARD_POLL_MS, which is exactly the period the southbridge repeats
        // a held key at, so leaning on it alone put up to a full repeat period
        // of jitter on every repeated character - the reason key repeat felt
        // uneven.  We are blocked with nothing else to do, so poll faster here
        // in thread context, where the ~5 ms I2C read costs us nothing.
        uint64_t now = time_us_64();
        if (now >= next_poll)
        {
            keyboard_poll();
            next_poll = now + (uint64_t)KEYBOARD_IDLE_POLL_MS * 1000u;
        }

        // Check if user pressed BREAK (interrupt flag set but buffer might be full)
        if (user_interrupt)
        {
            user_interrupt = false;  // Clear the flag
            if (screensaver_on_key_press()) {  // Returns true if screensaver was active
                screensaver_dismissed = true;  // Reader should do full redraw
            }
            return KEY_BREAK;
        }
        // Update screen saver (checks idle time, cycles palette if active)
        screensaver_update();
        // Blink the cursor here, in thread context; the blink timer only
        // sets a flag (the LCD must never be drawn from an IRQ).
        lcd_cursor_blink();
        // Poll `when` demons and advance autonomous turtles while we idle at
        // the prompt, so they stay live as the user types.
        if (keyboard_idle_callback)
        {
            keyboard_idle_callback();
        }
        tight_loop_contents();
    }

    // Key is available - notify screen saver to restore palette if active
    if (screensaver_on_key_press()) {  // Returns true if screensaver was active
        screensaver_dismissed = true;  // Reader should do full redraw
    }

    char ch = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) & (KBD_BUFFER_SIZE - 1);
    return ch;
}


//
// Key state API (games)
//

// Refresh the key-state view.  A game calls this once at the top of its frame;
// the two queries below are then free, so it can ask about as many keys as it
// likes without touching the 10 kHz bus again.
//
// Runs in thread context only, so `poll_busy` is false on entry and setting it
// keeps the timer IRQ off the bitmaps while the latch is swapped - the IRQ
// checks the same flag and returns rather than draining on top of us.
void keyboard_poll_keys(void)
{
    poll_busy = true;

    keyboard_drain();

    // Hand the reader every press seen since its last visit - including any the
    // background timer picked up between frames - and start a fresh set.  Doing
    // the clear here rather than in keyboard_key_hit() means a frame can ask
    // about the same key twice and get the same answer.
    for (int i = 0; i < KEY_STATE_WORDS; i++)
    {
        key_hit_latched[i] = key_hit_pending[i];
        key_hit_pending[i] = 0;
    }

    // Discard the characters those same events also buffered.  A game reading
    // key state never reads them, so leaving them queued would rebuild exactly
    // the backlog this mechanism exists to avoid, and would fire stale
    // keystrokes at whatever menu the game returns to afterwards.
    rx_tail = rx_head;

    poll_busy = false;
}

bool keyboard_key_down(uint8_t key_code)
{
    return key_bit_test(key_down, key_code);
}

bool keyboard_key_hit(uint8_t key_code)
{
    return key_bit_test(key_hit_latched, key_code);
}


//
// Keyboard Callback Setters
//

void keyboard_set_key_available_callback(keyboard_key_available_callback_t callback)
{
    keyboard_key_available_callback = callback;
}

void keyboard_set_idle_callback(keyboard_idle_callback_t callback)
{
    keyboard_idle_callback = callback;
}


void keyboard_set_background_poll(bool enable)
{
    if (enable)
    {
        // Start the repeating timer to poll the keyboard
        // poll every 100 ms for key events
        add_repeating_timer_ms(-KEYBOARD_POLL_MS, on_keyboard_timer, NULL, &key_timer);
    }
    else
    {
        // Stop the repeating timer
        cancel_repeating_timer(&key_timer);
    }
}

//
//  Initialize the keyboard driver
//

void keyboard_init(void)
{
    if (keyboard_initialised)
    {
        return; // already initialized
    }

    // Initialize the south bridge if not already done
    sb_init(); // Initialize the south bridge

    // Initialize the screen saver
    screensaver_init();

    keyboard_initialised = true;
}