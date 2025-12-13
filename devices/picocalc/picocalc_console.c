//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a PicoCalc device that uses standard input and output.
// 

#include "picocalc_console.h"
#include "devices/console.h"
#include "devices/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <pico/stdlib.h>
#include <pico/rand.h>

//
// Host console context - shared between input and output streams
//
typedef struct LogoContext
{
    FILE *input;
    FILE *output;
} LogoContext;

#ifndef _WIN32
// Put terminal into raw mode for single-character input without echo
static void set_raw_mode(LogoContext *ctx)
{

}

// Restore original terminal settings
static void restore_mode(LogoContext *ctx)
{

}
#endif

//
// Stream operations for keyboard input
//

static int input_read_char(LogoStream *stream)
{
    LogoContext *ctx = (LogoContext *)stream->context;
    if (!ctx)
    {
        return -1;
    }
}

static int input_read_chars(LogoStream *stream, char *buffer, int count)
{
    LogoContext *ctx = (LogoContext *)stream->context;
    if (!ctx || !buffer || count <= 0)
    {
        return 0;
    }

}

static int input_read_line(LogoStream *stream, char *buffer, size_t size)
{
    LogoContext *ctx = (LogoContext *)stream->context;
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

static bool input_can_read(LogoStream *stream)
{
    LogoContext *ctx = (LogoContext *)stream->context;
    if (!ctx)
    {
        return false;
    }
}

//
// Stream operations for screen output
//

static void output_write(LogoStream *stream, const char *text)
{
    LogoContext *ctx = (LogoContext *)stream->context;
    if (!ctx || !text)
    {
        return;
    }

    fputs(text, ctx->output);
}

static void output_flush(LogoStream *stream)
{
    LogoContext *ctx = (LogoContext *)stream->context;
    if (!ctx)
    {
        return;
    }

    fflush(ctx->output);
}

//
// Stream ops tables
//

static const LogoStreamOps picocalc_input_ops = {
    .read_char = input_read_char,
    .read_chars = input_read_chars,
    .read_line = input_read_line,
    .can_read = input_can_read,
    .write = NULL,
    .flush = NULL,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = NULL,
};

static const LogoStreamOps picocalc_output_ops = {
    .read_char = NULL,
    .read_chars = NULL,
    .read_line = NULL,
    .can_read = NULL,
    .write = output_write,
    .flush = output_flush,
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
    LogoContext *context = (LogoContext *)malloc(sizeof(LogoContext));

    if (!console || !context)
    {
        free(console);
        free(context);
        return NULL;
    }

    context->input = stdin;
    context->output = stdout;

    logo_console_init(console, &picocalc_input_ops, &picocalc_output_ops, context);
    
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
