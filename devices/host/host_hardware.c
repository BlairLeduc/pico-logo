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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET socket_t;
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#define CLOSE_SOCKET closesocket
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
typedef int socket_t;
#define INVALID_SOCKET_VALUE (-1)
#define SOCKET_ERROR_VALUE (-1)
#define CLOSE_SOCKET close
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

//
// TCP network functions
//

#ifdef _WIN32
static bool winsock_initialized = false;

static bool init_winsock(void)
{
    if (winsock_initialized)
    {
        return true;
    }
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        return false;
    }
    winsock_initialized = true;
    return true;
}
#endif

static void *host_network_tcp_connect(const char *host, uint16_t port, int timeout_ms)
{
    if (!host || port == 0)
    {
        return NULL;
    }

#ifdef _WIN32
    if (!init_winsock())
    {
        return NULL;
    }
#endif

    // Resolve hostname
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int status = getaddrinfo(host, port_str, &hints, &result);
    if (status != 0)
    {
        return NULL;
    }

    // Create socket
    socket_t sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET_VALUE)
    {
        freeaddrinfo(result);
        return NULL;
    }

    // Set socket to non-blocking for timeout handling
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    // Attempt connection
    int connect_result = connect(sock, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

#ifdef _WIN32
    if (connect_result == SOCKET_ERROR_VALUE && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }
#else
    if (connect_result < 0 && errno != EINPROGRESS)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }
#endif

    // Wait for connection with timeout
    if (timeout_ms > 0)
    {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);

        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int select_result = select((int)(sock + 1), NULL, &write_fds, NULL, &timeout);
        if (select_result <= 0)
        {
            // Timeout or error
            CLOSE_SOCKET(sock);
            return NULL;
        }

        // Check if connection succeeded
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0 || error != 0)
        {
            CLOSE_SOCKET(sock);
            return NULL;
        }
    }

    // Set socket back to blocking mode
#ifdef _WIN32
    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif

    // Allocate handle to return (store socket in a dynamically allocated int)
    socket_t *handle = (socket_t *)malloc(sizeof(socket_t));
    if (!handle)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }
    *handle = sock;
    return handle;
}

static void host_network_tcp_close(void *handle)
{
    if (!handle)
    {
        return;
    }
    socket_t *sock_ptr = (socket_t *)handle;
    CLOSE_SOCKET(*sock_ptr);
    free(sock_ptr);
}

static int host_network_tcp_read(void *handle, char *buffer, int max_len, int timeout_ms)
{
    if (!handle || !buffer || max_len <= 0)
    {
        return -1;
    }

    socket_t sock = *((socket_t *)handle);

    // Set socket to non-blocking for timeout handling
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    if (timeout_ms > 0)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int select_result = select((int)(sock + 1), &read_fds, NULL, NULL, &timeout);
        if (select_result <= 0)
        {
            // Timeout (0) or error (-1)
#ifdef _WIN32
            mode = 0;
            ioctlsocket(sock, FIONBIO, &mode);
#else
            fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
            return select_result == 0 ? 0 : -1; // 0 for timeout (per hardware.h spec)
        }
    }

    int bytes_read = recv(sock, buffer, max_len, 0);

    // Set socket back to blocking mode
#ifdef _WIN32
    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif

    if (bytes_read == 0)
    {
        return -1; // Connection closed
    }
    else if (bytes_read < 0)
    {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK)
        {
            return 0; // Would block (timeout)
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0; // Would block (timeout)
        }
#endif
        return -1; // Error
    }

    return bytes_read;
}

static int host_network_tcp_write(void *handle, const char *data, int len)
{
    if (!handle || !data || len <= 0)
    {
        return -1;
    }

    socket_t sock = *((socket_t *)handle);

    int bytes_written = send(sock, data, len, 0);
    if (bytes_written < 0)
    {
        return -1; // Error
    }

    return bytes_written;
}

static bool host_network_tcp_can_read(void *handle)
{
    if (!handle)
    {
        return false;
    }

    socket_t sock = *((socket_t *)handle);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    // Non-blocking check
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int select_result = select((int)(sock + 1), &read_fds, NULL, NULL, &timeout);
    return select_result > 0;
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
    .network_ntp = NULL,
    .network_tcp_connect = host_network_tcp_connect,
    .network_tcp_close = host_network_tcp_close,
    .network_tcp_read = host_network_tcp_read,
    .network_tcp_write = host_network_tcp_write,
    .network_tcp_can_read = host_network_tcp_can_read,
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
