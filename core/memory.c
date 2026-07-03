//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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

#include <assert.h>
#include <ctype.h>
#include <string.h>

// Compile-time invariant:
//   The high bit of a 16-bit cell index is reserved as the "word" marker
//   (0x8000) and 0x7FFF is reserved as the empty-list marker. List pool
//   indices must therefore fall in [1, 0x7FFE]. With 4-byte cells, the
//   maximum addressable pool size is 0x7FFE * 4 bytes; the rest of the
//   memory block is reserved for the atom table.
_Static_assert(LOGO_MEMORY_SIZE % 4 == 0,
    "LOGO_MEMORY_SIZE must be a multiple of cell size (4 bytes)");

//==========================================================================
// Cell encoding constants
//==========================================================================
//
// Each cons cell stores two 16-bit references (car and cdr). A reference
// encodes either a list index, a word reference, NIL, or the empty list.
// The encoding uses the upper bits of the 16-bit word as type tags:
//
//   reference == 0x0000             -> NODE_NIL (no reference)
//   reference == CELL_EMPTY_LIST    -> the empty list ([])
//   (reference & CELL_WORD_MARKER)  -> word reference; low 15 bits are the
//                                      atom offset within the atom table
//   otherwise                       -> list pool index in [1, MAX_LIST_INDEX]
//
// These markers must be kept in sync with mem_atom() (which caps atom
// offsets at LOGO_ATOM_LIMIT) and alloc_cell() (which caps pool indices
// at MAX_LIST_INDEX).

// High bit set => the reference points to an interned word (atom).
#define CELL_WORD_MARKER  0x8000u

// Mask for extracting the 15-bit atom offset from a word reference.
#define CELL_WORD_MASK    0x7FFFu

// Reserved sentinel meaning "the empty list" (distinct from NIL).
// Equal to CELL_WORD_MASK so we get one free bit pattern for the marker.
#define CELL_EMPTY_LIST   0x7FFFu

// Maximum valid pool index for a list reference (avoids the empty-list
// sentinel and the word-reference marker bit).
#define MAX_LIST_INDEX    0x7FFEu

// Upper bound on the atom table size in bytes. Atom offsets must fit in
// the 15-bit CELL_WORD_MASK so that a word reference fits in a 16-bit cell.
#define LOGO_ATOM_LIMIT   ((size_t)CELL_WORD_MASK + 1u)  // 0x8000 = 32768

_Static_assert(CELL_EMPTY_LIST == CELL_WORD_MASK,
    "CELL_EMPTY_LIST must equal CELL_WORD_MASK so encodings remain disjoint");
_Static_assert(MAX_LIST_INDEX < CELL_EMPTY_LIST,
    "MAX_LIST_INDEX must leave room for the empty-list sentinel");
_Static_assert(LOGO_ATOM_LIMIT <= LOGO_MEMORY_SIZE,
    "Atom table cannot exceed the total memory pool");

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

// Heads of the per-bucket atom hash chains (atom offsets; ATOM_CHAIN_END =
// empty). See the Atom Table section for the entry layout and rationale.
#define ATOM_BUCKET_COUNT 256
#define ATOM_CHAIN_END 0xFFFFu
static uint16_t atom_buckets[ATOM_BUCKET_COUNT];

//==========================================================================
// Blob Heap (auxiliary region, e.g. PSRAM)
//==========================================================================
//
// Large values that do not fit the 255-byte interned-atom limit live here.
// Blobs are NOT interned, ARE garbage-collected, and are referenced by a
// handle (descriptor index) encoded in a NODE_TYPE_WORD with NODE_WORD_BLOB_BIT
// set. Because a blob cannot be packed into a 16-bit cell, blob words are never
// stored inside cons cells (mem_cons rejects them) and are therefore only
// reachable through the GC root set, never through cell walks.
//
// The region is managed by a simple first-fit free-list allocator with
// boundary coalescing. It is non-moving, so pointers from mem_word_ptr remain
// valid across GC (consistent with the documented contract).

#define BLOB_ALIGN 8u
#define BLOB_ALIGN_UP(x) (((x) + (BLOB_ALIGN - 1)) & ~((size_t)BLOB_ALIGN - 1))

// A free block in the region. Allocated blocks share the same leading `size`
// field; only free blocks use `next`. `size` is the total block size in bytes
// including this header. The header is padded to BLOB_ALIGN so payloads align.
typedef struct FreeBlock
{
    size_t size;
    struct FreeBlock *next; // address-ascending free list (free blocks only)
} FreeBlock;

#define BLOB_HDR BLOB_ALIGN_UP(sizeof(FreeBlock))

typedef struct
{
    void *ptr;     // pointer to payload within the region (NULL = free slot)
    uint32_t len;  // payload length in bytes (excludes the NUL terminator)
} BlobDesc;

static BlobDesc blob_table[LOGO_MAX_BLOBS];
static uint8_t blob_mark[(LOGO_MAX_BLOBS + 7) / 8];

static uint8_t *blob_region;       // base of the aux region, NULL if none
static size_t blob_region_size;    // total bytes of the aux region
static FreeBlock *blob_free_list;  // head of the free list

// Reset the blob subsystem (called from logo_mem_init and set_aux_region).
static void blob_reset(void)
{
    memset(blob_table, 0, sizeof(blob_table));
    memset(blob_mark, 0, sizeof(blob_mark));
    blob_free_list = NULL;
    if (blob_region == NULL)
    {
        return;
    }

    // Align the start up and the size down so every block header is aligned
    // and all block sizes stay multiples of BLOB_ALIGN.
    uintptr_t base = (uintptr_t)blob_region;
    uintptr_t aligned = BLOB_ALIGN_UP(base);
    size_t adjust = (size_t)(aligned - base);
    if (blob_region_size <= adjust)
    {
        return;
    }
    size_t usable = (blob_region_size - adjust) & ~((size_t)BLOB_ALIGN - 1);
    if (usable >= BLOB_HDR + BLOB_ALIGN)
    {
        FreeBlock *first = (FreeBlock *)aligned;
        first->size = usable;
        first->next = NULL;
        blob_free_list = first;
    }
}

// First-fit allocation of `len` payload bytes from the region.
// Returns a payload pointer, or NULL if no region / out of space.
static void *blob_alloc(size_t len)
{
    if (blob_region == NULL)
    {
        return NULL;
    }

    size_t need = BLOB_ALIGN_UP(BLOB_HDR + len);
    if (need < BLOB_HDR + BLOB_ALIGN)
    {
        need = BLOB_HDR + BLOB_ALIGN; // ensure a split remainder can hold a header
    }

    FreeBlock *prev = NULL;
    FreeBlock *cur = blob_free_list;
    while (cur != NULL)
    {
        if (cur->size >= need)
        {
            size_t remainder = cur->size - need;
            if (remainder >= BLOB_HDR + BLOB_ALIGN)
            {
                // Split: shrink cur to `need`, create a free block after it.
                FreeBlock *rest = (FreeBlock *)((uint8_t *)cur + need);
                rest->size = remainder;
                rest->next = cur->next;
                cur->size = need;
                if (prev != NULL)
                    prev->next = rest;
                else
                    blob_free_list = rest;
            }
            else
            {
                // Take the whole block.
                if (prev != NULL)
                    prev->next = cur->next;
                else
                    blob_free_list = cur->next;
            }
            return (uint8_t *)cur + BLOB_HDR;
        }
        prev = cur;
        cur = cur->next;
    }
    return NULL;
}

// Return a previously allocated payload to the free list, coalescing with
// physically adjacent free blocks.
static void blob_free(void *payload)
{
    if (payload == NULL)
    {
        return;
    }

    FreeBlock *block = (FreeBlock *)((uint8_t *)payload - BLOB_HDR);

    // Insert into the address-ascending free list.
    FreeBlock *prev = NULL;
    FreeBlock *cur = blob_free_list;
    while (cur != NULL && cur < block)
    {
        prev = cur;
        cur = cur->next;
    }
    block->next = cur;
    if (prev != NULL)
        prev->next = block;
    else
        blob_free_list = block;

    // Coalesce with the next block if physically adjacent.
    if (cur != NULL && (uint8_t *)block + block->size == (uint8_t *)cur)
    {
        block->size += cur->size;
        block->next = cur->next;
    }
    // Coalesce with the previous block if physically adjacent.
    if (prev != NULL && (uint8_t *)prev + prev->size == (uint8_t *)block)
    {
        prev->size += block->size;
        prev->next = block->next;
    }
}

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

    if (index > MAX_LIST_INDEX)
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

// Would allocating `extra` more bytes from one allocator overlap the other?
// The atom table grows upward from offset 0; the node pool grows downward
// from LOGO_MEMORY_SIZE. Both allocators share this single memory block, so
// they collide when atom_next + extra exceeds node_bottom.
static inline bool mem_would_collide(size_t extra)
{
    return atom_next + extra > node_bottom;
}

//==========================================================================
// Initialization
//==========================================================================

// Newline marker.
//
// This is an interned atom (initialised once in `logo_mem_init`) used as a
// sentinel inside list bodies returned by `text` and friends to preserve
// source-line boundaries. Because it lives in the atom region and not the
// cell region, the sweep phase cannot reclaim it; it is effectively a
// permanent GC root. `mem_gc()` marks it unconditionally so callers do not
// need to remember to add it to their root set.
Node mem_newline_marker = NODE_NIL;

// Cached boolean atoms.
//
// Every predicate and comparison produces the word `true` or `false`;
// interning them once at init lets those hot paths reuse the Node instead
// of re-hashing the string per call. Like all atoms they are never swept,
// so no GC rooting is needed.
Node mem_true_node = NODE_NIL;
Node mem_false_node = NODE_NIL;

// Initialize the memory system.
// Must be called before any other memory functions.
void logo_mem_init(void)
{
    // Initialize atom table (grows upward from 0)
    atom_next = 0;
    memset(memory_block, 0, LOGO_MEMORY_SIZE);
    memset(atom_buckets, 0xFF, sizeof(atom_buckets)); // all chains empty

    // Initialize node region (grows downward from top)
    // Start with no nodes allocated
    node_bottom = LOGO_MEMORY_SIZE;
    node_count = 0;
    
    // Initialize free list as empty
    free_list = 0;
    free_count = 0;

    // Drop any blob region: a fresh interpreter has no PSRAM until the device
    // (or a test) supplies one via logo_mem_set_aux_region().
    blob_region = NULL;
    blob_region_size = 0;
    blob_reset();

    // Create the newline marker atom (SOH character, non-printable)
    mem_newline_marker = mem_atom("\x01", 1);

    // Intern the boolean words once for the predicate/comparison hot paths
    mem_true_node = mem_atom("true", 4);
    mem_false_node = mem_atom("false", 5);
}

// Provide an auxiliary memory region (e.g. PSRAM) to back the blob heap.
void logo_mem_set_aux_region(void *base, size_t size)
{
    blob_region = (uint8_t *)base;
    blob_region_size = (base != NULL) ? size : 0;
    blob_reset();
}

// Permanent raw allocation from the aux region (never freed, never GC'd).
// Shares the blob free-list allocator; the block simply has no descriptor and
// is never returned, so the sweep never touches it.
void *mem_region_alloc(size_t size)
{
    return blob_alloc(size);
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
    if (node_bottom < 4 || mem_would_collide(0) || (node_bottom - 4) < atom_next)
    {
        return 0; // Out of memory - would collide with atom table
    }
    
    // Allocate new node at the bottom of the node region
    node_bottom -= 4;
    node_count++;
    
    // Calculate the index for this new node
    uint16_t index = (uint16_t)((LOGO_MEMORY_SIZE - node_bottom) / 4);
    
    // Reject indices that would collide with the empty-list (0x7FFF) or
    // word-reference (0x8000+) marker bits used inside cell storage.
    if (index == 0 || index > MAX_LIST_INDEX)
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
// Words use the high bit (CELL_WORD_MARKER) plus the atom offset.
// Lists use the pool index directly.
// Empty list (NODE_MAKE_LIST(0)) uses the special CELL_EMPTY_LIST sentinel.

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
            return CELL_EMPTY_LIST;
        }
        return (uint16_t)index;
    }
    else if (type == NODE_TYPE_WORD)
    {
        // Word - encode with high bit set
        uint32_t offset = NODE_GET_INDEX(n);
        if (offset < LOGO_ATOM_LIMIT)
        {
            return (uint16_t)(CELL_WORD_MARKER | offset);
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
    if (index == CELL_EMPTY_LIST)
    {
        return NODE_MAKE_LIST(0);
    }

    // Check if this is a word reference (high bit set)
    if (index & CELL_WORD_MARKER)
    {
        uint32_t offset = index & CELL_WORD_MASK;
        return NODE_MAKE_WORD(offset);
    }

    // It's a list reference
    return NODE_MAKE_LIST(index);
}

//==========================================================================
// Cons Cells (List Nodes)
//==========================================================================

// Create a cons cell (list node) with car and cdr.
// Returns NODE_NIL if out of memory or if either operand cannot be encoded
// in 16 bits (e.g. an atom whose offset is >= LOGO_ATOM_LIMIT).
Node mem_cons(Node car, Node cdr)
{
    // Encode operands first so allocation isn't wasted on an unencodable cell.
    // node_to_index returns 0 for NIL *and* for out-of-range references; we
    // must distinguish these cases to avoid silently corrupting the cell.
    uint16_t car_idx = node_to_index(car);
    uint16_t cdr_idx = node_to_index(cdr);
    if ((car != NODE_NIL && car_idx == 0) || (cdr != NODE_NIL && cdr_idx == 0))
    {
        return NODE_NIL; // Operand reference would be lost in 16-bit cell encoding
    }

    uint16_t index = alloc_cell();
    if (index == 0)
    {
        return NODE_NIL; // Out of memory
    }

    uint32_t *cell_ptr = get_node_ptr(index);
    if (cell_ptr == NULL)
    {
        return NODE_NIL; // Invalid index
    }
    
    *cell_ptr = CELL_MAKE(car_idx, cdr_idx);

    return NODE_MAKE_LIST(index);
}

// Append `item` to a list under construction (head/tail pointer pattern).
// Returns false when the node pool is exhausted so callers can surface
// ERR_OUT_OF_SPACE instead of silently truncating the result.
bool mem_list_append(Node *head, Node *tail, Node item)
{
    Node new_cell = mem_cons(item, NODE_NIL);
    if (mem_is_nil(new_cell))
    {
        return false;
    }
    if (mem_is_nil(*head))
    {
        *head = new_cell;
    }
    else
    {
        mem_set_cdr(*tail, new_cell);
    }
    *tail = new_cell;
    return true;
}

//==========================================================================
// Atom Table (Interned Words)
//==========================================================================
//
// Each atom entry is aligned to a 4-byte boundary and laid out as:
//     [next:2][len:1][chars:len][nul:1][padding]
// `next` chains entries whose names hash to the same bucket (0xFFFF ends
// the chain), so interning is O(chain length) instead of a linear scan of
// the whole table — mem_atom runs for every quoted word, list element,
// and character extraction, making it a hot path. The hash is
// case-SENSITIVE to match the interner's exact-match semantics (case
// variants intern as distinct atoms; see find_atom).

// FNV-1a over the atom's bytes, folded to a bucket index.
static uint8_t atom_hash(const char *str, size_t len)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        h ^= (uint8_t)str[i];
        h *= 16777619u;
    }
    return (uint8_t)(h ^ (h >> 8) ^ (h >> 16) ^ (h >> 24));
}

// Read/write the 2-byte chain link at the start of an atom entry.
static uint16_t atom_entry_next(size_t offset)
{
    uint16_t next;
    memcpy(&next, &memory_block[offset], sizeof(next));
    return next;
}

static void atom_entry_set_next(size_t offset, uint16_t next)
{
    memcpy(&memory_block[offset], &next, sizeof(next));
}

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
static size_t find_atom(const char *str, size_t len)
{
    uint16_t offset = atom_buckets[atom_hash(str, len)];
    while (offset != ATOM_CHAIN_END)
    {
        uint8_t atom_len = memory_block[offset + 2];
        // Use case-sensitive comparison to preserve original case
        if (str_eq(str, len, (const char *)&memory_block[offset + 3], atom_len))
        {
            return offset;
        }
        offset = atom_entry_next(offset);
    }
    return SIZE_MAX; // Not found
}

// Intern a word in the atom table
Node mem_atom(const char *str, size_t len)
{
    // Atom length is capped at 255 by the 1-byte length prefix. Refuse rather
    // than silently truncate, so callers can surface an error.
    if (len > 255)
    {
        return NODE_NIL;
    }

    // Check if atom already exists
    size_t offset = find_atom(str, len);
    if (offset != SIZE_MAX)
    {
        // Found existing atom
        return NODE_MAKE_WORD(offset);
    }

    // Calculate aligned size for this entry:
    // [next:2][len:1][chars:len][nul:1][padding]
    size_t entry_size = ALIGN4(2 + 1 + len + 1);

    // Need to add new atom - check for collision with node region
    if (mem_would_collide(entry_size))
    {
        return NODE_NIL; // Out of atom space - would collide with nodes
    }

    // Check offset fits in our encoding (15 bits = 32KB max)
    if (atom_next >= LOGO_ATOM_LIMIT)
    {
        return NODE_NIL; // Atom table too large
    }

    offset = atom_next;
    uint8_t bucket = atom_hash(str, len);
    atom_entry_set_next(offset, atom_buckets[bucket]);
    memory_block[offset + 2] = (uint8_t)len;
    memcpy(&memory_block[offset + 3], str, len);
    memory_block[offset + 3 + len] = '\0';  // Null terminator
    atom_buckets[bucket] = (uint16_t)offset;
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

    // Atom length is capped at 255 by the 1-byte length prefix. Refuse rather
    // than silently truncate, so callers can surface an error.
    if (unescaped_len > 255)
    {
        return NODE_NIL;
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
// Blobs (large values in the auxiliary region)
//==========================================================================

// Allocate a large value in the blob heap and return it as a word node.
Node mem_blob(const char *str, size_t len)
{
    if (blob_region == NULL)
    {
        return NODE_NIL; // No aux region: large values are unavailable
    }

    // Find a free descriptor slot.
    int handle = -1;
    for (int i = 0; i < LOGO_MAX_BLOBS; i++)
    {
        if (blob_table[i].ptr == NULL)
        {
            handle = i;
            break;
        }
    }
    if (handle < 0)
    {
        return NODE_NIL; // Descriptor table full
    }

    // Allocate payload plus a NUL terminator so mem_word_ptr stays C-string safe.
    void *p = blob_alloc(len + 1);
    if (p == NULL)
    {
        return NODE_NIL; // Region out of space
    }

    memcpy(p, str, len);
    ((char *)p)[len] = '\0';
    blob_table[handle].ptr = p;
    blob_table[handle].len = (uint32_t)len;

    return NODE_MAKE_BLOB((uint32_t)handle);
}

// Create a word of any length: atom if it fits the interned limit, else a blob.
Node mem_word(const char *str, size_t len)
{
    if (len <= 255)
    {
        return mem_atom(str, len);
    }
    return mem_blob(str, len);
}

// Resolve a blob node to its descriptor, or NULL if invalid.
static BlobDesc *blob_desc(Node n)
{
    if (!NODE_WORD_IS_BLOB(n))
    {
        return NULL;
    }
    uint32_t handle = NODE_GET_BLOB_HANDLE(n);
    if (handle >= LOGO_MAX_BLOBS || blob_table[handle].ptr == NULL)
    {
        return NULL;
    }
    return &blob_table[handle];
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

// Check if a node is a blob (a large word held in the PSRAM blob heap).
bool mem_is_blob(Node n)
{
    return NODE_WORD_IS_BLOB(n);
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

    if (NODE_WORD_IS_BLOB(n))
    {
        BlobDesc *d = blob_desc(n);
        return d ? (const char *)d->ptr : NULL;
    }

    uint32_t offset = NODE_GET_INDEX(n);
    if (offset >= atom_next)
    {
        return NULL;
    }

    // Return pointer to the string (after chain link and length byte)
    return (const char *)&memory_block[offset + 3];
}

// Get the length of a word node's string.
// Returns 0 if node is not a word.
size_t mem_word_len(Node n)
{
    if (NODE_GET_TYPE(n) != NODE_TYPE_WORD)
    {
        return 0;
    }

    if (NODE_WORD_IS_BLOB(n))
    {
        BlobDesc *d = blob_desc(n);
        return d ? d->len : 0;
    }

    uint32_t offset = NODE_GET_INDEX(n);
    if (offset >= atom_next)
    {
        return 0;
    }

    return memory_block[offset + 2];
}

// Compare a word node to a given string (case-insensitive).
// Returns true if they match.
bool mem_word_eq(Node n, const char *str, size_t len)
{
    if (NODE_GET_TYPE(n) != NODE_TYPE_WORD)
    {
        return false;
    }

    const char *p = mem_word_ptr(n);
    if (p == NULL)
    {
        return false;
    }
    return str_eq_nocase(str, len, p, mem_word_len(n));
}

// Compare two word nodes for equality.
// Returns true if they are the same word.
bool mem_words_equal(Node a, Node b)
{
    if (NODE_GET_TYPE(a) != NODE_TYPE_WORD || NODE_GET_TYPE(b) != NODE_TYPE_WORD)
    {
        return false;
    }

    // Word equality is case-INSENSITIVE, matching classic Logo `equal?`
    // semantics (interning stays case-sensitive so words print in the case
    // they were typed). Same-offset atoms are trivially equal; otherwise
    // (case-variant atoms, or blobs, which are not interned) compare by
    // content, ignoring case.
    if (!NODE_WORD_IS_BLOB(a) && !NODE_WORD_IS_BLOB(b) &&
        NODE_GET_INDEX(a) == NODE_GET_INDEX(b))
    {
        return true;
    }

    const char *pa = mem_word_ptr(a);
    const char *pb = mem_word_ptr(b);
    if (pa == NULL || pb == NULL)
    {
        return false;
    }
    return str_eq_nocase(pa, mem_word_len(a), pb, mem_word_len(b));
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

    if (type == NODE_TYPE_WORD)
    {
        // Atoms are never freed, so they need no marking. Blobs ARE collected:
        // mark the reachable descriptor so the sweep keeps it.
        if (NODE_WORD_IS_BLOB(n))
        {
            uint32_t handle = NODE_GET_BLOB_HANDLE(n);
            if (handle < LOGO_MAX_BLOBS && blob_table[handle].ptr != NULL)
            {
                blob_mark[handle / 8] |= (uint8_t)(1u << (handle % 8));
            }
        }
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
    // Iteratively follow cdr chains to avoid stack overflow on long lists.
    // Only car branches recurse (bounded by list nesting depth, not length).
    while (index != 0)
    {
        // Reject sentinel/word-tagged values that should never reach the GC
        // marker — they indicate either a corrupted reference or a caller
        // that forgot to strip the type bits before calling us.
        if (index > MAX_LIST_INDEX)
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

        // Get car and cdr
        uint32_t cell = *cell_ptr;
        uint16_t car_idx = CELL_GET_CAR(cell);
        uint16_t cdr_idx = CELL_GET_CDR(cell);

        // Mark car recursively (only for nested lists, not words)
        if (car_idx != 0 && !(car_idx & CELL_WORD_MARKER))
        {
            gc_mark_index(car_idx);
        }

        // Follow cdr iteratively (tail-call elimination)
        if (cdr_idx == 0 || (cdr_idx & CELL_WORD_MARKER))
        {
            return;
        }
        index = cdr_idx;
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

    // Sweep the blob heap: free any descriptor not reached during marking.
    for (int i = 0; i < LOGO_MAX_BLOBS; i++)
    {
        if (blob_table[i].ptr == NULL)
        {
            continue; // free slot
        }
        if (blob_mark[i / 8] & (uint8_t)(1u << (i % 8)))
        {
            continue; // reachable, keep
        }
        blob_free(blob_table[i].ptr);
        blob_table[i].ptr = NULL;
        blob_table[i].len = 0;
    }

    // Clear any remaining marks (nodes and blobs)
    memset(gc_marks, 0, sizeof(gc_marks));
    memset(blob_mark, 0, sizeof(blob_mark));
}

// Run garbage collection.
// Caller must mark all roots first with mem_gc_mark().
void mem_gc(Node *roots, size_t num_roots)
{
    // Clear mark bits
    memset(gc_marks, 0, sizeof(gc_marks));

    // Always mark the newline sentinel — callers must never have to
    // remember it, and forgetting would corrupt list outputs from `text`.
    mem_gc_mark(mem_newline_marker);

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

// Get the number of blob descriptors currently in use.
size_t mem_blob_used(void)
{
    size_t count = 0;
    for (int i = 0; i < LOGO_MAX_BLOBS; i++)
    {
        if (blob_table[i].ptr != NULL)
        {
            count++;
        }
    }
    return count;
}

// Get the number of free bytes remaining in the blob (aux) region.
size_t mem_blob_free_bytes(void)
{
    size_t total = 0;
    for (FreeBlock *b = blob_free_list; b != NULL; b = b->next)
    {
        total += b->size;
    }
    return total;
}
