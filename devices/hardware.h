//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the LogoStream interface for abstract I/O sources and sinks.
//  Streams can represent files, serial ports, or console I/O.
//

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoStorage interface
    // Platform-specific implementation should be provided to logo_io_init()
    //
    typedef struct LogoHardwareOps
    {
        // Sleep for specified milliseconds
        void (*sleep)(int milliseconds);

        // Get a random 32-bit number
        uint32_t (*random)(void);

        // Get battery level as a percentage (0-100) 
        void (*get_battery_level)(int *level, bool *charging);

        // Power management functions
        bool (*power_off)(void);

        // Check if user interrupt has been requested
        // Returns true if interrupt was requested
        bool (*check_user_interrupt)(void);

        // Clear the user interrupt flag
        void (*clear_user_interrupt)(void);

        // Check if pause has been requested (F9 key)
        // Returns true if pause was requested
        bool (*check_pause_request)(void);

        // Clear the pause request flag
        void (*clear_pause_request)(void);

        // Check if freeze has been requested (F4 key)
        // Returns true if freeze was requested
        bool (*check_freeze_request)(void);

        // Clear the freeze request flag
        void (*clear_freeze_request)(void);

        // Play a tone
        // duration is in milliseconds, frequencies are in Hz
        // If the device is already playing a tone, block until it finishes
        void (*toot)(uint32_t duration_ms, uint32_t left_freq, uint32_t right_freq);

        //
        // WiFi operations (only available on Pico W boards with LOGO_HAS_WIFI)
        //

        // Check if WiFi is connected
        // Returns true if connected to a network
        bool (*wifi_is_connected)(void);

        // Connect to a WiFi network
        // Returns true on success, false on failure
        bool (*wifi_connect)(const char *ssid, const char *password);

        // Disconnect from the current WiFi network
        void (*wifi_disconnect)(void);

        // Get the current IP address
        // Returns true if connected and IP is valid, false otherwise
        // ip_buffer should be at least 16 bytes
        bool (*wifi_get_ip)(char *ip_buffer, size_t buffer_size);

        // Get the SSID of the connected network
        // Returns true if connected, false otherwise
        // ssid_buffer should be at least 33 bytes (32 chars + null)
        bool (*wifi_get_ssid)(char *ssid_buffer, size_t buffer_size);

        // Scan for available WiFi networks
        // Returns number of networks found (up to max_networks), -1 on error
        // Each network's SSID is stored in ssids[i] (max 32 chars + null)
        // Signal strength (RSSI in dBm, typically -30 to -90) stored in strengths[i]
        int (*wifi_scan)(char ssids[][33], int8_t strengths[], int max_networks);

        //
        // Network operations (require WiFi to be connected)
        //

        // Ping an IP address
        // Returns the round-trip time in milliseconds (e.g., 22.413) if successful, -1 on failure
        // ip_address should be in dotted-decimal notation (e.g., "192.168.1.1")
        float (*network_ping)(const char *ip_address);

        // Resolve a hostname to an IP address
        // Returns true if resolution was successful, false otherwise
        // hostname is the domain name to resolve (e.g., "www.example.com")
        // ip_buffer should be at least 16 bytes to hold the IP address in dotted-decimal notation
        bool (*network_resolve)(const char *hostname, char *ip_buffer, size_t buffer_size);

        // Synchronize with an NTP server
        // Returns true if synchronization was successful, false otherwise
        // server is the NTP server hostname (e.g., "pool.ntp.org")
        // If successful, the device's date and time should be updated
        bool (*network_ntp)(const char *server);

        //
        // Time management operations
        //

        // Get the current date
        // Returns true if successful, false otherwise
        // year: full year (e.g., 2024)
        // month: 1-12
        // day: 1-31
        bool (*get_date)(int *year, int *month, int *day);

        // Get the current time
        // Returns true if successful, false otherwise
        // hour: 0-23
        // minute: 0-59
        // second: 0-59
        bool (*get_time)(int *hour, int *minute, int *second);

        // Set the current date
        // Returns true if successful, false otherwise
        bool (*set_date)(int year, int month, int day);

        // Set the current time
        // Returns true if successful, false otherwise
        bool (*set_time)(int hour, int minute, int second);
    } LogoHardwareOps;

    typedef struct LogoHardware
    {
        LogoHardwareOps *ops;
    } LogoHardware;

    //
    // Hardware lifecycle functions
    // Implementations should provide these for their specific device.
    //

    // Initialize a hardware with streams and optional capabilities
    void logo_hardware_init(LogoHardware *hardware,
                            LogoHardwareOps *ops);

    

#ifdef __cplusplus
}
#endif
