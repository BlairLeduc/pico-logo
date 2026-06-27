//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PSRAM bring-up for the Pimoroni Pico Plus 2 W (RP2350 + 8 MB APS6404 PSRAM).
//

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Detect and initialise the external PSRAM on the given QMI chip-select pin,
    // configuring the QMI so the PSRAM is memory-mapped and writable at
    // PICOCALC_PSRAM_BASE. Returns the verified usable size in bytes, or 0 if no
    // working PSRAM is found. The region is only returned non-zero after a
    // read-back self-test passes, so a wrong QMI config degrades to SRAM-only
    // rather than handing the allocator a non-functional region.
    //
    // Must run early in boot (before the region is handed to the memory system).
    size_t picocalc_psram_init(uint32_t cs_pin);

    // Diagnostics from the most recent picocalc_psram_init() call:
    //   detected size from the chip id (0 if the id probe failed), and whether
    //   the read-back self-test passed. Useful for a boot-time status line.
    extern size_t picocalc_psram_detected_size;
    extern bool picocalc_psram_selftest_ok;

    // Base address of the PSRAM window (QMI chip-select 1) on RP2350.
#define PICOCALC_PSRAM_BASE 0x11000000u

#ifdef __cplusplus
}
#endif
