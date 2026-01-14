//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Main entry point for the host Logo interpreter.
//

#include <stdio.h>
#include <stdlib.h>

#include "devices/console.h"
#include "devices/io.h"
#include "devices/storage.h"
#include "devices/host/host_console.h"
#include "devices/host/host_storage.h"
#include "devices/host/host_hardware.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "core/repl.h"
#include "core/variables.h"

// Forward declaration for the I/O setter
extern void primitives_set_io(LogoIO *io);

int main(void)
{
    // Initialise the console for I/O
    LogoConsole *console = logo_host_console_create();
    if (!console)
    {
        fprintf(stderr, "Failed to create console\n");
        return EXIT_FAILURE;
    }

    // Initialise the storage for file I/O
    LogoStorage *storage = logo_host_storage_create();
    if (!storage)
    {
        fprintf(stderr, "Failed to create storage\n");
        logo_host_console_destroy(console);
        return EXIT_FAILURE;
    }

    // Initialise the hardware (abstraction layer)
    LogoHardware *hardware = logo_host_hardware_create();
    if (!hardware)
    {
        fprintf(stderr, "Failed to create hardware\n");
        logo_host_storage_destroy(storage);
        logo_host_console_destroy(console);
        return EXIT_FAILURE;
    }

    // Initialise the I/O manager
    LogoIO io;
    logo_io_init(&io, console, storage, hardware);
    snprintf(io.prefix, sizeof io.prefix, "/Logo/"); // Default prefix
    
    // Initialise Logo subsystems
    logo_mem_init();
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
    logo_io_write_line(&io, "Copyright 2025-2026 Blair Leduc");
    logo_io_write_line(&io, "Welcome to Pico Logo.");

    // Run the main REPL in a loop (empty prefix for top level)
    // throw "toplevel exits the current REPL, but we restart it
    // Only exit on EOF (Ctrl+D on host)
    while (1)
    {
        ReplState repl;
        repl_init(&repl, &io, REPL_FLAGS_FULL, "");
        Result r = repl_run(&repl);
        
        // Exit on EOF, restart on throw "toplevel or other
        if (r.status == RESULT_EOF)
        {
            break;
        }
        // Otherwise (throw "toplevel), just restart the REPL
        // Workspace is preserved
    }

    // Cleanup
    logo_io_cleanup(&io);
    logo_host_hardware_destroy(hardware);
    logo_host_storage_destroy(storage);
    logo_host_console_destroy(console);

    return EXIT_SUCCESS;
}
