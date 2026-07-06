//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the frame-cadence pacing module (setrefresh "sync).
//

#include "unity.h"
#include "core/frame_sync.h"

void setUp(void)
{
    frame_sync_reset();
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void test_inactive_by_default(void)
{
    TEST_ASSERT_FALSE(frame_sync_active());
    TEST_ASSERT_EQUAL_UINT32(0, frame_sync_period());
    // No pacing when inactive.
    TEST_ASSERT_EQUAL_UINT32(0, frame_sync_wait_ms(1000));
}

void test_set_activates_with_period(void)
{
    frame_sync_set(true, 33);
    TEST_ASSERT_TRUE(frame_sync_active());
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_period());
}

void test_set_inactive_clears_period(void)
{
    frame_sync_set(true, 33);
    frame_sync_set(false, 33); // period ignored when disabling
    TEST_ASSERT_FALSE(frame_sync_active());
    TEST_ASSERT_EQUAL_UINT32(0, frame_sync_period());
}

void test_reset_returns_to_inactive(void)
{
    frame_sync_set(true, 40);
    frame_sync_reset();
    TEST_ASSERT_FALSE(frame_sync_active());
    TEST_ASSERT_EQUAL_UINT32(0, frame_sync_period());
    TEST_ASSERT_EQUAL_UINT32(0, frame_sync_wait_ms(500));
}

// ---------------------------------------------------------------------------
// Cadence
// ---------------------------------------------------------------------------

void test_first_wait_seeds_full_period(void)
{
    frame_sync_set(true, 33);
    // The first frame has no prior baseline, so it paces a full period out.
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_wait_ms(1000));
}

void test_boundary_is_fixed_regardless_of_work(void)
{
    // Boundaries must land every 33 ms from the baseline even as per-frame
    // work varies -- that is the whole point of syncing to a fixed cadence.
    frame_sync_set(true, 33);

    // Frame 1: sync called at t=1000, sleep 33 -> next boundary 1033.
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_wait_ms(1000));

    // Frame 2: 5 ms of work after the boundary (t=1038). Sleep the remaining
    // 28 ms so the next boundary is exactly 1066, not 1071.
    TEST_ASSERT_EQUAL_UINT32(28, frame_sync_wait_ms(1038));

    // Frame 3: 12 ms of work (t=1078). Remaining to 1099 is 21 ms.
    TEST_ASSERT_EQUAL_UINT32(21, frame_sync_wait_ms(1078));
}

void test_partial_overrun_shortens_next_sleep(void)
{
    frame_sync_set(true, 33);
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_wait_ms(1000)); // boundary 1033

    // Work ran to 1050 -- 17 ms past the boundary but under a full period.
    // The next boundary stays 1066, so only 16 ms remains.
    TEST_ASSERT_EQUAL_UINT32(16, frame_sync_wait_ms(1050));
}

void test_full_overrun_drops_slippage_and_returns_zero(void)
{
    frame_sync_set(true, 33);
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_wait_ms(1000)); // boundary 1033

    // Work blew past a whole period (t=1070 > 1066). Do not sleep, and do not
    // try to catch up; re-baseline the cadence from now.
    TEST_ASSERT_EQUAL_UINT32(0, frame_sync_wait_ms(1070));

    // Cadence restarts cleanly from the overrun point (1070 + 33 = 1103).
    TEST_ASSERT_EQUAL_UINT32(30, frame_sync_wait_ms(1073));
}

void test_set_reseeds_baseline(void)
{
    frame_sync_set(true, 33);
    frame_sync_wait_ms(1000); // advance the baseline

    // Re-selecting sync mode must forget the old baseline, so the next frame
    // seeds a fresh full period rather than a stale remainder.
    frame_sync_set(true, 40);
    TEST_ASSERT_EQUAL_UINT32(40, frame_sync_wait_ms(5000));
}

void test_clock_wraparound(void)
{
    // A 32-bit ms clock wraps roughly every 49.7 days; the signed-difference
    // arithmetic must still produce the right sleep across the boundary.
    frame_sync_set(true, 33);
    // Seed at t just below the wrap: the first boundary is 0xFFFFFFF0 + 33,
    // which wraps to 0x00000011. The remaining computation spans the wrap.
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_wait_ms(0xFFFFFFF0u));
    // Loop keeps up: sync called at that first boundary (0x11). The next
    // boundary is 0x11 + 33 = 0x32, so a full period remains.
    TEST_ASSERT_EQUAL_UINT32(33, frame_sync_wait_ms(0x00000011u));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_inactive_by_default);
    RUN_TEST(test_set_activates_with_period);
    RUN_TEST(test_set_inactive_clears_period);
    RUN_TEST(test_reset_returns_to_inactive);

    RUN_TEST(test_first_wait_seeds_full_period);
    RUN_TEST(test_boundary_is_fixed_regardless_of_work);
    RUN_TEST(test_partial_overrun_shortens_next_sleep);
    RUN_TEST(test_full_overrun_drops_slippage_and_returns_zero);
    RUN_TEST(test_set_reseeds_baseline);
    RUN_TEST(test_clock_wraparound);

    return UNITY_END();
}
