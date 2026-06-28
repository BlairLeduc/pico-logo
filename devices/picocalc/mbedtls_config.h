//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  mbedTLS configuration for the HTTPS client. A minimal TLS 1.2 *client*
//  profile: enough to verify mainstream public servers (ECDHE key exchange,
//  RSA/ECDSA certs, AES-GCM, SHA-2) against the bundled CA roots, with entropy
//  from the RP2350 hardware RNG (pico_mbedtls supplies mbedtls_hardware_poll).
//
//  Record buffers are tuned to bound transient SRAM during the handshake: the
//  inbound buffer must hold a full TLS record from the server (16 KB), but the
//  client sends small requests, so the outbound buffer is kept small.
//

#ifndef PICOCALC_MBEDTLS_CONFIG_H
#define PICOCALC_MBEDTLS_CONFIG_H

// System / platform
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY            // route calloc/free (to PSRAM)

// Bind mbedTLS allocation to the PSRAM-backed TLS heap at COMPILE time. This is
// deliberate and load-bearing: lwIP's altcp_tls, on seeing
// MBEDTLS_PLATFORM_MEMORY *without* these macros, installs its own allocator
// (ALTCP_MBEDTLS_PLATFORM_ALLOC) that draws from the small lwIP MEM_SIZE heap
// and rejects any single allocation larger than MEM_SIZE -- which cannot hold
// the CA chain or the 16 KB TLS record buffer. Defining the macros makes lwIP
// leave mbedTLS allocation alone, and routes it all to PSRAM via tls_heap.
#include <stddef.h>
extern void *tls_heap_calloc(size_t nmemb, size_t size);
extern void tls_heap_free(void *ptr);
#define MBEDTLS_PLATFORM_CALLOC_MACRO   tls_heap_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO     tls_heap_free

#define MBEDTLS_NO_PLATFORM_ENTROPY        // no /dev/urandom on bare metal
#define MBEDTLS_ENTROPY_HARDWARE_ALT       // pico_mbedtls: mbedtls_hardware_poll
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

// HAVE_TIME (without HAVE_TIME_DATE) gives the mbedtls_ssl_session.start field
// that lwIP's altcp_tls layer references, and timing for DRBG reseed, without
// making certificate validity-period checks depend on a wall clock the device
// may not have at handshake time. Chain, hostname and signature are still
// verified; certificate expiry is not enforced.
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_MS_TIME_ALT       // mbedtls_ms_time() in picocalc_hardware.c

// Entropy / RNG
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

// TLS core (1.2 client only)
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION // mbedtls_ssl_set_hostname (SNI)

// Key exchange (forward-secret ECDHE with RSA or ECDSA server certs)
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

// Public-key crypto
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_BIGNUM_C

// Symmetric crypto / AEAD
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C

// Hashes (SHA-1 retained for older certificate signatures)
#define MBEDTLS_MD_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SHA1_C

// X.509 certificate parsing / chain verification
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_X509_REMOVE_INFO           // drop human-readable strings (save flash)

// Record buffer sizing (bound handshake SRAM)
#define MBEDTLS_SSL_IN_CONTENT_LEN  16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

#endif // PICOCALC_MBEDTLS_CONFIG_H
