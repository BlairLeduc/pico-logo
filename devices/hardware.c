//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface.
//

#include "hardware.h"

#include <string.h>

void logo_hardware_init(LogoHardware *hardware, LogoHardwareOps *ops)
{
    if (!hardware)
    {
        return;
    }

    hardware->ops = ops;
}
