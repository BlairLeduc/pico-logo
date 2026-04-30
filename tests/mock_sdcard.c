//
//  Mock SD card backend implementation.  See mock_sdcard.h.
//
#include <stdlib.h>
#include <string.h>

#include "sdcard.h"
#include "mock_sdcard.h"

static uint8_t  *g_image       = NULL;
static uint32_t  g_total_blocks = 0;
static bool      g_present      = false;
static bool      g_initialised  = false;
static uint32_t  g_read_count   = 0;
static uint32_t  g_write_count  = 0;

void mock_sd_init(uint32_t total_blocks)
{
    mock_sd_destroy();
    g_image = (uint8_t *)calloc((size_t)total_blocks, SD_BLOCK_SIZE);
    g_total_blocks = total_blocks;
    g_present = true;
    g_initialised = false;
    g_read_count = 0;
    g_write_count = 0;
}

void mock_sd_destroy(void)
{
    free(g_image);
    g_image = NULL;
    g_total_blocks = 0;
    g_present = false;
    g_initialised = false;
}

void mock_sd_set_present(bool present)
{
    g_present = present;
    if (!present)
    {
        g_initialised = false;
    }
}

void mock_sd_write_raw(uint32_t block, const uint8_t *buffer)
{
    if (!g_image || block >= g_total_blocks) return;
    memcpy(g_image + (size_t)block * SD_BLOCK_SIZE, buffer, SD_BLOCK_SIZE);
}

void mock_sd_read_raw(uint32_t block, uint8_t *buffer)
{
    if (!g_image || block >= g_total_blocks) { memset(buffer, 0, SD_BLOCK_SIZE); return; }
    memcpy(buffer, g_image + (size_t)block * SD_BLOCK_SIZE, SD_BLOCK_SIZE);
}

uint32_t mock_sd_total_blocks(void) { return g_total_blocks; }

uint8_t *mock_sd_block_ptr(uint32_t block)
{
    if (!g_image || block >= g_total_blocks) return NULL;
    return g_image + (size_t)block * SD_BLOCK_SIZE;
}

uint32_t mock_sd_read_count(void)  { return g_read_count; }
uint32_t mock_sd_write_count(void) { return g_write_count; }
void     mock_sd_reset_stats(void) { g_read_count = 0; g_write_count = 0; }

//
// sd_* API implementations consumed by fat32.c
//

sd_error_t sd_card_init(void)
{
    if (!g_present) return SD_ERROR_NO_CARD;
    g_initialised = true;
    return SD_OK;
}

bool sd_card_present(void) { return g_present; }

void sd_init(void) { /* nothing to do on host */ }

bool sd_is_sdhc(void) { return true; }

sd_error_t sd_read_block(uint32_t block, uint8_t *buffer)
{
    if (!g_present)        return SD_ERROR_NO_CARD;
    if (!g_image)          return SD_ERROR_INIT_FAILED;
    if (block >= g_total_blocks) return SD_ERROR_READ_FAILED;
    memcpy(buffer, g_image + (size_t)block * SD_BLOCK_SIZE, SD_BLOCK_SIZE);
    g_read_count++;
    return SD_OK;
}

sd_error_t sd_write_block(uint32_t block, const uint8_t *buffer)
{
    if (!g_present)        return SD_ERROR_NO_CARD;
    if (!g_image)          return SD_ERROR_INIT_FAILED;
    if (block >= g_total_blocks) return SD_ERROR_WRITE_FAILED;
    memcpy(g_image + (size_t)block * SD_BLOCK_SIZE, buffer, SD_BLOCK_SIZE);
    g_write_count++;
    return SD_OK;
}

sd_error_t sd_read_blocks(uint32_t start_block, uint32_t num_blocks, uint8_t *buffer)
{
    for (uint32_t i = 0; i < num_blocks; i++)
    {
        sd_error_t r = sd_read_block(start_block + i, buffer + (size_t)i * SD_BLOCK_SIZE);
        if (r != SD_OK) return r;
    }
    return SD_OK;
}

sd_error_t sd_write_blocks(uint32_t start_block, uint32_t num_blocks, const uint8_t *buffer)
{
    for (uint32_t i = 0; i < num_blocks; i++)
    {
        sd_error_t r = sd_write_block(start_block + i, buffer + (size_t)i * SD_BLOCK_SIZE);
        if (r != SD_OK) return r;
    }
    return SD_OK;
}

const char *sd_error_string(sd_error_t error)
{
    switch (error)
    {
    case SD_OK:                 return "OK";
    case SD_ERROR_NO_CARD:      return "No card";
    case SD_ERROR_INIT_FAILED:  return "Init failed";
    case SD_ERROR_READ_FAILED:  return "Read failed";
    case SD_ERROR_WRITE_FAILED: return "Write failed";
    default:                    return "Unknown";
    }
}
