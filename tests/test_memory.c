//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the memory system.
//

#include "unity.h"
#include "core/memory.h"

#include <stdio.h>
#include <string.h>

// A static, 8-byte-aligned region used to back the blob heap in blob tests.
// 1 MB is enough to exercise large blobs and the descriptor table.
_Alignas(8) static uint8_t test_blob_region[1u << 20];

// Enable the blob heap over the static test region. Call inside a test (after
// the setUp() logo_mem_init, which clears any region).
static void enable_blob_region(void)
{
    logo_mem_set_aux_region(test_blob_region, sizeof(test_blob_region));
}

void setUp(void)
{
    logo_mem_init();
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
    // After init the only consumed space is the bootstrap atoms (newline
    // marker, "true", "false"); every remaining word of the shared block
    // is a potential node.
    TEST_ASSERT_EQUAL((LOGO_MEMORY_SIZE - 28) / 4, mem_free_nodes());
}

void test_init_free_atoms(void)
{
    // After init the atom table holds the bootstrap atoms — the newline
    // marker (8 bytes), "true" (8), and "false" (12) — each laid out as
    // [next:2][len:1][chars][nul][pad-to-4].
    TEST_ASSERT_EQUAL(mem_total_atoms() - 28, mem_free_atoms());
}

void test_total_nodes(void)
{
    // With unified memory, theoretical max is LOGO_MEMORY_SIZE / 4 - 1
    TEST_ASSERT_EQUAL((LOGO_MEMORY_SIZE / 4) - 1, mem_total_nodes());
}

void test_total_atoms(void)
{
    TEST_ASSERT_EQUAL(LOGO_MEMORY_SIZE / 4, mem_total_atoms());
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

void test_atom_max_length(void)
{
    // 255 bytes is the largest atom the 1-byte length prefix can hold; it must
    // be stored in full, not truncated.
    char buf[255];
    memset(buf, 'a', sizeof(buf));
    Node word = mem_atom(buf, sizeof(buf));
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_EQUAL(255, mem_word_len(word));
}

void test_atom_too_long_is_error(void)
{
    // A word longer than 255 bytes cannot be encoded. It must fail (NIL),
    // never be silently truncated to 255.
    char buf[256];
    memset(buf, 'a', sizeof(buf));
    Node word = mem_atom(buf, sizeof(buf));
    TEST_ASSERT_TRUE(mem_is_nil(word));
}

void test_atom_unescape_too_long_is_error(void)
{
    // The unescape path must also error rather than truncate. Build a string
    // whose unescaped length exceeds 255.
    char buf[300];
    memset(buf, 'a', sizeof(buf));
    Node word = mem_atom_unescape(buf, sizeof(buf));
    TEST_ASSERT_TRUE(mem_is_nil(word));
}

void test_atom_unescape_strips_backslash(void)
{
    // \X becomes just X.
    Node word = mem_atom_unescape("a\\+b", 4);
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_EQUAL(3, mem_word_len(word));
    TEST_ASSERT_EQUAL_STRING_LEN("a+b", mem_word_ptr(word), 3);
}

void test_atom_unescape_strips_bars(void)
{
    // The bars of a |...| run are removed; the contents are kept verbatim.
    Node word = mem_atom_unescape("|a b|", 5);
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_EQUAL(3, mem_word_len(word));
    TEST_ASSERT_EQUAL_STRING_LEN("a b", mem_word_ptr(word), 3);
}

void test_atom_unescape_bar_with_delimiters(void)
{
    // Delimiters inside bars survive as ordinary characters.
    Node word = mem_atom_unescape("|[a]+(b)|", 9);
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_EQUAL(7, mem_word_len(word));
    TEST_ASSERT_EQUAL_STRING_LEN("[a]+(b)", mem_word_ptr(word), 7);
}

void test_atom_unescape_escaped_bar_inside_bars(void)
{
    // Inside bars, \| is a literal bar (the only chars needing a backslash
    // inside bars are | and \ themselves).
    Node word = mem_atom_unescape("|a\\|b|", 6);
    TEST_ASSERT_TRUE(mem_is_word(word));
    TEST_ASSERT_EQUAL(3, mem_word_len(word));
    TEST_ASSERT_EQUAL_STRING_LEN("a|b", mem_word_ptr(word), 3);
}

void test_atom_unescape_bar_and_backslash_agree(void)
{
    // "\( and "|(| must intern to the same single-character word "(".
    Node a = mem_atom_unescape("\\(", 2);
    Node b = mem_atom_unescape("|(|", 3);
    TEST_ASSERT_EQUAL(1, mem_word_len(a));
    TEST_ASSERT_EQUAL(1, mem_word_len(b));
    TEST_ASSERT_EQUAL_STRING_LEN("(", mem_word_ptr(a), 1);
    TEST_ASSERT_EQUAL_STRING_LEN("(", mem_word_ptr(b), 1);
}

//============================================================================
// Blob Tests (PSRAM-backed large values)
//============================================================================

void test_blob_no_region_returns_nil(void)
{
    // Without an aux region, large values are unavailable.
    char buf[300];
    memset(buf, 'x', sizeof(buf));
    Node b = mem_blob(buf, sizeof(buf));
    TEST_ASSERT_TRUE(mem_is_nil(b));
}

void test_blob_basic_exceeds_atom_limit(void)
{
    enable_blob_region();
    char buf[1000];
    memset(buf, 'z', sizeof(buf));
    Node b = mem_blob(buf, sizeof(buf));

    TEST_ASSERT_FALSE(mem_is_nil(b));
    TEST_ASSERT_TRUE(mem_is_word(b));   // a blob is still a word to the outside
    TEST_ASSERT_TRUE(mem_is_blob(b));
    TEST_ASSERT_EQUAL(1000, mem_word_len(b));
    TEST_ASSERT_EQUAL_STRING_LEN(buf, mem_word_ptr(b), 1000);
    // mem_word_ptr is NUL-terminated for C-string safety.
    TEST_ASSERT_EQUAL('\0', mem_word_ptr(b)[1000]);
    TEST_ASSERT_EQUAL(1, mem_blob_used());
}

void test_blob_not_storable_in_cell(void)
{
    enable_blob_region();
    char buf[300];
    memset(buf, 'q', sizeof(buf));
    Node b = mem_blob(buf, sizeof(buf));
    TEST_ASSERT_TRUE(mem_is_blob(b));

    // A blob cannot be packed into a cons cell; mem_cons rejects it.
    TEST_ASSERT_TRUE(mem_is_nil(mem_cons(b, NODE_NIL)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cons(NODE_NIL, b)));
}

void test_blob_survives_gc_when_rooted(void)
{
    enable_blob_region();
    char buf[400];
    memset(buf, 'r', sizeof(buf));
    Node b = mem_blob(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(1, mem_blob_used());

    Node roots[1] = {b};
    mem_gc(roots, 1);

    // Still alive, same handle, still readable.
    TEST_ASSERT_EQUAL(1, mem_blob_used());
    TEST_ASSERT_EQUAL(400, mem_word_len(b));
    TEST_ASSERT_EQUAL_STRING_LEN(buf, mem_word_ptr(b), 400);
}

void test_blob_collected_when_unreferenced(void)
{
    enable_blob_region();
    size_t free_before = mem_blob_free_bytes();

    char buf[400];
    memset(buf, 's', sizeof(buf));
    Node b = mem_blob(buf, sizeof(buf));
    (void)b;
    TEST_ASSERT_EQUAL(1, mem_blob_used());
    TEST_ASSERT_TRUE(mem_blob_free_bytes() < free_before);

    // No roots -> the blob is unreachable and must be reclaimed.
    mem_gc(NULL, 0);
    TEST_ASSERT_EQUAL(0, mem_blob_used());
    TEST_ASSERT_EQUAL(free_before, mem_blob_free_bytes());
}

void test_blob_equals_atom_by_content(void)
{
    enable_blob_region();
    Node atom = mem_atom("abc", 3);
    Node blob = mem_blob("abc", 3);
    Node other = mem_blob("abd", 3);

    TEST_ASSERT_TRUE(mem_words_equal(atom, blob));  // content match across kinds
    TEST_ASSERT_TRUE(mem_words_equal(blob, atom));
    TEST_ASSERT_FALSE(mem_words_equal(blob, other));
}

void test_blob_table_full(void)
{
    enable_blob_region();
    // Allocate LOGO_MAX_BLOBS small blobs, then one more must fail.
    int created = 0;
    for (int i = 0; i < 4096; i++)
    {
        Node b = mem_blob("a", 1);
        if (mem_is_nil(b))
        {
            break;
        }
        created++;
    }
    // The table cap (not the 1 MB region) is the limit for tiny blobs.
    TEST_ASSERT_EQUAL(LOGO_MAX_BLOBS, created);
    TEST_ASSERT_TRUE(mem_is_nil(mem_blob("a", 1)));
}

void test_blob_region_full(void)
{
    enable_blob_region();
    // A blob larger than the whole region cannot be allocated.
    static char big[(1u << 20) + 16];
    memset(big, 'b', sizeof(big));
    Node b = mem_blob(big, sizeof(big));
    TEST_ASSERT_TRUE(mem_is_nil(b));
}

void test_region_alloc_no_region_returns_null(void)
{
    // Without an aux region, pinned allocation fails so callers fall back.
    TEST_ASSERT_NULL(mem_region_alloc(64));
}

void test_region_alloc_pinned_survives_gc(void)
{
    enable_blob_region();
    size_t free_before = mem_blob_free_bytes();

    char *buf = (char *)mem_region_alloc(256);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 'k', 256); // writable
    size_t free_after_alloc = mem_blob_free_bytes();
    TEST_ASSERT_TRUE(free_after_alloc < free_before);

    // A pinned region allocation is never reclaimed by GC.
    mem_gc(NULL, 0);
    TEST_ASSERT_EQUAL(free_after_alloc, mem_blob_free_bytes());
    TEST_ASSERT_EQUAL('k', buf[0]);
    TEST_ASSERT_EQUAL('k', buf[255]);
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

void test_atom_intern_many_dedup(void)
{
    // 300 distinct atoms across 256 hash buckets guarantees multi-entry
    // chains; re-interning must still find the exact original atoms.
    char buf[16];
    Node first_pass[300];
    for (int i = 0; i < 300; i++)
    {
        int len = snprintf(buf, sizeof(buf), "w%d", i);
        first_pass[i] = mem_atom(buf, (size_t)len);
        TEST_ASSERT_TRUE(mem_is_word(first_pass[i]));
    }

    size_t free_before = mem_free_atoms();
    for (int i = 0; i < 300; i++)
    {
        int len = snprintf(buf, sizeof(buf), "w%d", i);
        TEST_ASSERT_EQUAL(first_pass[i], mem_atom(buf, (size_t)len));
    }
    TEST_ASSERT_EQUAL(free_before, mem_free_atoms());
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
    // Case variants intern as distinct atoms but compare equal, matching
    // classic Logo equal? semantics
    TEST_ASSERT_TRUE(mem_words_equal(word1, word3));
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

// Exhaust the node pool, returning the head of the garbage chain so it
// stays "allocated" for the duration of the test.
static Node exhaust_node_pool(void)
{
    Node chain = NODE_NIL;
    for (;;)
    {
        Node c = mem_cons(NODE_NIL, chain);
        if (mem_is_nil(c))
        {
            return chain;
        }
        chain = c;
    }
}

void test_list_append_builds_list(void)
{
    Node head = NODE_NIL;
    Node tail = NODE_NIL;

    TEST_ASSERT_TRUE(mem_list_append(&head, &tail, mem_atom_cstr("a")));
    TEST_ASSERT_TRUE(mem_list_append(&head, &tail, mem_atom_cstr("b")));

    TEST_ASSERT_TRUE(mem_word_eq(mem_car(head), "a", 1));
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(mem_cdr(head)), "b", 1));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(mem_cdr(head))));
}

void test_list_append_fails_when_pool_exhausted(void)
{
    Node head = NODE_NIL;
    Node tail = NODE_NIL;
    Node item = mem_atom_cstr("a");

    TEST_ASSERT_TRUE(mem_list_append(&head, &tail, item));
    Node head_before = head;
    Node tail_before = tail;

    exhaust_node_pool();

    // Append must report failure and leave the list untouched.
    TEST_ASSERT_FALSE(mem_list_append(&head, &tail, item));
    TEST_ASSERT_EQUAL(head_before, head);
    TEST_ASSERT_EQUAL(tail_before, tail);
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(head)));
}

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

void test_gc_reclaims_unreachable_atoms(void)
{
    Node word = mem_atom("temporary", 9);
    mem_gc(NULL, 0);

    TEST_ASSERT_NULL(mem_word_ptr(word));
    Node replacement = mem_atom("replaced!", 9);
    TEST_ASSERT_EQUAL(word, replacement);
    TEST_ASSERT_TRUE(mem_word_eq(replacement, "replaced!", 9));
}

void test_gc_preserves_rooted_atoms_in_cells(void)
{
    Node car = mem_atom("carword", 7);
    Node cdr = mem_atom("cdrword", 7);
    Node list = mem_cons(car, cdr);

    mem_gc(&list, 1);

    TEST_ASSERT_TRUE(mem_word_eq(mem_car(list), "carword", 7));
    TEST_ASSERT_TRUE(mem_word_eq(mem_cdr(list), "cdrword", 7));
}

void test_gc_preserves_atom_pointer_root(void)
{
    Node word = mem_atom("name", 4);
    const char *name = mem_word_ptr(word);

    mem_gc_mark_atom_ptr(name);
    mem_gc_sweep();

    TEST_ASSERT_TRUE(mem_word_eq(word, "name", 4));
}

void test_interning_ignores_atom_mark_bits(void)
{
    Node atoms[257];
    char name[16];

    // More names than hash buckets guarantees at least one chain. Mark every
    // entry, then prove a lookup can still traverse each marked link.
    for (int i = 0; i < 257; i++)
    {
        snprintf(name, sizeof(name), "atom%d", i);
        atoms[i] = mem_atom(name, strlen(name));
        TEST_ASSERT_TRUE(mem_is_word(atoms[i]));
    }
    for (int i = 0; i < 257; i++)
        mem_gc_mark_atom_ptr(mem_word_ptr(atoms[i]));
    for (int i = 0; i < 257; i++)
    {
        snprintf(name, sizeof(name), "atom%d", i);
        TEST_ASSERT_EQUAL(atoms[i], mem_atom(name, strlen(name)));
    }
}

void test_gc_recovers_exhausted_atom_space(void)
{
    char word[256];
    int created = 0;
    for (int i = 0; ; i++)
    {
        memset(word, 'x', 255);
        snprintf(word, sizeof(word), "%d", i);
        Node atom = mem_atom(word, 255);
        if (mem_is_nil(atom))
            break;
        created++;
    }
    TEST_ASSERT_TRUE(created > 100);

    mem_gc(NULL, 0);
    TEST_ASSERT_TRUE(mem_is_word(mem_atom("after-recycle", 13)));
}

void test_atom_chain_end_is_not_an_allocatable_offset(void)
{
    // Two-byte words occupy eight bytes. Leave exactly one four-byte entry
    // after filling the atom region, then allocate the empty word there.
    size_t fill_count = (mem_free_atoms() - 4) / 8;
    for (size_t i = 0; i < fill_count; i++)
    {
        char word[2] = {(char)(i & 0xff), (char)(i >> 8)};
        TEST_ASSERT_TRUE(mem_is_word(mem_atom(word, sizeof(word))));
    }

    Node empty = mem_atom("", 0);
    TEST_ASSERT_TRUE(mem_is_word(empty));
    TEST_ASSERT_EQUAL(empty, mem_atom("", 0));
}

void test_gc_reuses_interior_atom_hole_without_losing_live_atom(void)
{
    Node dead = mem_atom("discarded", 9);
    Node live = mem_atom("preserved", 9);

    mem_gc(&live, 1);

    Node replacement = mem_atom("replaced!", 9);
    TEST_ASSERT_EQUAL(dead, replacement);
    TEST_ASSERT_TRUE(mem_word_eq(live, "preserved", 9));
    TEST_ASSERT_TRUE(mem_word_eq(replacement, "replaced!", 9));
}

void test_gc_preserves_long_list(void)
{
    // Build a list of 200 elements - previously this would cause deep
    // C-stack recursion in gc_mark_index (~20 bytes per level = 4KB).
    // With iterative cdr traversal, this uses constant stack.
    Node word = mem_atom("x", 1);
    Node list = NODE_NIL;
    for (int i = 0; i < 200; i++)
    {
        list = mem_cons(word, list);
    }
    
    size_t free_before = mem_free_nodes();
    
    Node roots[] = {list};
    mem_gc(roots, 1);
    
    size_t free_after = mem_free_nodes();
    
    // All 200 nodes should be preserved (no change in free count)
    TEST_ASSERT_EQUAL(free_before, free_after);
    
    // Verify the list is still intact by walking it
    Node cur = list;
    int count = 0;
    while (cur != NODE_NIL)
    {
        TEST_ASSERT_TRUE(mem_is_list(cur));
        TEST_ASSERT_TRUE(mem_word_eq(mem_car(cur), "x", 1));
        cur = mem_cdr(cur);
        count++;
    }
    TEST_ASSERT_EQUAL(200, count);
}

void test_gc_preserves_nested_lists(void)
{
    // Test that nested lists (car branches) are still marked correctly
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    
    // Inner list: [b]
    Node inner = mem_cons(b, NODE_NIL);
    // Outer list: [[b] a]
    Node outer = mem_cons(inner, mem_cons(a, NODE_NIL));
    
    Node roots[] = {outer};
    mem_gc(roots, 1);
    
    // Outer list should still be valid
    TEST_ASSERT_TRUE(mem_is_list(outer));
    
    // Car of outer is the inner list
    Node car = mem_car(outer);
    TEST_ASSERT_TRUE(mem_is_list(car));
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(car), "b", 1));
    
    // Cadr of outer is "a"
    Node cadr = mem_car(mem_cdr(outer));
    TEST_ASSERT_TRUE(mem_word_eq(cadr, "a", 1));
}

void test_free_nodes_accurate(void)
{
    // Get initial count - this is after logo_mem_init which allocates the newline marker atom
    size_t initial = mem_free_nodes();
    
    // Create an atom - this uses atom space but not node space
    Node word = mem_atom("x", 1);
    // Note: the atom entry is ALIGN4(2+1+1+1) = 8 bytes, reducing
    // potential_nodes by 2 (4 bytes = 1 node)

    // Create two cons cells
    mem_cons(word, NODE_NIL);
    mem_cons(word, NODE_NIL);

    // After creating 2 nodes and 1 atom (8 bytes), we should have:
    // - 2 fewer nodes (from cons cells)
    // - 2 fewer potential nodes (from the atom entry)
    TEST_ASSERT_EQUAL(initial - 4, mem_free_nodes());

    // Atom collection returns its storage as well as the two cells.
    mem_gc(NULL, 0);

    TEST_ASSERT_EQUAL(initial, mem_free_nodes());
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
// Node Pool Exhaustion Tests
//============================================================================

void test_cons_until_full_returns_nil(void)
{
    // Keep cons-ing until we run out of memory
    Node word = mem_atom("x", 1);
    TEST_ASSERT_FALSE(mem_is_nil(word));

    int allocated = 0;
    Node last = NODE_NIL;
    while (true)
    {
        Node cell = mem_cons(word, last);
        if (mem_is_nil(cell))
            break;
        last = cell;
        allocated++;
        // Safety limit to avoid infinite loop
        if (allocated > 100000)
            break;
    }

    // We should have allocated many nodes before running out
    TEST_ASSERT_TRUE(allocated > 100);
    // The last allocation should have returned NODE_NIL
    TEST_ASSERT_TRUE(mem_is_nil(mem_cons(word, NODE_NIL)));
}

void test_free_nodes_zero_when_exhausted(void)
{
    Node word = mem_atom("y", 1);

    // Exhaust node pool
    Node list = NODE_NIL;
    while (true)
    {
        Node cell = mem_cons(word, list);
        if (mem_is_nil(cell))
            break;
        list = cell;
    }

    // The gap between atom_next and node_bottom may leave up to 1
    // potential node that can't actually be allocated
    TEST_ASSERT_LESS_OR_EQUAL(1, mem_free_nodes());
    // Verify we truly can't allocate
    TEST_ASSERT_TRUE(mem_is_nil(mem_cons(word, NODE_NIL)));
}

void test_gc_recovers_exhausted_nodes(void)
{
    Node word = mem_atom("z", 1);
    size_t initial_free = mem_free_nodes();

    // Exhaust by building a long list (not rooted for GC)
    Node list = NODE_NIL;
    int allocated = 0;
    while (true)
    {
        Node cell = mem_cons(word, list);
        if (mem_is_nil(cell))
            break;
        list = cell;
        allocated++;
    }
    TEST_ASSERT_TRUE(allocated > 100);
    TEST_ASSERT_LESS_OR_EQUAL(1, mem_free_nodes());
    TEST_ASSERT_TRUE(mem_is_nil(mem_cons(word, NODE_NIL)));

    // GC with no roots — everything should be freed
    mem_gc(NULL, 0);
    TEST_ASSERT_TRUE(mem_free_nodes() > 0);
}

void test_gc_partial_recovery(void)
{
    Node word = mem_atom("a", 1);

    // Build 10 rooted cells
    Node rooted = NODE_NIL;
    for (int i = 0; i < 10; i++)
    {
        Node cell = mem_cons(word, rooted);
        TEST_ASSERT_FALSE(mem_is_nil(cell));
        rooted = cell;
    }

    // Build many unreachable cells
    Node throwaway = NODE_NIL;
    int allocated = 0;
    while (true)
    {
        Node cell = mem_cons(word, throwaway);
        if (mem_is_nil(cell))
            break;
        throwaway = cell;
        allocated++;
    }
    TEST_ASSERT_TRUE(allocated > 0);
    TEST_ASSERT_LESS_OR_EQUAL(1, mem_free_nodes());
    TEST_ASSERT_TRUE(mem_is_nil(mem_cons(word, NODE_NIL)));

    // GC with rooted list — should recover throwaway cells
    mem_gc(&rooted, 1);
    TEST_ASSERT_TRUE(mem_free_nodes() > 0);

    // Rooted list should still be accessible
    Node cursor = rooted;
    int count = 0;
    while (!mem_is_nil(cursor))
    {
        count++;
        cursor = mem_cdr(cursor);
    }
    TEST_ASSERT_EQUAL(10, count);
}

void test_allocate_after_gc_recovery(void)
{
    Node word = mem_atom("b", 1);

    // Exhaust pool
    Node throwaway = NODE_NIL;
    while (true)
    {
        Node cell = mem_cons(word, throwaway);
        if (mem_is_nil(cell))
            break;
        throwaway = cell;
    }

    // Recover via GC
    mem_gc(NULL, 0);
    size_t free_after_gc = mem_free_nodes();
    TEST_ASSERT_TRUE(free_after_gc > 0);

    // New allocations should work
    Node new_cell = mem_cons(word, NODE_NIL);
    TEST_ASSERT_FALSE(mem_is_nil(new_cell));
}

//============================================================================
// Atom Space Exhaustion Tests
//============================================================================

void test_large_atoms_exhaust_space(void)
{
    // Create atoms with increasingly large names to exhaust atom table
    // Also consume node space with cons cells so the shared pool exhausts
    int created = 0;
    Node list = NODE_NIL;
    for (int i = 0; i < 100000; i++)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "lau%05d", i);
        Node word = mem_atom(buf, strlen(buf));
        if (mem_is_nil(word))
            break;
        // Also consume node space to converge the two regions
        Node cell = mem_cons(word, list);
        if (!mem_is_nil(cell))
            list = cell;
        created++;
    }

    // Should have created many atoms before exhaustion
    TEST_ASSERT_TRUE(created > 50);
}

void test_interleaved_atom_node_exhaustion(void)
{
    // Alternate between creating atoms and cons cells to exhaust the shared pool
    int atoms_created = 0;
    int nodes_created = 0;
    Node list = NODE_NIL;

    for (int i = 0; i < 50000; i++)
    {
        if (i % 2 == 0)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "iw%d", i);
            Node word = mem_atom(buf, strlen(buf));
            if (mem_is_nil(word))
                break;
            atoms_created++;
        }
        else
        {
            Node cell = mem_cons(mem_atom("c", 1), list);
            if (mem_is_nil(cell))
                break;
            list = cell;
            nodes_created++;
        }
    }

    // Both atoms and nodes should have been created
    TEST_ASSERT_TRUE(atoms_created > 10);
    TEST_ASSERT_TRUE(nodes_created > 10);
}

void test_cons_returns_nil_not_crash_on_full(void)
{
    Node word = mem_atom("d", 1);

    // Exhaust completely
    while (!mem_is_nil(mem_cons(word, NODE_NIL)))
    {
        // keep allocating
    }

    // Multiple OOM calls should all safely return NODE_NIL
    for (int i = 0; i < 20; i++)
    {
        TEST_ASSERT_TRUE(mem_is_nil(mem_cons(word, NODE_NIL)));
    }
}

void test_cons_rejects_word_with_offset_too_large(void)
{
    // Words referenced from cons cells are encoded with the high bit (0x8000)
    // set on top of the atom offset, so any atom offset >= 0x8000 cannot fit.
    // mem_cons must report failure (NODE_NIL) rather than silently writing a
    // 0 cell index that would be indistinguishable from NIL.
    Node bad_word = NODE_MAKE_WORD(0x8000);
    Node result = mem_cons(bad_word, NODE_NIL);
    TEST_ASSERT_TRUE(mem_is_nil(result));

    // Also reject when only the cdr is unencodable
    Node ok_word = mem_atom("ok", 2);
    Node result2 = mem_cons(ok_word, bad_word);
    TEST_ASSERT_TRUE(mem_is_nil(result2));
}

void test_atom_returns_nil_not_crash_when_full(void)
{
    // Fill atom space with large unique atoms
    int created = 0;
    for (int i = 0; i < 100000; i++)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "fill_atom_%06d", i);
        Node word = mem_atom(buf, strlen(buf));
        if (mem_is_nil(word))
            break;
        created++;
    }
    TEST_ASSERT_TRUE(created > 0);

    // Multiple OOM atom calls should safely return NODE_NIL
    for (int i = 0; i < 20; i++)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "overflow_%d", i);
        Node word = mem_atom(buf, strlen(buf));
        TEST_ASSERT_TRUE(mem_is_nil(word));
    }
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
    RUN_TEST(test_atom_max_length);
    RUN_TEST(test_atom_too_long_is_error);
    RUN_TEST(test_atom_unescape_too_long_is_error);
    RUN_TEST(test_atom_unescape_strips_backslash);
    RUN_TEST(test_atom_unescape_strips_bars);
    RUN_TEST(test_atom_unescape_bar_with_delimiters);
    RUN_TEST(test_atom_unescape_escaped_bar_inside_bars);
    RUN_TEST(test_atom_unescape_bar_and_backslash_agree);
    RUN_TEST(test_blob_no_region_returns_nil);
    RUN_TEST(test_blob_basic_exceeds_atom_limit);
    RUN_TEST(test_blob_not_storable_in_cell);
    RUN_TEST(test_blob_survives_gc_when_rooted);
    RUN_TEST(test_blob_collected_when_unreferenced);
    RUN_TEST(test_blob_equals_atom_by_content);
    RUN_TEST(test_blob_table_full);
    RUN_TEST(test_blob_region_full);
    RUN_TEST(test_region_alloc_no_region_returns_null);
    RUN_TEST(test_region_alloc_pinned_survives_gc);
    RUN_TEST(test_atom_cstr);
    RUN_TEST(test_atom_interning);
    RUN_TEST(test_atom_interning_case_insensitive);
    RUN_TEST(test_different_atoms);
    RUN_TEST(test_atom_uses_memory);
    RUN_TEST(test_interned_atom_no_extra_memory);
    RUN_TEST(test_atom_intern_many_dedup);
    RUN_TEST(test_word_eq);
    RUN_TEST(test_words_equal);
    RUN_TEST(test_empty_word);
    RUN_TEST(test_number_as_atom);
    RUN_TEST(test_negative_number_as_atom);

    // Cons/Lists
    RUN_TEST(test_list_append_builds_list);
    RUN_TEST(test_list_append_fails_when_pool_exhausted);
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
    RUN_TEST(test_gc_reclaims_unreachable_atoms);
    RUN_TEST(test_gc_preserves_rooted_atoms_in_cells);
    RUN_TEST(test_gc_preserves_atom_pointer_root);
    RUN_TEST(test_interning_ignores_atom_mark_bits);
    RUN_TEST(test_gc_recovers_exhausted_atom_space);
    RUN_TEST(test_atom_chain_end_is_not_an_allocatable_offset);
    RUN_TEST(test_gc_reuses_interior_atom_hole_without_losing_live_atom);
    RUN_TEST(test_gc_preserves_long_list);
    RUN_TEST(test_gc_preserves_nested_lists);
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

    // Node Pool Exhaustion
    RUN_TEST(test_cons_until_full_returns_nil);
    RUN_TEST(test_free_nodes_zero_when_exhausted);
    RUN_TEST(test_gc_recovers_exhausted_nodes);
    RUN_TEST(test_gc_partial_recovery);
    RUN_TEST(test_allocate_after_gc_recovery);

    // Atom Space Exhaustion
    RUN_TEST(test_large_atoms_exhaust_space);
    RUN_TEST(test_interleaved_atom_node_exhaustion);
    RUN_TEST(test_cons_returns_nil_not_crash_on_full);
    RUN_TEST(test_cons_rejects_word_with_offset_too_large);
    RUN_TEST(test_atom_returns_nil_not_crash_when_full);

    return UNITY_END();
}
