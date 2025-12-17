//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the PicoCalc device interface for standard input and output.
//

#pragma once

#include "devices/console.h"

// Turtle definitions
#define TURTLE_HOME_X (SCREEN_WIDTH / 2.0f)  // Home position x coordinate
#define TURTLE_HOME_Y (SCREEN_HEIGHT / 2.0f) // Home position y coordinate
#define TURTLE_DEFAULT_ANGLE (0.0f)          // Default angle for turtle graphics
#define TURTLE_DEFAULT_COLOUR (127)          // Default turtle color (white)
#define TURTLE_DEFAULT_VISIBILITY (true)     // Default turtle visibility state
#define TURTLE_DEFAULT_PEN_DOWN (true)       // Default turtle pen state (down)

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // New LogoConsole API
    //

    LogoConsole *logo_picocalc_console_create(void);
    void logo_picocalc_console_destroy(LogoConsole *console);

#ifdef __cplusplus
}
#endif
