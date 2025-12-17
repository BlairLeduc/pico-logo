//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoConsole initialization and helper functions.
//

#include "console.h"

#include <string.h>

void logo_console_init(LogoConsole *console,
                       const LogoStreamOps *input_ops,
                       const LogoStreamOps *output_ops,
                       void *context)
{
    if (!console)
    {
        return;
    }

    // Initialize the input stream (keyboard)
    logo_stream_init(&console->input,
                     LOGO_STREAM_CONSOLE_INPUT,
                     input_ops,
                     context,
                     "keyboard");

    // Initialize the output stream (screen)
    logo_stream_init(&console->output,
                     LOGO_STREAM_CONSOLE_OUTPUT,
                     output_ops,
                     context,
                     "screen");

    // No optional features by default
    console->turtle = NULL;
    console->text = NULL;
    console->screen = NULL;
    console->context = context;
}

bool logo_console_has_turtle(const LogoConsole *console)
{
    return console && console->turtle != NULL;
}

bool logo_console_has_text(const LogoConsole *console)
{
    return console && console->text != NULL;
}

bool logo_console_has_screen_modes(const LogoConsole *console)
{
    return console && console->screen != NULL;
}
