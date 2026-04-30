//
//  Mock SD card backend for host unit tests.
//
//  Implements the sd_* API declared in devices/picocalc/sdcard.h on top of
//  an in-memory ramdisk.  Storage is a flat calloc()ed buffer; on Linux/
//  macOS this is lazily backed by demand-paged zero pages, so even large
//  test images (tens of MB) cost almost nothing until they are written.
//
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Initialise the mock with a given capacity (in 512-byte blocks).  Frees
// any prior image.  All blocks start as zero.
void mock_sd_init(uint32_t total_blocks);

// Tear down the mock and release its backing buffer.
void mock_sd_destroy(void);

// Toggle simulated card presence.  When false, sd_card_present() returns
// false and all I/O fails with SD_ERROR_NO_CARD.
void mock_sd_set_present(bool present);

// Direct (uninstrumented) access for test setup.
void mock_sd_write_raw(uint32_t block, const uint8_t *buffer);
void mock_sd_read_raw(uint32_t block, uint8_t *buffer);

// Total capacity in blocks.
uint32_t mock_sd_total_blocks(void);

// Pointer into the backing buffer for direct inspection / mutation.
uint8_t *mock_sd_block_ptr(uint32_t block);

// Statistics counters — useful for asserting that read-only opens do not
// trigger directory-entry writes, etc.  Reset with mock_sd_reset_stats().
uint32_t mock_sd_read_count(void);
uint32_t mock_sd_write_count(void);
void     mock_sd_reset_stats(void);
