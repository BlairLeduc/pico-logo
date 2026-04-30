//
//  Build minimal valid FAT32 images directly inside the mock SD ramdisk.
//  See fat32_image.h.
//
#include <string.h>
#include <stdlib.h>

#include "fat32.h"
#include "sdcard.h"
#include "mock_sdcard.h"
#include "fat32_image.h"

#define RESERVED_SECTORS 32u
#define NUM_FATS         2u
#define ROOT_CLUSTER     2u
#define FSINFO_SECTOR    1u

// Compute the FAT size in sectors required to address (cluster_count + 2)
// FAT entries, each 4 bytes wide.
static uint32_t compute_fat_size(uint32_t cluster_count)
{
    uint64_t bytes = ((uint64_t)cluster_count + 2) * 4;
    return (uint32_t)((bytes + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE);
}

static void write_boot_sector(uint32_t volume_start_lba,
                              uint32_t cluster_count,
                              uint8_t  sectors_per_cluster,
                              uint32_t fat_size_32,
                              uint32_t total_sectors_32)
{
    uint8_t sec[SD_BLOCK_SIZE];
    memset(sec, 0, sizeof(sec));

    fat32_boot_sector_t *bs = (fat32_boot_sector_t *)sec;
    bs->jump[0] = 0xEB;
    bs->jump[1] = 0x58;
    bs->jump[2] = 0x90;
    memcpy(bs->oem_name, "MSWIN4.1", 8);
    bs->bytes_per_sector    = SD_BLOCK_SIZE;
    bs->sectors_per_cluster = sectors_per_cluster;
    bs->reserved_sectors    = RESERVED_SECTORS;
    bs->num_fats            = NUM_FATS;
    bs->root_entries        = 0;
    bs->total_sectors_16    = 0;
    bs->media_type          = 0xF8;
    bs->fat_size_16         = 0;
    bs->sectors_per_track   = 63;
    bs->num_heads           = 255;
    bs->hidden_sectors      = volume_start_lba;
    bs->total_sectors_32    = total_sectors_32;
    bs->fat_size_32         = fat_size_32;
    bs->ext_flags           = 0;
    bs->fat32_version       = 0;
    bs->root_cluster        = ROOT_CLUSTER;
    bs->fat32_info          = FSINFO_SECTOR;
    bs->backup_boot         = 6;
    bs->drive_number        = 0x80;
    bs->boot_signature      = 0x29;
    bs->volume_id           = 0x12345678;
    memcpy(bs->volume_label,    "MOCKVOL    ", 11);
    memcpy(bs->file_system_type,"FAT32   ", 8);

    // 0x55AA boot signature
    sec[510] = 0x55;
    sec[511] = 0xAA;
    (void)cluster_count; // Used only by callers for FSInfo

    mock_sd_write_raw(volume_start_lba, sec);
}

static void write_fsinfo(uint32_t volume_start_lba, uint32_t cluster_count)
{
    uint8_t sec[SD_BLOCK_SIZE];
    memset(sec, 0, sizeof(sec));

    fat32_fsinfo_t *fsi = (fat32_fsinfo_t *)sec;
    fsi->lead_sig   = 0x41615252u;
    fsi->struc_sig  = 0x61417272u;
    fsi->free_count = cluster_count - 1; // root cluster occupies one
    fsi->next_free  = 3;                 // cluster 2 = root, start search at 3
    fsi->trail_sig  = 0xAA550000u;

    mock_sd_write_raw(volume_start_lba + FSINFO_SECTOR, sec);
}

static void write_initial_fat(uint32_t volume_start_lba, uint32_t fat_size_32)
{
    uint8_t sec[SD_BLOCK_SIZE];
    memset(sec, 0, sizeof(sec));

    // First FAT sector contains the reserved entries and the EOC for the
    // root cluster (cluster 2).  All other FAT sectors are zero (free).
    uint32_t *entries = (uint32_t *)sec;
    entries[0] = 0x0FFFFFF8u; // media descriptor + reserved bits
    entries[1] = 0x0FFFFFFFu; // EOC marker
    entries[2] = 0x0FFFFFFFu; // root directory cluster — single-cluster chain

    for (uint8_t fat_idx = 0; fat_idx < NUM_FATS; fat_idx++)
    {
        uint32_t fat_start = volume_start_lba + RESERVED_SECTORS + fat_idx * fat_size_32;
        // Write the populated first sector
        mock_sd_write_raw(fat_start, sec);
        // The remaining FAT sectors are already zeroed by calloc.
        (void)0;
    }
}

static void clear_root_cluster(uint32_t volume_start_lba,
                               uint32_t fat_size_32,
                               uint8_t  sectors_per_cluster)
{
    // calloc already zeroed the data region, but be explicit so future
    // changes to the mock don't surprise tests.
    uint32_t first_data_sector = volume_start_lba + RESERVED_SECTORS + NUM_FATS * fat_size_32;
    uint8_t zero[SD_BLOCK_SIZE] = {0};
    for (uint32_t i = 0; i < sectors_per_cluster; i++)
    {
        mock_sd_write_raw(first_data_sector + i, zero);
    }
}

void fat32_image_format_superfloppy(uint32_t num_clusters,
                                    uint8_t  sectors_per_cluster)
{
    if (num_clusters < 65525u)        num_clusters = 65525u;
    if (sectors_per_cluster == 0)     sectors_per_cluster = 1;

    uint32_t fat_size = compute_fat_size(num_clusters);
    uint32_t data_sectors = num_clusters * sectors_per_cluster;
    uint32_t total_sectors = RESERVED_SECTORS + NUM_FATS * fat_size + data_sectors;

    mock_sd_init(total_sectors);

    write_boot_sector(0, num_clusters, sectors_per_cluster, fat_size, total_sectors);
    write_fsinfo(0, num_clusters);
    write_initial_fat(0, fat_size);
    clear_root_cluster(0, fat_size, sectors_per_cluster);
}

void fat32_image_format_mbr(uint32_t num_clusters,
                            uint8_t  sectors_per_cluster,
                            uint32_t partition_start_lba)
{
    if (num_clusters < 65525u)    num_clusters = 65525u;
    if (sectors_per_cluster == 0) sectors_per_cluster = 1;
    if (partition_start_lba == 0) partition_start_lba = 2048;

    uint32_t fat_size = compute_fat_size(num_clusters);
    uint32_t data_sectors = num_clusters * sectors_per_cluster;
    uint32_t partition_sectors = RESERVED_SECTORS + NUM_FATS * fat_size + data_sectors;
    uint32_t total_sectors = partition_start_lba + partition_sectors;

    mock_sd_init(total_sectors);

    // Write MBR at sector 0
    uint8_t mbr[SD_BLOCK_SIZE];
    memset(mbr, 0, sizeof(mbr));
    // Bootstrap stub — non-zero filler bytes that previously fooled
    // is_sector_mbr() into accepting random data as a partition.
    mbr[0] = 0xFA;            // CLI
    mbr[1] = 0x33; mbr[2] = 0xC0;
    // Partition entry 0: bootable, type 0x0C (FAT32 LBA)
    uint8_t *pe = mbr + 446;
    pe[0]  = 0x80;
    pe[4]  = 0x0C;
    // start_lba (little-endian)
    pe[8]  = (uint8_t)(partition_start_lba       & 0xFF);
    pe[9]  = (uint8_t)((partition_start_lba >> 8)  & 0xFF);
    pe[10] = (uint8_t)((partition_start_lba >> 16) & 0xFF);
    pe[11] = (uint8_t)((partition_start_lba >> 24) & 0xFF);
    // size
    pe[12] = (uint8_t)(partition_sectors       & 0xFF);
    pe[13] = (uint8_t)((partition_sectors >> 8)  & 0xFF);
    pe[14] = (uint8_t)((partition_sectors >> 16) & 0xFF);
    pe[15] = (uint8_t)((partition_sectors >> 24) & 0xFF);
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    mock_sd_write_raw(0, mbr);

    write_boot_sector(partition_start_lba, num_clusters, sectors_per_cluster,
                      fat_size, partition_sectors);
    write_fsinfo(partition_start_lba, num_clusters);
    write_initial_fat(partition_start_lba, fat_size);
    clear_root_cluster(partition_start_lba, fat_size, sectors_per_cluster);
}
