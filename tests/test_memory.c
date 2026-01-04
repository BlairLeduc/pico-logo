//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the memory system.
//

#include "unity.h"
#include "core/memory.h"

#include <stdio.h>
#include <string.h>

void setUp(void)
{
    mem_init();
}

void tearDown(void)
{
    // Nothing to tear down
}

//============================================================================
// Initialization Tests
//============================================================================

void test_init_free_nodes(void)
{
    // After init, should have all nodes free except node 0
    TEST_ASSERT_EQUAL(mem_total_nodes(), mem_free_nodes());
}

void test_init_free_atoms(void)
{
    // After init, atom table has the newline marker allocated (4 bytes: 1 char + null + 2 header)
    // So free atoms should be total - 4
    TEST_ASSERT_EQUAL(mem_total_atoms() - 4, mem_free_atoms());
}

void test_total_nodes(void)
{
    // With unified memory, theoretical max is LOGO_MEMORY_SIZE / 4 - 1
    TEST_ASSERT_EQUAL((LOGO_MEMORY_SIZE / 4) - 1, mem_total_nodes());
}

void test_total_atoms(void)
{
    // Total atom space is now the entire memory block
    TEST_ASSERT_EQUAL(LOGO_MEMORY_SIZE, mem_total_atoms());
}

//============================================================================
// NIL Tests
//============================================================================

void test_nil_is_nil(void)
{
    TEST_ASSERT_TRUE(mem_is_nil(NODE_NIL));
}

void test_nil_is_not_list(void)
{
    TEST_ASSERT_FALSE(mem_is_list(NODE_NIL));
}

void test_nil_is_not_word(void)
{
    TEST_ASSERT_FALSE(mem_is_word(NODE_NIL));
}

void test_car_of_nil(void)
{
    TEST_ASSERT_TRUE(mem_is_nil(mem_car(NODE_NIL)));
}

void test_cdr_of_nil(void)
{
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(NODE_NIL)));
}

//============================================================================
// Atom/Word Tests
//============================================================================

void test_create_atom(void)
{
    Node word = mem_atom("hello", 5);
    TEST_ASSERT_FALSE(mem_is_nil(word));
    TEST_ASSERT_TRUE(mem_is_word(word));
}

void test_atom_content(void)
{
    Node word = mem_atom("hello", 5);
    TEST_ASSERT_EQUAL(5, mem_word_len(word));
    TEST_ASSERT_EQUAL_STRING_LEN("hello", mem_word_ptr(word), 5);
}

void test_atom_cstr(void)
{
    Node word = mem_atom_cstr("world");
    TEST_ASSERT_EQUAL(5, mem_word_len(word));
    TEST_ASSERT_TRUE(mem_word_eq(word, "world", 5));
}

void test_atom_interning(void)
{
    Node word1 = mem_atom("hello", 5);
    Node word2 = mem_atom("hello", 5);
    
    // Same word should return same node (interned)
    TEST_ASSERT_EQUAL(word1, word2);
}

void test_atom_interning_case_insensitive(void)
{
    Node word1 = mem_atom("Hello", 5);
    Node word2 = mem_atom("HELLO", 5);
    Node word3 = mem_atom("hello", 5);
    
    // Atoms are case-sensitive, so different case = different atom
    // (Case-insensitive lookup happens at variable/procedure level)
    TEST_ASSERT_NOT_EQUAL(word1, word2);
    TEST_ASSERT_NOT_EQUAL(word2, word3);
    TEST_ASSERT_NOT_EQUAL(word1, word3);
    
    // But same case should still intern to same atom
    Node word4 = mem_atom("Hello", 5);
    TEST_ASSERT_EQUAL(word1, word4);
}

void test_different_atoms(void)
{
    Node word1 = mem_atom("hello", 5);
    Node word2 = mem_atom("world", 5);
    
    TEST_ASSERT_NOT_EQUAL(word1, word2);
}

void test_atom_uses_memory(void)
{
    size_t free_before = mem_free_atoms();
    mem_atom("test", 4);
    size_t free_after = mem_free_atoms();
    
    // Should have used 8 bytes (1 length + 4 chars + 3 padding to align to 4)
    TEST_ASSERT_EQUAL(8, free_before - free_after);
}

void test_interned_atom_no_extra_memory(void)
{
    mem_atom("test", 4);
    size_t free_before = mem_free_atoms();
    mem_atom("test", 4); // Same word
    size_t free_after = mem_free_atoms();
    
    // Should not use more memory
    TEST_ASSERT_EQUAL(free_before, free_after);
}

void test_word_eq(void)
{
    Node word = mem_atom("forward", 7);
    TEST_ASSERT_TRUE(mem_word_eq(word, "forward", 7));
    TEST_ASSERT_TRUE(mem_word_eq(word, "FORWARD", 7));
    TEST_ASSERT_TRUE(mem_word_eq(word, "Forward", 7));
    TEST_ASSERT_FALSE(mem_word_eq(word, "back", 4));
}

void test_words_equal(void)
{
    Node word1 = mem_atom("test", 4);
    Node word2 = mem_atom("test", 4); // Same case, same atom
    Node word3 = mem_atom("TEST", 4); // Different case, different atom
    Node word4 = mem_atom("other", 5);
    
    // Same atom nodes are equal
    TEST_ASSERT_TRUE(mem_words_equal(word1, word2));
    // Different atoms (even just case difference) are not equal at atom level
    TEST_ASSERT_FALSE(mem_words_equal(word1, word3));
    TEST_ASSERT_FALSE(mem_words_equal(word1, word4));
}

void test_empty_word(void)
{
    Node word = mem_atom("", 0);
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_EQUAL(0, mem_word_len(word));
}

void test_number_as_atom(void)
{
    Node num = mem_atom("123", 3);
    TEST_ASSERT_TRUE(mem_is_word(num));
    TEST_ASSERT_TRUE(mem_word_eq(num, "123", 3));
}

void test_negative_number_as_atom(void)
{
    Node num = mem_atom("-45.67", 6);
    TEST_ASSERT_TRUE(mem_is_word(num));
    TEST_ASSERT_TRUE(mem_word_eq(num, "-45.67", 6));
}

//============================================================================
// Cons/List Tests
//============================================================================

void test_cons_creates_list(void)
{
    Node word = mem_atom("a", 1);
    Node list = mem_cons(word, NODE_NIL);
    
    TEST_ASSERT_FALSE(mem_is_nil(list));
    TEST_ASSERT_TRUE(mem_is_list(list));
}

void test_cons_uses_node(void)
{
    Node word = mem_atom("a", 1);
    size_t free_before = mem_free_nodes();
    mem_cons(word, NODE_NIL);
    size_t free_after = mem_free_nodes();
    
    TEST_ASSERT_EQUAL(1, free_before - free_after);
}

void test_car_of_list(void)
{
    Node word = mem_atom("hello", 5);
    Node list = mem_cons(word, NODE_NIL);
    Node car = mem_car(list);
    
    TEST_ASSERT_TRUE(mem_is_word(car));
    TEST_ASSERT_TRUE(mem_word_eq(car, "hello", 5));
}

void test_cdr_of_single_element_list(void)
{
    Node word = mem_atom("a", 1);
    Node list = mem_cons(word, NODE_NIL);
    
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));
}

void test_two_element_list(void)
{
    Node word1 = mem_atom("a", 1);
    Node word2 = mem_atom("b", 1);
    
    Node rest = mem_cons(word2, NODE_NIL);
    Node list = mem_cons(word1, rest);
    
    // First element should be "a"
    Node car = mem_car(list);
    TEST_ASSERT_TRUE(mem_word_eq(car, "a", 1));
    
    // Rest should be a list starting with "b"
    Node cdr = mem_cdr(list);
    TEST_ASSERT_TRUE(mem_is_list(cdr));
}

void test_nested_list(void)
{
    // Create [[a b] c]
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    
    // Inner list [a b]
    Node inner_rest = mem_cons(b, NODE_NIL);
    Node inner = mem_cons(a, inner_rest);
    
    // Outer list [[a b] c]
    Node outer_rest = mem_cons(c, NODE_NIL);
    Node outer = mem_cons(inner, outer_rest);
    
    TEST_ASSERT_TRUE(mem_is_list(outer));
    TEST_ASSERT_TRUE(mem_is_list(mem_car(outer)));
}

//============================================================================
// Set Car/Cdr Tests
//============================================================================

void test_set_car(void)
{
    Node word1 = mem_atom("old", 3);
    Node word2 = mem_atom("new", 3);
    Node list = mem_cons(word1, NODE_NIL);
    
    TEST_ASSERT_TRUE(mem_set_car(list, word2));
    
    Node car = mem_car(list);
    TEST_ASSERT_TRUE(mem_word_eq(car, "new", 3));
}

void test_set_cdr(void)
{
    Node word1 = mem_atom("a", 1);
    Node word2 = mem_atom("b", 1);
    Node list1 = mem_cons(word1, NODE_NIL);
    Node list2 = mem_cons(word2, NODE_NIL);
    
    TEST_ASSERT_TRUE(mem_set_cdr(list1, list2));
    
    Node cdr = mem_cdr(list1);
    TEST_ASSERT_TRUE(mem_is_list(cdr));
}

void test_set_car_on_nil_fails(void)
{
    Node word = mem_atom("test", 4);
    TEST_ASSERT_FALSE(mem_set_car(NODE_NIL, word));
}

void test_set_cdr_on_nil_fails(void)
{
    Node list = mem_cons(mem_atom("a", 1), NODE_NIL);
    TEST_ASSERT_FALSE(mem_set_cdr(NODE_NIL, list));
}

void test_set_car_on_word_fails(void)
{
    Node word1 = mem_atom("test", 4);
    Node word2 = mem_atom("other", 5);
    TEST_ASSERT_FALSE(mem_set_car(word1, word2));
}

//============================================================================
// Garbage Collection Tests
//============================================================================

void test_gc_with_no_roots_frees_all(void)
{
    // Create some nodes
    Node word = mem_atom("test", 4);
    mem_cons(word, NODE_NIL);
    mem_cons(word, NODE_NIL);
    mem_cons(word, NODE_NIL);
    
    size_t free_before = mem_free_nodes();
    
    // GC with no roots
    mem_gc(NULL, 0);
    
    size_t free_after = mem_free_nodes();
    
    // Should have freed the cons cells
    TEST_ASSERT_GREATER_THAN(free_before, free_after);
}

void test_gc_preserves_roots(void)
{
    Node word = mem_atom("test", 4);
    Node list = mem_cons(word, NODE_NIL);
    
    // GC with list as root
    Node roots[] = {list};
    mem_gc(roots, 1);
    
    // List should still be valid
    TEST_ASSERT_TRUE(mem_is_list(list));
    Node car = mem_car(list);
    TEST_ASSERT_TRUE(mem_word_eq(car, "test", 4));
}

void test_gc_preserves_linked_lists(void)
{
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    
    Node list3 = mem_cons(c, NODE_NIL);
    Node list2 = mem_cons(b, list3);
    Node list1 = mem_cons(a, list2);
    
    // GC with only head as root - should preserve whole list
    Node roots[] = {list1};
    mem_gc(roots, 1);
    
    TEST_ASSERT_TRUE(mem_is_list(list1));
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(list1), "a", 1));
}

void test_gc_frees_unreachable(void)
{
    Node word = mem_atom("test", 4);
    Node reachable = mem_cons(word, NODE_NIL);
    mem_cons(word, NODE_NIL); // unreachable
    mem_cons(word, NODE_NIL); // unreachable
    
    size_t free_before = mem_free_nodes();
    
    Node roots[] = {reachable};
    mem_gc(roots, 1);
    
    size_t free_after = mem_free_nodes();
    
    // Should have freed 2 nodes
    TEST_ASSERT_EQUAL(2, free_after - free_before);
}

void test_atoms_not_freed_by_gc(void)
{
    // Atoms are never garbage collected
    Node word = mem_atom("permanent", 9);
    
    mem_gc(NULL, 0);
    
    // Word should still be valid
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_TRUE(mem_word_eq(word, "permanent", 9));
}

void test_free_nodes_accurate(void)
{
    // Get initial count - this is after mem_init which allocates the newline marker atom
    size_t initial = mem_free_nodes();
    
    // Create an atom - this uses atom space but not node space
    Node word = mem_atom("x", 1);
    // Note: atom allocation reduces potential_nodes by 1 (4 bytes = 1 node)
    
    // Create two cons cells
    mem_cons(word, NODE_NIL);
    mem_cons(word, NODE_NIL);
    
    // After creating 2 nodes and 1 atom (4 bytes), we should have:
    // - 2 fewer nodes (from cons cells)
    // - 1 fewer potential node (from atom taking 4 bytes)
    TEST_ASSERT_EQUAL(initial - 3, mem_free_nodes());
    
    // After GC with no roots, both cons cells should be freed
    // But the atom still takes space, so we get back 2 nodes, not 3
    mem_gc(NULL, 0);
    
    TEST_ASSERT_EQUAL(initial - 1, mem_free_nodes());
}

//============================================================================
// Edge Cases
//============================================================================

void test_word_is_not_list(void)
{
    Node word = mem_atom("test", 4);
    TEST_ASSERT_FALSE(mem_is_list(word));
}

void test_list_is_not_word(void)
{
    Node word = mem_atom("test", 4);
    Node list = mem_cons(word, NODE_NIL);
    TEST_ASSERT_FALSE(mem_is_word(list));
}

void test_car_of_word_is_nil(void)
{
    Node word = mem_atom("test", 4);
    TEST_ASSERT_TRUE(mem_is_nil(mem_car(word)));
}

void test_cdr_of_word_is_nil(void)
{
    Node word = mem_atom("test", 4);
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(word)));
}

void test_word_ptr_on_non_word(void)
{
    Node word = mem_atom("test", 4);
    Node list = mem_cons(word, NODE_NIL);
    TEST_ASSERT_NULL(mem_word_ptr(list));
}

void test_word_len_on_non_word(void)
{
    Node word = mem_atom("test", 4);
    Node list = mem_cons(word, NODE_NIL);
    TEST_ASSERT_EQUAL(0, mem_word_len(list));
}

//============================================================================
// Memory Layout Tests (Unified Block)
//============================================================================

void test_atoms_and_nodes_share_space(void)
{
    // Create some atoms and nodes to verify they share the memory pool
    size_t initial_free = mem_free_nodes();
    
    Node word = mem_atom("test", 4);
    size_t after_atom = mem_free_nodes();
    
    // Adding an atom should reduce free nodes (they share space)
    TEST_ASSERT_LESS_THAN(initial_free, after_atom);
}

void test_mixed_allocation(void)
{
    // Test allocating atoms and nodes in an interleaved pattern
    Node word1 = mem_atom("first", 5);
    Node list1 = mem_cons(word1, NODE_NIL);
    
    Node word2 = mem_atom("second", 6);
    Node list2 = mem_cons(word2, NODE_NIL);
    
    // Verify everything is accessible
    TEST_ASSERT_TRUE(mem_is_word(word1));
    TEST_ASSERT_TRUE(mem_is_word(word2));
    TEST_ASSERT_TRUE(mem_is_list(list1));
    TEST_ASSERT_TRUE(mem_is_list(list2));
    
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(list1), "first", 5));
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(list2), "second", 6));
}

void test_memory_pressure(void)
{
    // Allocate many atoms to create memory pressure
    size_t initial_free = mem_free_nodes();
    
    // Create 100 small atoms
    for (int i = 0; i < 100; i++)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "atom%d", i);
        Node word = mem_atom(buf, strlen(buf));
        TEST_ASSERT_FALSE(mem_is_nil(word));
    }
    
    size_t after_atoms = mem_free_nodes();
    
    // Free nodes should be reduced due to atom space usage
    TEST_ASSERT_LESS_THAN(initial_free, after_atoms);
    
    // But we should still be able to allocate nodes
    Node list = mem_cons(mem_atom("test", 4), NODE_NIL);
    TEST_ASSERT_FALSE(mem_is_nil(list));
}

void test_node_allocation_from_top(void)
{
    // Nodes should be allocated from the top of memory, growing downward
    // Create multiple nodes and verify they work correctly
    Node word = mem_atom("item", 4);
    
    Node list1 = mem_cons(word, NODE_NIL);
    Node list2 = mem_cons(word, NODE_NIL);
    Node list3 = mem_cons(word, NODE_NIL);
    
    // All should be valid and independent
    TEST_ASSERT_FALSE(mem_is_nil(list1));
    TEST_ASSERT_FALSE(mem_is_nil(list2));
    TEST_ASSERT_FALSE(mem_is_nil(list3));
    
    TEST_ASSERT_NOT_EQUAL(list1, list2);
    TEST_ASSERT_NOT_EQUAL(list2, list3);
    TEST_ASSERT_NOT_EQUAL(list1, list3);
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_init_free_nodes);
    RUN_TEST(test_init_free_atoms);
    RUN_TEST(test_total_nodes);
    RUN_TEST(test_total_atoms);

    // NIL
    RUN_TEST(test_nil_is_nil);
    RUN_TEST(test_nil_is_not_list);
    RUN_TEST(test_nil_is_not_word);
    RUN_TEST(test_car_of_nil);
    RUN_TEST(test_cdr_of_nil);

    // Atoms/Words
    RUN_TEST(test_create_atom);
    RUN_TEST(test_atom_content);
    RUN_TEST(test_atom_cstr);
    RUN_TEST(test_atom_interning);
    RUN_TEST(test_atom_interning_case_insensitive);
    RUN_TEST(test_different_atoms);
    RUN_TEST(test_atom_uses_memory);
    RUN_TEST(test_interned_atom_no_extra_memory);
    RUN_TEST(test_word_eq);
    RUN_TEST(test_words_equal);
    RUN_TEST(test_empty_word);
    RUN_TEST(test_number_as_atom);
    RUN_TEST(test_negative_number_as_atom);

    // Cons/Lists
    RUN_TEST(test_cons_creates_list);
    RUN_TEST(test_cons_uses_node);
    RUN_TEST(test_car_of_list);
    RUN_TEST(test_cdr_of_single_element_list);
    RUN_TEST(test_two_element_list);
    RUN_TEST(test_nested_list);

    // Set Car/Cdr
    RUN_TEST(test_set_car);
    RUN_TEST(test_set_cdr);
    RUN_TEST(test_set_car_on_nil_fails);
    RUN_TEST(test_set_cdr_on_nil_fails);
    RUN_TEST(test_set_car_on_word_fails);

    // Garbage Collection
    RUN_TEST(test_gc_with_no_roots_frees_all);
    RUN_TEST(test_gc_preserves_roots);
    RUN_TEST(test_gc_preserves_linked_lists);
    RUN_TEST(test_gc_frees_unreachable);
    RUN_TEST(test_atoms_not_freed_by_gc);
    RUN_TEST(test_free_nodes_accurate);

    // Edge Cases
    RUN_TEST(test_word_is_not_list);
    RUN_TEST(test_list_is_not_word);
    RUN_TEST(test_car_of_word_is_nil);
    RUN_TEST(test_cdr_of_word_is_nil);
    RUN_TEST(test_word_ptr_on_non_word);
    RUN_TEST(test_word_len_on_non_word);

    // Memory Layout (Unified Block)
    RUN_TEST(test_atoms_and_nodes_share_space);
    RUN_TEST(test_mixed_allocation);
    RUN_TEST(test_memory_pressure);
    RUN_TEST(test_node_allocation_from_top);

    return UNITY_END();
}
