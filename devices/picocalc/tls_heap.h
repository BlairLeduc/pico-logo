//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  A small, self-contained heap allocator over a fixed memory region, used to
//  back mbedTLS allocations from PSRAM during the TLS handshake so they do not
//  compete for the tight SRAM heap. Classic K&R first-fit free list with
//  coalescing (portable; host-testable). Not thread-safe -- the Logo runtime
//  drives TLS from a single context.
//

#ifndef PICOCALC_TLS_HEAP_H
#define PICOCALC_TLS_HEAP_H

#include <stddef.h>

// Initialise the allocator over [region, region+bytes). Replaces any prior
// state (so it can be re-init in tests). The region must outlive all use.
void tls_heap_init(void *region, size_t bytes);

// malloc/free/calloc over the region. calloc has the signature mbedTLS expects
// for mbedtls_platform_set_calloc_free(). Return NULL when the region is full.
void *tls_heap_malloc(size_t nbytes);
void tls_heap_free(void *ptr);
void *tls_heap_calloc(size_t nmemb, size_t size);

#endif // PICOCALC_TLS_HEAP_H
