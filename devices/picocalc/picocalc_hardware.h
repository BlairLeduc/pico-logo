//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface for PicoCalc device.
//

#pragma once

#include "devices/hardware.h"

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoStorage API
    //

    LogoHardware *logo_picocalc_hardware_create(void);
    void logo_picocalc_hardware_destroy(LogoHardware *hardware);

#ifdef LOGO_HAS_WIFI
    // Point mbedTLS's allocator at a PSRAM-backed heap [base, base+size) for the
    // HTTPS client. Call once at boot, before any TLS use.
    void picocalc_tls_heap_setup(void *base, size_t size);
#endif

#ifdef __cplusplus
}
#endif
