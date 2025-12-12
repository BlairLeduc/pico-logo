//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the host device interface for standard input and output.
//

#pragma once

#include "devices/device.h"
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

    //
    // Legacy LogoDevice API (for backward compatibility)
    //
    LogoDevice *logo_host_device_create(void);
    void logo_host_device_destroy(LogoDevice *device);

#ifdef __cplusplus
}
#endif
