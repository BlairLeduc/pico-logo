//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  DEPRECATED: This file defines the legacy LogoDevice interface.
//  New code should use the LogoStream/LogoConsole/LogoIO architecture instead:
//    - devices/stream.h  - Abstract I/O stream interface
//    - devices/console.h - Physical console with keyboard/screen
//    - devices/io.h      - I/O state manager (reader/writer/dribble)
//
//  This legacy interface is kept for backward compatibility and will be
//  removed in a future version.
//

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // DEPRECATED: Use LogoConsoleTurtle from console.h instead
    typedef struct LogoDeviceTurtle
    {
        void (*clear)(void);
        void (*draw)();
        void (*move)(float distance);
        void (*home)(void);
        void (*set_position)(float x, float y);
        void (*get_position)(float *x, float *y);
        void (*set_angle)(float angle);
        float (*get_angle)(void);
        void (*set_colour)(uint16_t colour);
        uint16_t (*get_colour)(void);
        void (*set_pen_down)(bool down);
        bool (*get_pen_down)(void);
        void (*set_visibility)(bool visible);
        bool (*get_visibility)(void);
    } LogoDeviceTurtle;

    // DEPRECATED: Use LogoConsoleText from console.h instead
    typedef struct LogoDeviceText
    {
        void (*clear)(void);
        void (*set_position)(uint8_t column, uint8_t row);
        void (*get_position)(uint8_t *column, uint8_t *row);
        void (*set_width)(uint8_t width);
        uint8_t (*get_width)(void);
    } LogoDeviceText;

    // DEPRECATED: Use LogoStreamOps from stream.h instead
    typedef struct LogoDeviceOps
    {
        // Line-based input (echoes to output)
        int (*read_line)(void *context, char *buffer, size_t buffer_size);
        
        // Character-based input (does NOT echo to output)
        int (*read_char)(void *context);        // Returns char or -1 on EOF/error
        int (*read_chars)(void *context, char *buffer, int count);  // Returns count read
        
        // Check if input is available without blocking
        bool (*key_available)(void *context);
        
        // Output operations
        void (*write)(void *context, const char *text);
        void (*flush)(void *context);
        
        // Screen mode operations
        void (*fullscreen)(void *context);
        void (*splitscreen)(void *context);
        void (*textscreen)(void *context);
    } LogoDeviceOps;

    // DEPRECATED: Use LogoConsole from console.h and LogoIO from io.h instead
    typedef struct LogoDevice
    {
        const LogoDeviceTurtle *turtle;
        const LogoDeviceText *text;
        const LogoDeviceOps *ops;
        void *context;
    } LogoDevice;

    // DEPRECATED: Use logo_console_init() and logo_io_init() instead
    void logo_device_init(LogoDevice *device, const LogoDeviceOps *ops, void *context);
    
    // DEPRECATED: Use logo_io_read_line() instead
    int logo_device_read_line(const LogoDevice *device, char *buffer, size_t buffer_size);
    
    // DEPRECATED: Use logo_io_read_char() instead
    int logo_device_read_char(const LogoDevice *device);
    
    // DEPRECATED: Use logo_io_read_chars() instead
    int logo_device_read_chars(const LogoDevice *device, char *buffer, int count);
    
    // DEPRECATED: Use logo_io_key_available() instead
    bool logo_device_key_available(const LogoDevice *device);
    
    // DEPRECATED: Use logo_io_write() instead
    void logo_device_write(const LogoDevice *device, const char *text);
    
    // DEPRECATED: Use logo_io_write_line() instead
    void logo_device_write_line(const LogoDevice *device, const char *text);
    
    // DEPRECATED: Use logo_io_flush() instead
    void logo_device_flush(const LogoDevice *device);

#ifdef __cplusplus
}
#endif
