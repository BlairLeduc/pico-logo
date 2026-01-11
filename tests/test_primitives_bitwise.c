//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Bitwise Primitive Tests - BITAND
//==========================================================================

void test_bitand_basic(void)
{
    Result r = eval_string("bitand 15 7");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);  // 1111 & 0111 = 0111
}

void test_bitand_zero(void)
{
    Result r = eval_string("bitand 255 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.value.as.number);
}

void test_bitand_variadic(void)
{
    Result r = eval_string("(bitand 255 15 7)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);  // 11111111 & 1111 & 111 = 111
}

void test_bitand_negative(void)
{
    Result r = eval_string("bitand -1 255");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(255.0f, r.value.as.number);  // -1 is all 1s, & 255 = 255
}

void test_bitand_error_not_number(void)
{
    Result r = eval_string("bitand 5 \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("bitand", r.error_proc);
}

//==========================================================================
// Bitwise Primitive Tests - BITOR
//==========================================================================

void test_bitor_basic(void)
{
    Result r = eval_string("bitor 8 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, r.value.as.number);  // 1000 | 0100 = 1100
}

void test_bitor_zero(void)
{
    Result r = eval_string("bitor 42 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_bitor_variadic(void)
{
    Result r = eval_string("(bitor 1 2 4 8)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r.value.as.number);  // 0001 | 0010 | 0100 | 1000 = 1111
}

void test_bitor_same_bits(void)
{
    Result r = eval_string("bitor 255 255");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(255.0f, r.value.as.number);
}

void test_bitor_error_not_number(void)
{
    Result r = eval_string("bitor \"abc 5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("bitor", r.error_proc);
}

//==========================================================================
// Bitwise Primitive Tests - BITXOR
//==========================================================================

void test_bitxor_basic(void)
{
    Result r = eval_string("bitxor 15 6");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(9.0f, r.value.as.number);  // 1111 ^ 0110 = 1001
}

void test_bitxor_zero(void)
{
    Result r = eval_string("bitxor 42 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_bitxor_same(void)
{
    Result r = eval_string("bitxor 100 100");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.value.as.number);  // Same value XOR = 0
}

void test_bitxor_variadic(void)
{
    Result r = eval_string("(bitxor 255 128 64)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(63.0f, r.value.as.number);  // 11111111 ^ 10000000 ^ 01000000 = 00111111
}

void test_bitxor_toggle(void)
{
    // XOR can toggle bits: a ^ mask ^ mask = a
    Result r = eval_string("(bitxor 42 255 255)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_bitxor_error_not_number(void)
{
    Result r = eval_string("bitxor 5 []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("bitxor", r.error_proc);
}

//==========================================================================
// Bitwise Primitive Tests - BITNOT
//==========================================================================

void test_bitnot_zero(void)
{
    Result r = eval_string("bitnot 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.value.as.number);  // ~0 = -1 (all 1s)
}

void test_bitnot_minus_one(void)
{
    Result r = eval_string("bitnot -1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.value.as.number);  // ~(-1) = 0
}

void test_bitnot_positive(void)
{
    Result r = eval_string("bitnot 255");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-256.0f, r.value.as.number);
}

void test_bitnot_double(void)
{
    // bitnot bitnot x = x
    Result r = eval_string("bitnot bitnot 42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_bitnot_error_not_number(void)
{
    Result r = eval_string("bitnot \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("bitnot", r.error_proc);
}

//==========================================================================
// Bitwise Primitive Tests - ASHIFT (Arithmetic Shift)
//==========================================================================

void test_ashift_left(void)
{
    Result r = eval_string("ashift 1 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(16.0f, r.value.as.number);  // 1 << 4 = 16
}

void test_ashift_left_multiple(void)
{
    Result r = eval_string("ashift 5 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, r.value.as.number);  // 5 << 3 = 40
}

void test_ashift_right_positive(void)
{
    Result r = eval_string("ashift 16 -2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);  // 16 >> 2 = 4
}

void test_ashift_right_negative_sign_extend(void)
{
    // Arithmetic shift right preserves sign bit
    Result r = eval_string("ashift -16 -2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-4.0f, r.value.as.number);  // -16 >> 2 = -4 (sign extended)
}

void test_ashift_zero_shift(void)
{
    Result r = eval_string("ashift 42 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_ashift_error_not_number(void)
{
    Result r = eval_string("ashift \"hello 2");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("ashift", r.error_proc);
}

//==========================================================================
// Bitwise Primitive Tests - LSHIFT (Logical Shift)
//==========================================================================

void test_lshift_left(void)
{
    Result r = eval_string("lshift 1 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(16.0f, r.value.as.number);  // 1 << 4 = 16
}

void test_lshift_left_multiple(void)
{
    Result r = eval_string("lshift 5 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, r.value.as.number);  // 5 << 3 = 40
}

void test_lshift_right_positive(void)
{
    Result r = eval_string("lshift 16 -2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);  // 16 >> 2 = 4
}

void test_lshift_right_negative_zero_fill(void)
{
    // Logical shift right fills with zeros (treating as unsigned, result reinterpreted as signed)
    // -16 in 32-bit is 0xFFFFFFF0
    // >> 2 logical gives 0x3FFFFFFC which is 1073741820
    Result r = eval_string("lshift -16 -2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(1073741820.0f, r.value.as.number);
}

void test_lshift_zero_shift(void)
{
    Result r = eval_string("lshift 42 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_lshift_error_not_number(void)
{
    Result r = eval_string("lshift [] 2");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("lshift", r.error_proc);
}

//==========================================================================
// Combined Tests
//==========================================================================

void test_bitwise_combined_mask(void)
{
    // Extract bits 4-7: (value & 0xF0) >> 4
    // 0xAB = 171, bits 4-7 = 0xA = 10
    Result r = eval_string("ashift bitand 171 240 -4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.value.as.number);
}

void test_bitwise_combined_set_bit(void)
{
    // Set bit 3: value | 8
    Result r = eval_string("bitor 0 8");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(8.0f, r.value.as.number);
}

void test_bitwise_combined_clear_bit(void)
{
    // Clear bit 3: value & ~8
    Result r = eval_string("bitand 15 bitnot 8");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);  // 1111 & ~1000 = 0111
}

void test_bitwise_combined_toggle_bit(void)
{
    // Toggle bit 2: value ^ 4
    Result r = eval_string("bitxor 7 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);  // 0111 ^ 0100 = 0011
}

void test_bitwise_with_arithmetic(void)
{
    // Combine bitwise with arithmetic: (bitand 255 15) + 1
    Result r = eval_string("sum bitand 255 15 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(16.0f, r.value.as.number);
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();

    // BITAND tests
    RUN_TEST(test_bitand_basic);
    RUN_TEST(test_bitand_zero);
    RUN_TEST(test_bitand_variadic);
    RUN_TEST(test_bitand_negative);
    RUN_TEST(test_bitand_error_not_number);

    // BITOR tests
    RUN_TEST(test_bitor_basic);
    RUN_TEST(test_bitor_zero);
    RUN_TEST(test_bitor_variadic);
    RUN_TEST(test_bitor_same_bits);
    RUN_TEST(test_bitor_error_not_number);

    // BITXOR tests
    RUN_TEST(test_bitxor_basic);
    RUN_TEST(test_bitxor_zero);
    RUN_TEST(test_bitxor_same);
    RUN_TEST(test_bitxor_variadic);
    RUN_TEST(test_bitxor_toggle);
    RUN_TEST(test_bitxor_error_not_number);

    // BITNOT tests
    RUN_TEST(test_bitnot_zero);
    RUN_TEST(test_bitnot_minus_one);
    RUN_TEST(test_bitnot_positive);
    RUN_TEST(test_bitnot_double);
    RUN_TEST(test_bitnot_error_not_number);

    // ASHIFT tests
    RUN_TEST(test_ashift_left);
    RUN_TEST(test_ashift_left_multiple);
    RUN_TEST(test_ashift_right_positive);
    RUN_TEST(test_ashift_right_negative_sign_extend);
    RUN_TEST(test_ashift_zero_shift);
    RUN_TEST(test_ashift_error_not_number);

    // LSHIFT tests
    RUN_TEST(test_lshift_left);
    RUN_TEST(test_lshift_left_multiple);
    RUN_TEST(test_lshift_right_positive);
    RUN_TEST(test_lshift_right_negative_zero_fill);
    RUN_TEST(test_lshift_zero_shift);
    RUN_TEST(test_lshift_error_not_number);

    // Combined tests
    RUN_TEST(test_bitwise_combined_mask);
    RUN_TEST(test_bitwise_combined_set_bit);
    RUN_TEST(test_bitwise_combined_clear_bit);
    RUN_TEST(test_bitwise_combined_toggle_bit);
    RUN_TEST(test_bitwise_with_arithmetic);

    return UNITY_END();
}
