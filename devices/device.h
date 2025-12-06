//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the LogoDevice interface for input and output devices.
//

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

    typedef struct LogoDeviceText
    {
        void (*clear)(void);
        void (*set_position)(uint8_t column, uint8_t row);
        void (*get_position)(uint8_t *column, uint8_t *row);
        void (*set_width)(uint8_t width);
        uint8_t (*get_width)(void);
    } LogoDeviceText;

    typedef struct LogoDeviceOps
    {
        int (*read_line)(void *context, char *buffer, size_t buffer_size);
        void (*write)(void *context, const char *text);
        void (*flush)(void *context);
        void (*fullscreen)(void *context);
        void (*splitscreen)(void *context);
        void (*textscreen)(void *context);
    } LogoDeviceOps;

    typedef struct LogoDevice
    {
        const LogoDeviceTurtle *turtle;
        const LogoDeviceText *text;
        const LogoDeviceOps *ops;
        void *context;
    } LogoDevice;

    void logo_device_init(LogoDevice *device, const LogoDeviceOps *ops, void *context);
    int logo_device_read_line(const LogoDevice *device, char *buffer, size_t buffer_size);
    void logo_device_write(const LogoDevice *device, const char *text);
    void logo_device_write_line(const LogoDevice *device, const char *text);
    void logo_device_flush(const LogoDevice *device);

#ifdef __cplusplus
}
#endif
