//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Main entry point for the PicoCalc Logo interpreter.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "devices/console.h"
#include "devices/io.h"
#include "devices/storage.h"
#include "devices/stream.h"
#include "devices/picocalc/picocalc_console.h"
#include "devices/picocalc/picocalc_storage.h"
#include "devices/picocalc/picocalc_hardware.h"
#include "devices/picocalc/picocalc.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "core/repl.h"
#include "core/variables.h"


volatile bool user_interrupt;
volatile bool pause_requested = false;  // F9 key triggers pause during execution
volatile bool freeze_requested = false; // F4 key triggers freeze during execution
volatile bool input_active = false;  // When true, keyboard_poll skips F1/F2/F3 mode switching

// Forward declaration for the I/O setter
extern void primitives_set_io(LogoIO *io);

int main(void)
{
    picocalc_init();

    // Initialise the console for I/O
    LogoConsole *console = logo_picocalc_console_create();
    if (!console)
    {
        fprintf(stderr, "Failed to create console\n");
        return EXIT_FAILURE;
    }

    // Initialise the storage for file I/O
    LogoStorage *storage = logo_picocalc_storage_create();
    if (!storage)
    {
        fprintf(stderr, "Failed to create storage\n");
        return EXIT_FAILURE;
    }

    // Initialise the hardware (abstraction layer)
    LogoHardware *hardware = logo_picocalc_hardware_create();
    if (!hardware)
    {
        fprintf(stderr, "Failed to create hardware\n");
        return EXIT_FAILURE;
    }

    // Initialize the I/O manager
    LogoIO io;
    logo_io_init(&io, console, storage, hardware);
    strcpy(io.prefix, "/Logo/"); // Default prefix
    
    // Initialize Logo subsystems
    mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    primitives_set_io(&io);

    // Load startup file if it exists (uses default prefix)
    if (logo_io_file_exists(&io, "startup"))
    {
        Lexer startup_lexer;
        Evaluator startup_eval;
        lexer_init(&startup_lexer, "load \"startup");
        eval_init(&startup_eval, &startup_lexer);
        Result r = eval_instruction(&startup_eval);
        if (r.status == RESULT_ERROR)
        {
            logo_io_write_line(&io, error_format(r));
        }
    }

    // Print welcome banner
    logo_io_write_line(&io, "Copyright 2025 Blair Leduc");
    logo_io_write_line(&io, "Welcome to Pico Logo.");

    // Run the main REPL in a loop (empty prefix for top level)
    // throw "toplevel exits the current REPL, but on device we just restart
    while (1)
    {
        ReplState repl;
        repl_init(&repl, &io, REPL_FLAGS_FULL, "");
        Result r = repl_run(&repl);
        
        // If throw "toplevel or any other reason to exit, just restart
        // On device we should never exit main()
        // The workspace is preserved across restarts
        (void)r;  // Suppress unused warning
    }

    // Should never reach here on device
    logo_io_cleanup(&io);

    return EXIT_SUCCESS;
}
