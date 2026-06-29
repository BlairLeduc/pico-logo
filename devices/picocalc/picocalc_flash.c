//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Safe-write path for the reserved on-board flash region (see picocalc_flash.h
//  and docs/littlefs-filesystem-design.md section 4).
//
//  Erase/program drive the QMI that also serves the PSRAM window (chip-select 1).
//  Each operation therefore runs as a bounded stop-the-world step:
//    1. disable interrupts (a flash-resident ISR would hardfault while XIP down)
//    2. xip_cache_clean_all() — push dirty PSRAM lines to the chip first
//    3. flash_range_erase / flash_range_program (bootrom path, runs from RAM)
//    4. picocalc_psram_rearm_qmi() — bootrom can clear WRITABLE_M1 / disturb M1
//    5. xip_cache_invalidate_all() — drop now-stale flash/PSRAM cache lines
//    6. restore interrupts
//  One sector (erase) / one page (program) per window, so the interrupts-off
//  stall is bounded to a single flash operation even for large transfers.
//
//  NOTE: the source buffer passed to picocalc_flash_program() MUST live in RAM.
//  The bootrom programs directly from it while XIP is exited, so a pointer into
//  flash/XIP rodata would fault.
//

#include "devices/picocalc/picocalc_flash.h"
#include "devices/picocalc/picocalc_psram.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "hardware/regs/addressmap.h" // XIP_BASE

#include <string.h>

// The reserved region is carved off the top of flash and must leave room for
// the firmware below it. We cannot know the exact firmware size here, but we can
// at least require the region to be sector-aligned and strictly smaller than the
// board's total flash. (Per-board sizing lives in CMakePresets.json.)
_Static_assert(PICOCALC_FLASH_LFS_SIZE % PICOCALC_FLASH_SECTOR_SIZE == 0,
               "PICOCALC_FLASH_LFS_SIZE must be a multiple of the sector size");
_Static_assert(PICOCALC_FLASH_LFS_SIZE < PICO_FLASH_SIZE_BYTES,
               "PICOCALC_FLASH_LFS_SIZE must be smaller than the board's flash");

void picocalc_flash_read(uint32_t offset, void *dst, size_t len)
{
    if (!dst || offset > PICOCALC_FLASH_LFS_SIZE ||
        len > (size_t)(PICOCALC_FLASH_LFS_SIZE - offset))
    {
        return;
    }
    const uint8_t *src =
        (const uint8_t *)(XIP_BASE + PICOCALC_FLASH_LFS_OFFSET + offset);
    memcpy(dst, src, len);
}

// Erase exactly one sector at absolute flash offset `flash_offs`, bracketed by
// the safe-write recipe. RAM-resident: only the flash op itself runs with XIP
// down, but keeping the whole helper out of flash is cheap insurance.
static void __not_in_flash_func(flash_erase_one_sector)(uint32_t flash_offs)
{
    uint32_t save = save_and_disable_interrupts();
    xip_cache_clean_all();
    flash_range_erase(flash_offs, PICOCALC_FLASH_SECTOR_SIZE);
    picocalc_psram_rearm_qmi();
    xip_cache_invalidate_all();
    restore_interrupts(save);
}

// Program exactly one page from RAM buffer `data` at absolute flash offset
// `flash_offs`, bracketed by the safe-write recipe.
static void __not_in_flash_func(flash_program_one_page)(uint32_t flash_offs,
                                                        const uint8_t *data)
{
    uint32_t save = save_and_disable_interrupts();
    xip_cache_clean_all();
    flash_range_program(flash_offs, data, PICOCALC_FLASH_PAGE_SIZE);
    picocalc_psram_rearm_qmi();
    xip_cache_invalidate_all();
    restore_interrupts(save);
}

bool picocalc_flash_erase(uint32_t offset, size_t len)
{
    if ((offset % PICOCALC_FLASH_SECTOR_SIZE) != 0 ||
        (len % PICOCALC_FLASH_SECTOR_SIZE) != 0)
    {
        return false;
    }
    if (offset > PICOCALC_FLASH_LFS_SIZE ||
        len > (size_t)(PICOCALC_FLASH_LFS_SIZE - offset))
    {
        return false;
    }
    for (size_t o = 0; o < len; o += PICOCALC_FLASH_SECTOR_SIZE)
    {
        flash_erase_one_sector(PICOCALC_FLASH_LFS_OFFSET + offset + o);
    }
    return true;
}

bool picocalc_flash_program(uint32_t offset, const void *src, size_t len)
{
    if (!src || (offset % PICOCALC_FLASH_PAGE_SIZE) != 0 ||
        (len % PICOCALC_FLASH_PAGE_SIZE) != 0)
    {
        return false;
    }
    if (offset > PICOCALC_FLASH_LFS_SIZE ||
        len > (size_t)(PICOCALC_FLASH_LFS_SIZE - offset))
    {
        return false;
    }
    const uint8_t *p = (const uint8_t *)src;
    for (size_t o = 0; o < len; o += PICOCALC_FLASH_PAGE_SIZE)
    {
        flash_program_one_page(PICOCALC_FLASH_LFS_OFFSET + offset + o, p + o);
    }
    return true;
}

// ===========================================================================
// Phase-0 spike (docs/littlefs-filesystem-design.md section 4)
// ===========================================================================
#ifdef PICOCALC_FLASH_SPIKE

#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"

// Snapshot of the QMI M1 (PSRAM) window registers, used to detect whether the
// bootrom flash path disturbs them (acceptance criterion 2).
typedef struct
{
    uint32_t timing;
    uint32_t rcmd;
    uint32_t rfmt;
    uint32_t wcmd;
    uint32_t wfmt;
} m1_snapshot_t;

static void m1_capture(m1_snapshot_t *s)
{
    s->timing = qmi_hw->m[1].timing;
    s->rcmd = qmi_hw->m[1].rcmd;
    s->rfmt = qmi_hw->m[1].rfmt;
    s->wcmd = qmi_hw->m[1].wcmd;
    s->wfmt = qmi_hw->m[1].wfmt;
}

static bool m1_equal(const m1_snapshot_t *a, const m1_snapshot_t *b)
{
    return a->timing == b->timing && a->rcmd == b->rcmd && a->rfmt == b->rfmt &&
           a->wcmd == b->wcmd && a->wfmt == b->wfmt;
}

static bool writable_m1(void)
{
    return (xip_ctrl_hw->ctrl & XIP_CTRL_WRITABLE_M1_BITS) != 0;
}

bool picocalc_flash_selftest(void)
{
    printf("\n=== Phase 0: flash / PSRAM-QMI spike ===\n");

    bool psram = picocalc_psram_selftest_ok && picocalc_psram_detected_size > 0;
    printf("PSRAM: %s (id-size %u bytes)\n", psram ? "UP" : "ABSENT",
           (unsigned)picocalc_psram_detected_size);

    // --- snapshot the M1 window + WRITABLE_M1 + CS pin function BEFORE a raw,
    //     un-rearmed flash erase, so we can observe exactly what the bootrom
    //     clobbers (criteria 1 observe, 2, 3). ---
    m1_snapshot_t before, after;
    m1_capture(&before);
    bool wm1_before = writable_m1();
#ifdef PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN
    uint32_t cs_before = gpio_get_function(PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN);
#endif

    // Raw erase of sector 0 of the region: NO rearm, NO cache fix-up, so the
    // post-state reflects the bootrom alone. We rearm immediately afterward
    // (still interrupts-off) to restore PSRAM health before doing anything else.
    uint32_t save = save_and_disable_interrupts();
    flash_range_erase(PICOCALC_FLASH_LFS_OFFSET, PICOCALC_FLASH_SECTOR_SIZE);
    m1_capture(&after);
    bool wm1_after = writable_m1();
#ifdef PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN
    uint32_t cs_after = gpio_get_function(PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN);
#endif
    picocalc_psram_rearm_qmi();
    xip_cache_invalidate_all();
    restore_interrupts(save);

    // Criterion 2: M1 window registers unchanged by the bootrom.
    bool crit2 = m1_equal(&before, &after);
    printf("[2] M1 regs unchanged by flash op : %s\n", crit2 ? "PASS" : "CHANGED");
    if (!crit2)
    {
        printf("    timing %08x->%08x rcmd %08x->%08x rfmt %08x->%08x\n",
               (unsigned)before.timing, (unsigned)after.timing,
               (unsigned)before.rcmd, (unsigned)after.rcmd,
               (unsigned)before.rfmt, (unsigned)after.rfmt);
        printf("    wcmd %08x->%08x wfmt %08x->%08x\n",
               (unsigned)before.wcmd, (unsigned)after.wcmd,
               (unsigned)before.wfmt, (unsigned)after.wfmt);
    }

    // Criterion 1 (observation): did the bootrom clear WRITABLE_M1?
    printf("[1] WRITABLE_M1 before=%d after(raw)=%d -> %s\n", wm1_before,
           wm1_after, wm1_after ? "survived" : "CLEARED by bootrom (rearm needed)");

    // Criterion 3: PSRAM CS pin function survives connect_internal_flash.
#ifdef PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN
    bool crit3 = (cs_before == cs_after) && (cs_after == GPIO_FUNC_XIP_CS1);
    printf("[3] CS%u func %u->%u (XIP_CS1=%u) : %s\n",
           (unsigned)PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN, (unsigned)cs_before,
           (unsigned)cs_after, (unsigned)GPIO_FUNC_XIP_CS1,
           crit3 ? "PASS" : "CHANGED");
#else
    bool crit3 = true;
    printf("[3] CS pin macro unavailable - skipped\n");
#endif

    // Criterion 1 (restore) + flash data integrity: program a known page through
    // the FULL safe-write path, then read it back via XIP.
    static uint8_t page[PICOCALC_FLASH_PAGE_SIZE];
    static uint8_t readback[PICOCALC_FLASH_PAGE_SIZE];
    for (size_t i = 0; i < PICOCALC_FLASH_PAGE_SIZE; i++)
    {
        page[i] = (uint8_t)(i ^ 0xA5);
    }
    bool prog_ok = picocalc_flash_program(0, page, PICOCALC_FLASH_PAGE_SIZE);
    picocalc_flash_read(0, readback, PICOCALC_FLASH_PAGE_SIZE);
    bool data_ok = prog_ok &&
                   memcmp(page, readback, PICOCALC_FLASH_PAGE_SIZE) == 0;
    printf("[1b] flash program+readback integrity : %s\n",
           data_ok ? "PASS" : "FAIL");

    // PSRAM still writable after the full recipe (confirms rearm restores it).
    bool psram_ok = true;
    if (psram)
    {
        volatile uint32_t *p = (volatile uint32_t *)PICOCALC_PSRAM_BASE;
        p[0] = 0xCAFEBABEu;
        p[1] = 0x0BADF00Du;
        xip_cache_clean_all();
        xip_cache_invalidate_all();
        psram_ok = (p[0] == 0xCAFEBABEu && p[1] == 0x0BADF00Du);
        printf("[1c] PSRAM writable after recipe : %s\n",
               psram_ok ? "PASS" : "FAIL");
    }

    // Criterion 4: looped multi-sector erase with interrupts re-enabled between
    // sectors. If we get here, the system survived; report timing and hand off
    // the keyboard/network liveness check to the operator.
    absolute_time_t t0 = get_absolute_time();
    bool loop_ok = picocalc_flash_erase(0, 8u * PICOCALC_FLASH_SECTOR_SIZE);
    int64_t us = absolute_time_diff_us(t0, get_absolute_time());
    printf("[4] 8-sector chunked erase: %s, %lld us (%lld us/sector)\n",
           loop_ok ? "returned" : "BAD ARGS", (long long)us, (long long)(us / 8));
    printf("    -> verify keyboard still responds and no crash followed.\n");

    bool all = crit2 && crit3 && data_ok && psram_ok && loop_ok &&
               (psram ? wm1_after || true : true); // wm1 cleared is OK if rearm fixes it
    printf("=== Phase 0 automated result: %s ===\n\n", all ? "PASS" : "FAIL");
    return all;
}

#endif // PICOCALC_FLASH_SPIKE
