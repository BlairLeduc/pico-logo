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
void keyboard_poll(void)
{
    if (poll_busy)
    {
        return; // the timer IRQ landed inside the idle-loop poll
    }
    poll_busy = true;

    for (int i = 0; i < KEYBOARD_DRAIN_MAX && keyboard_poll_once(); i++)
    {
        // keyboard_poll_once() does the work; the loop just bounds the drain.
    }

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