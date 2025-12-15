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
