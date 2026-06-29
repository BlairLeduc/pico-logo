//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Storage router: a LogoStorage whose ops dispatch by the leading path
//  component to one of two backends — the root filesystem and the SD-card mount
//  at "/sd". This is the VFS layer that lets LittleFS be root `/` while the FAT32
//  SD card appears under `/sd` (see docs/littlefs-filesystem-design.md section 6).
//
//  The LogoStorageOps callbacks carry no per-instance context, so the router
//  keeps its two backends + availability predicate in module state: one router
//  per process, which is all the device (and the tests) need.
//

#pragma once

#include "storage.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Name of the SD-card mount as it appears under root (path "/sd").
#define LOGO_SD_MOUNT_NAME "sd"

    // Reports whether the SD mount is currently available (card present). Used
    // only to decide whether a synthetic "sd" entry is listed under root "/".
    typedef bool (*LogoMountAvailableFn)(void);

    // Initialise `router` to dispatch by leading path component:
    //   "/sd" or "/sd/..."  -> sd_ops, with the "/sd" prefix stripped
    //                          ("/sd" -> "/", "/sd/foo" -> "/foo")
    //   anything else        -> root_ops, path unchanged
    // The "sd" segment is matched case-insensitively. A cross-mount rename (one
    // side under /sd, the other not) moves a file by streamed copy + delete;
    // cross-mount directory moves are rejected.
    void logo_storage_router_init(LogoStorage *router,
                                  const LogoStorageOps *root_ops,
                                  const LogoStorageOps *sd_ops,
                                  LogoMountAvailableFn sd_available);

#ifdef __cplusplus
}
#endif
