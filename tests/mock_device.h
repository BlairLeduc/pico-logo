//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Mock device for testing turtle graphics and text screen primitives.
//  Provides trackable state and command history for verification in tests.
//

#pragma once

#include "devices/console.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // Turtle boundary mode enumeration
    //
    typedef enum MockTurtleBoundaryMode
    {
        MOCK_BOUNDARY_FENCE,   // Turtle stops at boundary (error if hits edge)
        MOCK_BOUNDARY_WINDOW,  // Turtle can go off-screen (unbounded)
        MOCK_BOUNDARY_WRAP     // Turtle wraps around edges
    } MockTurtleBoundaryMode;

    //
    // Pen mode enumeration
    //
    typedef enum MockPenMode
    {
        MOCK_PEN_DOWN,     // Normal drawing
        MOCK_PEN_UP,       // No drawing
        MOCK_PEN_ERASE,    // Erases as it moves
        MOCK_PEN_REVERSE   // Reverses colors
    } MockPenMode;

    //
    // Screen mode enumeration
    //
    typedef enum MockScreenMode
    {
        MOCK_SCREEN_TEXT,       // Full text mode
        MOCK_SCREEN_SPLIT,      // Split: graphics top, text bottom
        MOCK_SCREEN_FULLSCREEN  // Full graphics mode
    } MockScreenMode;

    //
    // Command types for history tracking
    //
    typedef enum MockCommandType
    {
        MOCK_CMD_NONE,
        // Turtle movement
        MOCK_CMD_MOVE,
        MOCK_CMD_HOME,
        MOCK_CMD_SET_POSITION,
        MOCK_CMD_SET_HEADING,
        // Turtle appearance
        MOCK_CMD_SET_PEN_STATE,
        MOCK_CMD_SET_PEN_COLOUR,
        MOCK_CMD_SET_BG_COLOUR,
        MOCK_CMD_SET_VISIBLE,
        // Graphics operations
        MOCK_CMD_CLEAR_GRAPHICS,
        MOCK_CMD_DOT,
        MOCK_CMD_FILL,
        // Boundary modes
        MOCK_CMD_SET_FENCE,
        MOCK_CMD_SET_WINDOW,
        MOCK_CMD_SET_WRAP,
        // Text operations
        MOCK_CMD_CLEAR_TEXT,
        MOCK_CMD_SET_CURSOR,
        MOCK_CMD_SET_WIDTH,
        // Screen modes
        MOCK_CMD_FULLSCREEN,
        MOCK_CMD_SPLITSCREEN,
        MOCK_CMD_TEXTSCREEN,
        // Draw (redraw turtle)
        MOCK_CMD_DRAW
    } MockCommandType;

    //
    // Recorded command with parameters
    //
    typedef struct MockCommand
    {
        MockCommandType type;
        union
        {
            float distance;                  // MOVE
            struct { float x, y; } position; // SET_POSITION, DOT
            float heading;                   // SET_HEADING
            bool flag;                       // SET_PEN_DOWN, SET_VISIBLE
            MockPenMode pen_mode;            // SET_PEN_MODE
            uint16_t colour;                 // SET_PEN_COLOUR, SET_BG_COLOUR
            struct { uint8_t col, row; } cursor;  // SET_CURSOR
            uint8_t width;                   // SET_WIDTH
        } params;
    } MockCommand;

    //
    // Maximum commands to record in history
    //
    #define MOCK_COMMAND_HISTORY_SIZE 256

    //
    // Mock dot record for tracking drawn dots
    //
    typedef struct MockDot
    {
        float x, y;
        uint16_t colour;
    } MockDot;

    //
    // Maximum dots to track
    //
    #define MOCK_MAX_DOTS 1024

    //
    // Mock line segment for tracking drawn lines
    //
    typedef struct MockLine
    {
        float x1, y1;
        float x2, y2;
        uint16_t colour;
    } MockLine;

    //
    // Maximum lines to track
    //
    #define MOCK_MAX_LINES 1024

    //
    // Mock device state - all trackable state in one structure
    //
    typedef struct MockDeviceState
    {
        // Turtle state
        struct
        {
            float x, y;                      // Current position
            float heading;                   // 0 = north, 90 = east
            LogoPen pen_state;               // Pen state (up/down/erase/reverse)
            uint16_t pen_colour;             // Current pen color
            uint16_t bg_colour;              // Background color
            bool visible;                    // Is turtle visible?
            MockTurtleBoundaryMode boundary_mode;  // Fence/window/wrap
        } turtle;

        // Text screen state
        struct
        {
            uint8_t cursor_col;              // Current cursor column (0-based)
            uint8_t cursor_row;              // Current cursor row (0-based)
            uint8_t width;                   // Screen width (40 or 64)
            bool cleared;                    // Was screen cleared?
        } text;

        // Screen mode
        MockScreenMode screen_mode;

        // Graphics tracking
        struct
        {
            bool cleared;                    // Was graphics screen cleared?
            MockDot dots[MOCK_MAX_DOTS];     // Recorded dots
            int dot_count;
            MockLine lines[MOCK_MAX_LINES];  // Recorded lines
            int line_count;
        } graphics;

        // Command history
        MockCommand commands[MOCK_COMMAND_HISTORY_SIZE];
        int command_count;

        // Error tracking
        bool boundary_error;                 // Set when turtle hits boundary in fence mode

        // Graphics file operations tracking
        struct
        {
            char last_save_filename[256];    // Last filename passed to gfx_save
            char last_load_filename[256];    // Last filename passed to gfx_load
            int gfx_save_call_count;         // Number of times gfx_save was called
            int gfx_load_call_count;         // Number of times gfx_load was called
            int gfx_save_result;             // Result to return from gfx_save (0 = success)
            int gfx_load_result;             // Result to return from gfx_load (0 = success)
        } gfx_io;

        // Palette tracking
        struct
        {
            uint8_t r[256];                  // Red components (0-255)
            uint8_t g[256];                  // Green components (0-255)
            uint8_t b[256];                  // Blue components (0-255)
            bool restore_palette_called;     // Was restore_palette called?
        } palette;

        // Shape tracking
        struct
        {
            uint8_t current_shape;           // Current shape number (0-15)
            uint8_t shapes[15][16];          // Shape data for shapes 1-15 (8 columns x 16 rows)
        } shape;

        // WiFi state tracking
        struct
        {
            bool connected;                  // Is WiFi connected?
            char ssid[33];                   // Connected SSID (max 32 chars + null)
            char ip_address[16];             // IP address as string (e.g., "192.168.1.100")
            int connect_result;              // Result to return from wifi_connect (0 = success)
            int disconnect_result;           // Result to return from wifi_disconnect (0 = success)
            // Scan results
            struct {
                char ssid[33];
                int8_t rssi;
            } scan_results[16];
            int scan_result_count;
            int scan_return_value;           // Result to return from wifi_scan (0 = success)
        } wifi;

        // Network state tracking
        struct
        {
            float ping_result_ms;            // Result to return from network_ping (ms or -1)
            char last_ping_ip[16];           // Last IP address passed to network_ping
            char resolve_result_ip[16];      // IP address to return from network_resolve
            bool resolve_success;            // Whether network_resolve should succeed
            char last_resolve_hostname[256]; // Last hostname passed to network_resolve
        } network;

        // Time state tracking
        struct
        {
            int year;                        // Current year (e.g., 2025)
            int month;                       // Current month (1-12)
            int day;                         // Current day (1-31)
            int hour;                        // Current hour (0-23)
            int minute;                      // Current minute (0-59)
            int second;                      // Current second (0-59)
            bool get_date_enabled;           // Whether get_date is supported
            bool get_time_enabled;           // Whether get_time is supported
            bool set_date_enabled;           // Whether set_date is supported
            bool set_time_enabled;           // Whether set_time is supported
        } time;
    } MockDeviceState;

    //
    // Mock device API
    //

    // Initialize/reset mock device state
    void mock_device_init(void);
    void mock_device_reset(void);

    // Get the current mock device state (read-only)
    const MockDeviceState *mock_device_get_state(void);

    // Get the mock console (to register with IO)
    LogoConsole *mock_device_get_console(void);

    // Command history helpers
    int mock_device_command_count(void);
    const MockCommand *mock_device_get_command(int index);
    const MockCommand *mock_device_last_command(void);
    void mock_device_clear_commands(void);

    // Graphics tracking helpers
    int mock_device_dot_count(void);
    const MockDot *mock_device_get_dot(int index);
    int mock_device_line_count(void);
    const MockLine *mock_device_get_line(int index);
    void mock_device_clear_graphics(void);

    // Verify helpers for tests
    bool mock_device_verify_position(float x, float y, float tolerance);
    bool mock_device_verify_heading(float heading, float tolerance);
    bool mock_device_has_line_from_to(float x1, float y1, float x2, float y2, float tolerance);
    bool mock_device_has_dot_at(float x, float y, float tolerance);

    // I/O helpers for testing
    void mock_device_set_input(const char *input);
    const char *mock_device_get_output(void);
    void mock_device_clear_output(void);

    // Graphics file I/O helpers for testing
    void mock_device_set_gfx_save_result(int result);
    void mock_device_set_gfx_load_result(int result);
    const char *mock_device_get_last_gfx_save_filename(void);
    const char *mock_device_get_last_gfx_load_filename(void);
    int mock_device_get_gfx_save_call_count(void);
    int mock_device_get_gfx_load_call_count(void);

    // Palette helpers for testing
    bool mock_device_verify_palette(uint8_t slot, uint8_t r, uint8_t g, uint8_t b);
    bool mock_device_was_restore_palette_called(void);

    // Editor helpers for testing
    // Set the result that the mock editor should return
    void mock_device_set_editor_result(LogoEditorResult result);
    // Set the content that the mock editor should return (replaces buffer content)
    void mock_device_set_editor_content(const char *content);
    // Get the content that was passed to the editor
    const char *mock_device_get_editor_input(void);
    // Check if the editor was called
    bool mock_device_was_editor_called(void);
    // Clear editor state
    void mock_device_clear_editor(void);

    // WiFi helpers for testing
    void mock_device_set_wifi_connected(bool connected);
    void mock_device_set_wifi_ssid(const char *ssid);
    void mock_device_set_wifi_ip(const char *ip);
    void mock_device_set_wifi_connect_result(int result);
    void mock_device_set_wifi_disconnect_result(int result);
    void mock_device_add_wifi_scan_result(const char *ssid, int8_t rssi);
    void mock_device_clear_wifi_scan_results(void);
    void mock_device_set_wifi_scan_result(int result);

    // Mock WiFi operations (for use by test_scaffold in mock_hardware_ops)
    bool mock_wifi_is_connected(void);
    bool mock_wifi_connect(const char *ssid, const char *password);
    void mock_wifi_disconnect(void);
    bool mock_wifi_get_ip(char *ip_buffer, size_t buffer_size);
    bool mock_wifi_get_ssid(char *ssid_buffer, size_t buffer_size);
    int mock_wifi_scan(char ssids[][33], int8_t strengths[], int max_networks);

    // Network helpers for testing
    void mock_device_set_ping_result(float result_ms);
    const char *mock_device_get_last_ping_ip(void);
    void mock_device_set_resolve_result(const char *ip, bool success);
    const char *mock_device_get_last_resolve_hostname(void);

    // Mock network operations (for use by test_scaffold in mock_hardware_ops)
    float mock_network_ping(const char *ip_address);
    bool mock_network_resolve(const char *hostname, char *ip_buffer, size_t buffer_size);

    // Time helpers for testing
    void mock_device_set_time(int year, int month, int day, int hour, int minute, int second);
    void mock_device_set_time_enabled(bool get_date, bool get_time, bool set_date, bool set_time);

    // Mock time operations (for use by test_scaffold in mock_hardware_ops)
    bool mock_get_date(int *year, int *month, int *day);
    bool mock_get_time(int *hour, int *minute, int *second);
    bool mock_set_date(int year, int month, int day);
    bool mock_set_time(int hour, int minute, int second);

#ifdef __cplusplus
}
#endif
