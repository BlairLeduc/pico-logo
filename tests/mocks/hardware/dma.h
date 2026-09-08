//
//  Host-side stub for <hardware/dma.h>.  See mocks/hardware/pwm.h -- the
//  mixer under test is driven by the test, not by a DMA IRQ, so none of
//  these do anything.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DMA_SIZE_32 2

typedef struct { uint32_t ints0; } dma_hw_t;
extern dma_hw_t *const dma_hw;

typedef struct { uint32_t ctrl; } dma_channel_config;

static inline int dma_claim_unused_channel(bool required) { (void)required; return 0; }
static inline dma_channel_config dma_channel_get_default_config(int ch) { (void)ch; dma_channel_config c = {0}; return c; }
static inline void channel_config_set_transfer_data_size(dma_channel_config *c, int sz) { (void)c; (void)sz; }
static inline void channel_config_set_read_increment(dma_channel_config *c, bool on) { (void)c; (void)on; }
static inline void channel_config_set_write_increment(dma_channel_config *c, bool on) { (void)c; (void)on; }
static inline void channel_config_set_ring(dma_channel_config *c, bool write, unsigned bits) { (void)c; (void)write; (void)bits; }
static inline void channel_config_set_dreq(dma_channel_config *c, unsigned dreq) { (void)c; (void)dreq; }
static inline void channel_config_set_chain_to(dma_channel_config *c, int ch) { (void)c; (void)ch; }
static inline void dma_channel_configure(int ch, dma_channel_config *c, volatile void *w, const volatile void *r, unsigned n, bool go) { (void)ch; (void)c; (void)w; (void)r; (void)n; (void)go; }
static inline void dma_channel_set_irq0_enabled(int ch, bool on) { (void)ch; (void)on; }
static inline void dma_channel_set_read_addr(int ch, const volatile void *a, bool go) { (void)ch; (void)a; (void)go; }
static inline void dma_channel_set_trans_count(int ch, unsigned n, bool go) { (void)ch; (void)n; (void)go; }
static inline void dma_channel_start(int ch) { (void)ch; }
