//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  WiFi management primitives: wifi?, wifi.connect, wifi.disconnect,
//  wifi.ip, wifi.ssid, wifi.scan
//

#include "primitives.h"
#include "memory.h"
#include "devices/io.h"

#include <stdio.h>
#include <string.h>

// wifi?
// Returns true if WiFi is connected, false otherwise
static Result prim_wifi_connected(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_is_connected)
    {
        bool connected = io->hardware->ops->wifi_is_connected();
        return result_ok(value_word(mem_atom(connected ? "true" : "false", connected ? 4 : 5)));
    }

    // WiFi not available - return false
    return result_ok(value_word(mem_atom("false", 5)));
}

// wifi.connect ssid password
// Connects to a WiFi network
static Result prim_wifi_connect(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_WORD(args[0]);
    REQUIRE_WORD(args[1]);

    const char *ssid = mem_word_ptr(args[0].as.node);
    const char *password = mem_word_ptr(args[1].as.node);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_connect)
    {
        bool success = io->hardware->ops->wifi_connect(ssid, password);
        if (!success)
        {
            // Connection failed - return error
            return result_error_arg(ERR_DISK_TROUBLE, NULL, NULL);
        }
        return result_none();
    }

    // WiFi not available on this device
    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// wifi.disconnect
// Disconnects from the current WiFi network
static Result prim_wifi_disconnect(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_disconnect)
    {
        io->hardware->ops->wifi_disconnect();
        return result_none();
    }

    // WiFi not available on this device
    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// wifi.ip
// Returns the current IP address as a word, or empty list if not connected
static Result prim_wifi_ip(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_get_ip)
    {
        char ip_buffer[16];
        if (io->hardware->ops->wifi_get_ip(ip_buffer, sizeof(ip_buffer)))
        {
            return result_ok(value_word(mem_atom_cstr(ip_buffer)));
        }
    }

    // Not connected or WiFi not available - return empty list
    return result_ok(value_list(NODE_NIL));
}

// wifi.ssid
// Returns the SSID of the connected network, or empty list if not connected
static Result prim_wifi_ssid(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_get_ssid)
    {
        char ssid_buffer[33];
        if (io->hardware->ops->wifi_get_ssid(ssid_buffer, sizeof(ssid_buffer)))
        {
            return result_ok(value_word(mem_atom_cstr(ssid_buffer)));
        }
    }

    // Not connected or WiFi not available - return empty list
    return result_ok(value_list(NODE_NIL));
}

// wifi.scan
// Returns a list of [ssid strength] pairs for available networks
static Result prim_wifi_scan(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_scan)
    {
        // Scan for up to 16 networks
        #define MAX_SCAN_NETWORKS 16
        char ssids[MAX_SCAN_NETWORKS][33];
        int8_t strengths[MAX_SCAN_NETWORKS];

        int count = io->hardware->ops->wifi_scan(ssids, strengths, MAX_SCAN_NETWORKS);
        if (count < 0)
        {
            // Scan failed
            return result_error_arg(ERR_DISK_TROUBLE, NULL, NULL);
        }

        // Build result list: [[ssid1 strength1] [ssid2 strength2] ...]
        Node result = NODE_NIL;

        // Build list in reverse order (last to first) so it ends up in correct order
        for (int i = count - 1; i >= 0; i--)
        {
            // Create strength number as word
            char strength_str[8];
            snprintf(strength_str, sizeof(strength_str), "%d", (int)strengths[i]);
            Node strength_node = mem_atom_cstr(strength_str);

            // Create SSID word
            Node ssid_node = mem_atom_cstr(ssids[i]);

            // Create [ssid strength] pair
            Node pair = mem_cons(ssid_node, mem_cons(strength_node, NODE_NIL));

            // Add to result list
            result = mem_cons(pair, result);
        }

        return result_ok(value_list(result));
    }

    // WiFi not available - return empty list
    return result_ok(value_list(NODE_NIL));
}

void primitives_wifi_init(void)
{
    primitive_register("wifi?", 0, prim_wifi_connected);
    primitive_register("wifip", 0, prim_wifi_connected);  // Alias
    primitive_register("wifi.connect", 2, prim_wifi_connect);
    primitive_register("wifi.disconnect", 0, prim_wifi_disconnect);
    primitive_register("wifi.ip", 0, prim_wifi_ip);
    primitive_register("wifi.ssid", 0, prim_wifi_ssid);
    primitive_register("wifi.scan", 0, prim_wifi_scan);
}
