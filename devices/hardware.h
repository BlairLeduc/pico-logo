//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the LogoStream interface for abstract I/O sources and sinks.
//  Streams can represent files, serial ports, or console I/O.
//

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoStorage interface
    // Platform-specific implementation should be provided to logo_io_init()
    //
    typedef struct LogoHardwareOps
    {
        // Sleep for specified milliseconds
        void (*sleep)(int milliseconds);

        // Get a random 32-bit number
        uint32_t (*random)(void);

        // Get battery level as a percentage (0-100) 
        void (*get_battery_level)(int *level, bool *charging);

        // Power management functions
        bool (*power_off)(void);

        // Check if user interrupt has been requested
        // Returns true if interrupt was requested
        bool (*check_user_interrupt)(void);

        // Clear the user interrupt flag
        void (*clear_user_interrupt)(void);

        // Check if pause has been requested (F9 key)
        // Returns true if pause was requested
        bool (*check_pause_request)(void);

        // Clear the pause request flag
        void (*clear_pause_request)(void);

        // Check if freeze has been requested (F4 key)
        // Returns true if freeze was requested
        bool (*check_freeze_request)(void);

        // Clear the freeze request flag
        void (*clear_freeze_request)(void);

        // Play a tone
        // duration is in milliseconds, frequencies are in Hz
        // If the device is already playing a tone, block until it finishes
        void (*toot)(uint32_t duration_ms, uint32_t left_freq, uint32_t right_freq);
    } LogoHardwareOps;

    typedef struct LogoHardware
    {
        LogoHardwareOps *ops;
    } LogoHardware;

    //
    // Hardware lifecycle functions
    // Implementations should provide these for their specific device.
    //

    // Initialize a hardware with streams and optional capabilities
    void logo_hardware_init(LogoHardware *hardware,
                            LogoHardwareOps *ops);

    

#ifdef __cplusplus
}
#endif
