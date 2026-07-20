//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  On-board flash storage region for the internal (LittleFS) filesystem on the
//  Pimoroni Pico Plus 2 W (RP2350, 16 MB flash + 8 MB PSRAM).
//
//  A fixed region at the TOP of flash is reserved for the filesystem and kept
//  out of the firmware image, so a normal flash-and-debug cycle (OpenOCD/picotool
//  `load`, UF2) only programs the firmware sectors and leaves this region intact.
//  Only an explicit mass erase wipes it.
//
//  Erase/program drive the QMI that also serves PSRAM (QMI chip-select 1). The
//  safe-write path here brackets each operation to keep the PSRAM window healthy
//  and the interrupts-off window bounded to a single sector. See
//  docs/littlefs-filesystem-design.md section 4.
//

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Size of the reserved filesystem region, carved off the TOP of flash.
    // Overridable per board via the PICOCALC_FLASH_LFS_SIZE CMake cache variable
    // (set in CMakePresets.json) — the Pico Plus 2 W has 16 MB of flash, a plain
    // Pico 2 / 2 W only 4 MB, so the right size differs by board. Defaults to
    // 4 MB. Must be a multiple of the sector size and smaller than the board's
    // total flash (asserted in picocalc_flash.c).
#ifndef PICOCALC_FLASH_LFS_SIZE
#define PICOCALC_FLASH_LFS_SIZE (4u * 1024u * 1024u)
#endif

    // Byte offset of the region from the start of flash (i.e. from XIP_BASE).
    // PICO_FLASH_SIZE_BYTES is provided by the board header, so the region stays
    // anchored to the top of whatever flash the board has.
#define PICOCALC_FLASH_LFS_OFFSET (PICO_FLASH_SIZE_BYTES - PICOCALC_FLASH_LFS_SIZE)

    // Erase granularity (flash sector) and program granularity (flash page).
    // These mirror the SDK's FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE.
#define PICOCALC_FLASH_SECTOR_SIZE (4096u)
#define PICOCALC_FLASH_PAGE_SIZE (256u)

    // Read `len` bytes from the reserved region at `offset` (relative to the
    // start of the region, 0 .. PICOCALC_FLASH_LFS_SIZE) into `dst`. This is a
    // plain XIP-cached read; no special handling required.
    void picocalc_flash_read(uint32_t offset, void *dst, size_t len);

    // Erase the sectors covering [offset, offset+len). `offset` and `len` must be
    // sector-aligned and within the region. Erases ONE sector per interrupts-off
    // window, re-enabling interrupts between sectors so the worst-case stall is a
    // single sector erase. Returns false on bad arguments.
    bool picocalc_flash_erase(uint32_t offset, size_t len);

    // Program `len` bytes from `src` at `offset`. `offset` and `len` must be
    // page-aligned and within the region; the target must already be erased.
    // Programs ONE page per interrupts-off window. Returns false on bad arguments.
    bool picocalc_flash_program(uint32_t offset, const void *src, size_t len);

#ifdef PICOCALC_FLASH_SPIKE
    // Phase-0 spike (docs section 4): exercise the PSRAM/QMI-vs-flash-write
    // interaction and print PASS/FAIL for the four acceptance criteria to stdio
    // (the LCD console). Must run BEFORE the PSRAM region is handed to the
    // allocator, because it scribbles a test pattern into PSRAM and
    // erases/programs the reserved flash region. Returns true iff the automated
    // criteria (1-3 + data integrity) all passed.
    bool picocalc_flash_selftest(void);
#endif

#ifdef __cplusplus
}
#endif
