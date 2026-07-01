//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Whole-filesystem image backup and restore for a LittleFS volume. The image
//  is *sparse*: only the blocks currently in use (as reported by
//  lfs_fs_traverse) are stored, so a near-empty filesystem produces a small
//  file regardless of the volume's size.
//
//  These operate on a mounted lfs_t and its configured block device (via the
//  lfs_config callbacks), so they are device-independent and host-testable.
//

#pragma once

#include "third_party/littlefs/lfs.h"
#include "devices/stream.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Write a sparse image of the mounted filesystem `lfs` to the binary stream
    // `out`. `out` must support write_bytes (binary). Returns false on any block
    // read or stream write failure. The filesystem is left mounted and unchanged.
    bool logo_lfs_backup(lfs_t *lfs, LogoStream *out);

    // Restore a filesystem image from the binary stream `in` onto the block
    // device backing the currently mounted `lfs`. `in` must be seekable (the
    // whole image is validated by CRC *before* any flash is erased).
    //
    // The image's block size must match this device. The image's block count may
    // be smaller than this device's — the filesystem is grown to fill the device
    // after restoring (going the other way, a larger image onto a smaller device,
    // is rejected). On success `lfs` is left mounted on the restored filesystem.
    //
    // On a validation/fit failure the existing filesystem is left mounted and
    // intact. A failure *during* reflashing is unrecoverable (the old filesystem
    // is already gone); the function still attempts to leave `lfs` mounted and
    // returns false.
    bool logo_lfs_restore(lfs_t *lfs, LogoStream *in);

#ifdef __cplusplus
}
#endif
