//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface for PicoCalc device.
//

#include "../hardware.h"
#include "picocalc_hardware.h"
#include "southbridge.h"
#include "sound.h"
#include "keyboard.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/bootrom.h>

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
#include <lwip/altcp.h>
#include <lwip/altcp_tcp.h>
#include <lwip/apps/mdns.h>
#ifdef LOGO_HAS_TLS
#include <lwip/altcp_tls.h>
#include <mbedtls/ssl.h>
#include "ca_certs.h"
#include "tls_heap.h"
#endif // LOGO_HAS_TLS
#include <time.h>

// WiFi state tracking
static bool wifi_initialized = false;
static char current_ssid[33] = {0};

// Network name for mDNS (`<hostname>.local`) and DHCP. Core owns the canonical
// value and pushes it via network_set_hostname; this copy is what the netif and
// mDNS responder are configured with. Defaults to "picologo".
static char current_hostname[33] = "picologo";
static bool mdns_initialized = false;  // mdns_resp_init() called once
static bool mdns_active = false;       // responder added to the netif

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
    screen_gfx_flush();  // Show latest frame before blocking
    sleep_ms((uint32_t)milliseconds);
}

static uint32_t picocalc_ticks_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
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

static void picocalc_reboot_bootloader(void)
{
    // Reset into the USB bootloader: (0, 0) => no activity LED, leave both the
    // mass-storage and PICOBOOT interfaces enabled. This call does not return.
    rom_reset_usb_boot(0, 0);
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

// ============================================================================
// Time management operations
// ============================================================================

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

// Convert year/month/day/hour/min/sec to milliseconds since Jan 1, 2026 00:00:00
static int64_t datetime_to_ms(int year, int month, int day, int hour, int min, int sec)
{
    int64_t total_days = 0;
    
    // Add days for years from 2026 to year-1
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

// Convert milliseconds since Jan 1, 2026 to date/time components
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
        // Default to Jan 1, 2026 00:00:00
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
    if (year < 2026 || year > 4095) return false;
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

// Set once wifi_start kicks off an asynchronous attempt, cleared when that
// attempt resolves. The cyw43 link status reports LINK_DOWN both for "never
// started" and "still associating", so this is what separates OFF from
// CONNECTING.
static bool wifi_connect_pending = false;

// State for retrying an attempt that has stopped making progress.
//
// Joining is unreliable enough on some networks that a single attempt is not
// much of a signal: individual joins fail with LINK_FAIL and only succeed after
// several tries. So wifi_start keeps trying until it connects -- the blocking
// connect gives up on LINK_FAIL after one attempt, which is why it too often
// failed to connect from a startup file. Only a refused password stops us,
// since that will not come right on a retry, and wifi_disconnect.
//
// Retries must be spaced out in wall-clock time. cyw43_wifi_join resets
// wifi_join_state, discarding the AUTH/LINK/KEYED flags a join accumulates from
// async events over a second or two, so issuing one per poll restarts the
// association faster than it can ever complete: the firmware starts rejecting
// SET_SSID (-> WIFI_JOIN_STATE_FAIL) or the link comes half-up and DHCP never
// answers. The SDK's blocking loop is safe from this because
// cyw43_arch_wait_for_work_until paces it; a status getter polled every 20 ms
// by a demon has no such brake.
//
// How to retry depends on how the attempt ended, mirroring the SDK's blocking
// loop wherever it has a proven answer:
//
// - Not-found (NONET) is re-joined immediately with no disassociation, which
//   is exactly what cyw43_arch_wifi_connect_until does -- the firmware lost
//   the scan race and just needs the join issued again. The re-issue is
//   self-pacing: the new join reads as "connecting" until the firmware
//   settles it, so it cannot fire once per poll.
// - A failed join (FAIL) is retried in two steps, because re-joining on its
//   own does not work: the station is left associated-but-broken, and every
//   subsequent join fails too, however long it is left. Disassociating first
//   is what clears it -- which is precisely what `wifi.disconnect` followed
//   by `wifi.connect` does by hand.
// - An association still connecting or waiting on DHCP gets as long as the
//   blocking connect's 30-second timeout before it is disturbed; tearing it
//   down sooner kills attempts that were going to succeed.
//
// Rejected joins are retried as a short burst, then lone probes separated by
// growing rests. The AP refuses associations as a penalty that retrying
// sustains and deepens: traces showed attempts at any steady cadence (2s to
// 16s gaps) rejected identically ~3.1s after issue for minutes on end, while
// the joins that succeeded each followed a genuinely attempt-free stretch --
// and the stretch needed grows with how many attempts preceded it (~19-36s of
// quiet sufficed after a handful of attempts; after ~19 attempts, 27s was
// still refused and ~127s was needed). This is also why a by-hand
// disconnect-pause-connect worked. So: a few quick retries in case the
// failure was a blip, then one probe per rest, resting 30s, 60s, then 120s
// repeating.
#define WIFI_RETRY_SETTLED_MS 3000
#define WIFI_RETRY_STALLED_MS 30000
#define WIFI_REJOIN_DELAY_MS 2000
#define WIFI_FAIL_BURST 3
#define WIFI_REST_MS 30000
#define WIFI_REST_MAX_DOUBLINGS 2
static char current_password[64] = {0};
static uint32_t wifi_last_change_ms = 0;
static uint32_t wifi_rejoin_at_ms = 0;
static bool wifi_awaiting_rejoin = false;
static int wifi_last_state = -1;
static int wifi_fail_streak = 0;

static void wifi_configure_link(void);
static int picocalc_wifi_status(void);

static bool picocalc_wifi_is_connected(void)
{
    return picocalc_wifi_status() == WIFI_STATE_CONNECTED;
}

// Connection state without blocking. Uses cyw43_tcpip_link_status rather than
// cyw43_wifi_link_status: the latter reports LINK_JOIN as soon as the radio
// associates, before DHCP has assigned an address, which would let a
// `when [wifi?] [network.ntp ...]` demon fire while name resolution still has
// no route.
static int picocalc_wifi_status(void)
{
    if (!wifi_initialized)
    {
        return WIFI_STATE_OFF;
    }

    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    int state;
    switch (link_status)
    {
        case CYW43_LINK_UP:      state = WIFI_STATE_CONNECTED;  break;
        case CYW43_LINK_NOIP:    state = WIFI_STATE_NOIP;       break;
        case CYW43_LINK_JOIN:    state = WIFI_STATE_CONNECTING; break;
        case CYW43_LINK_NONET:   state = WIFI_STATE_NONET;      break;
        case CYW43_LINK_BADAUTH: state = WIFI_STATE_BADAUTH;    break;
        case CYW43_LINK_FAIL:    state = WIFI_STATE_FAILED;     break;
        default:                 state = WIFI_STATE_OFF;        break; // LINK_DOWN
    }

    // LINK_DOWN while an attempt is in flight is the gap before the driver has
    // anything to report, not "nothing is happening".
    if (state == WIFI_STATE_OFF && wifi_connect_pending)
    {
        state = WIFI_STATE_CONNECTING;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (state != wifi_last_state)
    {
        wifi_last_state = state;
        wifi_last_change_ms = now;
    }

    if (state == WIFI_STATE_CONNECTED)
    {
        if (wifi_connect_pending)
        {
            // The attempt just landed. Do the work the blocking connect path
            // does on success, now that the interface is really up; polling
            // this op is the only notification we get.
            wifi_connect_pending = false;
            wifi_fail_streak = 0;
            wifi_configure_link();
        }
        return state;
    }

    if (!wifi_connect_pending)
    {
        return state;
    }

    // Being refused is final: the password will not become right on a retry.
    if (state == WIFI_STATE_BADAUTH)
    {
        wifi_connect_pending = false;
        return state;
    }

    // Step two: disassociated a moment ago, so now try the join. If the join
    // cannot even be issued, keep waiting and try again next cycle instead of
    // treating it as in flight -- otherwise nothing is joining at all and the
    // "failed" state just sits there forever.
    if (wifi_awaiting_rejoin)
    {
        if (now >= wifi_rejoin_at_ms)
        {
            int rc = cyw43_arch_wifi_connect_async(current_ssid, current_password,
                                                   CYW43_AUTH_WPA2_AES_PSK);
            if (rc == 0)
            {
                wifi_awaiting_rejoin = false;
            }
            else
            {
                wifi_rejoin_at_ms = now + WIFI_REJOIN_DELAY_MS;
            }
            wifi_last_change_ms = now;
        }
        return state;
    }

    // Not-found settles a join that lost the scan race; re-issue it at once,
    // no disassociation, as the SDK's blocking loop does.
    if (state == WIFI_STATE_NONET)
    {
        int rc = cyw43_arch_wifi_connect_async(current_ssid, current_password,
                                               CYW43_AUTH_WPA2_AES_PSK);
        if (rc == 0)
        {
            wifi_last_change_ms = now;
            return WIFI_STATE_CONNECTING;
        }
        // The join could not be issued: fall through to the two-step retry.
    }

    // Step one: stuck, so disassociate and let the radio settle before joining
    // again. A settled failure gets a short pause; an association still in
    // progress gets the full blocking-connect timeout before being disturbed.
    uint32_t patience = (state == WIFI_STATE_FAILED || state == WIFI_STATE_NONET)
                            ? WIFI_RETRY_SETTLED_MS
                            : WIFI_RETRY_STALLED_MS;

    if (now - wifi_last_change_ms >= patience)
    {
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        wifi_awaiting_rejoin = true;
        uint32_t delay = WIFI_REJOIN_DELAY_MS;
        if (state == WIFI_STATE_FAILED && ++wifi_fail_streak >= WIFI_FAIL_BURST)
        {
            int doublings = wifi_fail_streak - WIFI_FAIL_BURST;
            if (doublings > WIFI_REST_MAX_DOUBLINGS)
            {
                doublings = WIFI_REST_MAX_DOUBLINGS;
            }
            delay = WIFI_REST_MS << doublings;
        }
        wifi_rejoin_at_ms = now + delay;
        wifi_last_change_ms = now;
    }

    return state;
}

// Apply current_hostname to the netif (for DHCP) and (re)start the mDNS
// responder so the device answers `<hostname>.local`. Called once the interface
// is up (from wifi.connect) and again on rename; a no-op if there is no netif.
static void mdns_start(void)
{
    struct netif *netif = netif_default;
    if (netif == NULL)
    {
        return;
    }

    cyw43_arch_lwip_begin();
    // netif_set_hostname stores the pointer, not a copy; current_hostname is
    // static so it stays valid, and updating it in place updates DHCP too.
    netif_set_hostname(netif, current_hostname);
    if (!mdns_initialized)
    {
        mdns_resp_init();
        mdns_initialized = true;
    }
    if (mdns_active)
    {
        mdns_resp_rename_netif(netif, current_hostname);
    }
    else if (mdns_resp_add_netif(netif, current_hostname) == ERR_OK)
    {
        mdns_active = true;
    }
    cyw43_arch_lwip_end();
}

// Stop answering mDNS queries (on wifi.disconnect).
static void mdns_stop(void)
{
    if (!mdns_active)
    {
        return;
    }
    struct netif *netif = netif_default;
    cyw43_arch_lwip_begin();
    if (netif != NULL)
    {
        mdns_resp_remove_netif(netif);
    }
    cyw43_arch_lwip_end();
    mdns_active = false;
}

// Set the device's network name (mDNS + DHCP). Takes effect immediately when the
// interface is up, otherwise at the next wifi.connect.
static void picocalc_network_set_hostname(const char *name)
{
    if (name == NULL)
    {
        return;
    }
    strncpy(current_hostname, name, sizeof(current_hostname) - 1);
    current_hostname[sizeof(current_hostname) - 1] = '\0';

    if (wifi_initialized && picocalc_wifi_is_connected())
    {
        mdns_start();
    }
}

// Work that has to happen once the interface is up, shared by the blocking
// connect and the asynchronous one (where picocalc_wifi_status runs it on the
// link-up edge).
static void wifi_configure_link(void)
{
    // DHCP normally supplies a DNS server (applied at index 0). Some
    // networks omit the DNS option, which would leave name resolution with
    // no server at all. Register a public fallback as the secondary
    // resolver (index 1) so lookups degrade gracefully; the DHCP-provided
    // server stays preferred.
    ip_addr_t dns_fallback;
    IP_ADDR4(&dns_fallback, 1, 1, 1, 1); // Cloudflare 1.1.1.1
    dns_setserver(1, &dns_fallback);

    // Findable on the LAN as `<hostname>.local` as soon as WiFi is up,
    // independent of whether anything is being served.
    mdns_start();
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

        wifi_connect_pending = false;
        wifi_configure_link();

        return true;
    }

    return false;
}

// Non-blocking counterpart to picocalc_wifi_connect: kicks off the association
// and returns at once, leaving picocalc_wifi_status to report progress.
static bool picocalc_wifi_start(const char *ssid, const char *password)
{
    if (!ensure_wifi_initialized())
    {
        return false;
    }

    // Recorded up front so the link-up edge needs no state beyond the flag, and
    // so a NONET blip can re-issue the join; wifi_get_ssid gates on being
    // connected, so the SSID is not reported early.
    strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
    current_ssid[sizeof(current_ssid) - 1] = '\0';
    strncpy(current_password, password, sizeof(current_password) - 1);
    current_password[sizeof(current_password) - 1] = '\0';

    if (cyw43_arch_wifi_connect_async(ssid, password, CYW43_AUTH_WPA2_AES_PSK) != 0)
    {
        return false;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    wifi_last_change_ms = now;
    wifi_awaiting_rejoin = false;
    wifi_last_state = -1;
    wifi_fail_streak = 0;
    wifi_connect_pending = true;
    return true;
}

static void picocalc_wifi_disconnect(void)
{
    if (!wifi_initialized)
    {
        return;
    }
    
    mdns_stop();
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);

    // cyw43_wifi_leave only queues the disassociation; the EV_DISASSOC event
    // that settles it arrives asynchronously a moment later, and its handler
    // zeroes the driver's whole join state. A join issued before it lands
    // (wifi.disconnect then wifi.start back-to-back in a procedure) has its
    // active flag wiped mid-flight: the radio associates but the driver never
    // reports the link up. Wait, bounded, until the driver has left the
    // joined/joining state so a follow-on join starts from a settled driver,
    // the same as typing the two commands separately.
    for (int i = 0; i < 100; i++)
    {
        if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_JOIN)
        {
            break;
        }
        sleep_ms(10);
    }

    current_ssid[0] = '\0';
    current_password[0] = '\0';
    wifi_connect_pending = false;
    wifi_awaiting_rejoin = false;
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

static bool picocalc_wifi_get_mac(uint8_t mac_buffer[6])
{
    if (!ensure_wifi_initialized())
    {
        return false;
    }

    int err = cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac_buffer);
    return err == 0;
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

// TCP connection receive-ring size. Must exceed the advertised TCP window
// (TCP_WND) so a full window's worth of coalesced data always fits, letting the
// receive callback apply backpressure (refuse-when-full) without ever
// deadlocking on a segment larger than the ring.
#define TCP_RECV_BUFFER_SIZE 4096

// TCP connection state structure
typedef struct {
    struct altcp_pcb *pcb;           // lwIP application-layered PCB (plain or TLS)
    uint8_t recv_buffer[TCP_RECV_BUFFER_SIZE];  // Circular receive buffer
    volatile int recv_head;          // Write position in buffer
    volatile int recv_tail;          // Read position in buffer
    volatile bool connected;         // Connection established
    volatile bool connect_complete;  // Connection attempt finished (for polling)
    volatile err_t connect_error;    // Error from connect callback
    volatile bool closed;            // Connection closed by remote
    volatile err_t last_error;       // Last error code
    bool from_listener;              // Owned by a listener (server conn): close detaches, does not free
    bool wrote_data;                 // A response was written (graceful close) vs. abandoned (abort)
} TcpClientState;

// Forward declarations for callbacks
static err_t tcp_client_connected_cb(void *arg, struct altcp_pcb *tpcb, err_t err);
static err_t tcp_client_recv_cb(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err);
static void tcp_client_err_cb(void *arg, err_t err);
static err_t tcp_client_poll_cb(void *arg, struct altcp_pcb *tpcb);

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
static err_t tcp_client_connected_cb(void *arg, struct altcp_pcb *tpcb, err_t err)
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
    state->connect_complete = true;
    
    return ERR_OK;
}

// Receive callback - called when data arrives
static err_t tcp_client_recv_cb(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err)
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
    
    (void)tpcb;
    cyw43_arch_lwip_check();

    // Flow control: the advertised receive window is only reopened as the
    // application drains the ring (altcp_recved is deferred to the read path),
    // so lwIP never delivers more than the ring can hold and a fast sender is
    // throttled instead of overrunning us. The ring is sized above TCP_WND, so
    // a delivery always fits; the ERR_MEM guard is a safety net that makes lwIP
    // keep the data (re-delivered later) rather than us dropping it.
    int used = (state->recv_head - state->recv_tail + TCP_RECV_BUFFER_SIZE) % TCP_RECV_BUFFER_SIZE;
    int free_space = (TCP_RECV_BUFFER_SIZE - 1) - used;
    if ((int)p->tot_len > free_space)
    {
        return ERR_MEM;
    }

    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        uint8_t *data = (uint8_t *)q->payload;
        for (u16_t i = 0; i < q->len; i++)
        {
            state->recv_buffer[state->recv_head] = data[i];
            state->recv_head = (state->recv_head + 1) % TCP_RECV_BUFFER_SIZE;
        }
    }

    // NB: do NOT altcp_recved() here -- the read path reopens the window as it
    // consumes bytes, which is what keeps the window <= ring free space.
    pbuf_free(p);

    return ERR_OK;
}

// Error callback - called on connection errors
static void tcp_client_err_cb(void *arg, err_t err)
{
    TcpClientState *state = (TcpClientState *)arg;
    
    state->last_error = err;
    state->closed = true;
    state->connect_complete = true;  // Signal connection attempt is done (failed)
    state->pcb = NULL;  // PCB is already freed by lwIP when this is called
}

// Poll callback - called periodically by lwIP
static err_t tcp_client_poll_cb(void *arg, struct altcp_pcb *tpcb)
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

// Allocate and zero-initialise a fresh connection state.
static TcpClientState *tcp_state_alloc(void)
{
    TcpClientState *state = (TcpClientState *)calloc(1, sizeof(TcpClientState));
    if (!state)
    {
        return NULL;
    }
    state->recv_head = 0;
    state->recv_tail = 0;
    state->connected = false;
    state->connect_complete = false;
    state->connect_error = ERR_OK;
    state->closed = false;
    state->last_error = ERR_OK;
    return state;
}

// Resolve a numeric IP or hostname into target_addr. Returns false on failure.
static bool tcp_resolve_addr(const char *host, ip_addr_t *target_addr, int timeout_ms)
{
    // First try to parse as a literal IPv4 address.
    ip4_addr_t test_addr;
    if (ip4addr_aton(host, &test_addr))
    {
        ip_addr_copy_from_ip4(*target_addr, test_addr);
        return true;
    }

    // Otherwise resolve the hostname via DNS.
    tcp_dns_complete = false;
    tcp_dns_success = false;
    ip_addr_set_zero(&tcp_dns_resolved_addr);

    cyw43_arch_lwip_begin();
    err_t dns_err = dns_gethostbyname(host, target_addr, tcp_dns_callback, NULL);
    cyw43_arch_lwip_end();

    if (dns_err == ERR_OK)
    {
        return true;  // Address was cached
    }
    if (dns_err == ERR_INPROGRESS)
    {
        poll_lwip_with_timeout(timeout_ms > 0 ? timeout_ms : 10000, &tcp_dns_complete);
        if (!tcp_dns_success)
        {
            return false;
        }
        ip_addr_copy(*target_addr, tcp_dns_resolved_addr);
        return true;
    }
    return false;
}

// Wire callbacks on an already-created pcb (state->pcb), initiate the connection
// to target_addr:port, and poll until it is established. On any failure the pcb
// is aborted and state freed; returns the state on success, NULL otherwise.
static void *tcp_connect_and_wait(TcpClientState *state, ip_addr_t *target_addr,
                                  uint16_t port, int timeout_ms)
{
    cyw43_arch_lwip_begin();
    altcp_arg(state->pcb, state);
    altcp_recv(state->pcb, tcp_client_recv_cb);
    altcp_err(state->pcb, tcp_client_err_cb);
    altcp_poll(state->pcb, tcp_client_poll_cb, 10);  // Poll every 5 seconds (10 * 500ms)

    err_t err = altcp_connect(state->pcb, target_addr, port, tcp_client_connected_cb);
    cyw43_arch_lwip_end();

    if (err != ERR_OK)
    {
        cyw43_arch_lwip_begin();
        altcp_abort(state->pcb);
        cyw43_arch_lwip_end();
        free(state);
        return NULL;
    }

    // Wait for connection (or, for TLS, the handshake) to complete.
    poll_lwip_with_timeout(timeout_ms > 0 ? timeout_ms : 30000, &state->connect_complete);

    if (!state->connect_complete || !state->connected)
    {
        if (state->pcb)
        {
            cyw43_arch_lwip_begin();
            altcp_arg(state->pcb, NULL);
            altcp_recv(state->pcb, NULL);
            altcp_err(state->pcb, NULL);
            altcp_poll(state->pcb, NULL, 0);
            altcp_abort(state->pcb);
            cyw43_arch_lwip_end();
        }
        free(state);
        return NULL;
    }

    return state;
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

    TcpClientState *state = tcp_state_alloc();
    if (!state)
    {
        return NULL;
    }

    ip_addr_t target_addr;
    if (!tcp_resolve_addr(ip_address, &target_addr, timeout_ms))
    {
        free(state);
        return NULL;
    }

    // Create a plain (non-TLS) layered PCB.
    cyw43_arch_lwip_begin();
    state->pcb = altcp_tcp_new_ip_type(IPADDR_TYPE_V4);
    cyw43_arch_lwip_end();

    if (!state->pcb)
    {
        free(state);
        return NULL;
    }

    return tcp_connect_and_wait(state, &target_addr, port, timeout_ms);
}

#ifdef LOGO_HAS_TLS
// mbedTLS time backend (MBEDTLS_PLATFORM_MS_TIME_ALT). A monotonic millisecond
// clock since boot is all mbedTLS needs here (DRBG reseed timing, session
// lifetime); it does not require wall-clock time.
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)to_ms_since_boot(get_absolute_time());
}

// Route all mbedTLS allocations to a PSRAM-backed heap so the TLS config and
// handshake (tens of KB, transiently) do not compete for the tight SRAM heap.
// Call once at boot with a region carved from PSRAM. mbedTLS calloc/free are
// bound to tls_heap_* at compile time (MBEDTLS_PLATFORM_CALLOC_MACRO), so if
// this is never called the heap stays uninitialised and every mbedTLS
// allocation fails -- i.e. HTTPS requires PSRAM, while plain HTTP is unaffected.
void picocalc_tls_heap_setup(void *base, size_t size)
{
    // mbedTLS calloc/free are bound to tls_heap_* at compile time via
    // MBEDTLS_PLATFORM_CALLOC_MACRO; here we just initialise the region.
    tls_heap_init(base, size);
}

// Build (once) the TLS client config that verifies servers against the bundled
// CA roots. Returns NULL if the config cannot be created.
static struct altcp_tls_config *tls_client_config(void)
{
    static struct altcp_tls_config *config = NULL;
    if (config == NULL)
    {
        config = altcp_tls_create_config_client(
            (const u8_t *)ca_certs_pem, ca_certs_pem_len);
    }
    return config;
}

static void *picocalc_network_tls_connect(const char *hostname, uint16_t port, int timeout_ms)
{
    if (!ensure_wifi_initialized() || !picocalc_wifi_is_connected())
    {
        return NULL;
    }

    if (!hostname || port == 0)
    {
        return NULL;
    }

    struct altcp_tls_config *config = tls_client_config();
    if (!config)
    {
        return NULL;
    }

    TcpClientState *state = tcp_state_alloc();
    if (!state)
    {
        return NULL;
    }

    ip_addr_t target_addr;
    if (!tcp_resolve_addr(hostname, &target_addr, timeout_ms))
    {
        free(state);
        return NULL;
    }

    // Create a TLS layered PCB and set the SNI / verification hostname so the
    // handshake checks the certificate against this name.
    cyw43_arch_lwip_begin();
    state->pcb = altcp_tls_new(config, IPADDR_TYPE_V4);
    if (state->pcb)
    {
        mbedtls_ssl_set_hostname((mbedtls_ssl_context *)altcp_tls_context(state->pcb),
                                 hostname);
    }
    cyw43_arch_lwip_end();

    if (!state->pcb)
    {
        free(state);
        return NULL;
    }

    return tcp_connect_and_wait(state, &target_addr, port, timeout_ms);
}
#endif // LOGO_HAS_TLS

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
        altcp_arg(state->pcb, NULL);
        altcp_recv(state->pcb, NULL);
        altcp_err(state->pcb, NULL);
        altcp_poll(state->pcb, NULL, 0);
        altcp_sent(state->pcb, NULL);

        // A server connection that was abandoned without a response (e.g. an
        // upload that failed or was torn down mid-transfer) is aborted with a
        // RST: this frees the PCB and its local port immediately, with no
        // lingering TIME_WAIT that would make a re-`http.listen` on the same
        // port fail. A connection that did send its response is closed
        // gracefully so the response is flushed first.
        if (state->from_listener && !state->wrote_data)
        {
            altcp_abort(state->pcb);
        }
        else if (altcp_close(state->pcb) != ERR_OK)
        {
            altcp_abort(state->pcb);
        }

        // Clear the PCB inside the lock so the listener's accept callback never
        // sees a stale pointer for a just-closed slot.
        state->pcb = NULL;
        cyw43_arch_lwip_end();
    }

    // A listener-owned server connection is a reusable slot: detach the PCB but
    // keep the struct so the listener's accept callback can reuse it for the
    // next connection. The listener frees it in unlisten.
    if (state->from_listener)
    {
        state->closed = true;
        return;
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

    // Reopen the receive window by what we just consumed (the receive callback
    // deferred this), so the sender may send more -- continuous flow control
    // that tracks how fast the application drains. Re-check the PCB inside the
    // lock: a background error callback could have freed it.
    if (bytes_read > 0)
    {
        cyw43_arch_lwip_begin();
        if (state->pcb)
        {
            altcp_recved(state->pcb, (u16_t)bytes_read);
        }
        cyw43_arch_lwip_end();
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
        u16_t available = altcp_sndbuf(state->pcb);
        if (available == 0)
        {
            cyw43_arch_lwip_end();

            // Wait for buffer space with timeout
            absolute_time_t timeout = make_timeout_time_ms(5000);
            while (altcp_sndbuf(state->pcb) == 0)
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
        err_t err = altcp_write(state->pcb, data + total_written, to_write, TCP_WRITE_FLAG_COPY);

        if (err != ERR_OK)
        {
            cyw43_arch_lwip_end();
            return total_written > 0 ? total_written : -1;
        }

        // Flush the output
        err = altcp_output(state->pcb);
        cyw43_arch_lwip_end();
        
        if (err != ERR_OK)
        {
            return total_written > 0 ? total_written : -1;
        }
        
        total_written += to_write;
    }

    // Mark that a response was written so a server connection is closed
    // gracefully (flushing the response) rather than aborted.
    state->wrote_data = true;
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

// ----------------------------------------------------------------------------
// TCP server (listener) support
// ----------------------------------------------------------------------------

// A listener owns the listening PCB and one pre-allocated connection slot
// (TCP_LISTEN_BACKLOG is 1, one connection at a time). The slot is a
// TcpClientState like the client path uses, so read/write/close are unchanged;
// it is allocated once in listen (never in the lwIP callback) and reused across
// connections until unlisten frees it.
typedef struct {
    struct altcp_pcb *listen_pcb;  // The listening PCB
    TcpClientState *conn;          // Pre-allocated connection slot (backlog 1)
    volatile bool ready;           // conn is wired to an accepted PCB, awaiting a claim
    ip_addr_t remote;              // Remote address of the accepted connection
} TcpListenerState;

// Accept callback: attach the accepted PCB to the pre-allocated slot and wire
// its receive callback immediately, so request bytes that arrive before the
// pump claims the connection are buffered instead of dropped. (lwIP frees
// inbound data delivered to a PCB whose recv callback is still NULL, which would
// otherwise lose the whole request and leave the client waiting for a reply.)
static err_t tcp_listener_accept_cb(void *arg, struct altcp_pcb *newpcb, err_t err)
{
    TcpListenerState *ls = (TcpListenerState *)arg;

    if (err != ERR_OK || newpcb == NULL)
    {
        return ERR_VAL;
    }
    // Backlog is one: refuse a new connection while the slot is occupied.
    if (ls == NULL || ls->conn == NULL || ls->conn->pcb != NULL)
    {
        altcp_abort(newpcb);
        return ERR_ABRT;
    }

    TcpClientState *st = ls->conn;
    st->recv_head = 0;
    st->recv_tail = 0;
    st->connected = true;
    st->connect_complete = true;
    st->connect_error = ERR_OK;
    st->closed = false;
    st->last_error = ERR_OK;
    st->wrote_data = false;
    st->pcb = newpcb;
    altcp_arg(newpcb, st);
    altcp_recv(newpcb, tcp_client_recv_cb);
    altcp_err(newpcb, tcp_client_err_cb);
    altcp_poll(newpcb, tcp_client_poll_cb, 10);

    ip_addr_t *remote = altcp_get_ip(newpcb, 0);  // 0 => remote address
    if (remote != NULL)
    {
        ip_addr_copy(ls->remote, *remote);
    }
    else
    {
        ip_addr_set_zero(&ls->remote);
    }
    ls->ready = true;
    return ERR_OK;
}

static void *picocalc_network_tcp_listen(uint16_t port)
{
    if (!ensure_wifi_initialized() || !picocalc_wifi_is_connected())
    {
        return NULL;
    }
    if (port == 0)
    {
        return NULL;
    }

    TcpListenerState *ls = (TcpListenerState *)calloc(1, sizeof(TcpListenerState));
    if (!ls)
    {
        return NULL;
    }
    // Allocate the connection slot up front (in the foreground; never in the
    // lwIP accept callback, where malloc would be unsafe).
    ls->conn = tcp_state_alloc();
    if (!ls->conn)
    {
        free(ls);
        return NULL;
    }
    ls->conn->from_listener = true;

    cyw43_arch_lwip_begin();
    struct altcp_pcb *pcb = altcp_tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!pcb)
    {
        cyw43_arch_lwip_end();
        free(ls->conn);
        free(ls);
        return NULL;
    }
    if (altcp_bind(pcb, IP_ANY_TYPE, port) != ERR_OK)
    {
        altcp_abort(pcb);
        cyw43_arch_lwip_end();
        free(ls->conn);
        free(ls);
        return NULL;
    }
    // altcp_listen returns the (possibly reallocated) listening PCB; the passed
    // PCB is consumed on success and must not be touched afterwards.
    struct altcp_pcb *listen_pcb = altcp_listen(pcb);
    if (!listen_pcb)
    {
        altcp_abort(pcb);
        cyw43_arch_lwip_end();
        free(ls->conn);
        free(ls);
        return NULL;
    }
    ls->listen_pcb = listen_pcb;
    altcp_arg(listen_pcb, ls);
    altcp_accept(listen_pcb, tcp_listener_accept_cb);
    cyw43_arch_lwip_end();

    return ls;
}

static void picocalc_network_tcp_unlisten(void *listener)
{
    TcpListenerState *ls = (TcpListenerState *)listener;
    if (!ls)
    {
        return;
    }

    cyw43_arch_lwip_begin();
    if (ls->conn && ls->conn->pcb)
    {
        altcp_arg(ls->conn->pcb, NULL);
        altcp_recv(ls->conn->pcb, NULL);
        altcp_err(ls->conn->pcb, NULL);
        altcp_poll(ls->conn->pcb, NULL, 0);
        if (altcp_close(ls->conn->pcb) != ERR_OK)
        {
            altcp_abort(ls->conn->pcb);
        }
        ls->conn->pcb = NULL;
    }
    if (ls->listen_pcb)
    {
        altcp_arg(ls->listen_pcb, NULL);
        altcp_accept(ls->listen_pcb, NULL);
        altcp_close(ls->listen_pcb);
    }
    cyw43_arch_lwip_end();

    free(ls->conn);
    free(ls);
}

static void *picocalc_network_tcp_accept(void *listener, char *remote_ip, size_t ip_size)
{
    TcpListenerState *ls = (TcpListenerState *)listener;
    if (!ls)
    {
        return NULL;
    }

    // Drive lwIP so a completed handshake turns into an accept callback.
    cyw43_arch_poll();
    sys_check_timeouts();

    // The accept callback has already wired the slot's receive callback; nothing
    // to do here but hand it over once a connection is ready.
    if (!ls->ready)
    {
        return NULL;
    }
    ls->ready = false;

    if (remote_ip && ip_size > 0)
    {
        const char *s = ipaddr_ntoa(&ls->remote);
        strncpy(remote_ip, s ? s : "", ip_size - 1);
        remote_ip[ip_size - 1] = '\0';
    }

    return ls->conn;
}

#endif // LOGO_HAS_WIFI

// ============================================================================
// Hardware ops structure
// ============================================================================

static LogoHardwareOps picocalc_hardware_ops = {
    .sleep = picocalc_sleep,
    .ticks_ms = picocalc_ticks_ms,
    .random = picocalc_random,
    .get_battery_level = picocalc_get_battery_level,
    .power_off = picocalc_power_off,
    .reboot_bootloader = picocalc_reboot_bootloader,
    .check_user_interrupt = picocalc_check_user_interrupt,
    .clear_user_interrupt = picocalc_clear_user_interrupt,
    .check_pause_request = picocalc_check_pause_request,
    .clear_pause_request = picocalc_clear_pause_request,
    .check_freeze_request = picocalc_check_freeze_request,
    .clear_freeze_request = picocalc_clear_freeze_request,
    // Sound synthesizer (P8) -- backed by the software PSG engine (sound.c)
    .sound_gate = sound_gate,
    .sound_queue = sound_queue,
    .sound_status = sound_status,
    .sound_stop = sound_stop,
    .sound_env = sound_env,
    .sound_wave = sound_wave,
#ifdef LOGO_HAS_WIFI
    .wifi_is_connected = picocalc_wifi_is_connected,
    .wifi_connect = picocalc_wifi_connect,
    .wifi_start = picocalc_wifi_start,
    .wifi_status = picocalc_wifi_status,
    .wifi_disconnect = picocalc_wifi_disconnect,
    .wifi_get_ip = picocalc_wifi_get_ip,
    .wifi_get_ssid = picocalc_wifi_get_ssid,
    .wifi_get_mac = picocalc_wifi_get_mac,
    .wifi_scan = picocalc_wifi_scan,
    .network_set_hostname = picocalc_network_set_hostname,
    .network_ping = picocalc_network_ping,
    .network_resolve = picocalc_network_resolve,
    .network_ntp = picocalc_network_ntp,
    .network_tcp_connect = picocalc_network_tcp_connect,
#ifdef LOGO_HAS_TLS
    .network_tls_connect = picocalc_network_tls_connect,
#else
    .network_tls_connect = NULL,
#endif
    .network_tcp_close = picocalc_network_tcp_close,
    .network_tcp_read = picocalc_network_tcp_read,
    .network_tcp_write = picocalc_network_tcp_write,
    .network_tcp_can_read = picocalc_network_tcp_can_read,
    .network_tcp_listen = picocalc_network_tcp_listen,
    .network_tcp_unlisten = picocalc_network_tcp_unlisten,
    .network_tcp_accept = picocalc_network_tcp_accept,
#else
    .wifi_is_connected = NULL,
    .wifi_connect = NULL,
    .wifi_start = NULL,
    .wifi_status = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_ip = NULL,
    .wifi_get_ssid = NULL,
    .wifi_get_mac = NULL,
    .wifi_scan = NULL,
    .network_set_hostname = NULL,
    .network_ping = NULL,
    .network_resolve = NULL,
    .network_ntp = NULL,
    .network_tcp_connect = NULL,
    .network_tls_connect = NULL,
    .network_tcp_close = NULL,
    .network_tcp_read = NULL,
    .network_tcp_write = NULL,
    .network_tcp_can_read = NULL,
    .network_tcp_listen = NULL,
    .network_tcp_unlisten = NULL,
    .network_tcp_accept = NULL,
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
