//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface for PicoCalc device.
//

#include "../hardware.h"
#include "picocalc_hardware.h"
#include "southbridge.h"
#include "audio.h"
#include "keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pico/stdlib.h>
#include <pico/rand.h>

#ifdef LOGO_HAS_WIFI
#include <pico/cyw43_arch.h>
#include <lwip/ip4_addr.h>

// WiFi state tracking
static bool wifi_initialized = false;
static char current_ssid[33] = {0};

// Scan results storage
#define MAX_SCAN_RESULTS 20
static char scan_ssids[MAX_SCAN_RESULTS][33];
static int8_t scan_strengths[MAX_SCAN_RESULTS];
static int scan_count = 0;
static volatile bool scan_complete = false;

// Scan callback for cyw43_wifi_scan
static int wifi_scan_callback(void *env, const cyw43_ev_scan_result_t *result)
{
    (void)env;
    
    if (result && scan_count < MAX_SCAN_RESULTS)
    {
        // Check if this SSID is already in our list (avoid duplicates)
        for (int i = 0; i < scan_count; i++)
        {
            if (strncmp(scan_ssids[i], (const char *)result->ssid, result->ssid_len) == 0)
            {
                // Update signal strength if this one is stronger
                if (result->rssi > scan_strengths[i])
                {
                    scan_strengths[i] = result->rssi;
                }
                return 0;
            }
        }
        
        // Add new network
        size_t len = result->ssid_len;
        if (len > 32) len = 32;
        memcpy(scan_ssids[scan_count], result->ssid, len);
        scan_ssids[scan_count][len] = '\0';
        scan_strengths[scan_count] = result->rssi;
        scan_count++;
    }
    
    return 0;
}

#endif // LOGO_HAS_WIFI

// Hardware operation implementations

static void picocalc_sleep(int milliseconds)
{
    sleep_ms((uint32_t)milliseconds);
}

static uint32_t picocalc_random(void)
{
    return get_rand_32();
}

static void picocalc_get_battery_level(int *level, bool *charging)
{
    // PicoCalc does not have a battery, return 100%
    int raw_level = sb_read_battery();
    *level = raw_level & 0x7F;    // Mask out the charging bit
    *charging = (raw_level & 0x80) != 0; // Check if charging
}

static bool picocalc_power_off(void)
{
    // Call southbridge power off function
    if (sb_is_power_off_supported())
    {
        sb_write_power_off_delay(5);
        return true;
    }

    return false;
}

static bool picocalc_check_user_interrupt(void)
{
    return user_interrupt;
}

static void picocalc_clear_user_interrupt(void)
{
    user_interrupt = false;
}

static bool picocalc_check_pause_request(void)
{
    return pause_requested;
}

static void picocalc_clear_pause_request(void)
{
    pause_requested = false;
}

static bool picocalc_check_freeze_request(void)
{
    return freeze_requested;
}

static void picocalc_clear_freeze_request(void)
{
    freeze_requested = false;
}

static void picocalc_toot(uint32_t duration_ms, uint32_t left_freq, uint32_t right_freq)
{
    // Wait for any existing tone to finish before starting a new one.
    // Use a cooperative wait: poll less frequently and respect user interrupts.
    while (audio_is_playing())
    {
        if (user_interrupt)
        {
            // If the user has requested an interrupt, abort waiting and skip this toot.
            return;
        }

        // Sleep for a short period to avoid tight busy-waiting.
        sleep_ms(1);
    }
    // Play the tone (non-blocking with automatic stop after duration)
    audio_play_sound_timed(left_freq, right_freq, duration_ms);
}

// ============================================================================
// WiFi operations (only available when LOGO_HAS_WIFI is defined)
// ============================================================================

#ifdef LOGO_HAS_WIFI

static bool ensure_wifi_initialized(void)
{
    if (!wifi_initialized)
    {
        if (cyw43_arch_init() != 0)
        {
            return false;
        }
        cyw43_arch_enable_sta_mode();
        wifi_initialized = true;
    }
    return true;
}

static bool picocalc_wifi_is_connected(void)
{
    if (!wifi_initialized)
    {
        return false;
    }
    
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    return link_status == CYW43_LINK_JOIN;
}

static bool picocalc_wifi_connect(const char *ssid, const char *password)
{
    if (!ensure_wifi_initialized())
    {
        return false;
    }
    
    // Connect with timeout (30 seconds)
    int result = cyw43_arch_wifi_connect_timeout_ms(
        ssid, 
        password, 
        CYW43_AUTH_WPA2_AES_PSK,
        30000
    );
    
    if (result == 0)
    {
        // Store the SSID for later retrieval
        strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
        current_ssid[sizeof(current_ssid) - 1] = '\0';
        return true;
    }
    
    return false;
}

static void picocalc_wifi_disconnect(void)
{
    if (!wifi_initialized)
    {
        return;
    }
    
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    current_ssid[0] = '\0';
}

static bool picocalc_wifi_get_ip(char *ip_buffer, size_t buffer_size)
{
    if (!wifi_initialized || buffer_size < 16)
    {
        return false;
    }
    
    if (!picocalc_wifi_is_connected())
    {
        return false;
    }
    
    // Get the IP address from the netif
    struct netif *netif = netif_default;
    if (netif == NULL || !ip4_addr_isany_val(*netif_ip4_addr(netif)))
    {
        const ip4_addr_t *ip = netif_ip4_addr(netif);
        snprintf(ip_buffer, buffer_size, "%s", ip4addr_ntoa(ip));
        return true;
    }
    
    return false;
}

static bool picocalc_wifi_get_ssid(char *ssid_buffer, size_t buffer_size)
{
    if (!wifi_initialized || buffer_size < 33)
    {
        return false;
    }
    
    if (!picocalc_wifi_is_connected())
    {
        return false;
    }
    
    strncpy(ssid_buffer, current_ssid, buffer_size - 1);
    ssid_buffer[buffer_size - 1] = '\0';
    return current_ssid[0] != '\0';
}

static int picocalc_wifi_scan(char ssids[][33], int8_t strengths[], int max_networks)
{
    if (!ensure_wifi_initialized())
    {
        return -1;
    }
    
    // Reset scan state
    scan_count = 0;
    scan_complete = false;
    
    // Start the scan
    cyw43_wifi_scan_options_t scan_options = {0};
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, wifi_scan_callback);
    if (err != 0)
    {
        return -1;
    }
    
    // Wait for scan to complete (with timeout)
    absolute_time_t timeout = make_timeout_time_ms(10000);
    while (cyw43_wifi_scan_active(&cyw43_state))
    {
        if (time_reached(timeout))
        {
            break;
        }
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    // Copy results to output buffers
    int count = (scan_count < max_networks) ? scan_count : max_networks;
    for (int i = 0; i < count; i++)
    {
        strncpy(ssids[i], scan_ssids[i], 32);
        ssids[i][32] = '\0';
        strengths[i] = scan_strengths[i];
    }
    
    return count;
}

#endif // LOGO_HAS_WIFI

// ============================================================================
// Hardware ops structure
// ============================================================================

static LogoHardwareOps picocalc_hardware_ops = {
    .sleep = picocalc_sleep,
    .random = picocalc_random,
    .get_battery_level = picocalc_get_battery_level,
    .power_off = picocalc_power_off,
    .check_user_interrupt = picocalc_check_user_interrupt,
    .clear_user_interrupt = picocalc_clear_user_interrupt,
    .check_pause_request = picocalc_check_pause_request,
    .clear_pause_request = picocalc_clear_pause_request,
    .check_freeze_request = picocalc_check_freeze_request,
    .clear_freeze_request = picocalc_clear_freeze_request,
    .toot = picocalc_toot,
#ifdef LOGO_HAS_WIFI
    .wifi_is_connected = picocalc_wifi_is_connected,
    .wifi_connect = picocalc_wifi_connect,
    .wifi_disconnect = picocalc_wifi_disconnect,
    .wifi_get_ip = picocalc_wifi_get_ip,
    .wifi_get_ssid = picocalc_wifi_get_ssid,
    .wifi_scan = picocalc_wifi_scan,
#else
    .wifi_is_connected = NULL,
    .wifi_connect = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_ip = NULL,
    .wifi_get_ssid = NULL,
    .wifi_scan = NULL,
#endif
}; 


//
// LogoStorage lifecycle functions
//

LogoHardware *logo_picocalc_hardware_create(void)
{
    LogoHardware *hardware = (LogoHardware *)malloc(sizeof(LogoHardware));

    if (!hardware)
    {
        return NULL;
    }

    logo_hardware_init(hardware, &picocalc_hardware_ops);

    return hardware;
}

void logo_picocalc_hardware_destroy(LogoHardware *hardware)
{
    if (!hardware)
    {
        return;
    }

    free(hardware);
}
