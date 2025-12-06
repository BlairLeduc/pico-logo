//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a host device that uses standard input and output
// 

#include "devices/host/host_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LogoHostDeviceContext
{
    FILE *input;
    FILE *output;
} LogoHostDeviceContext;

static int host_device_read_line(void *context, char *buffer, size_t buffer_size)
{
    LogoHostDeviceContext *ctx = (LogoHostDeviceContext *)context;
    if (!ctx || !buffer || buffer_size == 0)
    {
        return 0;
    }

    if (!fgets(buffer, (int)buffer_size, ctx->input))
    {
        return 0;
    }

    return 1;
}

static void host_device_write(void *context, const char *text)
{
    LogoHostDeviceContext *ctx = (LogoHostDeviceContext *)context;
    if (!ctx || !text)
    {
        return;
    }

    fputs(text, ctx->output);
}

static void host_device_flush(void *context)
{
    LogoHostDeviceContext *ctx = (LogoHostDeviceContext *)context;
    if (!ctx)
    {
        return;
    }

    fflush(ctx->output);
}

LogoDevice *logo_host_device_create(void)
{
    LogoDevice *device = (LogoDevice *)malloc(sizeof(LogoDevice));
    LogoHostDeviceContext *context = (LogoHostDeviceContext *)malloc(sizeof(LogoHostDeviceContext));

    if (!device || !context)
    {
        free(device);
        free(context);
        return NULL;
    }

    context->input = stdin;
    context->output = stdout;

    static const LogoDeviceOps ops = {
        .read_line = host_device_read_line,
        .write = host_device_write,
        .flush = host_device_flush,
    };

    logo_device_init(device, &ops, context);
    return device;
}

void logo_host_device_destroy(LogoDevice *device)
{
    if (!device)
    {
        return;
    }

    free(device->context);
    free(device);
}
