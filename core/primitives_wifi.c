//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  WiFi management primitives: wifi?, wifi.connect, wifi.disconnect,
//  wifi.ip, wifi.mac, wifi.ssid, wifi.scan
//

#include "primitives.h"
#include "memory.h"
#include "limits.h"
#include "devices/io.h"

#include <stdio.h>
#include <string.h>

// The device network name, excluding the `.local` mDNS suffix. Core owns it so
// `wifi.hostname` reads it back and reconnects reuse it; the device applies it. It
// resets to the default on init (no persistence across reboots).
static char g_hostname[HOSTNAME_MAX + 1] = "picologo";

// True if `name` is a valid hostname label: 1..HOSTNAME_MAX characters of
// letters, digits, and hyphens, not starting or ending with a hyphen (so the
// name is a legal DNS label once `.local` is appended).
static bool hostname_is_valid(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len > HOSTNAME_MAX) return false;
    if (name[0] == '-' || name[len - 1] == '-') return false;
    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

// wifi?
// Returns true if WiFi is connected, false otherwise
static Result prim_wifi_connected(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_is_connected)
    {
        bool connected = io->hardware->ops->wifi_is_connected();
        return result_ok(value_bool(connected));
    }

    // WiFi not available - return false
    return result_ok(value_bool(false));
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

// wifi.start ssid password
// Begins connecting to a WiFi network without waiting for the result, so a
// startup file can reach the prompt immediately. Progress is reported by
// `wifi?` and `wifi.status`, which a `when` demon can wait on.
static Result prim_wifi_start(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_WORD(args[0]);
    REQUIRE_WORD(args[1]);

    const char *ssid = mem_word_ptr(args[0].as.node);
    const char *password = mem_word_ptr(args[1].as.node);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_start)
    {
        if (!io->hardware->ops->wifi_start(ssid, password))
        {
            // The attempt could not be started (radio unavailable); a failure
            // to *associate* shows up later as `wifi.status` = failed.
            return result_error_arg(ERR_DEVICE_UNAVAILABLE, NULL, NULL);
        }
        return result_none();
    }

    // WiFi not available on this device
    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// wifi.status
// Outputs the connection state as a word: off, connecting, connected, failed.
// Lets a demon distinguish "still trying" from "gave up", which `wifi?` alone
// cannot express.
static Result prim_wifi_status(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    int state = WIFI_STATE_OFF;
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_status)
    {
        state = io->hardware->ops->wifi_status();
    }

    const char *name;
    switch (state)
    {
        case WIFI_STATE_CONNECTING: name = "connecting"; break;
        case WIFI_STATE_NOIP:       name = "noaddress";  break;
        case WIFI_STATE_CONNECTED:  name = "connected";  break;
        case WIFI_STATE_NONET:      name = "notfound";   break;
        case WIFI_STATE_BADAUTH:    name = "badpassword";break;
        case WIFI_STATE_FAILED:     name = "failed";     break;
        default:                    name = "off";        break;
    }
    return result_ok(value_word(mem_atom_cstr(name)));
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

// wifi.mac
// Returns the MAC address as a colon-separated hex word (e.g. "00:00:5E:12:34:56")
static Result prim_wifi_mac(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->wifi_get_mac)
    {
        uint8_t mac[6];
        if (io->hardware->ops->wifi_get_mac(mac))
        {
            char buf[18]; // "XX:XX:XX:XX:XX:XX" + null
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return result_ok(value_word(mem_atom_cstr(buf)));
        }
    }

    // WiFi not available - return empty list
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

// tls?
// Returns true if the device supports TLS/HTTPS (requires WiFi + PSRAM), false
// otherwise. Unlike wifi? this reports a hardware/build capability, not the
// live connection state: boards with a radio but no PSRAM (e.g. Pico 2 W) do
// WiFi and plain http:// but cannot open https:// URLs.
static Result prim_tls_supported(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    bool supported = io && io->hardware && io->hardware->ops &&
                     io->hardware->ops->network_tls_connect;
    return result_ok(value_bool(supported));
}

// wifi.sethostname name
// Sets the device's network name (mDNS `<name>.local` and DHCP), excluding the
// `.local` suffix. The name must be a valid hostname label.
static Result prim_wifi_sethostname(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);

    const char *name = mem_word_ptr(args[0].as.node);
    if (!hostname_is_valid(name))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, name);

    strcpy(g_hostname, name);  // Length checked by hostname_is_valid.

    // Apply to the device if it supports naming; a device without the op (or no
    // radio at all) still tracks the name for `wifi.hostname`.
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->network_set_hostname)
    {
        io->hardware->ops->network_set_hostname(g_hostname);
    }
    return result_none();
}

// wifi.hostname
// Outputs the current device network name (without the `.local` suffix).
static Result prim_wifi_hostname(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    return result_ok(value_word(mem_atom_cstr(g_hostname)));
}

void primitives_wifi_init(void)
{
    // No persistence across reboots: reset to the default name on init.
    strcpy(g_hostname, "picologo");

    primitive_register("wifi?", 0, prim_wifi_connected);
    primitive_register("wifip", 0, prim_wifi_connected);  // Alias
    primitive_register("wifi.connect", 2, prim_wifi_connect);
    primitive_register("wifi.start", 2, prim_wifi_start);
    primitive_register("wifi.status", 0, prim_wifi_status);
    primitive_register("wifi.disconnect", 0, prim_wifi_disconnect);
    primitive_register("wifi.ip", 0, prim_wifi_ip);
    primitive_register("wifi.mac", 0, prim_wifi_mac);
    primitive_register("wifi.ssid", 0, prim_wifi_ssid);
    primitive_register("wifi.scan", 0, prim_wifi_scan);
    primitive_register("tls?", 0, prim_tls_supported);
    primitive_register("tlsp", 0, prim_tls_supported);  // Alias
    primitive_register("wifi.sethostname", 1, prim_wifi_sethostname);
    primitive_register("wifi.hostname", 0, prim_wifi_hostname);
}
