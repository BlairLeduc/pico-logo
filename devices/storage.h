//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
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
    // File opening modes (used by file opener callback)
    //
    typedef enum LogoFileMode
    {
        LOGO_FILE_READ,     // Open for reading (file must exist)
        LOGO_FILE_WRITE,    // Open for writing (creates/truncates)
        LOGO_FILE_APPEND,   // Open for appending (creates if needed)
        LOGO_FILE_UPDATE,   // Open for reading and writing (file must exist)
    } LogoFileMode;

    //
    // LogoStorage interface
    // Platform-specific implementation should be provided to logo_io_init()
    //
    typedef struct LogoStorageOps
    {
        LogoStream *(*open)(const char *pathname, LogoFileMode mode);

        // Check if a file exists
        bool (*file_exists)(const char *pathname);

        // Check if a path is a directory
        bool (*dir_exists)(const char *pathname);

        // Delete a file
        bool (*file_delete)(const char *pathname);

        // Delete an empty directory
        bool (*dir_delete)(const char *pathname);

        // Rename/move a file or directory
        bool (*rename)(const char *old_path, const char *new_path);

        // Get file size in bytes, or -1 on error
        long (*file_size)(const char *pathname);
    } LogoStorageOps;

    typedef struct LogoStorage
    {
        LogoStorageOps *ops;
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
