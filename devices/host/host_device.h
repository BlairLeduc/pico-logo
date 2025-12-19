//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the host device interface for standard input and output.
//

#pragma once

#include "devices/console.h"
#include "devices/hardware.h"

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoConsole API
    //
    LogoConsole *logo_host_console_create(void);
    void logo_host_console_destroy(LogoConsole *console);

    //
    // LogoHardware API
    //
    LogoHardware *logo_host_hardware_create(void);
    void logo_host_hardware_destroy(LogoHardware *hardware);

#ifdef __cplusplus
}
#endif
