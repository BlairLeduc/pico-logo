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
