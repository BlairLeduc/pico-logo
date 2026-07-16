//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Centralised compile-time limits for the interpreter.
//
//  These were previously scattered as bare `#define`s near the top of
//  individual `.c` files (procedures.c, variables.c) and one in
//  procedures.h. Consolidating them here makes it possible to reason
//  about static memory footprint in one place and gives every limit a
//  documented rationale and overflow story.
//
//  All limits here are FIXED-CAPACITY arrays sized at compile time.
//  Pico-class targets (RP2040 / RP2350) have ~264 KB of SRAM and we
//  prefer a predictable static layout over a dynamically grown one.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of user-defined procedures (slots in `procedures[]`).
//
// OVERFLOW: `proc_define` returns `false` when no slot is available;
// callers (`prim_define`, `proc_define_from_text`, the REPL `to ... end`
// path) surface this as `ERR_OUT_OF_SPACE` via `error_context`.
#define MAX_PROCEDURES 128

// Maximum parameters per procedure body. Bounds the per-procedure
// `params[]` array and the per-call argument-collection arrays in
// `eval_expr.c` and the list-processing primitives.
//
// OVERFLOW: `proc_define_from_text` rejects definitions with too many
// parameters; primitives like `apply` / `map` / `crossmap` reject
// lambdas whose `param_count > MAX_PROC_PARAMS` with
// `ERR_TOO_MANY_INPUTS` (see P5b-009 / P4-011).
#define MAX_PROC_PARAMS 16

// Maximum global variable slots.
//
// OVERFLOW: `var_set` returns `false` when the table is full; callers
// surface this as `ERR_OUT_OF_SPACE`.
#define MAX_GLOBAL_VARIABLES 128

// Maximum depth of the "currently executing procedure" name stack used
// for the pause prompt and trace output. This is independent of the
// frame stack (which is sized in bytes by `FRAME_STACK_SIZE`); it only
// limits how deep the *display* stack can grow.
//
// OVERFLOW: `proc_push_current` silently drops names beyond the limit
// (the frame stack still grows correctly; only the prompt label is
// affected).
#define MAX_CURRENT_PROC_DEPTH 32

// Number of turtles (sprites). All eight are full turtles with pens;
// turtle 0 boots visible as the classic single turtle, 1-7 boot hidden
// at home. Z-order in the compositor: lower number on top. Kept modest
// so worst-case compositor and collision costs stay flat; the design
// doc (docs/multi-sprite-design.md §10) shows 16 is a one-line change.
//
// OVERFLOW: `tell` / `ask` reject turtle numbers >= MAX_TURTLES with
// ERR_DOESNT_LIKE_INPUT.
#define MAX_TURTLES 8

// Maximum number of armed `when` demons. Each demon holds two node
// references (a condition expression and an action list) plus a couple of
// flag bytes, so the table costs ~100 B — see docs/multi-sprite-design.md
// §10. Kept small because demons are polled on a time budget and each poll
// evaluates every armed condition.
//
// OVERFLOW: `when` returns ERR_OUT_OF_SPACE when the table is full and the
// condition does not match an already-armed demon.
#define MAX_DEMONS 8

// Minimum wall-clock gap, in milliseconds, between two demon polls. The
// poll point sits at the top of every instruction; without a budget a
// tight loop would re-evaluate every condition on every step. ~20 ms (≈50
// polls/second) keeps demons responsive while leaving tight loops untaxed.
#define DEMON_POLL_MS 20

// Maximum size, in bytes, of an HTTP request or response body for `http.get` /
// `http.post`. The effective cap is chosen at runtime by the active transfer
// buffer (see core/primitives_http.c):
//   - HTTP_MAX_BODY (SRAM fallback): used when no aux/PSRAM region is present.
//     Kept small because the RP2350's SRAM is largely consumed by the Logo
//     arena, the LCD frame buffer, and the operand stack.
//   - HTTP_MAX_BODY_PSRAM: used when an aux region backs the transfer buffer,
//     so the body (returned as a blob word) can be far larger.
//
// OVERFLOW: a response whose declared `Content-Length` (or decoded chunked
// length) exceeds the active limit produces `ERR_FILE_TOO_BIG` rather than a
// truncated word.
#define HTTP_MAX_BODY 2048
#define HTTP_MAX_BODY_PSRAM (512 * 1024)

// Maximum size, in bytes, of the HTTP response header block (status line plus
// header lines) that `http.get` / `http.post` will buffer and parse.
//
// OVERFLOW: headers exceeding this limit produce `ERR_NETWORK_ERROR`.
#define HTTP_MAX_HEADERS 1024

// Size, in bytes, of the stack buffer `write` formats its text into before
// drawing it on the graphics screen at the turtle. The argument is formatted
// like `print` (lists lose their outer brackets), so the longest text drawn
// is WRITE_MAX_LEN - 1 bytes (one byte reserved for the NUL terminator).
//
// OVERFLOW: text longer than WRITE_MAX_LEN - 1 is silently truncated.
#define WRITE_MAX_LEN 256

// Maximum length, in characters, of the device network hostname set by
// `sethostname` (the name answered over mDNS as `<hostname>.local` and given to
// DHCP). Excludes the `.local` suffix, which mDNS appends. 32 is comfortably
// within the 63-character DNS label limit.
//
// OVERFLOW: `sethostname` rejects a name longer than HOSTNAME_MAX (or one that
// is empty or contains anything but letters, digits, and interior hyphens) with
// ERR_DOESNT_LIKE_INPUT.
#define HOSTNAME_MAX 32

#ifdef __cplusplus
}
#endif
