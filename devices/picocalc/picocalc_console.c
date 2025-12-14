//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a PicoCalc device that uses standard input and output.
// 

#include "picocalc_console.h"
#include "devices/console.h"
#include "devices/stream.h"
#include "keyboard.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <pico/stdlib.h>
#include <pico/rand.h>

//
// Stream operations for keyboard input
//

static int input_read_char(LogoStream *stream)
{
    return keyboard_get_key();
}

static int input_read_chars(LogoStream *stream, char *buffer, int count)
{
    int n = 0;
    while (n < count)
    {
        int c = keyboard_get_key();
        buffer[n++] = (char)c;
    }
    return n;
}

static int input_read_line(LogoStream *stream, char *buffer, size_t size)
{
    return input_read_chars(stream, buffer, size - 1);
}

static bool input_can_read(LogoStream *stream)
{
    return true; // Always return true for simplicity
}

//
// Stream operations for screen output
//

static void output_write(LogoStream *stream, const char *text)
{
    screen_txt_puts(text);
}

static void output_flush(LogoStream *stream)
{
    screen_txt_update();
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
// LogoConsole API
//

LogoConsole *logo_picocalc_console_create(void)
{
    LogoConsole *console = (LogoConsole *)malloc(sizeof(LogoConsole));

    if (!console)
    {
        return NULL;
    }

    logo_console_init(console, &picocalc_input_ops, &picocalc_output_ops, NULL);
    
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
