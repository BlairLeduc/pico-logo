//
//  Host-side stub for <hardware/irq.h>.  See mocks/hardware/pwm.h.
//
#pragma once

#include <stdbool.h>

#define DMA_IRQ_0 0
#define PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY 0

static inline void irq_add_shared_handler(unsigned num, void (*h)(void), unsigned order) { (void)num; (void)h; (void)order; }
static inline void irq_set_priority(unsigned num, unsigned pri) { (void)num; (void)pri; }
static inline void irq_set_enabled(unsigned num, bool on) { (void)num; (void)on; }
