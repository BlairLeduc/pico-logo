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
#include <stdbool.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

typedef struct LogoHostDeviceContext
{
    FILE *input;
    FILE *output;
#ifndef _WIN32
    struct termios original_termios;
    bool termios_saved;
#endif
} LogoHostDeviceContext;

#ifndef _WIN32
// Put terminal into raw mode for single-character input without echo
static void set_raw_mode(LogoHostDeviceContext *ctx)
{
    if (!ctx->termios_saved && isatty(fileno(ctx->input)))
    {
        tcgetattr(fileno(ctx->input), &ctx->original_termios);
        ctx->termios_saved = true;
    }
    
    if (ctx->termios_saved)
    {
        struct termios raw = ctx->original_termios;
        raw.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
        raw.c_cc[VMIN] = 1;   // Read at least 1 character
        raw.c_cc[VTIME] = 0;  // No timeout
        tcsetattr(fileno(ctx->input), TCSANOW, &raw);
    }
}

// Restore original terminal settings
static void restore_mode(LogoHostDeviceContext *ctx)
{
    if (ctx->termios_saved)
    {
        tcsetattr(fileno(ctx->input), TCSANOW, &ctx->original_termios);
    }
}
#endif

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

static int host_device_read_char(void *context)
{
    LogoHostDeviceContext *ctx = (LogoHostDeviceContext *)context;
    if (!ctx)
    {
        return -1;
    }

#ifdef _WIN32
    return _getch();
#else
    set_raw_mode(ctx);
    int ch = fgetc(ctx->input);
    restore_mode(ctx);
    return ch;
#endif
}

static int host_device_read_chars(void *context, char *buffer, int count)
{
    LogoHostDeviceContext *ctx = (LogoHostDeviceContext *)context;
    if (!ctx || !buffer || count <= 0)
    {
        return 0;
    }

#ifdef _WIN32
    for (int i = 0; i < count; i++)
    {
        int ch = _getch();
        if (ch == EOF)
        {
            return i;
        }
        buffer[i] = (char)ch;
    }
    return count;
#else
    set_raw_mode(ctx);
    int read_count = 0;
    for (int i = 0; i < count; i++)
    {
        int ch = fgetc(ctx->input);
        if (ch == EOF)
        {
            break;
        }
        buffer[i] = (char)ch;
        read_count++;
    }
    restore_mode(ctx);
    return read_count;
#endif
}

static bool host_device_key_available(void *context)
{
    LogoHostDeviceContext *ctx = (LogoHostDeviceContext *)context;
    if (!ctx)
    {
        return false;
    }

#ifdef _WIN32
    return _kbhit() != 0;
#else
    if (!isatty(fileno(ctx->input)))
    {
        // For non-tty input (pipes, files), check if data is available
        int ch = fgetc(ctx->input);
        if (ch != EOF)
        {
            ungetc(ch, ctx->input);
            return true;
        }
        return false;
    }
    
    // Save current terminal settings
    struct termios old_termios;
    tcgetattr(fileno(ctx->input), &old_termios);
    
    // Set non-blocking mode
    struct termios new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(fileno(ctx->input), TCSANOW, &new_termios);
    
    // Use select to check if input is available
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fileno(ctx->input), &readfds);
    
    struct timeval timeout = {0, 0};  // Don't wait
    int result = select(fileno(ctx->input) + 1, &readfds, NULL, NULL, &timeout);
    
    // Restore terminal settings
    tcsetattr(fileno(ctx->input), TCSANOW, &old_termios);
    
    return result > 0;
#endif
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
#ifndef _WIN32
    context->termios_saved = false;
#endif

    static const LogoDeviceOps ops = {
        .read_line = host_device_read_line,
        .read_char = host_device_read_char,
        .read_chars = host_device_read_chars,
        .key_available = host_device_key_available,
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
