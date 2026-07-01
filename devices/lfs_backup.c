//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  LittleFS image backup/restore. See lfs_backup.h.
//
//  On-disk image format (all multi-byte fields little-endian):
//
//    Header (28 bytes):
//      [0..7]   magic  "PLFSIMG1"
//      [8..11]  version (1)
//      [12..15] block_size (bytes)
//      [16..19] block_count (source volume geometry)
//      [20..23] stored_blocks (number of block records that follow)
//      [24..27] crc32 of bytes [0..23]
//
//    Then `stored_blocks` records, each:
//      [0..3]            block index (little-endian)
//      [4..4+block_size] block data
//      [last 4]          crc32 of the block data
//

#include "devices/lfs_backup.h"

#include <stdlib.h>
#include <string.h>

#define LFS_BACKUP_MAGIC "PLFSIMG1"
#define LFS_BACKUP_MAGIC_LEN 8
#define LFS_BACKUP_VERSION 1u
#define LFS_BACKUP_HEADER_LEN 28u // magic(8)+ver(4)+bs(4)+bc(4)+stored(4)+crc(4)
#define LFS_BACKUP_CRC_INIT 0xffffffffu

//============================================================================
// Little-endian + stream helpers
//============================================================================

static void put_u32(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

// Read exactly `n` bytes from `in`, looping over short reads. False on EOF/error.
static bool read_exact(LogoStream *in, void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    while (got < n)
    {
        int r = logo_stream_read_chars(in, (char *)p + got, (int)(n - got));
        if (r <= 0)
        {
            return false;
        }
        got += (size_t)r;
    }
    return true;
}

// Write exactly `n` bytes to `out` (binary). False if the stream flagged a
// write error (e.g. disk full).
static bool write_exact(LogoStream *out, const void *buf, size_t n)
{
    logo_stream_write_bytes(out, (const char *)buf, n);
    return !logo_stream_has_write_error(out);
}

//============================================================================
// Backup
//============================================================================

typedef struct
{
    uint8_t *bits;     // one bit per block, set if in use
    lfs_size_t count;  // number of blocks (bounds the bitset)
} BlockSet;

static int mark_block(void *data, lfs_block_t block)
{
    BlockSet *s = (BlockSet *)data;
    if (block < s->count)
    {
        s->bits[block >> 3] |= (uint8_t)(1u << (block & 7u));
    }
    return 0;
}

bool logo_lfs_backup(lfs_t *lfs, LogoStream *out)
{
    if (!lfs || !out)
    {
        return false;
    }
    const struct lfs_config *cfg = lfs->cfg;
    lfs_size_t bs = cfg->block_size;
    lfs_size_t bc = lfs->block_count; // live geometry (from the superblock)

    BlockSet set = {calloc(((size_t)bc + 7) / 8, 1), bc};
    if (!set.bits)
    {
        return false;
    }
    // The superblock anchor pair {0,1} is always essential; traverse reports it
    // too, but mark it unconditionally as cheap insurance.
    set.bits[0] |= 1u;
    if (bc > 1)
    {
        set.bits[0] |= 2u;
    }
    if (lfs_fs_traverse(lfs, mark_block, &set) < 0)
    {
        free(set.bits);
        return false;
    }

    uint32_t stored = 0;
    for (lfs_size_t i = 0; i < bc; i++)
    {
        if (set.bits[i >> 3] & (1u << (i & 7u)))
        {
            stored++;
        }
    }

    uint8_t hdr[LFS_BACKUP_HEADER_LEN];
    memcpy(hdr, LFS_BACKUP_MAGIC, LFS_BACKUP_MAGIC_LEN);
    put_u32(hdr + 8, LFS_BACKUP_VERSION);
    put_u32(hdr + 12, bs);
    put_u32(hdr + 16, bc);
    put_u32(hdr + 20, stored);
    put_u32(hdr + 24, lfs_crc(LFS_BACKUP_CRC_INIT, hdr, 24));

    uint8_t *blk = (uint8_t *)malloc(bs);
    bool ok = (blk != NULL) && write_exact(out, hdr, sizeof(hdr));

    for (lfs_size_t i = 0; ok && i < bc; i++)
    {
        if (!(set.bits[i >> 3] & (1u << (i & 7u))))
        {
            continue;
        }
        if (cfg->read(cfg, i, 0, blk, bs) < 0)
        {
            ok = false;
            break;
        }
        uint8_t meta[4];
        put_u32(meta, i);
        ok = write_exact(out, meta, 4) && write_exact(out, blk, bs);
        if (ok)
        {
            put_u32(meta, lfs_crc(LFS_BACKUP_CRC_INIT, blk, bs));
            ok = write_exact(out, meta, 4);
        }
    }

    logo_stream_flush(out);
    free(blk);
    free(set.bits);
    return ok;
}

//============================================================================
// Restore
//============================================================================

bool logo_lfs_restore(lfs_t *lfs, LogoStream *in)
{
    if (!lfs || !in)
    {
        return false;
    }
    const struct lfs_config *orig = lfs->cfg;
    lfs_size_t bs = orig->block_size;
    lfs_size_t dst_bc = orig->block_count; // this device's full geometry

    // --- header ---
    uint8_t hdr[LFS_BACKUP_HEADER_LEN];
    if (!read_exact(in, hdr, sizeof(hdr)))
    {
        return false;
    }
    if (memcmp(hdr, LFS_BACKUP_MAGIC, LFS_BACKUP_MAGIC_LEN) != 0 ||
        get_u32(hdr + 24) != lfs_crc(LFS_BACKUP_CRC_INIT, hdr, 24) ||
        get_u32(hdr + 8) != LFS_BACKUP_VERSION)
    {
        return false;
    }
    lfs_size_t src_bs = get_u32(hdr + 12);
    lfs_size_t src_bc = get_u32(hdr + 16);
    uint32_t stored = get_u32(hdr + 20);
    if (src_bs != bs || src_bc > dst_bc)
    {
        return false; // size mismatch this device cannot accommodate
    }

    long rec_start = logo_stream_get_read_pos(in);
    if (rec_start < 0)
    {
        return false; // input must be seekable for the two-pass restore
    }

    uint8_t *blk = (uint8_t *)malloc(bs);
    if (!blk)
    {
        return false;
    }

    // --- pass 1: validate the entire image before erasing any flash ---
    bool ok = true;
    for (uint32_t k = 0; ok && k < stored; k++)
    {
        uint8_t meta[4];
        if (!read_exact(in, meta, 4) || !read_exact(in, blk, bs))
        {
            ok = false;
            break;
        }
        uint32_t idx = get_u32(meta);
        if (idx >= dst_bc || !read_exact(in, meta, 4) ||
            get_u32(meta) != lfs_crc(LFS_BACKUP_CRC_INIT, blk, bs))
        {
            ok = false;
            break;
        }
    }
    if (!ok)
    {
        free(blk);
        return false; // image bad: filesystem untouched, still mounted
    }

    // --- pass 2: reflash (point of no return) ---
    if (!logo_stream_set_read_pos(in, rec_start))
    {
        free(blk);
        return false; // could not rewind: filesystem still mounted and intact
    }
    lfs_unmount(lfs);
    for (uint32_t k = 0; ok && k < stored; k++)
    {
        uint8_t imeta[4]; // block index
        uint8_t cmeta[4]; // block-data crc (already validated in pass 1)
        if (!read_exact(in, imeta, 4) || !read_exact(in, blk, bs) ||
            !read_exact(in, cmeta, 4))
        {
            ok = false;
            break;
        }
        lfs_block_t idx = get_u32(imeta);
        if (orig->erase(orig, idx) < 0 || orig->prog(orig, idx, 0, blk, bs) < 0)
        {
            ok = false;
            break;
        }
        if (orig->sync)
        {
            orig->sync(orig);
        }
    }
    free(blk);

    // --- remount, growing to fill this device if the image was smaller ---
    if (ok)
    {
        struct lfs_config tmp = *orig;
        tmp.block_count = 0; // adopt the restored superblock's count
        if (lfs_mount(lfs, &tmp) < 0)
        {
            ok = false;
        }
        else
        {
            if (dst_bc != lfs->block_count && lfs_fs_grow(lfs, dst_bc) < 0)
            {
                ok = false;
            }
            lfs_unmount(lfs);
        }
    }

    // Always leave the system with a mounted filesystem if at all possible,
    // using the real (stable) config.
    if (lfs_mount(lfs, orig) < 0)
    {
        return false;
    }
    return ok;
}
