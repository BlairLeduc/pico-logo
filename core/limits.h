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

// Maximum pen diameter, in pixels, for `setpensize`. The pen draws by
// stamping a filled disc of this diameter at each line step, so the value is
// a draw-time cap only (no persistent buffer). `setpensize` rounds its input
// to an integer and clamps it to [1, MAX_PEN_SIZE].
#define MAX_PEN_SIZE 32

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

// Assumed console width, in characters, for `cat`'s multi-column listing. The
// primary interactive device is the PicoCalc (320 px / 8 px glyph = 40 columns,
// matching SCREEN_COLUMNS); the core is device-independent and has no live
// terminal width, so `cat` packs names into columns against this fixed width.
//
// OVERFLOW: names wider than the column budget simply list one per line.
#define CATALOG_DISPLAY_WIDTH 40

// Maximum length, in characters, of the device network hostname set by
// `sethostname` (the name answered over mDNS as `<hostname>.local` and given to
// DHCP). Excludes the `.local` suffix, which mDNS appends. 32 is comfortably
// within the 63-character DNS label limit.
//
// OVERFLOW: `wifi.sethostname` rejects a name longer than HOSTNAME_MAX (or one
// that is empty or contains anything but letters, digits, and interior hyphens)
// with ERR_DOESNT_LIKE_INPUT.
#define HOSTNAME_MAX 32

// HTTP server (core/httpd.c) buffer caps. The pump keeps one lazily-allocated
// request buffer, chosen at runtime like the client's transfer buffer:
//   - HTTPD_MAX_BODY (SRAM fallback): body cap when no aux/PSRAM region backs
//     the buffer (e.g. the Pico 2 W).
//   - HTTPD_MAX_BODY_PSRAM: body cap when an aux region is available.
//   - HTTPD_MAX_HEADERS: cap on the request header block (request line plus
//     header lines, up to the blank line).
//
// OVERFLOW: a header block over HTTPD_MAX_HEADERS auto-responds `431`. A body
// whose declared Content-Length exceeds the active body cap is not an error: the
// request fires with the body left unread, `http.body` errors, and
// `http.savebody` streams the bytes straight to a file (M5).
#define HTTPD_MAX_HEADERS 1024
#define HTTPD_MAX_BODY 4096
#define HTTPD_MAX_BODY_PSRAM (64 * 1024)

// Chunk buffer size for streaming files to/from a connection (http.respondfile /
// http.savebody). A stack buffer, so kept modest; bytes move file <-> socket in
// chunks of this size.
#define HTTPD_CHUNK_MAX 512

// Longest percent-decoded request path the pump records for `http.path`. A
// longer target auto-responds `414`.
#define HTTPD_PATH_MAX 512

// Longest request method the pump records for `http.method` (GET, DELETE, ...).
#define HTTPD_METHOD_MAX 16

// Pump timing. HTTPD_POLL_MS budgets the pump at the demon poll sites (like
// DEMON_POLL_MS). HTTPD_STALL_MS is how long a half-sent request may stall
// mid-parse before the pump auto-responds `408`. HTTPD_RESPOND_MS is how long a
// fully-parsed request may go unanswered (no handler, or a handler that forgot
// `http.respond`) before the pump auto-responds `503` and closes. Both timers
// pause while frozen.
#define HTTPD_POLL_MS 20
#define HTTPD_STALL_MS 5000
#define HTTPD_RESPOND_MS 10000

// Largest HTML fragment `http.element` builds in one call (a stack buffer). A
// single classroom element is far smaller; big pages are assembled by joining
// several elements with `word`.
//
// OVERFLOW: an element whose markup would exceed this errors with
// ERR_FILE_TOO_BIG rather than truncating.
#define HTTPD_ELEMENT_MAX 1024

// Sound synthesizer (P8, docs/sound-design.md). The engine renders eight
// voices: three tone plus one noise per stereo ear (the SN76489 layout,
// doubled). Voices are numbered by ear: 0-2 tone + 3 noise (left),
// 4-6 tone + 7 noise (right).
//
// OVERFLOW: `sound`/`play`/`setenv`/`setwave` reject a voice number >=
// MAX_VOICES with ERR_DOESNT_LIKE_INPUT, matching `tell`.
#define MAX_VOICES 8

// Per-voice sequencer queue depth, in note events (SoundEvent, 6 B). At
// eight voices that is SOUND_QUEUE_LEN * 8 * 6 B; 64 gives ~16 bars of
// eighth notes per voice before `play` has to wait. Halving to 32 saves
// ~1.5 KB if link-time SRAM pressure demands (docs/sound-design.md §8).
//
// OVERFLOW: `play` does NOT error when a voice queue is full -- it waits
// (BREAK-interruptible) for slots to drain, so long songs stream instead
// of failing.
#define SOUND_QUEUE_LEN 64

// DMA output ring geometry (docs/sound-design.md §6). Two halves so the
// wrap IRQ can refill the drained half while DMA streams the other; each
// slot is a 32-bit L|R PWM compare pair. Power of two so the whole ring
// (2 * 256 * 4 = 2048 B) is a hardware-ring-wrappable, aligned buffer: the
// DMA read address wraps in hardware, so if the refill IRQ is starved (the
// display driver masks interrupts during screen redraws) the DMA cleanly
// replays the ring instead of reading past it into garbage. 256 slots/half
// at the 36.6 kHz mix rate is ~3.5 ms of audio per half.
#define SOUND_RING_HALF 256

#ifdef __cplusplus
}
#endif
