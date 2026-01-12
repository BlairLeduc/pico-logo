//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a host device that uses standard input and output.
// 

#include "devices/host/host_hardware.h"
#include "devices/console.h"
#include "devices/hardware.h"
#include "devices/stream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

//
// LogoHardware API
//

static void host_hardware_sleep(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

static uint32_t host_hardware_random(void)
{
    static bool seeded = false;
    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = true;
    }
    // Generate a 32-bit random number from multiple rand() calls
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

static void host_hardware_get_battery_level(int *level, bool *charging)
{
    // Host doesn't have a battery, report 100% and not charging
    if (level)
    {
        *level = 100;
    }
    if (charging)
    {
        *charging = false;
    }
}

// User interrupt flag for host (can be set by signal handler if needed)
static volatile bool host_user_interrupt = false;
static volatile bool host_pause_requested = false;
static volatile bool host_freeze_requested = false;

static bool host_hardware_check_user_interrupt(void)
{
    return host_user_interrupt;
}

static void host_hardware_clear_user_interrupt(void)
{
    host_user_interrupt = false;
}

static bool host_hardware_check_pause_request(void)
{
    return host_pause_requested;
}

static void host_hardware_clear_pause_request(void)
{
    host_pause_requested = false;
}

static bool host_hardware_check_freeze_request(void)
{
    return host_freeze_requested;
}

static void host_hardware_clear_freeze_request(void)
{
    host_freeze_requested = false;
}

//
// Time management functions
//

static bool host_hardware_get_date(int *year, int *month, int *day)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info)
    {
        return false;
    }

    if (year)
    {
        *year = tm_info->tm_year + 1900;
    }
    if (month)
    {
        *month = tm_info->tm_mon + 1;
    }
    if (day)
    {
        *day = tm_info->tm_mday;
    }
    return true;
}

static bool host_hardware_get_time(int *hour, int *minute, int *second)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info)
    {
        return false;
    }

    if (hour)
    {
        *hour = tm_info->tm_hour;
    }
    if (minute)
    {
        *minute = tm_info->tm_min;
    }
    if (second)
    {
        *second = tm_info->tm_sec;
    }
    return true;
}

static bool host_hardware_set_date(int year, int month, int day)
{
    // Host device cannot set system date (would require root privileges)
    // Just return false to indicate the operation is not supported
    (void)year;
    (void)month;
    (void)day;
    return false;
}

static bool host_hardware_set_time(int hour, int minute, int second)
{
    // Host device cannot set system time (would require root privileges)
    // Just return false to indicate the operation is not supported
    (void)hour;
    (void)minute;
    (void)second;
    return false;
}

static LogoHardwareOps host_hardware_ops = {
    .sleep = host_hardware_sleep,
    .random = host_hardware_random,
    .get_battery_level = host_hardware_get_battery_level,
    .power_off = NULL,
    .check_user_interrupt = host_hardware_check_user_interrupt,
    .clear_user_interrupt = host_hardware_clear_user_interrupt,
    .check_pause_request = host_hardware_check_pause_request,
    .clear_pause_request = host_hardware_clear_pause_request,
    .check_freeze_request = host_hardware_check_freeze_request,
    .clear_freeze_request = host_hardware_clear_freeze_request,
    .toot = NULL,  // Host device has no audio
    .wifi_is_connected = NULL,
    .wifi_connect = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_ip = NULL,
    .wifi_get_ssid = NULL,
    .wifi_scan = NULL,
    .network_ping = NULL,
    .network_resolve = NULL,
    .get_date = host_hardware_get_date,
    .get_time = host_hardware_get_time,
    .set_date = host_hardware_set_date,
    .set_time = host_hardware_set_time,
};

LogoHardware *logo_host_hardware_create(void)
{
    LogoHardware *hardware = (LogoHardware *)malloc(sizeof(LogoHardware));
    if (!hardware)
    {
        return NULL;
    }

    logo_hardware_init(hardware, &host_hardware_ops);
    return hardware;
}

void logo_host_hardware_destroy(LogoHardware *hardware)
{
    if (!hardware)
    {
        return;
    }

    free(hardware);
}
