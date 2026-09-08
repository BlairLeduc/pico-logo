//
//  Host-side stub for <hardware/pwm.h>.  Used only by host unit tests that
//  compile devices/picocalc/sound.c against the host toolchain; the mixer
//  under test never touches the PWM slice, so every entry point here is a
//  no-op that exists to satisfy the compiler.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t cc; } pwm_slice_hw_t;
typedef struct { pwm_slice_hw_t slice[8]; } pwm_hw_t;
extern pwm_hw_t *const pwm_hw;

typedef struct { uint32_t top; } pwm_config;

static inline pwm_config pwm_get_default_config(void) { pwm_config c = {0}; return c; }
static inline void pwm_config_set_wrap(pwm_config *c, uint16_t wrap) { if (c) c->top = wrap; }
static inline void pwm_init(unsigned slice, pwm_config *c, bool start) { (void)slice; (void)c; (void)start; }
static inline void pwm_set_both_levels(unsigned slice, uint16_t a, uint16_t b) { (void)slice; (void)a; (void)b; }
static inline void pwm_set_enabled(unsigned slice, bool on) { (void)slice; (void)on; }
static inline unsigned pwm_gpio_to_slice_num(unsigned gpio) { (void)gpio; return 0; }
static inline unsigned pwm_get_dreq(unsigned slice) { (void)slice; return 0; }
