//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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

    // Maximum number of live blobs (large values in the auxiliary/PSRAM region).
    // Each descriptor is small; blobs are few and large, so this cap is modest.
#ifndef LOGO_MAX_BLOBS
#define LOGO_MAX_BLOBS 128
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
    // 2. Word reference. Two sub-kinds share NODE_TYPE_WORD, distinguished by
    //    bit 29 (NODE_WORD_BLOB_BIT):
    //    a) Atom (bit 29 == 0):
    //       Bits 31-30: 10 (NODE_TYPE_WORD)
    //       Bit  29:    0
    //       Bits 28-0:  Atom table offset. Atom offsets are capped at 32 KB
    //                   (LOGO_ATOM_LIMIT in memory.c) so a word reference
    //                   fits the 15-bit field of a 16-bit cell half; they
    //                   never collide with the blob bit.
    //    b) Blob (bit 29 == 1): a large value held in the PSRAM blob heap.
    //       Bits 31-30: 10 (NODE_TYPE_WORD)
    //       Bit  29:    1
    //       Bits 28-0:  Blob handle (index into the blob descriptor table)
    //    To the outside world a blob is still a word (mem_is_word is true);
    //    mem_word_ptr/mem_word_len read its bytes transparently.
    //
    // 3. List reference (index into node pool):
    //    Bits 31-30: 01 (NODE_TYPE_LIST) 
    //    Bits 29-0:  Node pool index (30 bits)
    //
    // The node pool stores cons cells. Each cons cell is 32 bits:
    //    Bits 31-16: Car index (16 bits) - index of car node in pool, or 0 for NIL
    //    Bits 15-0:  Cdr index (16 bits) - index of cdr node in pool, or 0 for NIL
    //
    // Within a cell, the high bit of each 16-bit half marks a word reference
    // and 0x7FFF is the empty-list sentinel (see memory.c), so pool indices
    // are capped at 32766 nodes (~128KB at 4 bytes each) and atom offsets at
    // 32KB. Of the LOGO_MEMORY_SIZE block, at most 32KB can ever be atoms.
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

    // Blob sub-encoding within NODE_TYPE_WORD (see comment above).
    // Bit 29 of the 30-bit payload marks a blob; the low 29 bits are the handle.
#define NODE_WORD_BLOB_BIT (1u << 29)
#define NODE_MAKE_BLOB(handle) NODE_MAKE_WORD(NODE_WORD_BLOB_BIT | (handle))
#define NODE_WORD_IS_BLOB(n) \
    ((NODE_GET_TYPE(n) == NODE_TYPE_WORD) && (NODE_GET_INDEX(n) & NODE_WORD_BLOB_BIT))
#define NODE_GET_BLOB_HANDLE(n) (NODE_GET_INDEX(n) & ~NODE_WORD_BLOB_BIT)

    // Cons cell macros (for cells stored in node pool)
#define CELL_GET_CAR(cell) ((cell) >> 16)
#define CELL_GET_CDR(cell) ((cell) & 0xFFFF)
#define CELL_MAKE(car, cdr) (((uint32_t)(car) << 16) | ((uint32_t)(cdr) & 0xFFFF))

    //==========================================================================
    // Memory API
    //==========================================================================

    // Initialize the memory system.
    // Must be called before any other memory functions.
    void logo_mem_init(void);

    // Provide an auxiliary memory region (e.g. PSRAM) used to back the blob
    // heap for large values. Call once, after logo_mem_init(), before any blob
    // allocation. If never called, blob allocation fails gracefully (mem_blob
    // returns NODE_NIL) and the interpreter is limited to interned atoms.
    // Passing base == NULL or size == 0 clears any region.
    void logo_mem_set_aux_region(void *base, size_t size);

    // Allocate a permanent (never-freed, never-GC'd) raw buffer from the
    // auxiliary region. Intended for long-lived device buffers that we want to
    // keep off scarce SRAM (e.g. the HTTP transfer buffer, editor buffers).
    // Returns NULL if there is no aux region or it is out of space, so callers
    // must fall back to a static/SRAM buffer. Must be called after
    // logo_mem_set_aux_region().
    void *mem_region_alloc(size_t size);

    // Create a cons cell (list node) with car and cdr.
    // Returns NODE_NIL if out of memory.
    //
    // OVERFLOW: the node pool is a fixed region shared with the atom table;
    // when it is exhausted (or an operand cannot be encoded in a 16-bit
    // cell), mem_cons returns NODE_NIL. Callers building lists must check
    // for this — silently consing NODE_NIL produces a truncated or empty
    // list that looks valid. Use mem_list_append for the common
    // build-with-tail-pointer loop; it reports the failure.
    Node mem_cons(Node car, Node cdr);

    // Append `item` as the next element of a list under construction,
    // maintaining *head and *tail. Start with *head == *tail == NODE_NIL.
    // Returns false if cell allocation failed (the list built so far is
    // unchanged); callers should surface ERR_OUT_OF_SPACE rather than
    // return a truncated list.
    bool mem_list_append(Node *head, Node *tail, Node item);

    // Intern a word (string) in the atom table.
    // If the word already exists, returns the existing node.
    // Returns NODE_NIL if out of memory.
    Node mem_atom(const char *str, size_t len);

    // Intern a word while processing backslash escapes.
    // Each \X sequence becomes just X in the resulting atom.
    // Returns NODE_NIL if out of memory.
    Node mem_atom_unescape(const char *str, size_t len);

    // Convenience: intern a null-terminated string
    Node mem_atom_cstr(const char *str);

    // Allocate a large value (blob) in the auxiliary region and return it as a
    // word node. Unlike atoms, blobs are not interned, are garbage-collected,
    // and can exceed 255 bytes. The bytes are copied in and NUL-terminated.
    // Returns NODE_NIL if there is no aux region, the descriptor table is full,
    // or the region is out of space. A blob word cannot be stored inside a cons
    // cell (mem_cons returns NODE_NIL for a blob operand).
    Node mem_blob(const char *str, size_t len);

    // Create a word of any length: interns as an atom when len <= 255, otherwise
    // allocates a blob in the aux region. Returns NODE_NIL on failure (e.g. a
    // long value with no aux region available). Use this for values that may
    // exceed the 255-byte atom limit, such as network responses.
    Node mem_word(const char *str, size_t len);

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

    // Check if a node is a blob (a large word held in the PSRAM blob heap).
    // A blob is also a word, so mem_is_word(n) is true whenever mem_is_blob(n) is.
    bool mem_is_blob(Node n);

    //==========================================================================
    // Newline Marker
    //==========================================================================

    // Special marker node used to represent newlines in procedure definitions.
    // This allows preserving line breaks inside brackets for pretty-printing.
    extern Node mem_newline_marker;

    // The words "true" and "false", interned once at init. Predicates and
    // comparisons run constantly; use these (or value_bool) instead of
    // re-interning the strings. Atoms are never swept, so no GC rooting is
    // needed.
    extern Node mem_true_node;
    extern Node mem_false_node;

    // Check if a node is the newline marker.
    bool mem_is_newline(Node n);

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

    // Get the number of blob descriptors currently in use.
    size_t mem_blob_used(void);

    // Get the number of free bytes remaining in the blob (aux) region.
    size_t mem_blob_free_bytes(void);

#ifdef __cplusplus
}
#endif
