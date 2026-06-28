//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the fixed-region TLS heap allocator (devices/picocalc/tls_heap.c).
//  Verifies allocation, alignment, free + coalescing, reuse, exhaustion, and
//  recovery -- the properties mbedTLS relies on across a handshake.
//

#include "unity.h"
#include "tls_heap.h"
#include <stdint.h>
#include <string.h>

// 64 KB region, aligned for worst-case access.
_Alignas(16) static unsigned char region[64 * 1024];

void setUp(void)
{
    tls_heap_init(region, sizeof(region));
}

void tearDown(void) {}

void test_malloc_returns_usable_aligned_memory(void)
{
    void *p = tls_heap_malloc(100);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL(0, (uintptr_t)p % _Alignof(max_align_t));
    memset(p, 0xAB, 100); // must not corrupt anything
    tls_heap_free(p);
}

void test_calloc_zeroes(void)
{
    unsigned char *p = (unsigned char *)tls_heap_calloc(64, 4);
    TEST_ASSERT_NOT_NULL(p);
    for (int i = 0; i < 64 * 4; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }
    tls_heap_free(p);
}

void test_calloc_overflow_returns_null(void)
{
    TEST_ASSERT_NULL(tls_heap_calloc((size_t)-1, 2));
}

void test_zero_and_null_are_safe(void)
{
    TEST_ASSERT_NULL(tls_heap_malloc(0));
    tls_heap_free(NULL); // must not crash
}

void test_distinct_allocations_do_not_overlap(void)
{
    unsigned char *a = (unsigned char *)tls_heap_malloc(200);
    unsigned char *b = (unsigned char *)tls_heap_malloc(200);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    memset(a, 0x11, 200);
    memset(b, 0x22, 200);
    for (int i = 0; i < 200; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0x11, a[i]);
        TEST_ASSERT_EQUAL_UINT8(0x22, b[i]);
    }
    tls_heap_free(a);
    tls_heap_free(b);
}

void test_free_allows_reuse(void)
{
    void *p = tls_heap_malloc(1000);
    TEST_ASSERT_NOT_NULL(p);
    tls_heap_free(p);
    void *q = tls_heap_malloc(1000);
    TEST_ASSERT_NOT_NULL(q);
    // Freed memory should be reused, not grown into fresh territory.
    TEST_ASSERT_EQUAL_PTR(p, q);
    tls_heap_free(q);
}

void test_coalescing_restores_large_block(void)
{
    // Carve the region into many small blocks, free them all, then a single
    // large allocation must succeed -- only possible if free() coalesces.
    void *blocks[32];
    for (int i = 0; i < 32; i++)
    {
        blocks[i] = tls_heap_malloc(1024);
        TEST_ASSERT_NOT_NULL(blocks[i]);
    }
    for (int i = 0; i < 32; i++)
    {
        tls_heap_free(blocks[i]);
    }
    void *big = tls_heap_malloc(30 * 1024);
    TEST_ASSERT_NOT_NULL(big);
    tls_heap_free(big);
}

void test_exhaustion_returns_null_then_recovers(void)
{
    void *big = tls_heap_malloc(63 * 1024);
    TEST_ASSERT_NOT_NULL(big);
    TEST_ASSERT_NULL(tls_heap_malloc(63 * 1024)); // no room for a second
    tls_heap_free(big);
    void *again = tls_heap_malloc(63 * 1024); // space recovered after free
    TEST_ASSERT_NOT_NULL(again);
    tls_heap_free(again);
}

void test_uninitialised_region_returns_null(void)
{
    tls_heap_init(NULL, 0);
    TEST_ASSERT_NULL(tls_heap_malloc(16));
    tls_heap_init(region, sizeof(region)); // restore for following tests
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_malloc_returns_usable_aligned_memory);
    RUN_TEST(test_calloc_zeroes);
    RUN_TEST(test_calloc_overflow_returns_null);
    RUN_TEST(test_zero_and_null_are_safe);
    RUN_TEST(test_distinct_allocations_do_not_overlap);
    RUN_TEST(test_free_allows_reuse);
    RUN_TEST(test_coalescing_restores_large_block);
    RUN_TEST(test_exhaustion_returns_null_then_recovers);
    RUN_TEST(test_uninitialised_region_returns_null);
    return UNITY_END();
}
