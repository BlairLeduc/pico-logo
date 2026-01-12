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
#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <time.h>

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

// ============================================================================
// Network DNS resolve implementation using lwIP DNS API
// ============================================================================

// DNS resolve state
static volatile bool dns_complete = false;
static volatile bool dns_success = false;
static ip_addr_t dns_resolved_addr;

// DNS callback
static void picocalc_dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    (void)name;
    (void)callback_arg;

    if (ipaddr != NULL)
    {
        ip_addr_copy(dns_resolved_addr, *ipaddr);
        dns_success = true;
    }
    else
    {
        dns_success = false;
    }
    dns_complete = true;
}

static bool picocalc_network_resolve(const char *hostname, char *ip_buffer, size_t buffer_size)
{
    if (!ensure_wifi_initialized() || !picocalc_wifi_is_connected())
    {
        return false;
    }

    if (!hostname || !ip_buffer || buffer_size < 16)
    {
        return false;
    }

    // First, check if the hostname is already an IP address
    ip4_addr_t test_addr;
    if (ip4addr_aton(hostname, &test_addr))
    {
        // It's already an IP address, just copy it
        snprintf(ip_buffer, buffer_size, "%s", ip4addr_ntoa(&test_addr));
        return true;
    }

    // Reset DNS state
    dns_complete = false;
    dns_success = false;
    ip_addr_set_zero(&dns_resolved_addr);

    // Attempt DNS lookup
    ip_addr_t addr;
    err_t err = dns_gethostbyname(hostname, &addr, picocalc_dns_callback, NULL);

    if (err == ERR_OK)
    {
        // Address was cached and returned immediately
        if (IP_IS_V4(&addr))
        {
            snprintf(ip_buffer, buffer_size, "%s", ip4addr_ntoa(ip_2_ip4(&addr)));
        }
        else
        {
            // IPv6 not supported for now
            return false;
        }
        return true;
    }
    else if (err == ERR_INPROGRESS)
    {
        // Wait for DNS callback with timeout (10 seconds)
        absolute_time_t timeout = make_timeout_time_ms(10000);
        while (!dns_complete)
        {
            if (time_reached(timeout))
            {
                return false;
            }
            cyw43_arch_poll();
            sys_check_timeouts();  // Process lwIP timeouts including DNS
            sleep_ms(10);
        }

        if (dns_success)
        {
            if (IP_IS_V4(&dns_resolved_addr))
            {
                snprintf(ip_buffer, buffer_size, "%s", ip4addr_ntoa(ip_2_ip4(&dns_resolved_addr)));
            }
            else
            {
                // IPv6 not supported for now
                return false;
            }
            return true;
        }
    }

    return false;
}

// ============================================================================
// Network NTP implementation using raw UDP (following pico-examples approach)
// ============================================================================

// NTP constants
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800UL  // Seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TIMEOUT_MS 10000
#define NTP_RESEND_MS 3000

// NTP state
static struct udp_pcb *ntp_pcb = NULL;
static ip_addr_t ntp_server_address;
static volatile bool ntp_complete = false;
static volatile bool ntp_success = false;
static volatile uint32_t ntp_received_sec = 0;

// NTP receive callback
static void ntp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    (void)pcb;

    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Validate response: correct source, correct port, correct length, mode=4 (server), stratum!=0
    if (ip_addr_cmp(addr, &ntp_server_address) && port == NTP_PORT &&
        p->tot_len == NTP_MSG_LEN && mode == 0x4 && stratum != 0)
    {
        // Extract transmit timestamp (bytes 40-43)
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = 
            (uint32_t)seconds_buf[0] << 24 |
            (uint32_t)seconds_buf[1] << 16 |
            (uint32_t)seconds_buf[2] << 8 |
            (uint32_t)seconds_buf[3];

        ntp_received_sec = seconds_since_1900;
        ntp_success = true;
    }

    ntp_complete = true;
    pbuf_free(p);
}

// Send NTP request
static void ntp_send_request(void)
{
    cyw43_arch_lwip_begin();

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    if (p)
    {
        uint8_t *req = (uint8_t *)p->payload;
        memset(req, 0, NTP_MSG_LEN);
        req[0] = 0x1b;  // LI=0, VN=3, Mode=3 (client)
        udp_sendto(ntp_pcb, p, &ntp_server_address, NTP_PORT);
        pbuf_free(p);
    }

    cyw43_arch_lwip_end();
}

// DNS callback for NTP server resolution
static void ntp_dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    (void)name;
    (void)callback_arg;

    if (ipaddr)
    {
        ip_addr_copy(ntp_server_address, *ipaddr);
        ntp_send_request();
    }
    else
    {
        // DNS failed
        ntp_complete = true;
        ntp_success = false;
    }
}

static bool picocalc_network_ntp(const char *server, float timezone_offset)
{
    if (!ensure_wifi_initialized() || !picocalc_wifi_is_connected())
    {
        return false;
    }

    if (!server || server[0] == '\0')
    {
        return false;
    }

    // Reset NTP state
    ntp_complete = false;
    ntp_success = false;
    ntp_received_sec = 0;
    ip_addr_set_zero(&ntp_server_address);

    // Create UDP PCB for NTP
    cyw43_arch_lwip_begin();
    ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    cyw43_arch_lwip_end();

    if (!ntp_pcb)
    {
        return false;
    }

    // Set up receive callback
    udp_recv(ntp_pcb, ntp_recv_callback, NULL);

    // Resolve NTP server hostname
    cyw43_arch_lwip_begin();
    err_t dns_err = dns_gethostbyname(server, &ntp_server_address, ntp_dns_callback, NULL);
    cyw43_arch_lwip_end();

    if (dns_err == ERR_OK)
    {
        // Address was cached, send request immediately
        ntp_send_request();
    }
    else if (dns_err != ERR_INPROGRESS)
    {
        // DNS lookup failed
        udp_remove(ntp_pcb);
        ntp_pcb = NULL;
        return false;
    }
    // else ERR_INPROGRESS: callback will be called when DNS completes

    // Wait for NTP response with timeout and resend logic
    absolute_time_t overall_timeout = make_timeout_time_ms(NTP_TIMEOUT_MS);
    absolute_time_t resend_timeout = make_timeout_time_ms(NTP_RESEND_MS);

    while (!ntp_complete)
    {
        if (time_reached(overall_timeout))
        {
            break;
        }

        // Resend request if no response received within resend timeout
        if (time_reached(resend_timeout) && !ip_addr_isany(&ntp_server_address))
        {
            ntp_send_request();
            resend_timeout = make_timeout_time_ms(NTP_RESEND_MS);
        }

        cyw43_arch_poll();
        sys_check_timeouts();
        sleep_ms(10);
    }

    // Clean up UDP PCB
    cyw43_arch_lwip_begin();
    udp_remove(ntp_pcb);
    ntp_pcb = NULL;
    cyw43_arch_lwip_end();

    // Set device time if successful
    if (ntp_success && ntp_received_sec > 0)
    {
        // Convert NTP timestamp to Unix timestamp and apply timezone offset
        // timezone_offset is in hours (e.g., -5 for EST, 5.5 for IST)
        time_t unix_time = (time_t)(ntp_received_sec - NTP_DELTA);
        unix_time += (time_t)(timezone_offset * 3600.0f);

        // Convert to broken-down time
        struct tm *tm_info = gmtime(&unix_time);
        if (tm_info)
        {
            picocalc_set_date(tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
            picocalc_set_time(tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
            return true;
        }
    }

    return false;
}

// ============================================================================
// TCP Client implementation using lwIP
// ============================================================================

// TCP connection buffer size
#define TCP_RECV_BUFFER_SIZE 2048

// TCP connection state structure
typedef struct {
    struct tcp_pcb *pcb;             // lwIP protocol control block
    uint8_t recv_buffer[TCP_RECV_BUFFER_SIZE];  // Circular receive buffer
    volatile int recv_head;          // Write position in buffer
    volatile int recv_tail;          // Read position in buffer
    volatile bool connected;         // Connection established
    volatile bool connect_pending;   // Connection in progress
    volatile err_t connect_error;    // Error from connect callback
    volatile bool closed;            // Connection closed by remote
    volatile err_t last_error;       // Last error code
} TcpClientState;

// Forward declarations for callbacks
static err_t tcp_client_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t tcp_client_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void tcp_client_err_cb(void *arg, err_t err);
static err_t tcp_client_poll_cb(void *arg, struct tcp_pcb *tpcb);

// Poll for lwIP events with user interrupt checking
static void poll_lwip_with_timeout(int timeout_ms, volatile bool *completion_flag)
{
    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
    
    while (!*completion_flag)
    {
        if (time_reached(timeout))
        {
            break;
        }
        
        // Check for user interrupt
        if (user_interrupt)
        {
            break;
        }
        
        cyw43_arch_poll();
        sys_check_timeouts();
        sleep_ms(1);
    }
}

// Connected callback - called when TCP connection is established
static err_t tcp_client_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    TcpClientState *state = (TcpClientState *)arg;
    (void)tpcb;
    
    if (err == ERR_OK)
    {
        state->connected = true;
    }
    else
    {
        state->connect_error = err;
    }
    state->connect_pending = false;
    
    return ERR_OK;
}

// Receive callback - called when data arrives
static err_t tcp_client_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    TcpClientState *state = (TcpClientState *)arg;
    
    if (p == NULL)
    {
        // Connection closed by remote peer
        state->closed = true;
        return ERR_OK;
    }
    
    if (err != ERR_OK)
    {
        state->last_error = err;
        pbuf_free(p);
        return err;
    }
    
    // Copy data to circular buffer
    cyw43_arch_lwip_check();
    
    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        uint8_t *data = (uint8_t *)q->payload;
        for (u16_t i = 0; i < q->len; i++)
        {
            int next_head = (state->recv_head + 1) % TCP_RECV_BUFFER_SIZE;
            if (next_head != state->recv_tail)
            {
                state->recv_buffer[state->recv_head] = data[i];
                state->recv_head = next_head;
            }
            // else: buffer full, discard data
        }
    }
    
    // Acknowledge received data
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    return ERR_OK;
}

// Error callback - called on connection errors
static void tcp_client_err_cb(void *arg, err_t err)
{
    TcpClientState *state = (TcpClientState *)arg;
    
    state->last_error = err;
    state->closed = true;
    state->connect_pending = false;
    state->pcb = NULL;  // PCB is already freed by lwIP when this is called
}

// Poll callback - called periodically by lwIP
static err_t tcp_client_poll_cb(void *arg, struct tcp_pcb *tpcb)
{
    (void)arg;
    (void)tpcb;
    // We don't need to do anything here, just keep the connection alive
    return ERR_OK;
}

// DNS resolution state for TCP connect
static volatile bool tcp_dns_complete = false;
static volatile bool tcp_dns_success = false;
static ip_addr_t tcp_dns_resolved_addr;

static void tcp_dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    (void)name;
    (void)callback_arg;
    
    if (ipaddr != NULL)
    {
        ip_addr_copy(tcp_dns_resolved_addr, *ipaddr);
        tcp_dns_success = true;
    }
    else
    {
        tcp_dns_success = false;
    }
    tcp_dns_complete = true;
}

static void *picocalc_network_tcp_connect(const char *ip_address, uint16_t port, int timeout_ms)
{
    if (!ensure_wifi_initialized() || !picocalc_wifi_is_connected())
    {
        return NULL;
    }
    
    if (!ip_address || port == 0)
    {
        return NULL;
    }
    
    // Allocate connection state
    TcpClientState *state = (TcpClientState *)calloc(1, sizeof(TcpClientState));
    if (!state)
    {
        return NULL;
    }
    
    state->recv_head = 0;
    state->recv_tail = 0;
    state->connected = false;
    state->connect_pending = true;
    state->connect_error = ERR_OK;
    state->closed = false;
    state->last_error = ERR_OK;
    
    // Resolve hostname/IP address
    ip_addr_t target_addr;
    
    // First try to parse as IP address
    ip4_addr_t test_addr;
    if (ip4addr_aton(ip_address, &test_addr))
    {
        // It's already an IP address
        ip_addr_copy_from_ip4(target_addr, test_addr);
    }
    else
    {
        // Need to resolve hostname
        tcp_dns_complete = false;
        tcp_dns_success = false;
        ip_addr_set_zero(&tcp_dns_resolved_addr);
        
        cyw43_arch_lwip_begin();
        err_t dns_err = dns_gethostbyname(ip_address, &target_addr, tcp_dns_callback, NULL);
        cyw43_arch_lwip_end();
        
        if (dns_err == ERR_OK)
        {
            // Address was cached
        }
        else if (dns_err == ERR_INPROGRESS)
        {
            // Wait for DNS resolution
            poll_lwip_with_timeout(timeout_ms > 0 ? timeout_ms : 10000, &tcp_dns_complete);
            
            if (!tcp_dns_success)
            {
                free(state);
                return NULL;
            }
            ip_addr_copy(target_addr, tcp_dns_resolved_addr);
        }
        else
        {
            free(state);
            return NULL;
        }
    }
    
    // Create TCP PCB
    cyw43_arch_lwip_begin();
    state->pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    cyw43_arch_lwip_end();
    
    if (!state->pcb)
    {
        free(state);
        return NULL;
    }
    
    // Set up callbacks
    cyw43_arch_lwip_begin();
    tcp_arg(state->pcb, state);
    tcp_recv(state->pcb, tcp_client_recv_cb);
    tcp_err(state->pcb, tcp_client_err_cb);
    tcp_poll(state->pcb, tcp_client_poll_cb, 10);  // Poll every 5 seconds (10 * 500ms)
    
    // Initiate connection
    err_t err = tcp_connect(state->pcb, &target_addr, port, tcp_client_connected_cb);
    cyw43_arch_lwip_end();
    
    if (err != ERR_OK)
    {
        cyw43_arch_lwip_begin();
        tcp_abort(state->pcb);
        cyw43_arch_lwip_end();
        free(state);
        return NULL;
    }
    
    // Wait for connection with timeout
    poll_lwip_with_timeout(timeout_ms > 0 ? timeout_ms : 30000, &state->connect_pending);
    
    // Invert the flag check - connect_pending becomes false when connection completes
    if (state->connect_pending || !state->connected)
    {
        // Connection timed out or failed
        if (state->pcb)
        {
            cyw43_arch_lwip_begin();
            tcp_arg(state->pcb, NULL);
            tcp_recv(state->pcb, NULL);
            tcp_err(state->pcb, NULL);
            tcp_poll(state->pcb, NULL, 0);
            tcp_abort(state->pcb);
            cyw43_arch_lwip_end();
        }
        free(state);
        return NULL;
    }
    
    return state;
}

static void picocalc_network_tcp_close(void *connection)
{
    if (!connection)
    {
        return;
    }
    
    TcpClientState *state = (TcpClientState *)connection;
    
    if (state->pcb)
    {
        cyw43_arch_lwip_begin();
        
        // Clear callbacks first
        tcp_arg(state->pcb, NULL);
        tcp_recv(state->pcb, NULL);
        tcp_err(state->pcb, NULL);
        tcp_poll(state->pcb, NULL, 0);
        tcp_sent(state->pcb, NULL);
        
        // Attempt graceful close
        err_t err = tcp_close(state->pcb);
        if (err != ERR_OK)
        {
            // If close fails, abort the connection
            tcp_abort(state->pcb);
        }
        
        cyw43_arch_lwip_end();
    }
    
    free(state);
}

static int picocalc_network_tcp_read(void *connection, char *buffer, int count, int timeout_ms)
{
    if (!connection || !buffer || count <= 0)
    {
        return -1;
    }
    
    TcpClientState *state = (TcpClientState *)connection;
    
    // Check if connection is closed
    if (state->closed && state->recv_head == state->recv_tail)
    {
        return -1;  // Connection closed and no more data
    }
    
    // Wait for data with timeout
    if (timeout_ms > 0)
    {
        absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
        
        while (state->recv_head == state->recv_tail)
        {
            if (time_reached(timeout))
            {
                return 0;  // Timeout, no data available
            }
            
            if (state->closed)
            {
                return -1;  // Connection closed
            }
            
            if (user_interrupt)
            {
                return -1;  // User interrupt
            }
            
            cyw43_arch_poll();
            sys_check_timeouts();
            sleep_ms(1);
        }
    }
    else if (timeout_ms == 0)
    {
        // Non-blocking: just check once
        cyw43_arch_poll();
        sys_check_timeouts();
        
        if (state->recv_head == state->recv_tail)
        {
            return 0;  // No data available
        }
    }
    
    // Read data from circular buffer
    int bytes_read = 0;
    while (bytes_read < count && state->recv_head != state->recv_tail)
    {
        buffer[bytes_read++] = state->recv_buffer[state->recv_tail];
        state->recv_tail = (state->recv_tail + 1) % TCP_RECV_BUFFER_SIZE;
    }
    
    return bytes_read;
}

static int picocalc_network_tcp_write(void *connection, const char *data, int count)
{
    if (!connection || !data || count <= 0)
    {
        return -1;
    }
    
    TcpClientState *state = (TcpClientState *)connection;
    
    if (!state->pcb || state->closed)
    {
        return -1;  // Connection closed
    }
    
    // Write data in chunks that fit the TCP send buffer
    int total_written = 0;
    
    while (total_written < count)
    {
        cyw43_arch_lwip_begin();
        
        // Check available space in send buffer
        u16_t available = tcp_sndbuf(state->pcb);
        if (available == 0)
        {
            cyw43_arch_lwip_end();
            
            // Wait for buffer space with timeout
            absolute_time_t timeout = make_timeout_time_ms(5000);
            while (tcp_sndbuf(state->pcb) == 0)
            {
                if (time_reached(timeout))
                {
                    // Timeout waiting for buffer space
                    return total_written > 0 ? total_written : -1;
                }
                
                if (state->closed || user_interrupt)
                {
                    return total_written > 0 ? total_written : -1;
                }
                
                cyw43_arch_poll();
                sys_check_timeouts();
                sleep_ms(1);
            }
            
            continue;  // Retry with new buffer space
        }
        
        // Calculate how much to write
        int remaining = count - total_written;
        u16_t to_write = (remaining < available) ? (u16_t)remaining : available;
        
        // Write to TCP
        err_t err = tcp_write(state->pcb, data + total_written, to_write, TCP_WRITE_FLAG_COPY);
        
        if (err != ERR_OK)
        {
            cyw43_arch_lwip_end();
            return total_written > 0 ? total_written : -1;
        }
        
        // Flush the output
        err = tcp_output(state->pcb);
        cyw43_arch_lwip_end();
        
        if (err != ERR_OK)
        {
            return total_written > 0 ? total_written : -1;
        }
        
        total_written += to_write;
    }
    
    return total_written;
}

static bool picocalc_network_tcp_can_read(void *connection)
{
    if (!connection)
    {
        return false;
    }
    
    TcpClientState *state = (TcpClientState *)connection;
    
    // Poll to process any pending data
    cyw43_arch_poll();
    sys_check_timeouts();
    
    // Check if data is available in the buffer
    return state->recv_head != state->recv_tail;
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
    .network_resolve = picocalc_network_resolve,
    .network_ntp = picocalc_network_ntp,
    .network_tcp_connect = picocalc_network_tcp_connect,
    .network_tcp_close = picocalc_network_tcp_close,
    .network_tcp_read = picocalc_network_tcp_read,
    .network_tcp_write = picocalc_network_tcp_write,
    .network_tcp_can_read = picocalc_network_tcp_can_read,
#else
    .wifi_is_connected = NULL,
    .wifi_connect = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_ip = NULL,
    .wifi_get_ssid = NULL,
    .wifi_scan = NULL,
    .network_ping = NULL,
    .network_resolve = NULL,
    .network_ntp = NULL,
    .network_tcp_connect = NULL,
    .network_tcp_close = NULL,
    .network_tcp_read = NULL,
    .network_tcp_write = NULL,
    .network_tcp_can_read = NULL,
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
