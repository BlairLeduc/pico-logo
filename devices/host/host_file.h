//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Host (desktop) file stream implementation using standard C FILE*.
//

#pragma once

#include "../stream.h"
#include "../io.h" // For LogoFileMode

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
    LogoStream *logo_host_file_open(const char *pathname);

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


    //
    // Callback for directory listing
    // Called for each entry in the directory
    // Return true to continue, false to stop
    //
    typedef bool (*LogoDirCallback)(const char *name, LogoEntryType type, void *user_data);

    // List directory contents
    // Calls callback for each entry (file or directory) in the given directory
    // If filter is not NULL, only entries matching the filter are included:
    //   - For files: filter is an extension (e.g., "logo", "*" for all)
    //   - For directories: filter is ignored
    // Returns true on success, false on error
    bool logo_host_list_directory(const char *pathname, LogoDirCallback callback,
                                  void *user_data, const char *filter);
#ifdef __cplusplus
}
#endif
