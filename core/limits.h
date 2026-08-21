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
// COST: one slot is a 16-byte `Variable` (name pointer, Value, three
// flags) in .bss, so this table is 3 KB on the target. Raising it does
// not slow *reads* down: `find_global` scans `global_count`, not this
// bound, so a lookup costs what the workspace actually holds. The two
// paths that do walk the whole array are `variables_init` (once, at
// startup) and creating a global that does not exist yet (once per
// name) -- neither is on a frame loop's path.
//
// A big workspace is still a linear scan on every global read, so this
// is not free to keep raising -- 192 is what lets a game (Turtle Trails
// is the fattest at ~119) load with the frame profiler or a second
// program on top, which 128 no longer did.
//
// OVERFLOW: `var_set` returns `false` when the table is full; callers
// surface this as `ERR_OUT_OF_SPACE`.
#define MAX_GLOBAL_VARIABLES 192

// Maximum depth of the "currently executing procedure" name stack used
// for the pause prompt and trace output. This is independent of the
// frame stack (which is sized in bytes by `FRAME_STACK_SIZE`); it only
// limits how deep the *display* stack can grow.
//
// OVERFLOW: `proc_push_current` silently drops names beyond the limit
// (the frame stack still grows correctly; only the prompt label is
// affected).
#define MAX_CURRENT_PROC_DEPTH 32

// Segregated free-list heads for reclaimed atom storage.  Atom entries are
// four-byte aligned and max out at 260 bytes; the last bin also accepts larger
// blocks produced by coalescing.
#define LOGO_ATOM_FREE_LIST_COUNT 65

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

// Tile bank and tile map (P9, docs/tilemap-scrolling-design.md §4). Both
// pools are allocated lazily on the first `newtiles` / `newmap` and, like the
// HTTP transfer buffer, choose their tier at run time:
//   - TILE_BANK_SIZE / TILE_MAP_SIZE (SRAM fallback): used when no aux/PSRAM
//     region is present. Kept small because SRAM is largely consumed by the
//     Logo arena, the frame buffer, and the operand stack -- 4 KB of bank is
//     64 tiles at 8x8 or 16 at 16x16, and 4 KB of map is a 64x64 world.
//   - TILE_BANK_SIZE_PSRAM / TILE_MAP_SIZE_PSRAM: used when an aux region
//     backs the pool (the Pico Plus 2 W), where a 512x512 world fits.
// Boards that never touch tiles pay nothing: there are no static arrays.
//
// OVERFLOW: `newmap` with more cells than the active tier allows is
// ERR_OUT_OF_SPACE, as is a `newtiles` whose pool cannot be allocated at all.
// A bank slot beyond the active tier's capacity is ERR_DOESNT_LIKE_INPUT.
#define TILE_BANK_SIZE (4 * 1024)
#define TILE_BANK_SIZE_PSRAM (64 * 1024)
#define TILE_MAP_SIZE (4 * 1024)
#define TILE_MAP_SIZE_PSRAM (256 * 1024)

// Widest run of screen pixels the tile sampler builds in one call, and so the
// size of the single row buffer `stampmap` bakes through. The PicoCalc screen
// is 320 px wide; a wider viewport than this is clipped rather than an error.
#define TILEMAP_ROW_MAX 320

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

// Vi mode (docs/vi-mode-design.md). Four fixed-capacity fields in `ViState`,
// which is one static struct inside the editor.
//
// The ex command line. The longest command the mode accepts is a substitute
// with both texts at their limit -- ":%s/" + pattern + "/" + replacement +
// "/g" -- so this is sized from LOGO_VI_TEXT_MAX rather than guessed.
//
// OVERFLOW: `editor_vi_key` drops printable keys once the line is full; the
// command line stops growing and nothing is truncated behind the user's back.
#define LOGO_VI_TEXT_MAX     32
#define LOGO_VI_CMDLINE_MAX  (LOGO_VI_TEXT_MAX * 2 + 8)

// A message the vi layer has to compose rather than point at a literal -- the
// `Ctrl` `G` report and the line numbers `:=` and `:.=` print. It is shown on
// the footer, which is one row of a 40-column screen, so there is nothing to
// be gained by making it longer.
//
// OVERFLOW: snprintf truncates, which loses the tail of a report rather than
// anything the user typed.
#define LOGO_VI_MSG_MAX      40

// The most a single `:s` match can expand to, on the stack, before it is
// spliced in (docs/vi-mode-design.md §16.4). A pattern match is at most one
// line and each `&` or `\1` in the replacement copies a piece of it, so a
// runaway is caught here in the counting pass -- before a byte moves -- rather
// than overrunning the buffer. Not static: it lives in editor_vi_substitute's
// frame only, which is the one place the pattern matcher puts depth on the
// stack, so this is the lever if that frame is ever measured tight.
#define LOGO_VI_SUB_EXPAND_MAX  256

// The most match steps one `editor_pattern_search` call may spend before it
// gives up (B36). Sequential stars backtrack combinatorially -- `.*.*.*x` on a
// 256-char line costs 189 million steps, and each added `.*` multiplies by the
// line length again -- so the matcher has to bound its own work or a `:s` can
// wedge the board with no key able to interrupt it. Measured, not guessed: real
// patterns cost tens to hundreds of steps (`\<n\>` on a 69-char line is 13),
// and the worst legitimate case is a single star failing on a full-width line,
// 33,410. This leaves ~6x headroom over that and refuses everything past it.
#define LOGO_VI_PATTERN_STEPS_MAX  200000

// The largest count a normal-mode command may be given (B47). Counts are
// accumulated a digit at a time and a held-down digit key repeats, so without a
// bound the eleventh digit runs an `int` past its end -- undefined behaviour
// rather than a wrapped count. Six digits is more than the biggest edit buffer
// has bytes (LOGO_EDITOR_PSRAM_BUFFER_SIZE, 256 KB), so no count this cap
// refuses could reach anywhere a count it allows cannot. `2d3w` multiplies the
// operator's count by the motion's, and two capped counts do not multiply into
// an `int`, so that product is taken wide and clamped back to this.
//
// OVERFLOW: further digits are consumed and ignored, so the count stops
// growing while the command being typed stays the one the user is typing.
#define LOGO_VI_COUNT_MAX    999999

// Keys recorded for `.` to replay. A change command is an optional operator,
// an optional prefix (`g`, `f`, ...) and a motion -- three keys covers every
// one of them, and eight leaves room without being worth counting.
//
// OVERFLOW: a command longer than this is simply not recorded, so `.` replays
// the last one that fit rather than a half command.
#define LOGO_VI_REPEAT_MAX   8

// The text of an insert session recorded for `.` to type again -- everything
// between the `i`/`a`/`o`/`c` and the `Esc` that closed it. Two 40-column
// lines, which is more than the one-line insert this exists for (a comment
// marker, a retyped word) and enough for the small multi-line one.
//
// OVERFLOW: a longer session is not recorded and the change it belongs to is
// dropped from the record, so `.` says it has nothing to repeat rather than
// putting back a truncated version of what was typed.
#define LOGO_VI_INSERT_MAX   80

// The undo journal (docs/vi-mode-design.md §8), tiered by where the editor
// buffers landed -- which is a run-time decision, not a build one. A record is
// a header plus the bytes a change removed and the bytes it put there, so a
// journal holds far more small changes than large ones; when one will not fit,
// the oldest *whole* steps are dropped, which is what leaves the SRAM tier with
// the one level the design promised rather than none.
//
// PSRAM: taken as part of primitives_editor_init's single region block, so it
// is all-or-nothing with the two 256 KB buffers. Negligible against 8 MB.
//
// SRAM: a one-time heap allocation beside the fallback edit buffers, which is
// the same heap they come out of -- so this is a starting figure, not a budget,
// and undo simply stays unavailable if the allocation fails.
//
// OVERFLOW: a single change larger than the whole journal clears it; that
// change and everything before it cannot be undone.
#define LOGO_VI_UNDO_PSRAM_SIZE (64 * 1024)
#define LOGO_VI_UNDO_SRAM_SIZE  1024

// Editor procedure-definition buffer (SRAM tier).
//
// The editor's second buffer does NOT hold the file: run_editor_and_process
// accumulates ONE procedure in it, resetting at every `to`, and hands it to
// proc_define_from_text at `end`. Sizing it to match the edit buffer -- which
// is what taking the pair as one block did -- reserved a file's worth of SRAM
// to hold a procedure's worth, and on a board with no PSRAM that is half the
// editor's heap cost spent on nothing.
//
// 4 KB is the same bound REPL_MAX_PROC_BUFFER puts on a definition *typed* at
// the prompt, for the identical job, so the two halves of the interpreter now
// agree on how long one procedure may be. It is measured, not guessed: the
// longest procedure anywhere in this tree is 2,850 bytes (`p10prof`), and the
// longest in a shipped game is 1,601 (`step.saucer` in a 101 KB asteroids).
//
// Overflow is already handled and says so -- "Procedure too long" -- and skips
// that definition rather than truncating it.
//
// PSRAM boards do not use this: they take both buffers at
// LOGO_EDITOR_PSRAM_BUFFER_SIZE out of a region with megabytes going spare,
// where there is nothing to be won by bounding it tighter.
#define LOGO_EDITOR_PROC_BUFFER_SIZE 4096

#ifdef __cplusplus
}
#endif
