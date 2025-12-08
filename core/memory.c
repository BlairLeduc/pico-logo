//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Memory management implementation.
//
//  Design:
//  - Node pool: array of 32-bit cons cells, indexed by 16-bit indices
//  - Each cell stores: car_index (16 bits) | cdr_index (16 bits)
//  - Node values (passed around) encode type + index/offset
//  - Words are references to interned atoms (never stored in pool)
//  - Lists are references to cons cells in the pool
//  - Word references in cells use high bit (0x8000) to distinguish from list indices
//

#include "core/memory.h"

#include <ctype.h>
#include <string.h>

// Align value up to 4-byte boundary
#define ALIGN4(x) (((x) + 3) & ~3)

//==========================================================================
// Memory Pools (static allocation)
//==========================================================================

// Node pool - array of cons cells
// Cell format: [car_index:16][cdr_index:16]
// Index 0 is reserved (represents NIL in car/cdr)
static uint32_t node_pool[LOGO_NODE_POOL_SIZE];

// Atom table - length-prefixed strings stored contiguously
// Format: [len:1][chars:len][len:1][chars:len]...
static uint8_t atom_table[LOGO_ATOM_TABLE_SIZE];

// Free list head (index into node_pool, or 0 if empty)
static uint16_t free_list;

// Number of free nodes
static size_t free_count;

// Next free position in atom table
static size_t atom_next;

//==========================================================================
// Initialization
//==========================================================================

// Initialize the memory system.
// Must be called before any other memory functions.
void mem_init(void)
{
    // Initialize node pool as a free list
    // Index 0 is reserved for NIL references
    node_pool[0] = 0;

    // Link all other nodes into free list
    // Free cells use cdr field to point to next free cell
    for (size_t i = 1; i < LOGO_NODE_POOL_SIZE - 1; i++)
    {
        node_pool[i] = CELL_MAKE(0, i + 1);
    }
    // Last node has cdr = 0 (end of free list)
    node_pool[LOGO_NODE_POOL_SIZE - 1] = CELL_MAKE(0, 0);

    free_list = 1;
    free_count = LOGO_NODE_POOL_SIZE - 1; // Exclude index 0

    // Initialize atom table
    atom_next = 0;
    memset(atom_table, 0, LOGO_ATOM_TABLE_SIZE);
}

//==========================================================================
// Node Allocation
//==========================================================================

// Allocate a cell from the free list
// Returns index, or 0 if out of memory
static uint16_t alloc_cell(void)
{
    if (free_list == 0)
    {
        return 0; // Out of memory
    }

    uint16_t index = free_list;
    uint32_t cell = node_pool[index];
    free_list = CELL_GET_CDR(cell);
    free_count--;

    return index;
}

//==========================================================================
// Helper: Convert Node to cell index (for storage in cells)
//==========================================================================

// Convert a Node value to an index for storage in a cell's car/cdr.
// Words use high bit (0x8000) + atom offset.
// Lists use the pool index directly.
static uint16_t node_to_index(Node n)
{
    if (n == NODE_NIL)
    {
        return 0;
    }

    NodeType type = NODE_GET_TYPE(n);

    if (type == NODE_TYPE_LIST)
    {
        // List - use pool index directly
        return (uint16_t)NODE_GET_INDEX(n);
    }
    else if (type == NODE_TYPE_WORD)
    {
        // Word - encode with high bit set
        uint32_t offset = NODE_GET_INDEX(n);
        if (offset < 0x8000)
        {
            return (uint16_t)(0x8000 | offset);
        }
        // Atom offset too large
        return 0;
    }

    return 0;
}

// Convert a cell index back to a Node value
static Node index_to_node(uint16_t index)
{
    if (index == 0)
    {
        return NODE_NIL;
    }

    // Check if this is a word reference (high bit set)
    if (index & 0x8000)
    {
        uint32_t offset = index & 0x7FFF;
        return NODE_MAKE_WORD(offset);
    }

    // It's a list reference
    return NODE_MAKE_LIST(index);
}

//==========================================================================
// Cons Cells (List Nodes)
//==========================================================================

// Create a cons cell (list node) with car and cdr.
// Returns NODE_NIL if out of memory.
Node mem_cons(Node car, Node cdr)
{
    uint16_t index = alloc_cell();
    if (index == 0)
    {
        return NODE_NIL; // Out of memory
    }

    uint16_t car_idx = node_to_index(car);
    uint16_t cdr_idx = node_to_index(cdr);

    node_pool[index] = CELL_MAKE(car_idx, cdr_idx);

    return NODE_MAKE_LIST(index);
}

//==========================================================================
// Atom Table (Interned Words)
//==========================================================================

// Case-insensitive string comparison
static bool str_eq_nocase(const char *a, size_t alen, const char *b, size_t blen)
{
    if (alen != blen)
    {
        return false;
    }
    for (size_t i = 0; i < alen; i++)
    {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
        {
            return false;
        }
    }
    return true;
}

// Find an existing atom in the table
// Returns the offset if found, or LOGO_ATOM_TABLE_SIZE if not found
// Each atom entry is aligned to 4-byte boundary: [len:1][chars:len][nul:1][padding]
static size_t find_atom(const char *str, size_t len)
{
    size_t offset = 0;
    while (offset < atom_next)
    {
        uint8_t atom_len = atom_table[offset];
        if (str_eq_nocase(str, len, (const char *)&atom_table[offset + 1], atom_len))
        {
            return offset;
        }
        // Advance to next aligned entry (includes null terminator)
        offset += ALIGN4(1 + atom_len + 1);
    }
    return LOGO_ATOM_TABLE_SIZE; // Not found
}

// Intern a word in the atom table
// Each entry is aligned to 4-byte boundary: [len:1][chars:len][nul:1][padding]
Node mem_atom(const char *str, size_t len)
{
    // Limit atom length to 255 (1 byte length prefix)
    if (len > 255)
    {
        len = 255;
    }

    // Check if atom already exists
    size_t offset = find_atom(str, len);
    if (offset < LOGO_ATOM_TABLE_SIZE)
    {
        // Found existing atom
        return NODE_MAKE_WORD(offset);
    }

    // Calculate aligned size for this entry
    // Include space for null terminator: [len:1][chars:len][nul:1][padding]
    size_t entry_size = ALIGN4(1 + len + 1);

    // Need to add new atom
    if (atom_next + entry_size > LOGO_ATOM_TABLE_SIZE)
    {
        return NODE_NIL; // Out of atom space
    }

    // Check offset fits in our encoding (15 bits = 32KB max)
    if (atom_next >= 0x8000)
    {
        return NODE_NIL; // Atom table too large
    }

    offset = atom_next;
    atom_table[atom_next] = (uint8_t)len;
    memcpy(&atom_table[atom_next + 1], str, len);
    atom_table[atom_next + 1 + len] = '\0';  // Null terminator
    atom_next += entry_size;

    return NODE_MAKE_WORD(offset);
}

// Convenience: intern a null-terminated string
Node mem_atom_cstr(const char *str)
{
    return mem_atom(str, strlen(str));
}

//==========================================================================
// Node Access
//==========================================================================

// Get the car (first element) of a list node.
// Returns NODE_NIL if node is not a list.
Node mem_car(Node n)
{
    if (n == NODE_NIL)
    {
        return NODE_NIL;
    }

    if (NODE_GET_TYPE(n) != NODE_TYPE_LIST)
    {
        return NODE_NIL;
    }

    uint32_t index = NODE_GET_INDEX(n);
    if (index == 0 || index >= LOGO_NODE_POOL_SIZE)
    {
        return NODE_NIL;
    }

    uint32_t cell = node_pool[index];
    uint16_t car_idx = CELL_GET_CAR(cell);

    return index_to_node(car_idx);
}

// Get the cdr (rest) of a list node.
// Returns NODE_NIL if node is not a list or at end.
Node mem_cdr(Node n)
{
    if (n == NODE_NIL)
    {
        return NODE_NIL;
    }

    if (NODE_GET_TYPE(n) != NODE_TYPE_LIST)
    {
        return NODE_NIL;
    }

    uint32_t index = NODE_GET_INDEX(n);
    if (index == 0 || index >= LOGO_NODE_POOL_SIZE)
    {
        return NODE_NIL;
    }

    uint32_t cell = node_pool[index];
    uint16_t cdr_idx = CELL_GET_CDR(cell);

    return index_to_node(cdr_idx);
}

bool mem_set_car(Node n, Node value)
{
    if (n == NODE_NIL || NODE_GET_TYPE(n) != NODE_TYPE_LIST)
    {
        return false;
    }

    uint32_t index = NODE_GET_INDEX(n);
    if (index == 0 || index >= LOGO_NODE_POOL_SIZE)
    {
        return false;
    }

    uint32_t cell = node_pool[index];
    uint16_t cdr_idx = CELL_GET_CDR(cell);
    uint16_t car_idx = node_to_index(value);

    node_pool[index] = CELL_MAKE(car_idx, cdr_idx);

    return true;
}

bool mem_set_cdr(Node n, Node value)
{
    if (n == NODE_NIL || NODE_GET_TYPE(n) != NODE_TYPE_LIST)
    {
        return false;
    }

    uint32_t index = NODE_GET_INDEX(n);
    if (index == 0 || index >= LOGO_NODE_POOL_SIZE)
    {
        return false;
    }

    uint32_t cell = node_pool[index];
    uint16_t car_idx = CELL_GET_CAR(cell);
    uint16_t cdr_idx = node_to_index(value);

    node_pool[index] = CELL_MAKE(car_idx, cdr_idx);

    return true;
}

//==========================================================================
// Node Type Checks
//==========================================================================

// Check if a node is the empty list.
bool mem_is_nil(Node n)
{
    return n == NODE_NIL;
}

// Check if a node is a list (cons cell).
bool mem_is_list(Node n)
{
    return n != NODE_NIL && NODE_GET_TYPE(n) == NODE_TYPE_LIST;
}

// Check if a node is a word.
bool mem_is_word(Node n)
{
    return NODE_GET_TYPE(n) == NODE_TYPE_WORD;
}

//==========================================================================
// Word Access
//==========================================================================

// Get pointer to the string for a word node.
// Returns NULL if node is not a word.
// The returned pointer is valid until the next GC.
const char *mem_word_ptr(Node n)
{
    if (NODE_GET_TYPE(n) != NODE_TYPE_WORD)
    {
        return NULL;
    }

    uint32_t offset = NODE_GET_INDEX(n);
    if (offset >= atom_next)
    {
        return NULL;
    }

    // Return pointer to the string (after length byte)
    return (const char *)&atom_table[offset + 1];
}

// Get the length of a word node's string.
// Returns 0 if node is not a word.
size_t mem_word_len(Node n)
{
    if (NODE_GET_TYPE(n) != NODE_TYPE_WORD)
    {
        return 0;
    }

    uint32_t offset = NODE_GET_INDEX(n);
    if (offset >= atom_next)
    {
        return 0;
    }

    return atom_table[offset];
}

// Compare a word node to a given string (case-insensitive).
// Returns true if they match.
bool mem_word_eq(Node n, const char *str, size_t len)
{
    if (NODE_GET_TYPE(n) != NODE_TYPE_WORD)
    {
        return false;
    }

    uint32_t offset = NODE_GET_INDEX(n);
    if (offset >= atom_next)
    {
        return false;
    }

    uint8_t atom_len = atom_table[offset];
    return str_eq_nocase(str, len, (const char *)&atom_table[offset + 1], atom_len);
}

// Compare two word nodes for equality.
// Returns true if they are the same word.
bool mem_words_equal(Node a, Node b)
{
    if (NODE_GET_TYPE(a) != NODE_TYPE_WORD || NODE_GET_TYPE(b) != NODE_TYPE_WORD)
    {
        return false;
    }

    // Since atoms are interned, same offset means same word
    return NODE_GET_INDEX(a) == NODE_GET_INDEX(b);
}

//==========================================================================
// Garbage Collection
//==========================================================================

// Bit array for marking (1 bit per node)
static uint32_t gc_marks[(LOGO_NODE_POOL_SIZE + 31) / 32];

// Recursive mark function
static void gc_mark_index(uint16_t index);

// Mark a node and its reachable nodes
static void gc_mark_node(Node n)
{
    if (n == NODE_NIL)
    {
        return;
    }

    NodeType type = NODE_GET_TYPE(n);

    // Words don't need marking (atoms are never freed)
    if (type == NODE_TYPE_WORD)
    {
        return;
    }

    if (type != NODE_TYPE_LIST)
    {
        return;
    }

    gc_mark_index((uint16_t)NODE_GET_INDEX(n));
}

static void gc_mark_index(uint16_t index)
{
    if (index == 0 || index >= LOGO_NODE_POOL_SIZE)
    {
        return;
    }

    // Check if already marked
    uint32_t word_idx = index / 32;
    uint32_t bit_idx = index % 32;
    if (gc_marks[word_idx] & (1u << bit_idx))
    {
        return; // Already marked
    }

    // Mark this node
    gc_marks[word_idx] |= (1u << bit_idx);

    // Recursively mark car and cdr
    uint32_t cell = node_pool[index];
    uint16_t car_idx = CELL_GET_CAR(cell);
    uint16_t cdr_idx = CELL_GET_CDR(cell);

    // Mark car (may be word reference with high bit, or list index)
    if (car_idx != 0 && !(car_idx & 0x8000))
    {
        // List reference - mark recursively
        gc_mark_index(car_idx);
    }

    // Mark cdr
    if (cdr_idx != 0 && !(cdr_idx & 0x8000))
    {
        gc_mark_index(cdr_idx);
    }
}

// Mark a node and its reachable nodes
void mem_gc_mark(Node n)
{
    gc_mark_node(n);
}

// Sweep unmarked nodes back to free list
void mem_gc_sweep(void)
{
    free_list = 0;
    free_count = 0;

    // Sweep through all nodes (skip 0 which is reserved)
    for (uint16_t i = 1; i < LOGO_NODE_POOL_SIZE; i++)
    {
        uint32_t word_idx = i / 32;
        uint32_t bit_idx = i % 32;

        if (gc_marks[word_idx] & (1u << bit_idx))
        {
            // Marked - keep it, clear the mark
            gc_marks[word_idx] &= ~(1u << bit_idx);
        }
        else
        {
            // Not marked - free it
            node_pool[i] = CELL_MAKE(0, free_list);
            free_list = i;
            free_count++;
        }
    }

    // Clear any remaining marks
    memset(gc_marks, 0, sizeof(gc_marks));
}

// Run garbage collection.
// Caller must mark all roots first with mem_gc_mark().
void mem_gc(Node *roots, size_t num_roots)
{
    // Clear mark bits
    memset(gc_marks, 0, sizeof(gc_marks));

    // Mark phase
    for (size_t i = 0; i < num_roots; i++)
    {
        mem_gc_mark(roots[i]);
    }

    // Sweep phase
    mem_gc_sweep();
}

//==========================================================================
// Memory Statistics
//==========================================================================

// Get the number of free nodes available.
size_t mem_free_nodes(void)
{
    return free_count;
}

// Get the total number of nodes in the pool.
size_t mem_total_nodes(void)
{
    return LOGO_NODE_POOL_SIZE - 1; // Exclude index 0
}

// Get the number of free atoms available.
size_t mem_free_atoms(void)
{
    return LOGO_ATOM_TABLE_SIZE - atom_next;
}

// Get the total number of atoms in the table.
size_t mem_total_atoms(void)
{
    return LOGO_ATOM_TABLE_SIZE;
}
