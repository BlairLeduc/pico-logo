//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Memory management implementation.
//
//  Design:
//  - Single unified memory block with dual-growing allocators
//  - Atom table grows upward from offset 0
//  - Node pool grows downward from top of memory
//  - Each node is a 32-bit cons cell: car_index (16 bits) | cdr_index (16 bits)
//  - Nodes indexed from 1 (index 0 reserved for NIL)
//  - Node index 1 is at LOGO_MEMORY_SIZE-4, index 2 at LOGO_MEMORY_SIZE-8, etc.
//  - Node values (passed around) encode type + index/offset in 32 bits
//  - Words are references to interned atoms (never stored in pool)
//  - Lists are references to cons cells in the pool
//  - Word references in cells use high bit (0x8000) to distinguish from list indices
//  - Free nodes managed via free list (reuses cell storage)
//  - Collision detection prevents atoms and nodes from overlapping
//

#include "core/memory.h"

#include <ctype.h>
#include <string.h>

// Align value up to 4-byte boundary
#define ALIGN4(x) (((x) + 3) & ~3)

//==========================================================================
// Memory Block (static allocation)
//==========================================================================

// Single unified memory block
// Atoms grow upward from offset 0
// Nodes grow downward from the top
static uint8_t memory_block[LOGO_MEMORY_SIZE];

// Free list head (index into node region, or 0 if empty)
static uint16_t free_list;

// Number of nodes on the free list
static size_t free_count;

// Next free position in atom table (grows upward from 0)
static size_t atom_next;

// Bottom of the node region (byte offset, grows downward from LOGO_MEMORY_SIZE)
// This is the address of the lowest allocated node
static size_t node_bottom;

// Number of nodes currently allocated in the node region
static size_t node_count;

//==========================================================================
// Node Indexing Helpers
//==========================================================================

// Get the memory address for a node index
// Index 0 is reserved for NIL
// Index 1 is the first node at the top of memory
// Index N corresponds to the Nth node from the top
static inline uint32_t *get_node_ptr(uint16_t index)
{
    if (index == 0)
    {
        return NULL;
    }
    
    // Validate index to prevent underflow
    // Each node is 4 bytes, so max index is LOGO_MEMORY_SIZE / 4
    if (index > LOGO_MEMORY_SIZE / 4)
    {
        return NULL;
    }
    
    // Calculate byte offset from top of memory
    // Each node is 4 bytes, index 1 is at the very top
    size_t byte_offset = LOGO_MEMORY_SIZE - (index * 4);
    
    return (uint32_t *)&memory_block[byte_offset];
}

// Get the node index from a memory address
// Returns 0 if invalid
static inline uint16_t get_node_index(const uint32_t *ptr)
{
    if (ptr == NULL)
    {
        return 0;
    }
    
    // Calculate byte offset from start of memory block
    size_t byte_offset = (uint8_t *)ptr - memory_block;
    
    // Check if this is within the valid node region
    if (byte_offset < node_bottom || byte_offset >= LOGO_MEMORY_SIZE)
    {
        return 0;
    }
    
    // Calculate index from top
    size_t index = (LOGO_MEMORY_SIZE - byte_offset) / 4;
    
    if (index > 0xFFFF)
    {
        return 0;
    }
    
    return (uint16_t)index;
}

// Get maximum possible node index based on current layout
static inline uint16_t get_max_node_index(void)
{
    return (uint16_t)((LOGO_MEMORY_SIZE - atom_next) / 4);
}

//==========================================================================
// Initialization
//==========================================================================

// Newline marker - initialized in logo_mem_init()
Node mem_newline_marker = NODE_NIL;

// Initialize the memory system.
// Must be called before any other memory functions.
void logo_mem_init(void)
{
    // Initialize atom table (grows upward from 0)
    atom_next = 0;
    memset(memory_block, 0, LOGO_MEMORY_SIZE);
    
    // Initialize node region (grows downward from top)
    // Start with no nodes allocated
    node_bottom = LOGO_MEMORY_SIZE;
    node_count = 0;
    
    // Initialize free list as empty
    free_list = 0;
    free_count = 0;
    
    // Create the newline marker atom (SOH character, non-printable)
    mem_newline_marker = mem_atom("\x01", 1);
}

//==========================================================================
// Node Allocation
//==========================================================================

// Allocate a cell from the free list or expand the node region downward
// Returns index, or 0 if out of memory
static uint16_t alloc_cell(void)
{
    // First, try to get a node from the free list
    if (free_list != 0)
    {
        uint16_t index = free_list;
        uint32_t *cell_ptr = get_node_ptr(index);
        if (cell_ptr != NULL)
        {
            uint32_t cell = *cell_ptr;
            free_list = CELL_GET_CDR(cell);
            free_count--;
            return index;
        }
    }
    
    // Free list is empty, need to expand the node region downward
    // Check if we have space (need 4 bytes for a new node)
    // After allocation: node_bottom will be node_bottom - 4
    // This must not overlap with atom_next
    if (node_bottom <= atom_next + 4)
    {
        return 0; // Out of memory - would collide with atom table
    }
    
    // Allocate new node at the bottom of the node region
    node_bottom -= 4;
    node_count++;
    
    // Calculate the index for this new node
    uint16_t index = (uint16_t)((LOGO_MEMORY_SIZE - node_bottom) / 4);
    
    // Check if index fits in 16 bits
    if (index == 0 || index > 0xFFFF)
    {
        // Restore state and fail
        node_bottom += 4;
        node_count--;
        return 0;
    }
    
    return index;
}

//==========================================================================
// Helper: Convert Node to cell index (for storage in cells)
//==========================================================================

// Convert a Node value to an index for storage in a cell's car/cdr.
// Words use high bit (0x8000) + atom offset.
// Lists use the pool index directly.
// Empty list (NODE_MAKE_LIST(0)) uses special marker 0x7FFF.
#define CELL_EMPTY_LIST_MARKER 0x7FFF

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
        uint32_t index = NODE_GET_INDEX(n);
        // Empty list (index 0) needs special marker to distinguish from NODE_NIL
        if (index == 0)
        {
            return CELL_EMPTY_LIST_MARKER;
        }
        return (uint16_t)index;
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

    // Check for empty list marker
    if (index == CELL_EMPTY_LIST_MARKER)
    {
        return NODE_MAKE_LIST(0);
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

    uint32_t *cell_ptr = get_node_ptr(index);
    if (cell_ptr == NULL)
    {
        return NODE_NIL; // Invalid index
    }
    
    *cell_ptr = CELL_MAKE(car_idx, cdr_idx);

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

// Case-sensitive string comparison
static bool str_eq(const char *a, size_t alen, const char *b, size_t blen)
{
    if (alen != blen)
    {
        return false;
    }
    for (size_t i = 0; i < alen; i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

// Find an existing atom in the table (case-sensitive for exact match)
// Returns the offset if found, or SIZE_MAX if not found
// Each atom entry is aligned to 4-byte boundary: [len:1][chars:len][nul:1][padding]
static size_t find_atom(const char *str, size_t len)
{
    size_t offset = 0;
    while (offset < atom_next)
    {
        uint8_t atom_len = memory_block[offset];
        // Use case-sensitive comparison to preserve original case
        if (str_eq(str, len, (const char *)&memory_block[offset + 1], atom_len))
        {
            return offset;
        }
        // Advance to next aligned entry (includes null terminator)
        offset += ALIGN4(1 + atom_len + 1);
    }
    return SIZE_MAX; // Not found
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
    if (offset != SIZE_MAX)
    {
        // Found existing atom
        return NODE_MAKE_WORD(offset);
    }

    // Calculate aligned size for this entry
    // Include space for null terminator: [len:1][chars:len][nul:1][padding]
    size_t entry_size = ALIGN4(1 + len + 1);

    // Need to add new atom - check for collision with node region
    if (atom_next + entry_size > node_bottom)
    {
        return NODE_NIL; // Out of atom space - would collide with nodes
    }

    // Check offset fits in our encoding (15 bits = 32KB max)
    if (atom_next >= 0x8000)
    {
        return NODE_NIL; // Atom table too large
    }

    offset = atom_next;
    memory_block[atom_next] = (uint8_t)len;
    memcpy(&memory_block[atom_next + 1], str, len);
    memory_block[atom_next + 1 + len] = '\0';  // Null terminator
    atom_next += entry_size;

    return NODE_MAKE_WORD(offset);
}

// Intern a word while processing backslash escapes.
// Each \X sequence becomes just X in the resulting atom.
Node mem_atom_unescape(const char *str, size_t len)
{
    // First, calculate the unescaped length
    size_t unescaped_len = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (str[i] == '\\' && i + 1 < len)
        {
            // Skip the backslash, count the next character
            i++;
        }
        unescaped_len++;
    }

    // If no escapes found, just use mem_atom directly
    if (unescaped_len == len)
    {
        return mem_atom(str, len);
    }

    // Limit atom length to 255 (1 byte length prefix)
    if (unescaped_len > 255)
    {
        unescaped_len = 255;
    }

    // Build unescaped string on stack (max 255 chars)
    char buffer[256];
    size_t j = 0;
    for (size_t i = 0; i < len && j < unescaped_len; i++)
    {
        if (str[i] == '\\' && i + 1 < len)
        {
            // Skip backslash, take next character
            i++;
            buffer[j++] = str[i];
        }
        else
        {
            buffer[j++] = str[i];
        }
    }

    return mem_atom(buffer, j);
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
    if (index == 0)
    {
        return NODE_NIL;
    }

    uint32_t *cell_ptr = get_node_ptr((uint16_t)index);
    if (cell_ptr == NULL)
    {
        return NODE_NIL;
    }

    uint32_t cell = *cell_ptr;
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
    if (index == 0)
    {
        return NODE_NIL;
    }

    uint32_t *cell_ptr = get_node_ptr((uint16_t)index);
    if (cell_ptr == NULL)
    {
        return NODE_NIL;
    }

    uint32_t cell = *cell_ptr;
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
    if (index == 0)
    {
        return false;
    }

    uint32_t *cell_ptr = get_node_ptr((uint16_t)index);
    if (cell_ptr == NULL)
    {
        return false;
    }

    uint32_t cell = *cell_ptr;
    uint16_t cdr_idx = CELL_GET_CDR(cell);
    uint16_t car_idx = node_to_index(value);

    *cell_ptr = CELL_MAKE(car_idx, cdr_idx);

    return true;
}

bool mem_set_cdr(Node n, Node value)
{
    if (n == NODE_NIL || NODE_GET_TYPE(n) != NODE_TYPE_LIST)
    {
        return false;
    }

    uint32_t index = NODE_GET_INDEX(n);
    if (index == 0)
    {
        return false;
    }

    uint32_t *cell_ptr = get_node_ptr((uint16_t)index);
    if (cell_ptr == NULL)
    {
        return false;
    }

    uint32_t cell = *cell_ptr;
    uint16_t car_idx = CELL_GET_CAR(cell);
    uint16_t cdr_idx = node_to_index(value);

    *cell_ptr = CELL_MAKE(car_idx, cdr_idx);

    return true;
}

//==========================================================================
// Node Type Checks
//==========================================================================

// Check if a node is the empty list (nil).
// Returns true for both NODE_NIL (0) and empty list markers (NODE_MAKE_LIST(0)).
// The distinction between these only matters when storing in cons cells.
bool mem_is_nil(Node n)
{
    // NODE_NIL (0) is the traditional nil value
    if (n == NODE_NIL)
    {
        return true;
    }
    // Empty list marker: type is LIST but index is 0
    if (NODE_GET_TYPE(n) == NODE_TYPE_LIST && NODE_GET_INDEX(n) == 0)
    {
        return true;
    }
    return false;
}

// Check if a node is a list (cons cell).
// Note: Empty lists (NODE_MAKE_LIST(0)) return true because they ARE lists,
// but they have no elements. Use mem_is_nil to check for empty.
bool mem_is_list(Node n)
{
    // NODE_NIL is not a list (it's the absence of a list)
    if (n == NODE_NIL)
    {
        return false;
    }
    // Any node with LIST type is a list, including empty lists
    return NODE_GET_TYPE(n) == NODE_TYPE_LIST;
}

// Check if a node is a word.
bool mem_is_word(Node n)
{
    return NODE_GET_TYPE(n) == NODE_TYPE_WORD;
}

// Check if a node is the newline marker.
bool mem_is_newline(Node n)
{
    return n == mem_newline_marker;
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
    return (const char *)&memory_block[offset + 1];
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

    return memory_block[offset];
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

    uint8_t atom_len = memory_block[offset];
    return str_eq_nocase(str, len, (const char *)&memory_block[offset + 1], atom_len);
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

// Bit array for marking (1 bit per possible node)
// Size based on maximum possible nodes (total memory / 4 bytes per node)
static uint32_t gc_marks[(LOGO_MEMORY_SIZE / 4 + 31) / 32];

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
    if (index == 0)
    {
        return;
    }

    // Validate that the index is within our allocated node region
    uint32_t *cell_ptr = get_node_ptr(index);
    if (cell_ptr == NULL)
    {
        return;
    }
    
    // Check if the node is actually allocated (within node_bottom to LOGO_MEMORY_SIZE)
    size_t byte_offset = (uint8_t *)cell_ptr - memory_block;
    if (byte_offset < node_bottom || byte_offset >= LOGO_MEMORY_SIZE)
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
    uint32_t cell = *cell_ptr;
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

    // Calculate the maximum node index based on allocated region
    uint16_t max_index = (uint16_t)((LOGO_MEMORY_SIZE - node_bottom) / 4);
    
    // Sweep through all allocated nodes (skip index 0 which is reserved)
    for (uint16_t i = 1; i <= max_index; i++)
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
            uint32_t *cell_ptr = get_node_ptr(i);
            if (cell_ptr != NULL)
            {
                *cell_ptr = CELL_MAKE(0, free_list);
                free_list = i;
                free_count++;
            }
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
// Includes nodes on the free list plus space that could be allocated.
size_t mem_free_nodes(void)
{
    // Nodes on the free list
    size_t free_list_nodes = free_count;
    
    // Plus potential nodes from unallocated space
    if (node_bottom > atom_next)
    {
        size_t free_space = node_bottom - atom_next;
        size_t potential_nodes = free_space / 4;
        
        // We need to account for the fact that we can't use index 0
        // Maximum usable nodes = (LOGO_MEMORY_SIZE / 4) - 1
        // Already allocated = node_count
        // Can still allocate = max - allocated
        size_t max_nodes = (LOGO_MEMORY_SIZE / 4) - 1;
        size_t can_allocate = (node_count < max_nodes) ? (max_nodes - node_count) : 0;
        
        if (potential_nodes > can_allocate)
        {
            potential_nodes = can_allocate;
        }
        
        return free_list_nodes + potential_nodes;
    }
    
    return free_list_nodes;
}

// Get the total number of nodes (theoretical maximum).
size_t mem_total_nodes(void)
{
    // Maximum nodes that could fit in the entire memory block
    return (LOGO_MEMORY_SIZE / 4) - 1; // Exclude index 0
}

// Get the number of free bytes in the atom table.
// This represents the free space between atoms and nodes.
size_t mem_free_atoms(void)
{
    if (node_bottom > atom_next)
    {
        return node_bottom - atom_next;
    }
    return 0;
}

// Get the total size of the atom table (shared with nodes).
size_t mem_total_atoms(void)
{
    return LOGO_MEMORY_SIZE;
}
