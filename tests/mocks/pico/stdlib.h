//
//  Host-side shim for <pico/stdlib.h>.  Used only by host unit tests that
//  compile device source files (such as devices/picocalc/fat32.c) against
//  the host toolchain.
//
//  This header provides just enough of the Pico SDK surface that the
//  device file under test can compile and link.  Behaviour is provided by
//  test fixtures (e.g. tests/mock_sdcard.c).
//
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Microsecond clock.  Provided by a test fixture (e.g. tests/fake_lcd.c) so
// tests that compile a device file with a rate limiter in it can drive time.
uint64_t time_us_64(void);

// Repeating timer support — fat32.c only stores the handle and registers a
// callback at init time.  The host shim ignores the registration entirely;
// tests drive the SD-card-presence callback directly when needed.
typedef struct repeating_timer
{
    int32_t  delay_ms;
    bool   (*callback)(struct repeating_timer *);
    void    *user_data;
} repeating_timer_t;

static inline bool add_repeating_timer_ms(int32_t delay_ms,
                                          bool (*callback)(repeating_timer_t *),
                                          void *user_data,
                                          repeating_timer_t *out)
{
    if (out)
    {
        out->delay_ms = delay_ms;
        out->callback = callback;
        out->user_data = user_data;
    }
    return true;
}

static inline bool cancel_repeating_timer(repeating_timer_t *timer)
{
    if (timer)
    {
        timer->callback = NULL;
    }
    return true;
}

// Busy-wait hint.  A no-op on the host.
static inline void tight_loop_contents(void) {}

// GPIO function select.  sound.c routes two pins to the PWM slice at init;
// the host build has no pins, so the call is dropped.
#define GPIO_FUNC_PWM 4
static inline void gpio_set_function(unsigned gpio, int fn) { (void)gpio, (void)fn; }

// The SDK's "put this function in RAM" attribute.  On the host it is the
// identity, so a device file that decorates its IRQ path still compiles.
#ifndef __not_in_flash_func
#define __not_in_flash_func(f) f
#endif

// The SDK's short name for an unsigned int.  Guarded because some hosts
// already provide it from <sys/types.h>.
#ifndef _UINT_DEFINED
#define _UINT_DEFINED
typedef unsigned int uint;
#endif
