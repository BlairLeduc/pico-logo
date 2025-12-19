//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the PicoCalc device interface for standard input and output.
//

#pragma once

#include "devices/console.h"

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // New LogoConsole API
    //

    LogoConsole *logo_host_console_create(void);
    void logo_host_console_destroy(LogoConsole *console);

#ifdef __cplusplus
}
#endif
