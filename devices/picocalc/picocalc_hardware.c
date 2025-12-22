//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface for PicoCalc device.
//

#include "../hardware.h"
#include "picocalc_hardware.h"
#include "southbridge.h"
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pico/stdlib.h>
#include <pico/rand.h>

// External reference to user interrupt flag (set by keyboard driver)
extern volatile bool user_interrupt;

// Hardware operation implementations

static void picocalc_sleep(int milliseconds)
{
    sleep_ms((uint32_t)milliseconds);
}

static uint32_t picocalc_random(void)
{
    return get_rand_32();
}

static void picocalc_get_battery_level(int *level, bool *charging)
{
    // PicoCalc does not have a battery, return 100%
    int raw_level = sb_read_battery();
    *level = raw_level & 0x7F;    // Mask out the charging bit
    *charging = (raw_level & 0x80) != 0; // Check if charging
}

static bool picocalc_power_off(void)
{
    // Call southbridge power off function
    if (sb_is_power_off_supported())
    {
        sb_write_power_off_delay(5);
        return true;
    }

    return false;
}

static bool picocalc_check_user_interrupt(void)
{
    return user_interrupt;
}

static void picocalc_clear_user_interrupt(void)
{
    user_interrupt = false;
}

static void picocalc_toot(uint32_t duration_ms, uint32_t left_freq, uint32_t right_freq)
{
    // Wait for any existing tone to finish before starting a new one.
    // Use a cooperative wait: poll less frequently and respect user interrupts.
    while (audio_is_playing())
    {
        if (user_interrupt)
        {
            // If the user has requested an interrupt, abort waiting and skip this toot.
            return;
        }

        // Sleep for a short period to avoid tight busy-waiting.
        sleep_ms(10);
    }
    // Play the tone (non-blocking with automatic stop after duration)
    audio_play_sound_timed(left_freq, right_freq, duration_ms);
}

static LogoHardwareOps picocalc_hardware_ops = {
    .sleep = picocalc_sleep,
    .random = picocalc_random,
    .get_battery_level = picocalc_get_battery_level,
    .power_off = picocalc_power_off,
    .check_user_interrupt = picocalc_check_user_interrupt,
    .clear_user_interrupt = picocalc_clear_user_interrupt,
    .toot = picocalc_toot,
}; 


//
// LogoStorage lifecycle functions
//

LogoHardware *logo_picocalc_hardware_create(void)
{
    LogoHardware *hardware = (LogoHardware *)malloc(sizeof(LogoHardware));

    if (!hardware)
    {
        return NULL;
    }

    logo_hardware_init(hardware, &picocalc_hardware_ops);

    return hardware;
}

void logo_picocalc_hardware_destroy(LogoHardware *hardware)
{
    if (!hardware)
    {
        return;
    }

    free(hardware);
}
