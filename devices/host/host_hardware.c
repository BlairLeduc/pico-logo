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

static bool host_hardware_check_user_interrupt(void)
{
    return host_user_interrupt;
}

static void host_hardware_clear_user_interrupt(void)
{
    host_user_interrupt = false;
}

static LogoHardwareOps host_hardware_ops = {
    .sleep = host_hardware_sleep,
    .random = host_hardware_random,
    .get_battery_level = host_hardware_get_battery_level,
    .power_off = NULL,
    .check_user_interrupt = host_hardware_check_user_interrupt,
    .clear_user_interrupt = host_hardware_clear_user_interrupt,
    .toot = NULL,  // Host device has no audio
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
