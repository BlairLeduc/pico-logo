//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a host device that uses standard input and output.
// 

#include "devices/host/host_device.h"
#include "devices/console.h"
#include "devices/hardware.h"
#include "devices/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

//
// Host console context - shared between input and output streams
//
typedef struct LogoContext
{
    FILE *input;
    FILE *output;
#ifndef _WIN32
    struct termios original_termios;
    bool termios_saved;
#endif
} LogoContext;

#ifndef _WIN32
// Put terminal into raw mode for single-character input without echo
static void set_raw_mode(LogoContext *ctx)
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
static void restore_mode(LogoContext *ctx)
{
    if (ctx->termios_saved)
    {
        tcsetattr(fileno(ctx->input), TCSANOW, &ctx->original_termios);
    }
}
#endif

//
// Stream operations for keyboard input
//

static int host_input_read_char(LogoStream *stream)
{
    LogoContext *ctx = (LogoContext *)stream->context;
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

static int host_input_read_chars(LogoStream *stream, char *buffer, int count)
{
    LogoContext *ctx = (LogoContext *)stream->context;
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

static int host_input_read_line(LogoStream *stream, char *buffer, size_t size)
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

static bool host_input_can_read(LogoStream *stream)
{
    LogoContext *ctx = (LogoContext *)stream->context;
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

//
// Stream operations for screen output
//

static void host_output_write(LogoStream *stream, const char *text)
{
    LogoContext *ctx = (LogoContext *)stream->context;
    if (!ctx || !text)
    {
        return;
    }

    fputs(text, ctx->output);
}

static void host_output_flush(LogoStream *stream)
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

LogoConsole *logo_host_console_create(void)
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
#ifndef _WIN32
    context->termios_saved = false;
#endif

    logo_console_init(console, &host_input_ops, &host_output_ops, context);
    
    return console;
}

void logo_host_console_destroy(LogoConsole *console)
{
    if (!console)
    {
        return;
    }

    free(console->context);
    free(console);
}

//
// LogoHardware API
//

static void host_hardware_sleep(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

static uint32_t host_hardware_random(void)
{
    static bool seeded = false;
    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = true;
    }
    // Generate a 32-bit random number from multiple rand() calls
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

static void host_hardware_get_battery_level(int *level, bool *charging)
{
    // Host doesn't have a battery, report 100% and not charging
    if (level)
    {
        *level = 100;
    }
    if (charging)
    {
        *charging = false;
    }
}

// User interrupt flag for host (can be set by signal handler if needed)
static volatile bool host_user_interrupt = false;

static bool host_hardware_check_user_interrupt(void)
{
    return host_user_interrupt;
}

static void host_hardware_clear_user_interrupt(void)
{
    host_user_interrupt = false;
}

static LogoHardwareOps host_hardware_ops = {
    .sleep = host_hardware_sleep,
    .random = host_hardware_random,
    .get_battery_level = host_hardware_get_battery_level,
    .check_user_interrupt = host_hardware_check_user_interrupt,
    .clear_user_interrupt = host_hardware_clear_user_interrupt,
};

LogoHardware *logo_host_hardware_create(void)
{
    LogoHardware *hardware = (LogoHardware *)malloc(sizeof(LogoHardware));
    if (!hardware)
    {
        return NULL;
    }

    logo_hardware_init(hardware, &host_hardware_ops);
    return hardware;
}

void logo_host_hardware_destroy(LogoHardware *hardware)
{
    if (!hardware)
    {
        return;
    }

    free(hardware);
}
