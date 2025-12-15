//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface for PicoCalc device.
//

#include "../hardware.h"
#include "picocalc_hardware.h"
#include "southbridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pico/stdlib.h>
#include <pico/rand.h>


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

static LogoHardwareOps picocalc_hardware_ops = {
    .sleep = picocalc_sleep,
    .random = picocalc_random,
    .get_battery_level = picocalc_get_battery_level,
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
