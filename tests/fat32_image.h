//
//  Helpers to format an in-memory FAT32 image inside the mock SD card.
//  Used by tests/test_fat32.c.
//
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Format the mock SD card as a FAT32 superfloppy (no MBR) with the given
// number of data clusters.  num_clusters MUST be >= 65525 so that the
// image is recognised as FAT32 per the Microsoft FAT spec.
//
// Re-initialises the mock SD card to a size large enough to hold the
// resulting image.  After this call the FAT, FSInfo, and root directory
// are valid and empty.
void fat32_image_format_superfloppy(uint32_t num_clusters,
                                    uint8_t  sectors_per_cluster);

// Format the mock SD card with a single MBR partition that itself holds a
// FAT32 file system.  partition_start_lba is where the FAT BPB will live.
void fat32_image_format_mbr(uint32_t num_clusters,
                            uint8_t  sectors_per_cluster,
                            uint32_t partition_start_lba);
