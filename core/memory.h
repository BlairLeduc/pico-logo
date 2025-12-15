//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Memory management for Logo objects (nodes and atoms).
//
//  Logo has two types of objects: words and lists.
//  - Words are interned in an atom table (stored once, referenced by index)
//  - Lists are linked sequences of nodes (cons cells)
//
//  Nodes are 32-bit values for ARM word-alignment efficiency.
//  Memory is managed with a free list and mark-and-sweep garbage collection.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //==========================================================================
    // Memory Configuration
    //==========================================================================

    // Default memory size (can be overridden at compile time)
    // RP2040: ~64KB available (after 200KB video RAM)
    // RP2350: ~320KB available (after 200KB video RAM)
    //
    // Memory Layout:
    // +------------------+ <- offset 0
    // |   Atom Table     |
    // |   (grows up ↓)   |
    // +------------------+ <- atom_next
    // |                  |
    // |   Free Space     |
    // |                  |
    // +------------------+ <- node_bottom (first allocated node address)
    // |   Node Pool      |
    // |   (grows down ↑) |
    // +------------------+ <- LOGO_MEMORY_SIZE (top of memory)

#ifndef LOGO_MEMORY_SIZE
#define LOGO_MEMORY_SIZE 131072 // Total memory block (128KB)
#endif

    //==========================================================================
    // Node Representation
    //==========================================================================

    // A Node is a 32-bit value, word-aligned for ARM efficiency.
    //
    // There are three kinds of Node values:
    //
    // 1. NODE_NIL (0x00000000) - the empty list []
    //
    // 2. Word reference:
    //    Bits 31-30: 10 (NODE_TYPE_WORD)
    //    Bits 29-0:  Atom table offset (30 bits, max 1GB)
    //
    // 3. List reference (index into node pool):
    //    Bits 31-30: 01 (NODE_TYPE_LIST) 
    //    Bits 29-0:  Node pool index (30 bits)
    //
    // The node pool stores cons cells. Each cons cell is 32 bits:
    //    Bits 31-16: Car index (16 bits) - index of car node in pool, or 0 for NIL
    //    Bits 15-0:  Cdr index (16 bits) - index of cdr node in pool, or 0 for NIL
    //
    // With 16-bit indices, we can have up to 65535 nodes (256KB at 4 bytes each).
    //
    typedef uint32_t Node;

    // Node types (2 bits in high position)
    typedef enum
    {
        NODE_TYPE_FREE = 0, // Free node in pool (on free list)
        NODE_TYPE_LIST = 1, // List reference (cons cell)
        NODE_TYPE_WORD = 2, // Word reference (atom)
        NODE_TYPE_MARK = 3, // Marked during GC (temporary)
    } NodeType;

    // Special node values
#define NODE_NIL 0  // Empty list []

    // Node reference macros (for Node values passed around)
#define NODE_TYPE_SHIFT 30
#define NODE_GET_TYPE(n) ((n) >> NODE_TYPE_SHIFT)
#define NODE_GET_INDEX(n) ((n) & 0x3FFFFFFF)
#define NODE_MAKE_WORD(offset) (((uint32_t)NODE_TYPE_WORD << NODE_TYPE_SHIFT) | (offset))
#define NODE_MAKE_LIST(index) (((uint32_t)NODE_TYPE_LIST << NODE_TYPE_SHIFT) | (index))

    // Cons cell macros (for cells stored in node pool)
#define CELL_GET_CAR(cell) ((cell) >> 16)
#define CELL_GET_CDR(cell) ((cell) & 0xFFFF)
#define CELL_MAKE(car, cdr) (((uint32_t)(car) << 16) | ((uint32_t)(cdr) & 0xFFFF))

    //==========================================================================
    // Memory API
    //==========================================================================

    // Initialize the memory system.
    // Must be called before any other memory functions.
    void mem_init(void);

    // Create a cons cell (list node) with car and cdr.
    // Returns NODE_NIL if out of memory.
    Node mem_cons(Node car, Node cdr);

    // Intern a word (string) in the atom table.
    // If the word already exists, returns the existing node.
    // Returns NODE_NIL if out of memory.
    Node mem_atom(const char *str, size_t len);

    // Convenience: intern a null-terminated string
    Node mem_atom_cstr(const char *str);

    // Get the car (first element) of a list node.
    // Returns NODE_NIL if node is not a list.
    Node mem_car(Node n);

    // Get the cdr (rest) of a list node.
    // Returns NODE_NIL if node is not a list or at end.
    Node mem_cdr(Node n);

    // Set the car of a list node.
    // Returns false if node is not a list.
    bool mem_set_car(Node n, Node value);

    // Set the cdr of a list node.
    // Returns false if node is not a list.
    bool mem_set_cdr(Node n, Node value);

    // Check if a node is the empty list.
    bool mem_is_nil(Node n);

    // Check if a node is a list (cons cell).
    bool mem_is_list(Node n);

    // Check if a node is a word.
    bool mem_is_word(Node n);

    // Get the string content of a word node.
    // Returns NULL if node is not a word.
    // The returned pointer is valid until the next GC.
    const char *mem_word_ptr(Node n);

    // Get the length of a word node's string.
    // Returns 0 if node is not a word.
    size_t mem_word_len(Node n);

    // Compare a word node to a string (case-insensitive).
    // Returns true if they match.
    bool mem_word_eq(Node n, const char *str, size_t len);

    // Compare two word nodes for equality (case-insensitive).
    bool mem_words_equal(Node a, Node b);

    //==========================================================================
    // Garbage Collection
    //==========================================================================

    // Mark a node as reachable (for GC roots).
    // Call this for all root nodes before calling mem_gc_sweep().
    void mem_gc_mark(Node n);

    // Run garbage collection.
    // Caller must mark all roots first with mem_gc_mark().
    void mem_gc_sweep(void);

    // Convenience: run full GC with automatic root marking.
    // Roots are provided as an array of node pointers.
    void mem_gc(Node *roots, size_t num_roots);

    //==========================================================================
    // Memory Statistics
    //==========================================================================

    // Get the number of free nodes available.
    size_t mem_free_nodes(void);

    // Get the total number of nodes in the pool.
    size_t mem_total_nodes(void);

    // Get the number of bytes free in the atom table.
    size_t mem_free_atoms(void);

    // Get the total size of the atom table in bytes.
    size_t mem_total_atoms(void);

#ifdef __cplusplus
}
#endif
