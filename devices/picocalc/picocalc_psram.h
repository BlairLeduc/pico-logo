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

    // Reprogram the QMI M1 (chip-select 1) read/write window for the PSRAM and
    // re-assert XIP_CTRL_WRITABLE_M1. Call after any flash erase/program: the
    // bootrom flash path can clear WRITABLE_M1 (PSRAM goes read-only) and may
    // disturb M1 state. This restores the controller-side config only; it does
    // NOT re-enter QPI on the chip (the chip stays in QPI across a flash op) and
    // does not re-probe size. Runs from SRAM and is safe to call with interrupts
    // already disabled. No-op effect if PSRAM was never brought up.
    void picocalc_psram_rearm_qmi(void);

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
