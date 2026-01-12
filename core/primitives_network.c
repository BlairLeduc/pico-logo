//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Network primitives: network.ping, network.resolve, network.open, network.ntp
//

#include "primitives.h"
#include "memory.h"
#include "devices/io.h"

#include <stdio.h>
#include <string.h>

// network.ping ipaddress
// Sends a ping request to the specified IP address
// Returns the round-trip time in milliseconds (e.g., 22.413), or -1 on failure
static Result prim_network_ping(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);

    const char *ip_address = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->network_ping)
    {
        float result_ms = io->hardware->ops->network_ping(ip_address);
        // Format the result as a number
        char buffer[24];
        if (result_ms < 0)
        {
            snprintf(buffer, sizeof(buffer), "-1");
        }
        else
        {
            // Format with up to 3 decimal places, trimming trailing zeros
            snprintf(buffer, sizeof(buffer), "%.3f", result_ms);
            // Trim trailing zeros after decimal point
            char *dot = strchr(buffer, '.');
            if (dot)
            {
                char *end = buffer + strlen(buffer) - 1;
                while (end > dot && *end == '0') end--;
                if (end == dot) end--;  // Remove decimal point if no decimals
                *(end + 1) = '\0';
            }
        }
        return result_ok(value_word(mem_atom_cstr(buffer)));
    }

    // Network ping not available on this device
    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// network.resolve hostname
// Resolves a hostname to an IP address
// Returns the IP address in dotted-decimal notation, or the empty list on failure
static Result prim_network_resolve(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);

    const char *hostname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->network_resolve)
    {
        char ip_buffer[16];
        if (io->hardware->ops->network_resolve(hostname, ip_buffer, sizeof(ip_buffer)))
        {
            return result_ok(value_word(mem_atom_cstr(ip_buffer)));
        }
        // Resolution failed - return empty list
        return result_ok(value_list(NODE_NIL));
    }

    // Network resolve not available on this device
    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

void primitives_network_init(void)
{
    primitive_register("network.ping", 1, prim_network_ping);
    primitive_register("network.resolve", 1, prim_network_resolve);
}
