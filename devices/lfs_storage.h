//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  A LogoStorage backend backed by a mounted LittleFS instance. Device-
//  independent: it operates on whatever lfs_t* is supplied at init, so the same
//  code serves the on-flash filesystem on the device and a RAM-backed instance
//  in the host tests.
//
//  The LogoStorageOps callbacks carry no per-instance context, so the mounted
//  instance is held in module state: one LittleFS storage backend per process.
//

#pragma once

#include "storage.h"
#include "third_party/littlefs/lfs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Bind `storage` to the given mounted LittleFS instance. `lfs` must remain
    // valid (mounted) for the lifetime of the storage. Pass NULL to detach (ops
    // then fail gracefully).
    void logo_lfs_storage_init(LogoStorage *storage, lfs_t *lfs);

#ifdef __cplusplus
}
#endif
