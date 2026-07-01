//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Defines the LogoStream interface for abstract I/O sources and sinks.
//  Streams can represent files, serial ports, or console I/O.
//

#pragma once

#include "stream.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // Directory entry type for listing
    //
    typedef enum LogoEntryType
    {
        LOGO_ENTRY_FILE,
        LOGO_ENTRY_DIRECTORY
    } LogoEntryType;

    //
    // Callback for directory listing
    // Called for each entry in the directory
    // Return true to continue, false to stop
    //
    typedef bool (*LogoDirCallback)(const char *name, LogoEntryType type, void *user_data);

    //
    // LogoStorage interface
    // Platform-specific implementation should be provided to logo_io_init()
    //
    typedef struct LogoStorageOps
    {
        LogoStream *(*open)(const char *pathname);

        // Check if a file exists
        bool (*file_exists)(const char *pathname);

        // Check if a path is a directory
        bool (*dir_exists)(const char *pathname);

        // Delete a file
        bool (*file_delete)(const char *pathname);

        // Create a new empty directory
        bool (*dir_create)(const char *pathname);

        // Delete an empty directory
        bool (*dir_delete)(const char *pathname);

        // Rename/move a file or directory
        bool (*rename)(const char *old_path, const char *new_path);

        // Get file size in bytes, or -1 on error
        long (*file_size)(const char *pathname);

        // List directory contents
        bool (*list_directory)(const char *pathname, LogoDirCallback callback,
                               void *user_data, const char *filter);

        // Report the number of free allocation blocks on the filesystem backing
        // `pathname`, and the size of one block in bytes. A "block" is the
        // filesystem's own allocation unit (a LittleFS block or a FAT cluster),
        // so block_size differs between volumes — returning it lets callers
        // convert to bytes and compare across volumes. Returns false if the
        // volume is unavailable (e.g. no SD card). Either out-pointer may be
        // NULL. Optional op: NULL if the backend cannot report free space.
        bool (*free_blocks)(const char *pathname, uint32_t *free_blocks,
                            uint32_t *block_size);

        // Report whether the filesystem backing `pathname` is currently mounted
        // and usable (e.g. an SD card is present and mounted). Optional: a NULL
        // pointer means the backend is always considered available.
        bool (*mount_available)(const char *pathname);

        // Report whether `pathname` lives on a filesystem *other* than the
        // internal/imageable root (i.e. removable storage such as the SD card).
        // Used to ensure a whole-filesystem backup is not written to — nor a
        // restore read from — the very volume being reflashed. Optional: NULL
        // means "no external volume" (treat every path as internal).
        bool (*is_external)(const char *pathname);

        // Write a sparse whole-filesystem image of the internal (root) volume to
        // `out`. Implemented only by the imageable root backend. Optional: NULL
        // means backup is unsupported on this backend.
        bool (*fs_image_backup)(LogoStream *out);

        // Reflash the internal (root) volume from a whole-filesystem image read
        // from `in` (validated before erasing; grows to fill a larger device).
        // Implemented only by the imageable root backend. Optional: NULL means
        // restore is unsupported on this backend.
        bool (*fs_image_restore)(LogoStream *in);

    } LogoStorageOps;

    typedef struct LogoStorage
    {
        const LogoStorageOps *ops;
    } LogoStorage;

    //
    // Storage lifecycle functions
    // Implementations should provide these for their specific device.
    //

    // Initialize a storage with streams and optional capabilities
    void logo_storage_init(LogoStorage *storage,
                           const LogoStorageOps *ops);

#ifdef __cplusplus
}
#endif
