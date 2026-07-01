//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  mklfsimg: build a LittleFS backup image (the same PLFSIMG1 sparse format the
//  device's `.restore` command consumes) from a host directory tree. Copy the
//  produced image to the SD card and run `.restore "/sd/<image>` on the device
//  to populate the internal filesystem.
//

#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Geometry of the internal LittleFS volume on the *smallest* supported board
// (Pico 2 / Pico 2 W: a 2 MB region). Authoring at this size makes the image
// restorable on every board -- restore grows a smaller image to fill a larger
// device (e.g. the Pico Plus 2 W's 8 MB), but rejects a larger image onto a
// smaller device. The block size (4 KB flash sector) is identical across
// boards, which restore requires.
#define MKLFSIMG_BLOCK_SIZE  4096u
#define MKLFSIMG_BLOCK_COUNT 512u // 2 MB / 4096

// Build a sparse LittleFS backup image from the host directory tree rooted at
// `src_dir`, writing it to `out_path`. Mirrors subdirectories and regular files
// into a fresh LittleFS volume, then emits the image. Hidden entries (names
// beginning with '.') and anything that is not a directory or regular file are
// skipped.
//
// Returns true on success. On failure returns false and, if `errbuf` is
// non-NULL, writes a human-readable reason into it.
bool mklfsimg_build(const char *src_dir, const char *out_path,
                    char *errbuf, size_t errbuf_len);

#ifdef __cplusplus
}
#endif
