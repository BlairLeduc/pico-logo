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

// Include RTC support for RP2040, use software clock for RP2350
#if PICO_RP2040
#include <hardware/rtc.h>
#endif

#ifdef LOGO_HAS_WIFI
#include <pico/cyw43_arch.h>
#include <lwip/ip4_addr.h>
#include <lwip/icmp.h>
#include <lwip/raw.h>
#include <lwip/inet_chksum.h>
#include <lwip/timeouts.h>

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
// Time management operations
// ============================================================================

#if PICO_RP2040
// RP2040 has hardware RTC
static bool rtc_initialized = false;

static bool ensure_rtc_initialized(void)
{
    if (!rtc_initialized)
    {
        rtc_init();
        // Set a default date/time if not already set
        datetime_t t = {
            .year = 2025,
            .month = 1,
            .day = 1,
            .dotw = 3,  // Wednesday
            .hour = 0,
            .min = 0,
            .sec = 0
        };
        rtc_set_datetime(&t);
        rtc_initialized = true;
    }
    return true;
}

static bool picocalc_get_date(int *year, int *month, int *day)
{
    if (!ensure_rtc_initialized())
    {
        return false;
    }

    datetime_t t;
    if (!rtc_get_datetime(&t))
    {
        return false;
    }

    if (year) *year = t.year;
    if (month) *month = t.month;
    if (day) *day = t.day;
    return true;
}

static bool picocalc_get_time(int *hour, int *minute, int *second)
{
    if (!ensure_rtc_initialized())
    {
        return false;
    }

    datetime_t t;
    if (!rtc_get_datetime(&t))
    {
        return false;
    }

    if (hour) *hour = t.hour;
    if (minute) *minute = t.min;
    if (second) *second = t.sec;
    return true;
}

// Helper to calculate day of week (Zeller's congruence, Sunday = 0)
static int calculate_dotw(int year, int month, int day)
{
    if (month < 3)
    {
        month += 12;
        year--;
    }
    int k = year % 100;
    int j = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    // Convert from Zeller (Sat=0, Sun=1, ..., Fri=6) to (Sun=0, Mon=1, ..., Sat=6)
    return ((h + 6) % 7);
}

static bool picocalc_set_date(int year, int month, int day)
{
    if (!ensure_rtc_initialized())
    {
        return false;
    }

    // Validate inputs
    if (year < 2000 || year > 4095) return false;  // RTC year range
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;

    // Get current time to preserve it
    datetime_t t;
    if (!rtc_get_datetime(&t))
    {
        return false;
    }

    t.year = (int16_t)year;
    t.month = (int8_t)month;
    t.day = (int8_t)day;
    t.dotw = (int8_t)calculate_dotw(year, month, day);

    return rtc_set_datetime(&t);
}

static bool picocalc_set_time(int hour, int minute, int second)
{
    if (!ensure_rtc_initialized())
    {
        return false;
    }

    // Validate inputs
    if (hour < 0 || hour > 23) return false;
    if (minute < 0 || minute > 59) return false;
    if (second < 0 || second > 59) return false;

    // Get current date to preserve it
    datetime_t t;
    if (!rtc_get_datetime(&t))
    {
        return false;
    }

    t.hour = (int8_t)hour;
    t.min = (int8_t)minute;
    t.sec = (int8_t)second;

    return rtc_set_datetime(&t);
}

#else
// RP2350 does not have hardware RTC - use software clock
// Store time as milliseconds since epoch (Jan 1, 2025)
static bool software_clock_initialized = false;
static absolute_time_t clock_base_time;
static int64_t clock_epoch_offset_ms = 0;  // Offset from base_time to epoch

// Days in each month (non-leap year)
static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month_of_year(int month, int year)
{
    if (month == 2 && is_leap_year(year))
    {
        return 29;
    }
    return days_in_month[month - 1];
}

// Convert year/month/day/hour/min/sec to milliseconds since Jan 1, 2025 00:00:00
static int64_t datetime_to_ms(int year, int month, int day, int hour, int min, int sec)
{
    int64_t total_days = 0;
    
    // Add days for years from 2025 to year-1
    for (int y = 2025; y < year; y++)
    {
        total_days += is_leap_year(y) ? 366 : 365;
    }
    
    // Add days for months in current year
    for (int m = 1; m < month; m++)
    {
        total_days += days_in_month_of_year(m, year);
    }
    
    // Add days in current month
    total_days += day - 1;
    
    // Convert to milliseconds
    int64_t total_seconds = total_days * 24LL * 60 * 60 + hour * 3600LL + min * 60 + sec;
    return total_seconds * 1000LL;
}

// Convert milliseconds since Jan 1, 2025 to date/time components
static void ms_to_datetime(int64_t ms, int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    int64_t total_seconds = ms / 1000;
    int64_t remaining_seconds = total_seconds;
    
    // Calculate time of day
    int time_of_day = (int)(remaining_seconds % (24 * 60 * 60));
    if (sec) *sec = time_of_day % 60;
    time_of_day /= 60;
    if (min) *min = time_of_day % 60;
    if (hour) *hour = time_of_day / 60;
    
    // Calculate date
    int64_t total_days = remaining_seconds / (24 * 60 * 60);
    
    int y = 2025;
    while (total_days >= (is_leap_year(y) ? 366 : 365))
    {
        total_days -= is_leap_year(y) ? 366 : 365;
        y++;
    }
    if (year) *year = y;
    
    int m = 1;
    while (total_days >= days_in_month_of_year(m, y))
    {
        total_days -= days_in_month_of_year(m, y);
        m++;
    }
    if (month) *month = m;
    if (day) *day = (int)(total_days + 1);
}

static void ensure_software_clock_initialized(void)
{
    if (!software_clock_initialized)
    {
        clock_base_time = get_absolute_time();
        // Default to Jan 1, 2025 00:00:00
        clock_epoch_offset_ms = 0;
        software_clock_initialized = true;
    }
}

static int64_t get_current_epoch_ms(void)
{
    ensure_software_clock_initialized();
    int64_t elapsed_ms = absolute_time_diff_us(clock_base_time, get_absolute_time()) / 1000;
    return clock_epoch_offset_ms + elapsed_ms;
}

static bool picocalc_get_date(int *year, int *month, int *day)
{
    int64_t ms = get_current_epoch_ms();
    ms_to_datetime(ms, year, month, day, NULL, NULL, NULL);
    return true;
}

static bool picocalc_get_time(int *hour, int *minute, int *second)
{
    int64_t ms = get_current_epoch_ms();
    ms_to_datetime(ms, NULL, NULL, NULL, hour, minute, second);
    return true;
}

static bool picocalc_set_date(int year, int month, int day)
{
    ensure_software_clock_initialized();
    
    // Validate inputs
    if (year < 2025 || year > 4095) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    
    // Get current time to preserve it
    int hour, min, sec;
    int64_t current_ms = get_current_epoch_ms();
    ms_to_datetime(current_ms, NULL, NULL, NULL, &hour, &min, &sec);
    
    // Reset the clock with new date but same time
    clock_base_time = get_absolute_time();
    clock_epoch_offset_ms = datetime_to_ms(year, month, day, hour, min, sec);
    
    return true;
}

static bool picocalc_set_time(int hour, int minute, int second)
{
    ensure_software_clock_initialized();
    
    // Validate inputs
    if (hour < 0 || hour > 23) return false;
    if (minute < 0 || minute > 59) return false;
    if (second < 0 || second > 59) return false;
    
    // Get current date to preserve it
    int year, month, day;
    int64_t current_ms = get_current_epoch_ms();
    ms_to_datetime(current_ms, &year, &month, &day, NULL, NULL, NULL);
    
    // Reset the clock with same date but new time
    clock_base_time = get_absolute_time();
    clock_epoch_offset_ms = datetime_to_ms(year, month, day, hour, minute, second);
    
    return true;
}

#endif // PICO_RP2040

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

// ============================================================================
// Network ping implementation using lwIP raw API
// ============================================================================

// Ping state
static volatile bool ping_complete = false;
static volatile bool ping_success = false;
static volatile uint64_t ping_recv_time_us = 0;
static struct raw_pcb *ping_pcb = NULL;
static uint16_t ping_seq_num = 0;

// ICMP echo request/reply
#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0
#define PING_ID           0x4C4F  // 'LO' for Logo

// Ping receive callback
static uint8_t ping_recv_callback(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    (void)arg;
    (void)pcb;
    (void)addr;

    // Check if this is an ICMP echo reply
    if (p->tot_len >= (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr)))
    {
        // Skip IP header (assume 20 bytes for IPv4)
        struct icmp_echo_hdr *icmp_hdr = (struct icmp_echo_hdr *)((uint8_t *)p->payload + PBUF_IP_HLEN);

        if (icmp_hdr->type == ICMP_ECHO_REPLY && 
            icmp_hdr->id == PP_HTONS(PING_ID) &&
            icmp_hdr->seqno == PP_HTONS(ping_seq_num))
        {
            ping_recv_time_us = to_us_since_boot(get_absolute_time());
            ping_success = true;
            ping_complete = true;
            pbuf_free(p);
            return 1;  // Packet consumed
        }
    }

    return 0;  // Not consumed, pass to next handler
}

static float picocalc_network_ping(const char *ip_address)
{
    if (!ensure_wifi_initialized() || !picocalc_wifi_is_connected())
    {
        return -1.0f;
    }

    // Parse IP address
    ip4_addr_t target_addr;
    if (!ip4addr_aton(ip_address, &target_addr))
    {
        return -1.0f;  // Invalid IP address format
    }

    // Reset ping state
    ping_complete = false;
    ping_success = false;
    ping_recv_time_us = 0;
    ping_seq_num++;

    // Create raw PCB for ICMP if not already created
    if (ping_pcb == NULL)
    {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        if (ping_pcb == NULL)
        {
            return -1.0f;
        }
        raw_recv(ping_pcb, ping_recv_callback, NULL);
        raw_bind(ping_pcb, IP_ADDR_ANY);
    }

    // Build ICMP echo request packet
    struct pbuf *p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr), PBUF_RAM);
    if (p == NULL)
    {
        return -1.0f;
    }

    struct icmp_echo_hdr *icmp_hdr = (struct icmp_echo_hdr *)p->payload;
    icmp_hdr->type = ICMP_ECHO_REQUEST;
    icmp_hdr->code = 0;
    icmp_hdr->chksum = 0;
    icmp_hdr->id = PP_HTONS(PING_ID);
    icmp_hdr->seqno = PP_HTONS(ping_seq_num);

    // Calculate checksum
    icmp_hdr->chksum = inet_chksum(icmp_hdr, sizeof(struct icmp_echo_hdr));

    // Record send time in microseconds
    uint64_t send_time_us = to_us_since_boot(get_absolute_time());

    // Send the ping
    ip_addr_t target;
    ip_addr_copy_from_ip4(target, target_addr);
    err_t err = raw_sendto(ping_pcb, p, &target);
    pbuf_free(p);

    if (err != ERR_OK)
    {
        return -1.0f;
    }

    // Wait for response with timeout (3 seconds)
    absolute_time_t timeout = make_timeout_time_ms(3000);
    while (!ping_complete)
    {
        if (time_reached(timeout))
        {
            break;
        }
        cyw43_arch_poll();
        sys_check_timeouts();  // Process lwIP timeouts
        sleep_ms(1);
    }

    if (ping_success)
    {
        // Convert microseconds to milliseconds as float
        return (float)(ping_recv_time_us - send_time_us) / 1000.0f;
    }
    return -1.0f;
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
    .network_ping = picocalc_network_ping,
#else
    .wifi_is_connected = NULL,
    .wifi_connect = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_ip = NULL,
    .wifi_get_ssid = NULL,
    .wifi_scan = NULL,
    .network_ping = NULL,
#endif
    .get_date = picocalc_get_date,
    .get_time = picocalc_get_time,
    .set_date = picocalc_set_date,
    .set_time = picocalc_set_time,
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
