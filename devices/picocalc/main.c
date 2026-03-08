//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Main entry point for the PicoCalc Logo interpreter.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

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
volatile bool screensaver_dismissed = false;  // Set when screen saver just dismissed, reader should clear

// Forward declaration for the I/O setter
extern void primitives_set_io(LogoIO *io);

// HardFault recovery: longjmp target set in main REPL loop
static jmp_buf fault_recovery;
static volatile bool fault_recovery_active = false;

// Called in thread mode with a clean stack after HardFault
static void __attribute__((used)) hardfault_recovery(void)
{
    longjmp(fault_recovery, 1);
}

// Override the SDK's weak isr_hardfault.
// Resets MSP to __StackTop, builds a fake Cortex-M exception frame
// pointing at hardfault_recovery, and returns to thread mode.
extern uint32_t __StackTop;
void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "ldr r0, =__StackTop\n"          // Reset MSP to known-good top of stack
        "msr msp, r0\n"
        "ldr r0, =hardfault_recovery\n"  // PC = recovery function (Thumb bit set by linker)
        "movs r1, #0\n"
        "ldr r2, =0x01000000\n"          // xPSR: Thumb bit set
        "push {r2}\n"                    // [7] xPSR
        "push {r0}\n"                    // [6] PC
        "push {r1}\n"                    // [5] LR
        "push {r1}\n"                    // [4] R12
        "push {r1}\n"                    // [3] R3
        "push {r1}\n"                    // [2] R2
        "push {r1}\n"                    // [1] R1
        "push {r1}\n"                    // [0] R0
        "ldr r0, =0xFFFFFFF9\n"         // EXC_RETURN: thread mode, MSP, basic frame
        "bx r0\n"
    );
}

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
    
    // Check if the default directory exists
    bool default_dir_exists = false;
    if (storage && storage->ops->dir_exists)
    {
        default_dir_exists = storage->ops->dir_exists("/Logo");
    }
    
    // Set prefix based on whether default directory exists
    if (default_dir_exists)
    {
        strcpy(io.prefix, "/Logo/");
    }
    else
    {
        strcpy(io.prefix, "/");
    }
    
    // Initialize Logo subsystems
    logo_mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    primitives_set_io(&io);

    // Load startup file if it exists (only if default directory exists)
    if (default_dir_exists && logo_io_file_exists(&io, "startup"))
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
    
    // Warn user if default directory is missing
    if (!default_dir_exists)
    {
        logo_io_write_line(&io, "I cannot find the default directory /Logo/");
    }

    // Run the main REPL in a loop (empty prefix for top level)
    // throw "toplevel exits the current REPL, but on device we just restart
    fault_recovery_active = true;
    while (1)
    {
        if (setjmp(fault_recovery) != 0)
        {
            // Arrived here from HardFault handler — treat as error 23
            proc_reset_execution_state();
            Result err = result_error(ERR_OUT_OF_SPACE);
            logo_io_write_line(&io, error_format(err));
            // Fall through to restart the REPL
        }

        ReplState repl;
        if (!repl_init(&repl, &io, REPL_FLAGS_FULL, ""))
        {
            logo_io_write_line(&io, "Out of memory");
            continue;
        }
        Result r = repl_run(&repl);
        repl_cleanup(&repl);
        
        // If throw "toplevel or any other reason to exit, just restart
        // On device we should never exit main()
        // The workspace is preserved across restarts
        (void)r;  // Suppress unused warning
    }

    // Should never reach here on device
    logo_io_cleanup(&io);

    return EXIT_SUCCESS;
}
