//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Fixed-region heap allocator (see tls_heap.h). This is the classic K&R 8.7
//  storage allocator with its sbrk-based "morecore" replaced by a single static
//  region supplied at init time. Free blocks form an address-ordered circular
//  list so free() can coalesce with both neighbours.
//

#include "tls_heap.h"
#include <string.h>

typedef union tls_header tls_header_t;
union tls_header
{
    struct
    {
        tls_header_t *next; // next free block (circular list)
        size_t size;        // size of this block in header-sized units
    } s;
    max_align_t align; // force worst-case alignment of the payload
};

static tls_header_t base;       // empty-list sentinel (zero-size block)
static tls_header_t *freep;     // rotating start of the free list
static int initialized = 0;

void tls_heap_init(void *region, size_t bytes)
{
    size_t units = bytes / sizeof(tls_header_t);
    if (region == NULL || units < 2)
    {
        freep = NULL;
        initialized = 0;
        return;
    }

    // One initial free block spanning the whole region (header included).
    tls_header_t *p = (tls_header_t *)region;
    p->s.size = units;

    base.s.next = p;
    base.s.size = 0;
    p->s.next = &base;
    freep = &base;
    initialized = 1;
}

void *tls_heap_malloc(size_t nbytes)
{
    if (!initialized || nbytes == 0)
    {
        return NULL;
    }

    // Reject sizes so large that rounding up to whole units would overflow
    // size_t (which could otherwise wrap to a tiny allocation).
    if (nbytes > (size_t)-1 - sizeof(tls_header_t))
    {
        return NULL;
    }

    // Units needed: payload rounded up, plus one unit for the block header.
    size_t nunits = (nbytes + sizeof(tls_header_t) - 1) / sizeof(tls_header_t) + 1;

    tls_header_t *prevp = freep;
    for (tls_header_t *p = prevp->s.next;; prevp = p, p = p->s.next)
    {
        if (p->s.size >= nunits)
        {
            if (p->s.size == nunits)
            {
                prevp->s.next = p->s.next; // exact fit: unlink
            }
            else
            {
                p->s.size -= nunits; // split: carve the tail
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)(p + 1);
        }
        if (p == freep)
        {
            return NULL; // wrapped the whole list without a fit
        }
    }
}

void tls_heap_free(void *ap)
{
    if (ap == NULL || !initialized)
    {
        return; // nothing to free / no free list to walk
    }

    tls_header_t *bp = (tls_header_t *)ap - 1; // block header

    // Find the free block just before bp in address order (the list is
    // address-ordered and circular, with the sentinel bridging the wrap).
    tls_header_t *p = freep;
    while (!(bp > p && bp < p->s.next))
    {
        if (p >= p->s.next && (bp > p || bp < p->s.next))
        {
            break; // bp is before the lowest or after the highest block
        }
        p = p->s.next;
    }

    // Coalesce with the upper neighbour.
    if (bp + bp->s.size == p->s.next)
    {
        bp->s.size += p->s.next->s.size;
        bp->s.next = p->s.next->s.next;
    }
    else
    {
        bp->s.next = p->s.next;
    }

    // Coalesce with the lower neighbour.
    if (p + p->s.size == bp)
    {
        p->s.size += bp->s.size;
        p->s.next = bp->s.next;
    }
    else
    {
        p->s.next = bp;
    }

    freep = p;
}

void *tls_heap_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size)
    {
        return NULL; // multiplication overflow
    }
    void *p = tls_heap_malloc(total);
    if (p != NULL)
    {
        memset(p, 0, total);
    }
    return p;
}
