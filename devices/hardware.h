//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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
    // Sound synthesizer (P8) shared types.
    //
    // A voice is 0..MAX_VOICES-1: 0-2 tone + 3 noise (left ear),
    // 4-6 tone + 7 noise (right ear). Waveform selectors are shared
    // between the core surface (setwave/wave) and the device engine.
    //
    typedef enum SoundWave
    {
        SOUND_WAVE_SQUARE = 0, // tone
        SOUND_WAVE_PULSE,      // tone, variable duty
        SOUND_WAVE_TRIANGLE,   // tone
        SOUND_WAVE_SAWTOOTH,   // tone
        SOUND_WAVE_WHITE,      // noise
        SOUND_WAVE_PERIODIC,   // noise
    } SoundWave;

    // A single resolved note/gate. The note-word parser turns a play list
    // into an array of these; tempo/octave/length/dots are already applied,
    // so the device queue holds nothing but gates. freq_hz == 0 is a rest.
    typedef struct SoundEvent
    {
        uint16_t freq_hz; // 0 = rest
        uint16_t dur_ms;
        uint8_t vol;   // 0-15
        uint8_t flags; // reserved; 0 for now
    } SoundEvent;

    // Snapshot of a voice, returned by sound_status. Backs `playing?` and
    // the `play` queue-full wait.
    typedef struct SoundStatus
    {
        bool sounding;      // envelope active (producing sound)
        uint8_t free_slots; // queue slots available for sound_queue
    } SoundStatus;

    //
    // LogoStorage interface
    // Platform-specific implementation should be provided to logo_io_init()
    //
    typedef struct LogoHardwareOps
    {
        // Sleep for specified milliseconds
        void (*sleep)(int milliseconds);

        // Monotonic millisecond clock since boot. Used to budget demon
        // polling and to advance autonomous turtle motion/animation. May be
        // NULL on devices without a clock; callers then get 0.
        uint32_t (*ticks_ms)(void);

        // Get a random 32-bit number
        uint32_t (*random)(void);

        // Get battery level as a percentage (0-100) 
        void (*get_battery_level)(int *level, bool *charging);

        // Power management functions
        bool (*power_off)(void);

        // Reboot into the USB bootloader (BOOTSEL / mass-storage mode) so a new
        // UF2 can be flashed. Does not return on real hardware. May be NULL on
        // devices that cannot do this.
        void (*reboot_bootloader)(void);

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

        //
        // Sound synthesizer (P8). All ops may be NULL on devices without an
        // audio engine (e.g. host); the sound primitives then silently
        // succeed, exactly as `toot` did before.
        //

        // Immediate note/rest on one voice: gate it now, return at once.
        // freq_hz == 0 gates the voice off (a rest, through its release).
        // vol is 0..15. Also `toot`'s backend.
        void (*sound_gate)(int voice, uint32_t freq_hz, uint32_t dur_ms, int vol);

        // Append n events to a voice's sequencer queue. Returns the number
        // actually accepted (< n when the queue fills); `play` waits and
        // retries the remainder.
        int (*sound_queue)(int voice, const SoundEvent *events, int n);

        // Snapshot a voice (sounding? / free queue slots).
        SoundStatus (*sound_status)(int voice);

        // Release + flush the voices whose bit is set in voice_mask.
        void (*sound_stop)(uint32_t voice_mask);

        // Set a voice's ADSR envelope: attack/decay/release in ms,
        // sustain level 0..15.
        void (*sound_env)(int voice, uint32_t attack_ms, uint32_t decay_ms, int sustain, uint32_t release_ms);

        // Set a voice's waveform. duty (1..99) applies to SOUND_WAVE_PULSE.
        void (*sound_wave)(int voice, int wave, int duty);

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

        // Get the MAC address of the WiFi interface
        // Returns true if MAC address was retrieved, false otherwise
        // mac_buffer should be at least 6 bytes
        bool (*wifi_get_mac)(uint8_t mac_buffer[6]);

        // Scan for available WiFi networks
        // Returns number of networks found (up to max_networks), -1 on error
        // Each network's SSID is stored in ssids[i] (max 32 chars + null)
        // Signal strength (RSSI in dBm, typically -30 to -90) stored in strengths[i]
        int (*wifi_scan)(char ssids[][33], int8_t strengths[], int max_networks);

        // Set the device's network hostname (used for mDNS `<name>.local` and
        // as the DHCP hostname). Core owns the name; the device applies it,
        // taking effect immediately if the network is already up, otherwise at
        // the next connect. May be NULL on devices without mDNS/DHCP naming.
        void (*network_set_hostname)(const char *name);

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
        // timezone_offset is the offset from UTC in hours (e.g., -5 for EST, 5.5 for IST)
        // If successful, the device's date and time should be updated
        bool (*network_ntp)(const char *server, float timezone_offset);

        //
        // TCP network connection operations (require WiFi to be connected)
        //

        // Open a TCP connection to the specified IP address and port
        // Returns an opaque connection handle on success, NULL on failure
        // timeout_ms is the connection timeout in milliseconds
        void *(*network_tcp_connect)(const char *ip_address, uint16_t port, int timeout_ms);

        // Open a TLS (HTTPS) connection to the specified hostname and port.
        // The hostname (not a resolved IP) is required for SNI and certificate
        // hostname verification; the device resolves it internally.
        // Returns an opaque connection handle on success, NULL on failure (which
        // includes a failed handshake or certificate verification).
        // The handle is used with the same network_tcp_read/write/close ops.
        void *(*network_tls_connect)(const char *hostname, uint16_t port, int timeout_ms);

        // Close a TCP connection
        void (*network_tcp_close)(void *connection);

        // Read from a TCP connection
        // Returns number of bytes read, 0 on timeout, -1 on error/closed
        // timeout_ms is the read timeout in milliseconds (0 = non-blocking)
        int (*network_tcp_read)(void *connection, char *buffer, int count, int timeout_ms);

        // Write to a TCP connection
        // Returns number of bytes written, -1 on error
        int (*network_tcp_write)(void *connection, const char *data, int count);

        // Check if data is available to read without blocking
        bool (*network_tcp_can_read)(void *connection);

        //
        // TCP server operations (require WiFi to be connected). Accepted
        // connections are used with the network_tcp_read/write/close/can_read
        // ops above, exactly like an outbound connection.
        //

        // Listen for TCP connections on a port (1-65535).
        // Returns an opaque listener handle, NULL on failure.
        void *(*network_tcp_listen)(uint16_t port);

        // Stop listening and free the listener (dropping any pending connection).
        void (*network_tcp_unlisten)(void *listener);

        // Accept a pending connection, if any (non-blocking). Returns a
        // connection handle usable with the tcp read/write/close ops, or NULL
        // when nothing is waiting. remote_ip (at least ip_size bytes, 16 is
        // enough) receives the client address in dotted-decimal form.
        void *(*network_tcp_accept)(void *listener, char *remote_ip, size_t ip_size);

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
