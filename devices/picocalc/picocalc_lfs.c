//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  LittleFS instance over the reserved on-flash region. See picocalc_lfs.h.
//

#include "devices/picocalc/picocalc_lfs.h"
#include "devices/picocalc/picocalc_flash.h"

#include <string.h>

// --- block device callbacks: map LittleFS (block, offset) onto the flat
//     reserved region via the picocalc_flash safe-write path. ---
//
// LittleFS guarantees prog/erase alignment to prog_size / block_size, so the
// derived byte offsets are page/sector aligned as picocalc_flash_* require. The
// prog buffer is LittleFS's static SRAM cache, satisfying "program source must
// be in RAM".

static int bd_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                   void *buffer, lfs_size_t size)
{
    picocalc_flash_read(block * c->block_size + off, buffer, size);
    return LFS_ERR_OK;
}

static int bd_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                   const void *buffer, lfs_size_t size)
{
    return picocalc_flash_program(block * c->block_size + off, buffer, size)
               ? LFS_ERR_OK
               : LFS_ERR_IO;
}

static int bd_erase(const struct lfs_config *c, lfs_block_t block)
{
    return picocalc_flash_erase(block * c->block_size, c->block_size)
               ? LFS_ERR_OK
               : LFS_ERR_IO;
}

static int bd_sync(const struct lfs_config *c)
{
    (void)c; // writes are synchronous in the safe-write path
    return LFS_ERR_OK;
}

// --- static buffers (SRAM) and config ---

static uint8_t __attribute__((aligned(4))) lfs_read_buf[PICOCALC_LFS_CACHE_SIZE];
static uint8_t __attribute__((aligned(4))) lfs_prog_buf[PICOCALC_LFS_CACHE_SIZE];
static uint8_t __attribute__((aligned(4)))
lfs_lookahead_buf[PICOCALC_LFS_LOOKAHEAD_SIZE];

static const struct lfs_config lfs_cfg = {
    .read = bd_read,
    .prog = bd_prog,
    .erase = bd_erase,
    .sync = bd_sync,

    .read_size = PICOCALC_LFS_READ_SIZE,
    .prog_size = PICOCALC_LFS_PROG_SIZE,
    .block_size = PICOCALC_FLASH_SECTOR_SIZE,
    .block_count = PICOCALC_FLASH_LFS_SIZE / PICOCALC_FLASH_SECTOR_SIZE,
    .block_cycles = PICOCALC_LFS_BLOCK_CYCLES,
    .cache_size = PICOCALC_LFS_CACHE_SIZE,
    .lookahead_size = PICOCALC_LFS_LOOKAHEAD_SIZE,

    .read_buffer = lfs_read_buf,
    .prog_buffer = lfs_prog_buf,
    .lookahead_buffer = lfs_lookahead_buf,
};

static lfs_t g_lfs;
static bool g_mounted = false;

int picocalc_lfs_mount(void)
{
    if (g_mounted)
    {
        return LFS_ERR_OK;
    }

    int err = lfs_mount(&g_lfs, &lfs_cfg);
    if (err)
    {
        // Blank region (first boot after flashing) or unrecognisable contents:
        // format once, then mount. lfs_format only writes the superblock pair,
        // so this is fast (it does not erase the whole region).
        err = lfs_format(&g_lfs, &lfs_cfg);
        if (err)
        {
            return err;
        }
        err = lfs_mount(&g_lfs, &lfs_cfg);
        if (err)
        {
            return err;
        }
    }

    g_mounted = true;
    return LFS_ERR_OK;
}

void picocalc_lfs_unmount(void)
{
    if (g_mounted)
    {
        lfs_unmount(&g_lfs);
        g_mounted = false;
    }
}

lfs_t *picocalc_lfs(void)
{
    return g_mounted ? &g_lfs : NULL;
}

bool picocalc_lfs_is_mounted(void)
{
    return g_mounted;
}

// ===========================================================================
// Phase-1 bring-up self-test
// ===========================================================================
#ifdef PICOCALC_LFS_SELFTEST

#include <stdio.h>

void picocalc_lfs_selftest(void)
{
    printf("\n=== Phase 1: LittleFS bring-up ===\n");

    // Detect whether this is a fresh format vs. an existing filesystem, for a
    // clearer message (mount succeeds -> existing; needs format -> first boot).
    bool existed = (lfs_mount(&g_lfs, &lfs_cfg) == LFS_ERR_OK);
    if (existed)
    {
        lfs_unmount(&g_lfs);
    }

    int err = picocalc_lfs_mount();
    if (err)
    {
        printf("mount/format FAILED: %d\n", err);
        printf("=== Phase 1 result: FAIL ===\n\n");
        return;
    }
    printf("%s, mounted OK (%lu blocks x %lu B)\n",
           existed ? "existing FS" : "FORMATTED (first boot)",
           (unsigned long)lfs_cfg.block_count,
           (unsigned long)lfs_cfg.block_size);

    // Persistent boot counter: read, increment, write back.
    lfs_file_t f;
    uint32_t count = 0;
    err = lfs_file_open(&g_lfs, &f, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    if (err)
    {
        printf("open boot_count FAILED: %d\n", err);
        printf("=== Phase 1 result: FAIL ===\n\n");
        return;
    }
    lfs_file_read(&g_lfs, &f, &count, sizeof(count)); // 0 bytes if brand new
    uint32_t prev = count;
    count++;
    lfs_file_rewind(&g_lfs, &f);
    lfs_ssize_t wrote = lfs_file_write(&g_lfs, &f, &count, sizeof(count));
    lfs_file_close(&g_lfs, &f);

    // Verify read-back from a fresh open (forces a real round-trip).
    uint32_t check = 0;
    err = lfs_file_open(&g_lfs, &f, "boot_count", LFS_O_RDONLY);
    if (!err)
    {
        lfs_file_read(&g_lfs, &f, &check, sizeof(check));
        lfs_file_close(&g_lfs, &f);
    }

    bool ok = (wrote == (lfs_ssize_t)sizeof(count)) && (check == count) &&
              (count == prev + 1);
    printf("boot_count: %lu -> %lu (readback %lu) : %s\n",
           (unsigned long)prev, (unsigned long)count, (unsigned long)check,
           ok ? "PASS" : "FAIL");
    printf("re-flash firmware and re-run: count should keep climbing.\n");
    printf("=== Phase 1 result: %s ===\n\n", ok ? "PASS" : "FAIL");
}

#endif // PICOCALC_LFS_SELFTEST
