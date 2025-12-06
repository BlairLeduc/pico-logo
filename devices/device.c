//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoDevice interface functions.
//

#include "devices/device.h"

#include <stdio.h>

void logo_device_init(LogoDevice *device, const LogoDeviceOps *ops, void *context)
{
    if (!device)
    {
        return;
    }

    device->ops = ops;
    device->context = context;
}

int logo_device_read_line(const LogoDevice *device, char *buffer, size_t buffer_size)
{
    if (!device || !device->ops || !device->ops->read_line || !buffer || buffer_size == 0)
    {
        return 0;
    }

    return device->ops->read_line(device->context, buffer, buffer_size);
}

void logo_device_write(const LogoDevice *device, const char *text)
{
    if (!device || !device->ops || !device->ops->write || !text)
    {
        return;
    }

    device->ops->write(device->context, text);
}

void logo_device_write_line(const LogoDevice *device, const char *text)
{
    if (!device)
    {
        return;
    }

    if (text)
    {
        logo_device_write(device, text);
    }
    logo_device_write(device, "\n");
}

void logo_device_flush(const LogoDevice *device)
{
    if (!device || !device->ops || !device->ops->flush)
    {
        return;
    }

    device->ops->flush(device->context);
}
