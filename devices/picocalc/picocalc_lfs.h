//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  LittleFS instance for the internal (on-flash) filesystem on the PicoCalc /
//  Pico Plus 2 W. Wraps the reserved-region safe-write path (picocalc_flash.*)
//  as a LittleFS block device and provides mount-with-format-on-first-boot.
//
//  This is the root `/` filesystem in the planned VFS (see
//  docs/littlefs-filesystem-design.md). Phase 1: bring-up + persistence.
//

#pragma once

#include "third_party/littlefs/lfs.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // LittleFS geometry / RAM config for this device. Block = flash sector.
    // Buffers live in SRAM (decision #3); footprint is read+prog+lookahead =
    // CACHE+CACHE+LOOKAHEAD bytes, plus one CACHE-sized cache per open file.
#define PICOCALC_LFS_READ_SIZE 256u
#define PICOCALC_LFS_PROG_SIZE 256u
#define PICOCALC_LFS_CACHE_SIZE 256u
#define PICOCALC_LFS_LOOKAHEAD_SIZE 32u
#define PICOCALC_LFS_BLOCK_CYCLES 500

    // Mount the internal filesystem, formatting it first if the region is blank
    // or unrecognisable (first boot after flashing). Idempotent: a no-op if
    // already mounted. Returns 0 on success or a negative LFS_ERR_* code.
    int picocalc_lfs_mount(void);

    // Unmount (flushes and releases). Safe to call when not mounted.
    void picocalc_lfs_unmount(void);

    // The mounted instance, or NULL if not currently mounted. Used by the VFS
    // storage backend (Phase 2).
    lfs_t *picocalc_lfs(void);

    // True once formatted/mounted successfully at least once this boot.
    bool picocalc_lfs_is_mounted(void);

#ifdef PICOCALC_LFS_SELFTEST
    // Phase-1 bring-up self-test: mount (format on first boot), then read /
    // increment / write a persistent boot counter and verify read-back. Prints
    // progress to stdio (the LCD). The counter persists across power cycles and
    // across firmware re-flashes (the region is outside the firmware image), so
    // a re-flash that shows the count continuing proves flash-and-debug survival.
    void picocalc_lfs_selftest(void);
#endif

#ifdef __cplusplus
}
#endif
