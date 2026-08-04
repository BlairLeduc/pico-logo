//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  LOGO_HOT -- place a function in RAM on the device, leave it alone on the
//  host (P10 M5, docs/interpreter-throughput-design.md §11.1).
//
//  The RP2350 executes the interpreter from flash through a 16 KB XIP cache.
//  Profiling both machines put a bare `repeat` iteration at 60x the host and
//  a user procedure call at 67x, but a whole arithmetic statement at 132x
//  and Logo's parenthesised-call path at 212x. That spread is not "the board
//  is slower"; it is one code path costing far more there than its share of
//  the work, and the leading explanation is instruction fetch -- the
//  expression evaluator is a lot of code entered once per statement, against
//  the tight loops calls and `repeat` run in. `__not_in_flash_func` moves a
//  function into SRAM, where fetch is free.
//
//  Measured on a Plus 2 W (design §11.3): the Trails frame went 81.0 to
//  65.5 ms, 1.24x, and the 212x path to 38x, for 6 KB of SRAM. A second
//  tier followed, 3.6 KB more.
//
//  Two reasons this is a macro rather than the SDK spelling used directly.
//  The SDK's is unavailable on the host, and core/ compiles for both -- the
//  shim P9's §13.2 already recorded as needed before `core/tilemap.c`'s
//  sampler could be made RAM-resident. And RAM is the scarce resource here,
//  so this is applied function by function with the memory report checked,
//  never wholesale.
//
//  Off by default so the host and test builds never see it; the `pico2w` and
//  `pico+2w` presets ask for it. `pico2` does not -- not because it cannot
//  afford it (its 21 KB is a 108 KB op stack left at 768 from the
//  single-board era, not the board) but because no one here has one to boot.
//

#ifndef LOGO_HOT_H
#define LOGO_HOT_H

#if defined(LOGO_HOT_IN_RAM) && LOGO_HOT_IN_RAM
#include "pico.h"
#define LOGO_HOT(name) __not_in_flash_func(name)
#else
#define LOGO_HOT(name) name
#endif

#endif // LOGO_HOT_H
