//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Host (desktop) file stream implementation using standard C FILE*.
//

#pragma once

#include "../stream.h"
#include "../io.h"  // For LogoFileMode

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // Host file stream operations
    //

    // Open a file and return a LogoStream, or NULL on error
    // The caller owns the returned stream and must call logo_stream_close()
    // when finished. The stream and its internal context are freed on close.
    LogoStream *logo_host_file_open(const char *pathname, LogoFileMode mode);

    // Check if a file exists
    bool logo_host_file_exists(const char *pathname);

    // Check if a path is a directory
    bool logo_host_dir_exists(const char *pathname);

    // Delete a file
    bool logo_host_file_delete(const char *pathname);

    // Delete an empty directory
    bool logo_host_dir_delete(const char *pathname);

    // Rename/move a file or directory
    bool logo_host_rename(const char *old_path, const char *new_path);

    // Get file size in bytes, or -1 on error
    long logo_host_file_size(const char *pathname);

#ifdef __cplusplus
}
#endif
