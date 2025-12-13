//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a host device that uses standard input and output.
//  Provides both the new LogoConsole API and the legacy LogoDevice API.
// 

#include "devices/picocalc/picocalc_device.h"
#include "devices/console.h"
#include "devices/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//
// Host console context - shared between input and output streams
//
typedef struct LogoHostContext
{
    FILE *input;
    FILE *output;
} LogoHostContext;

#ifndef _WIN32
// Put terminal into raw mode for single-character input without echo
static void set_raw_mode(LogoHostContext *ctx)
{

}

// Restore original terminal settings
static void restore_mode(LogoHostContext *ctx)
{

}
#endif

//
// Stream operations for keyboard input
//

static int host_input_read_char(LogoStream *stream)
{
    LogoHostContext *ctx = (LogoHostContext *)stream->context;
    if (!ctx)
    {
        return -1;
    }
}

static int host_input_read_chars(LogoStream *stream, char *buffer, int count)
{
    LogoHostContext *ctx = (LogoHostContext *)stream->context;
    if (!ctx || !buffer || count <= 0)
    {
        return 0;
    }

}

static int host_input_read_line(LogoStream *stream, char *buffer, size_t size)
{
    LogoHostContext *ctx = (LogoHostContext *)stream->context;
    if (!ctx || !buffer || size == 0)
    {
        return -1;
    }

    if (!fgets(buffer, (int)size, ctx->input))
    {
        return -1;
    }

    // Return length without newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
        len--;
    }
    return (int)len;
}

static bool host_input_can_read(LogoStream *stream)
{
    LogoHostContext *ctx = (LogoHostContext *)stream->context;
    if (!ctx)
    {
        return false;
    }
}

//
// Stream operations for screen output
//

static void host_output_write(LogoStream *stream, const char *text)
{
    LogoHostContext *ctx = (LogoHostContext *)stream->context;
    if (!ctx || !text)
    {
        return;
    }

    fputs(text, ctx->output);
}

static void host_output_flush(LogoStream *stream)
{
    LogoHostContext *ctx = (LogoHostContext *)stream->context;
    if (!ctx)
    {
        return;
    }

    fflush(ctx->output);
}

//
// Stream ops tables
//

static const LogoStreamOps host_input_ops = {
    .read_char = host_input_read_char,
    .read_chars = host_input_read_chars,
    .read_line = host_input_read_line,
    .can_read = host_input_can_read,
    .write = NULL,
    .flush = NULL,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = NULL,
};

static const LogoStreamOps host_output_ops = {
    .read_char = NULL,
    .read_chars = NULL,
    .read_line = NULL,
    .can_read = NULL,
    .write = host_output_write,
    .flush = host_output_flush,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = NULL,
};

//
// LogoConsole API (new)
//

LogoConsole *logo_picocalc_console_create(void)
{
    LogoConsole *console = (LogoConsole *)malloc(sizeof(LogoConsole));
    LogoHostContext *context = (LogoHostContext *)malloc(sizeof(LogoHostContext));

    if (!console || !context)
    {
        free(console);
        free(context);
        return NULL;
    }

    context->input = stdin;
    context->output = stdout;

    logo_console_init(console, &host_input_ops, &host_output_ops, context);
    
    return console;
}

void logo_picocalc_console_destroy(LogoConsole *console)
{
    if (!console)
    {
        return;
    }

    free(console->context);
    free(console);
}
