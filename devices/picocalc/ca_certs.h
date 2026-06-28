//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Trimmed CA root bundle for the HTTPS client (see ca_certs.c). The bundle is
//  generated from a system/Mozilla trust store by tools/gen_ca_certs.py; only a
//  small high-coverage set of roots is kept to bound flash and the RAM cost of
//  parsing the trust store during the TLS handshake.
//

#ifndef PICOCALC_CA_CERTS_H
#define PICOCALC_CA_CERTS_H

#include <stddef.h>

// PEM-encoded, NUL-terminated bundle of trusted CA roots.
extern const char ca_certs_pem[];

// Length of ca_certs_pem including the terminating NUL (as mbedTLS's PEM parser
// requires).
extern const size_t ca_certs_pem_len;

#endif // PICOCALC_CA_CERTS_H
