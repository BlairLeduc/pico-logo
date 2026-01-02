//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the frame arena allocator.
//

#include "unity.h"
#include "core/frame_arena.h"

#include <string.h>
#include <stdlib.h>

// Test arena memory - 4KB (1024 words)
#define TEST_ARENA_SIZE 4096
static uint32_t test_memory[TEST_ARENA_SIZE / sizeof(uint32_t)];
static FrameArena arena;

void setUp(void)
{
    memset(test_memory, 0, sizeof(test_memory));
    arena_init(&arena, test_memory, sizeof(test_memory));
}

void tearDown(void)
{
    // Nothing to tear down
}

//============================================================================
// Initialization Tests
//============================================================================

void test_init_sets_capacity(void)
{
    FrameArena a;
    arena_init(&a, test_memory, 1024);
    TEST_ASSERT_EQUAL(1024 / 4, arena_capacity(&a));
}

void test_init_empty_arena(void)
{
    TEST_ASSERT_TRUE(arena_is_empty(&arena));
    TEST_ASSERT_EQUAL(0, arena_used(&arena));
}

void test_init_full_available(void)
{
    TEST_ASSERT_EQUAL(TEST_ARENA_SIZE / 4, arena_available(&arena));
}

void test_init_null_memory_fails(void)
{
    FrameArena a;
    TEST_ASSERT_FALSE(arena_init(&a, NULL, 1024));
}

void test_init_null_arena_fails(void)
{
    TEST_ASSERT_FALSE(arena_init(NULL, test_memory, 1024));
}

void test_init_misaligned_memory_fails(void)
{
    // Create misaligned pointer (1 byte offset)
    uint8_t *misaligned = ((uint8_t *)test_memory) + 1;
    FrameArena a;
    TEST_ASSERT_FALSE(arena_init(&a, misaligned, 1024));
}

void test_init_capacity_clamped_to_max(void)
{
    // Try to init with more than max addressable (256KB)
    // This test uses a mock scenario since we can't actually allocate that much
    FrameArena a;
    // 0xFFFF words = 262140 bytes, anything larger should be clamped
    // We can't easily test this without allocating 256KB+, so we verify
    // the capacity is at most OFFSET_NONE - 1
    arena_init(&a, test_memory, sizeof(test_memory));
    TEST_ASSERT_TRUE(arena_capacity(&a) < OFFSET_NONE);
}

//============================================================================
// Allocation Tests
//============================================================================

void test_alloc_single_word(void)
{
    word_offset_t off = arena_alloc_words(&arena, 1);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);
    TEST_ASSERT_EQUAL(0, off);  // First allocation is at offset 0
}

void test_alloc_updates_used(void)
{
    arena_alloc_words(&arena, 10);
    TEST_ASSERT_EQUAL(10, arena_used(&arena));
}

void test_alloc_updates_available(void)
{
    word_offset_t initial = arena_available(&arena);
    arena_alloc_words(&arena, 10);
    TEST_ASSERT_EQUAL(initial - 10, arena_available(&arena));
}

void test_alloc_sequential_offsets(void)
{
    word_offset_t off1 = arena_alloc_words(&arena, 5);
    word_offset_t off2 = arena_alloc_words(&arena, 3);
    word_offset_t off3 = arena_alloc_words(&arena, 7);

    TEST_ASSERT_EQUAL(0, off1);
    TEST_ASSERT_EQUAL(5, off2);
    TEST_ASSERT_EQUAL(8, off3);
}

void test_alloc_zero_words_returns_none(void)
{
    word_offset_t off = arena_alloc_words(&arena, 0);
    TEST_ASSERT_EQUAL(OFFSET_NONE, off);
}

void test_alloc_too_large_returns_none(void)
{
    word_offset_t capacity = arena_capacity(&arena);
    word_offset_t off = arena_alloc_words(&arena, capacity + 1);
    TEST_ASSERT_EQUAL(OFFSET_NONE, off);
}

void test_alloc_exact_capacity(void)
{
    word_offset_t capacity = arena_capacity(&arena);
    word_offset_t off = arena_alloc_words(&arena, capacity);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);
    TEST_ASSERT_EQUAL(0, arena_available(&arena));
}

void test_alloc_exhausts_arena(void)
{
    // Allocate until full
    while (arena_available(&arena) >= 10)
    {
        word_offset_t off = arena_alloc_words(&arena, 10);
        TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);
    }

    // Remaining space is less than 10, try to allocate 10 more
    if (arena_available(&arena) < 10)
    {
        word_offset_t off = arena_alloc_words(&arena, 10);
        TEST_ASSERT_EQUAL(OFFSET_NONE, off);
    }
}

//============================================================================
// Offset/Pointer Conversion Tests
//============================================================================

void test_offset_to_ptr_none_returns_null(void)
{
    void *ptr = arena_offset_to_ptr(&arena, OFFSET_NONE);
    TEST_ASSERT_NULL(ptr);
}

void test_offset_to_ptr_zero_returns_base(void)
{
    void *ptr = arena_offset_to_ptr(&arena, 0);
    TEST_ASSERT_EQUAL_PTR(test_memory, ptr);
}

void test_offset_to_ptr_nonzero(void)
{
    void *ptr = arena_offset_to_ptr(&arena, 10);
    TEST_ASSERT_EQUAL_PTR(&test_memory[10], ptr);
}

void test_ptr_to_offset_null_returns_none(void)
{
    word_offset_t off = arena_ptr_to_offset(&arena, NULL);
    TEST_ASSERT_EQUAL(OFFSET_NONE, off);
}

void test_ptr_to_offset_base_returns_zero(void)
{
    word_offset_t off = arena_ptr_to_offset(&arena, test_memory);
    TEST_ASSERT_EQUAL(0, off);
}

void test_ptr_to_offset_roundtrip(void)
{
    word_offset_t original = 42;
    void *ptr = arena_offset_to_ptr(&arena, original);
    word_offset_t recovered = arena_ptr_to_offset(&arena, ptr);
    TEST_ASSERT_EQUAL(original, recovered);
}

//============================================================================
// Free Tests
//============================================================================

void test_free_to_zero_empties_arena(void)
{
    arena_alloc_words(&arena, 100);
    arena_alloc_words(&arena, 50);
    arena_free_to(&arena, 0);
    TEST_ASSERT_TRUE(arena_is_empty(&arena));
}

void test_free_to_mark_partial(void)
{
    arena_alloc_words(&arena, 10);
    word_offset_t mark = arena_top(&arena);
    arena_alloc_words(&arena, 20);
    arena_alloc_words(&arena, 30);

    arena_free_to(&arena, mark);
    TEST_ASSERT_EQUAL(10, arena_used(&arena));
}

void test_free_to_current_top_no_change(void)
{
    arena_alloc_words(&arena, 25);
    word_offset_t top = arena_top(&arena);
    arena_free_to(&arena, top);
    TEST_ASSERT_EQUAL(25, arena_used(&arena));
}

void test_free_to_invalid_mark_ignored(void)
{
    arena_alloc_words(&arena, 50);
    // Try to free to a mark beyond current top (invalid)
    arena_free_to(&arena, 100);
    // Should be ignored, arena unchanged
    TEST_ASSERT_EQUAL(50, arena_used(&arena));
}

void test_free_allows_reallocation(void)
{
    word_offset_t off1 = arena_alloc_words(&arena, 10);
    word_offset_t mark = arena_top(&arena);
    arena_alloc_words(&arena, 20);

    arena_free_to(&arena, mark);

    word_offset_t off2 = arena_alloc_words(&arena, 15);
    // New allocation should start at the same place as the freed one
    TEST_ASSERT_EQUAL(mark, off2);
}

//============================================================================
// Top/Mark Tests
//============================================================================

void test_top_starts_at_zero(void)
{
    TEST_ASSERT_EQUAL(0, arena_top(&arena));
}

void test_top_advances_with_alloc(void)
{
    arena_alloc_words(&arena, 7);
    TEST_ASSERT_EQUAL(7, arena_top(&arena));
    arena_alloc_words(&arena, 3);
    TEST_ASSERT_EQUAL(10, arena_top(&arena));
}

void test_top_decreases_with_free(void)
{
    arena_alloc_words(&arena, 20);
    word_offset_t mark = arena_top(&arena);
    arena_alloc_words(&arena, 30);
    arena_free_to(&arena, mark);
    TEST_ASSERT_EQUAL(mark, arena_top(&arena));
}

//============================================================================
// Extend Tests
//============================================================================

void test_extend_zero_succeeds(void)
{
    arena_alloc_words(&arena, 10);
    TEST_ASSERT_TRUE(arena_extend(&arena, 0));
    TEST_ASSERT_EQUAL(10, arena_used(&arena));
}

void test_extend_increases_used(void)
{
    arena_alloc_words(&arena, 10);
    TEST_ASSERT_TRUE(arena_extend(&arena, 5));
    TEST_ASSERT_EQUAL(15, arena_used(&arena));
}

void test_extend_decreases_available(void)
{
    arena_alloc_words(&arena, 10);
    word_offset_t before = arena_available(&arena);
    arena_extend(&arena, 5);
    TEST_ASSERT_EQUAL(before - 5, arena_available(&arena));
}

void test_extend_too_large_fails(void)
{
    arena_alloc_words(&arena, 10);
    word_offset_t available = arena_available(&arena);
    TEST_ASSERT_FALSE(arena_extend(&arena, available + 1));
    // Arena should be unchanged
    TEST_ASSERT_EQUAL(10, arena_used(&arena));
}

void test_extend_exact_available_succeeds(void)
{
    arena_alloc_words(&arena, 10);
    word_offset_t available = arena_available(&arena);
    TEST_ASSERT_TRUE(arena_extend(&arena, available));
    TEST_ASSERT_EQUAL(0, arena_available(&arena));
}

//============================================================================
// Is Top Allocation Tests
//============================================================================

void test_is_top_allocation_true(void)
{
    word_offset_t off = arena_alloc_words(&arena, 10);
    TEST_ASSERT_TRUE(arena_is_top_allocation(&arena, off, 10));
}

void test_is_top_allocation_false_after_another_alloc(void)
{
    word_offset_t off1 = arena_alloc_words(&arena, 10);
    arena_alloc_words(&arena, 5);
    TEST_ASSERT_FALSE(arena_is_top_allocation(&arena, off1, 10));
}

void test_is_top_allocation_latest_true(void)
{
    arena_alloc_words(&arena, 10);
    word_offset_t off2 = arena_alloc_words(&arena, 5);
    TEST_ASSERT_TRUE(arena_is_top_allocation(&arena, off2, 5));
}

void test_is_top_allocation_none_false(void)
{
    TEST_ASSERT_FALSE(arena_is_top_allocation(&arena, OFFSET_NONE, 10));
}

void test_is_top_allocation_wrong_size_false(void)
{
    word_offset_t off = arena_alloc_words(&arena, 10);
    // Wrong size - says 8 but actually 10
    TEST_ASSERT_FALSE(arena_is_top_allocation(&arena, off, 8));
}

//============================================================================
// Byte Query Tests
//============================================================================

void test_capacity_bytes(void)
{
    TEST_ASSERT_EQUAL(TEST_ARENA_SIZE, arena_capacity_bytes(&arena));
}

void test_used_bytes_empty(void)
{
    TEST_ASSERT_EQUAL(0, arena_used_bytes(&arena));
}

void test_used_bytes_after_alloc(void)
{
    arena_alloc_words(&arena, 10);
    TEST_ASSERT_EQUAL(40, arena_used_bytes(&arena));  // 10 words * 4 bytes
}

void test_available_bytes(void)
{
    arena_alloc_words(&arena, 10);
    TEST_ASSERT_EQUAL(TEST_ARENA_SIZE - 40, arena_available_bytes(&arena));
}

//============================================================================
// LIFO Pattern Tests (simulating frame push/pop)
//============================================================================

void test_lifo_push_pop_pattern(void)
{
    // Simulate pushing 3 frames and popping them
    word_offset_t mark1 = arena_top(&arena);
    word_offset_t frame1 = arena_alloc_words(&arena, 20);

    word_offset_t mark2 = arena_top(&arena);
    word_offset_t frame2 = arena_alloc_words(&arena, 15);

    word_offset_t mark3 = arena_top(&arena);
    word_offset_t frame3 = arena_alloc_words(&arena, 25);

    TEST_ASSERT_EQUAL(60, arena_used(&arena));

    // Pop frame3
    arena_free_to(&arena, mark3);
    TEST_ASSERT_EQUAL(35, arena_used(&arena));

    // Pop frame2
    arena_free_to(&arena, mark2);
    TEST_ASSERT_EQUAL(20, arena_used(&arena));

    // Pop frame1
    arena_free_to(&arena, mark1);
    TEST_ASSERT_TRUE(arena_is_empty(&arena));

    // Verify offsets are no longer valid (implicitly - would need to track)
    (void)frame1;
    (void)frame2;
    (void)frame3;
}

void test_lifo_extend_top_frame(void)
{
    // Push frame 1
    word_offset_t mark1 = arena_top(&arena);
    word_offset_t frame1 = arena_alloc_words(&arena, 10);

    // Push frame 2
    word_offset_t mark2 = arena_top(&arena);
    word_offset_t frame2 = arena_alloc_words(&arena, 8);

    // Extend frame 2 (adding locals)
    TEST_ASSERT_TRUE(arena_is_top_allocation(&arena, frame2, 8));
    TEST_ASSERT_TRUE(arena_extend(&arena, 3));
    // Frame 2 is now 11 words

    TEST_ASSERT_EQUAL(21, arena_used(&arena));

    // Pop frame 2
    arena_free_to(&arena, mark2);
    TEST_ASSERT_EQUAL(10, arena_used(&arena));

    // Pop frame 1
    arena_free_to(&arena, mark1);
    TEST_ASSERT_TRUE(arena_is_empty(&arena));

    (void)frame1;
}

void test_data_integrity(void)
{
    // Allocate and write data
    word_offset_t off1 = arena_alloc_words(&arena, 4);
    uint32_t *ptr1 = (uint32_t *)arena_offset_to_ptr(&arena, off1);
    ptr1[0] = 0xDEADBEEF;
    ptr1[1] = 0xCAFEBABE;
    ptr1[2] = 0x12345678;
    ptr1[3] = 0x87654321;

    word_offset_t off2 = arena_alloc_words(&arena, 2);
    uint32_t *ptr2 = (uint32_t *)arena_offset_to_ptr(&arena, off2);
    ptr2[0] = 0xAAAAAAAA;
    ptr2[1] = 0xBBBBBBBB;

    // Verify data
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, ptr1[0]);
    TEST_ASSERT_EQUAL_HEX32(0xCAFEBABE, ptr1[1]);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, ptr1[2]);
    TEST_ASSERT_EQUAL_HEX32(0x87654321, ptr1[3]);
    TEST_ASSERT_EQUAL_HEX32(0xAAAAAAAA, ptr2[0]);
    TEST_ASSERT_EQUAL_HEX32(0xBBBBBBBB, ptr2[1]);

    // Free second allocation
    arena_free_to(&arena, off2);

    // First allocation data should still be intact
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, ptr1[0]);
    TEST_ASSERT_EQUAL_HEX32(0xCAFEBABE, ptr1[1]);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, ptr1[2]);
    TEST_ASSERT_EQUAL_HEX32(0x87654321, ptr1[3]);
}

//============================================================================
// Test Runner
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_init_sets_capacity);
    RUN_TEST(test_init_empty_arena);
    RUN_TEST(test_init_full_available);
    RUN_TEST(test_init_null_memory_fails);
    RUN_TEST(test_init_null_arena_fails);
    RUN_TEST(test_init_misaligned_memory_fails);
    RUN_TEST(test_init_capacity_clamped_to_max);

    // Allocation tests
    RUN_TEST(test_alloc_single_word);
    RUN_TEST(test_alloc_updates_used);
    RUN_TEST(test_alloc_updates_available);
    RUN_TEST(test_alloc_sequential_offsets);
    RUN_TEST(test_alloc_zero_words_returns_none);
    RUN_TEST(test_alloc_too_large_returns_none);
    RUN_TEST(test_alloc_exact_capacity);
    RUN_TEST(test_alloc_exhausts_arena);

    // Offset/pointer conversion tests
    RUN_TEST(test_offset_to_ptr_none_returns_null);
    RUN_TEST(test_offset_to_ptr_zero_returns_base);
    RUN_TEST(test_offset_to_ptr_nonzero);
    RUN_TEST(test_ptr_to_offset_null_returns_none);
    RUN_TEST(test_ptr_to_offset_base_returns_zero);
    RUN_TEST(test_ptr_to_offset_roundtrip);

    // Free tests
    RUN_TEST(test_free_to_zero_empties_arena);
    RUN_TEST(test_free_to_mark_partial);
    RUN_TEST(test_free_to_current_top_no_change);
    RUN_TEST(test_free_to_invalid_mark_ignored);
    RUN_TEST(test_free_allows_reallocation);

    // Top/mark tests
    RUN_TEST(test_top_starts_at_zero);
    RUN_TEST(test_top_advances_with_alloc);
    RUN_TEST(test_top_decreases_with_free);

    // Extend tests
    RUN_TEST(test_extend_zero_succeeds);
    RUN_TEST(test_extend_increases_used);
    RUN_TEST(test_extend_decreases_available);
    RUN_TEST(test_extend_too_large_fails);
    RUN_TEST(test_extend_exact_available_succeeds);

    // Is top allocation tests
    RUN_TEST(test_is_top_allocation_true);
    RUN_TEST(test_is_top_allocation_false_after_another_alloc);
    RUN_TEST(test_is_top_allocation_latest_true);
    RUN_TEST(test_is_top_allocation_none_false);
    RUN_TEST(test_is_top_allocation_wrong_size_false);

    // Byte query tests
    RUN_TEST(test_capacity_bytes);
    RUN_TEST(test_used_bytes_empty);
    RUN_TEST(test_used_bytes_after_alloc);
    RUN_TEST(test_available_bytes);

    // LIFO pattern tests
    RUN_TEST(test_lifo_push_pop_pattern);
    RUN_TEST(test_lifo_extend_top_frame);
    RUN_TEST(test_data_integrity);

    return UNITY_END();
}
