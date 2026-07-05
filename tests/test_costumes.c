//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the colour costume pool (devices/picocalc/costumes.c)
//

#include "unity.h"
#include "costumes.h"

#include <string.h>

void setUp(void)
{
    costumes_clear();
}

void tearDown(void)
{
}

// Fill a pixel buffer with a recognizable pattern seeded per costume
static void fill_pattern(uint8_t *buf, int size, uint8_t seed)
{
    for (int i = 0; i < size; i++)
    {
        buf[i] = (uint8_t)(seed + i);
    }
}

void test_put_get_roundtrip(void)
{
    uint8_t pixels[16 * 16];
    fill_pattern(pixels, sizeof(pixels), 7);

    TEST_ASSERT_TRUE(costume_put(1, 16, 16, pixels));

    uint8_t w = 0, h = 0;
    const uint8_t *data = NULL;
    TEST_ASSERT_TRUE(costume_get(1, &w, &h, &data));
    TEST_ASSERT_EQUAL(16, w);
    TEST_ASSERT_EQUAL(16, h);
    TEST_ASSERT_EQUAL_MEMORY(pixels, data, sizeof(pixels));
}

void test_get_empty_slot_fails(void)
{
    TEST_ASSERT_FALSE(costume_get(1, NULL, NULL, NULL));
    TEST_ASSERT_FALSE(costume_get(15, NULL, NULL, NULL));
}

void test_put_rejects_bad_slots(void)
{
    uint8_t pixels[8 * 8] = {0};
    TEST_ASSERT_FALSE(costume_put(0, 8, 8, pixels));   // Slot 0 is the vector turtle
    TEST_ASSERT_FALSE(costume_put(16, 8, 8, pixels));  // Out of range
}

void test_put_rejects_bad_dimensions(void)
{
    uint8_t pixels[64 * 64] = {0};
    TEST_ASSERT_FALSE(costume_put(1, 7, 8, pixels));
    TEST_ASSERT_FALSE(costume_put(1, 8, 7, pixels));
    TEST_ASSERT_FALSE(costume_put(1, 33, 8, pixels));
    TEST_ASSERT_FALSE(costume_put(1, 8, 33, pixels));
    TEST_ASSERT_FALSE(costume_put(1, 8, 8, NULL));
}

void test_rectangular_costume(void)
{
    uint8_t pixels[8 * 24];
    fill_pattern(pixels, sizeof(pixels), 3);

    TEST_ASSERT_TRUE(costume_put(2, 8, 24, pixels));

    uint8_t w, h;
    const uint8_t *data;
    TEST_ASSERT_TRUE(costume_get(2, &w, &h, &data));
    TEST_ASSERT_EQUAL(8, w);
    TEST_ASSERT_EQUAL(24, h);
    TEST_ASSERT_EQUAL_MEMORY(pixels, data, sizeof(pixels));
}

void test_replace_frees_old_block(void)
{
    uint8_t big[32 * 32];
    fill_pattern(big, sizeof(big), 1);
    uint8_t small[8 * 8];
    fill_pattern(small, sizeof(small), 2);

    TEST_ASSERT_TRUE(costume_put(1, 32, 32, big));
    int free_after_big = costume_pool_free();

    TEST_ASSERT_TRUE(costume_put(1, 8, 8, small));
    TEST_ASSERT_EQUAL(free_after_big + 32 * 32 - 8 * 8, costume_pool_free());

    uint8_t w, h;
    const uint8_t *data;
    TEST_ASSERT_TRUE(costume_get(1, &w, &h, &data));
    TEST_ASSERT_EQUAL(8, w);
    TEST_ASSERT_EQUAL_MEMORY(small, data, sizeof(small));
}

void test_delete_compacts_and_preserves_others(void)
{
    uint8_t a[16 * 16], b[16 * 16], c[16 * 16];
    fill_pattern(a, sizeof(a), 10);
    fill_pattern(b, sizeof(b), 20);
    fill_pattern(c, sizeof(c), 30);

    TEST_ASSERT_TRUE(costume_put(1, 16, 16, a));
    TEST_ASSERT_TRUE(costume_put(2, 16, 16, b));
    TEST_ASSERT_TRUE(costume_put(3, 16, 16, c));

    costume_delete(2);

    TEST_ASSERT_FALSE(costume_get(2, NULL, NULL, NULL));

    const uint8_t *data;
    TEST_ASSERT_TRUE(costume_get(1, NULL, NULL, &data));
    TEST_ASSERT_EQUAL_MEMORY(a, data, sizeof(a));
    TEST_ASSERT_TRUE(costume_get(3, NULL, NULL, &data));
    TEST_ASSERT_EQUAL_MEMORY(c, data, sizeof(c));

    TEST_ASSERT_EQUAL(COSTUME_POOL_SIZE - 2 * 16 * 16, costume_pool_free());
}

void test_pool_exhaustion_and_recovery(void)
{
    uint8_t pixels[32 * 32];
    fill_pattern(pixels, sizeof(pixels), 5);

    // 8192 / 1024 = 8 costumes fill the pool exactly
    for (int slot = 1; slot <= 8; slot++)
    {
        TEST_ASSERT_TRUE(costume_put((uint8_t)slot, 32, 32, pixels));
    }
    TEST_ASSERT_EQUAL(0, costume_pool_free());

    // Ninth doesn't fit
    TEST_ASSERT_FALSE(costume_put(9, 32, 32, pixels));

    // Freeing one makes room again
    costume_delete(4);
    TEST_ASSERT_TRUE(costume_put(9, 32, 32, pixels));

    const uint8_t *data;
    TEST_ASSERT_TRUE(costume_get(9, NULL, NULL, &data));
    TEST_ASSERT_EQUAL_MEMORY(pixels, data, sizeof(pixels));
}

void test_failed_put_keeps_pool_intact(void)
{
    uint8_t pixels[32 * 32];
    fill_pattern(pixels, sizeof(pixels), 9);

    for (int slot = 1; slot <= 8; slot++)
    {
        TEST_ASSERT_TRUE(costume_put((uint8_t)slot, 32, 32, pixels));
    }

    // Adding a ninth costume to the full pool fails, and every existing
    // slot must be untouched.
    TEST_ASSERT_FALSE(costume_put(9, 32, 32, pixels));

    const uint8_t *data;
    for (int slot = 1; slot <= 8; slot++)
    {
        TEST_ASSERT_TRUE(costume_get((uint8_t)slot, NULL, NULL, &data));
        TEST_ASSERT_EQUAL_MEMORY(pixels, data, sizeof(pixels));
    }
}

void test_failed_replace_empties_slot_preserves_others(void)
{
    uint8_t small[8 * 8];
    fill_pattern(small, sizeof(small), 6);
    uint8_t big[32 * 32];
    fill_pattern(big, sizeof(big), 7);

    // Slot 1 small (64 B), slots 2-8 32x32 (7168 B), slot 9 8x16 (128 B):
    // 7360 used, 832 free
    TEST_ASSERT_TRUE(costume_put(1, 8, 8, small));
    for (int slot = 2; slot <= 8; slot++)
    {
        TEST_ASSERT_TRUE(costume_put((uint8_t)slot, 32, 32, big));
    }
    TEST_ASSERT_TRUE(costume_put(9, 8, 16, big));

    // Replacing slot 1 with a 32x32 frees its 64 bytes first but still
    // can't fit (1024 > 832 + 64): replace = delete then allocate, so
    // the failure leaves slot 1 empty — and every other slot untouched
    TEST_ASSERT_FALSE(costume_put(1, 32, 32, big));
    TEST_ASSERT_FALSE(costume_get(1, NULL, NULL, NULL));

    const uint8_t *data;
    for (int slot = 2; slot <= 8; slot++)
    {
        TEST_ASSERT_TRUE(costume_get((uint8_t)slot, NULL, NULL, &data));
        TEST_ASSERT_EQUAL_MEMORY(big, data, sizeof(big));
    }
    TEST_ASSERT_TRUE(costume_get(9, NULL, NULL, &data));
    TEST_ASSERT_EQUAL_MEMORY(big, data, 8 * 16);
}

void test_clear_empties_all_slots(void)
{
    uint8_t pixels[8 * 8];
    fill_pattern(pixels, sizeof(pixels), 4);

    TEST_ASSERT_TRUE(costume_put(1, 8, 8, pixels));
    TEST_ASSERT_TRUE(costume_put(15, 8, 8, pixels));

    costumes_clear();

    TEST_ASSERT_FALSE(costume_get(1, NULL, NULL, NULL));
    TEST_ASSERT_FALSE(costume_get(15, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(COSTUME_POOL_SIZE, costume_pool_free());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_put_get_roundtrip);
    RUN_TEST(test_get_empty_slot_fails);
    RUN_TEST(test_put_rejects_bad_slots);
    RUN_TEST(test_put_rejects_bad_dimensions);
    RUN_TEST(test_rectangular_costume);
    RUN_TEST(test_replace_frees_old_block);
    RUN_TEST(test_delete_compacts_and_preserves_others);
    RUN_TEST(test_pool_exhaustion_and_recovery);
    RUN_TEST(test_failed_put_keeps_pool_intact);
    RUN_TEST(test_failed_replace_empties_slot_preserves_others);
    RUN_TEST(test_clear_empties_all_slots);

    return UNITY_END();
}
