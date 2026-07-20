# Design: LittleFS internal filesystem + `/sd` FAT32 mount

Status: **Implemented** — Phases 0–4 complete (see §11 for per-phase results)
Target: Pimoroni Pico Plus 2 W (RP2350, 16 MB flash, 8 MB PSRAM), PicoCalc device
Author: design notes for review

## 1. Goals

1. Add a **LittleFS filesystem in on-board flash** that **survives flash-and-debug
   cycles** — reprogramming the firmware (via OpenOCD `load`, picotool, or UF2)
   must not erase stored files.
2. Make **LittleFS the root `/`** of the Logo filesystem and the default start
   directory. The device is fully usable with **no SD card inserted**.
3. **Auto-mount the SD card's FAT32 volume at `/sd`**, and re-mount automatically
   when a card is removed and a new one inserted.
4. Allow **copying and moving files between the two filesystems**.

Non-goals: wear-leveling tuning beyond LittleFS defaults; multiple SD partitions;
FAT on the internal flash; a generic mount-table (only two fixed mounts: `/` and
`/sd`).

## 2. Current architecture (baseline)

- **One storage backend.** `LogoStorage` holds a single `ops` table
  (`devices/storage.h:74`). On PicoCalc it is `picocalc_storage_ops`, which calls
  `fat32_*` directly against the SD card (`devices/picocalc/picocalc_storage.c:617`).
- **Single chokepoint.** Every Logo file primitive (load/save, editor, dribble,
  directory listing) goes through `logo_io_*` → `storage->ops`. This is the only
  place a router needs to be inserted.
- **No mount-point routing today.** `io.prefix` ("/Logo/" or "/") is only a
  relative-path convenience: `logo_io_resolve_path` (`devices/io.c:472`) prepends
  it unless the path is absolute. Absolute paths pass straight to FAT32.
- **SD hot-swap is half-built.** A 500 ms repeating timer (`fat32.c:2552`)
  unmounts on card removal; `fat32_is_ready()` (`fat32.c:646`) lazily re-mounts on
  the next access. So "re-mount a new card" already works for the FAT side.
- **No flash writes exist anywhere today** — flash is read-only XIP. This is
  greenfield.
- **Threading model (important, see §4):** effectively **single-core** (no
  `multicore_launch_core1`), but **background/IRQ-driven networking** —
  `pico_cyw43_arch_lwip_threadsafe_background` (`CMakeLists.txt:216`) services
  lwIP/cyw43 from a low-priority IRQ + alarm on core0 (the `cyw43_arch_poll()` at
  `picocalc_hardware.c:478` is belt-and-suspenders, not the primary driver). Plus
  several **flash-resident repeating-timer ISRs** (keyboard 100 ms
  `keyboard.c:238`, SD-detect 500 ms `fat32.c:2583`). So a flash write must run
  with interrupts disabled, and criterion 4 (no dropped CYW43 event) matters.

## 3. Flash layout — surviving flash-and-debug

### Principle
Each programming tool only erases the sectors it writes:
- OpenOCD `load` (used by `.vscode/launch.json`), `picotool load`, and UF2 all
  program **only the loadable sections of the ELF/image**. None mass-erase.
- A LittleFS region the firmware image never covers is therefore **untouched** by
  a normal flash-and-debug cycle.
- Only an explicit mass erase wipes it: `picotool erase`, `flash_nuke.uf2`, or
  OpenOCD `flash erase_sector`. These must be avoided / scoped.

### Layout
Reserve a fixed high region of the 16 MB flash for LittleFS; keep the firmware
image out of it.

```
0x10000000  +-------------------------------+  flash base (XIP, cached)
            |  firmware image (.text/.data) |  programmed by load/UF2
            |              ...              |
            +-------------------------------+  LFS_FLASH_OFFSET
            |     LittleFS partition        |  never in any ELF section
0x10000000  +-------------------------------+  +16 MB end of flash
 +16MB
```

- `LFS_FLASH_OFFSET` and `LFS_FLASH_SIZE` become named constants. Per project
  convention, sizes that affect footprint belong in `core/limits.h` (or a new
  `devices/picocalc` flash-map header if device-specific is cleaner).
- **Decided: reserve the top of flash, sized per board** via
  `PICOCALC_FLASH_LFS_SIZE` (8 MB on the 16 MB Pico Plus 2 W, 2 MB on a 4 MB
  Pico 2 / 2 W). `PICOCALC_FLASH_LFS_OFFSET = PICO_FLASH_SIZE_BYTES - size`, so
  the region stays anchored to the top of whatever flash the board has; firmware
  keeps the bottom (~0.73 MB) with a large unused gap between.
- Start simple with a **fixed constant**. The RP2350 partition-table / bootrom
  feature is a later option if picotool-managed partitions become desirable — not
  needed for a single-user board.
- Linker: nothing emitted into the reserved region (it already isn't; just don't
  add anything there). Optionally add an assertion that the image end is below
  `LFS_FLASH_OFFSET`.

## 4. The PSRAM / QMI-safe flash-write path (do this FIRST)

> **Update (July 2026, SDK 2.3.0):** the custom PSRAM driver this section
> references (`picocalc_psram.{c,h}` — init, self-test, `psram_rearm_qmi()`)
> has been replaced by the SDK's `hardware_psram` library. PSRAM is brought up
> during SDK runtime init (CS pin from the board header, size auto-detected
> from the chip id), and the SDK re-arms the QMI CS1 window inside every
> `flash_range_erase/program` via `flash_set_qmi_cs1_setup_function()` — so
> `picocalc_flash.c` no longer re-arms manually. The read-back self-test lives
> on as `psram_verify()` in `devices/picocalc/main.c`. The hazard analysis,
> spike criteria, and Phase-0 results below are kept as validation history —
their `picocalc_psram.c:<line>` / `PICOCALC_PSRAM_BASE` references describe the
now-removed driver; the equivalents today are `__psram_start__` and the SDK's
`hardware_psram`.

This is the highest-risk area and **step 1 of implementation is a spike to
validate it** before building on top.

### Why it is risky
The RP2350 QMI is **one controller with two chip-select windows** sharing the
QSPI bus and the **XIP cache**:
- `qmi_hw->m[0]` → CS0 → boot flash @ `0x10000000`.
- `qmi_hw->m[1]` → CS1 → PSRAM @ `PICOCALC_PSRAM_BASE 0x11000000`, made writable
  via `XIP_CTRL_WRITABLE_M1` (`picocalc_psram.c:171`).

A flash erase/program drives the shared controller and flushes the shared cache.
The PSRAM **chip** state (QPI mode from the one-time `0x35`) is preserved; what is
at risk is **QMI controller M1 state + the shared XIP cache** (SRAM-side).

### Three concrete hazards
1. **Shared XIP-cache flush loses dirty PSRAM lines.** `flash_flush_cache`
   invalidates the whole XIP cache, which caches both windows (the PSRAM selftest
   relies on this, `picocalc_psram.c:187`). Dirty PSRAM lines not yet written back
   are lost. PSRAM backs the blob heap and editor buffers → highest-probability
   corruption path.
2. **`XIP_CTRL_WRITABLE_M1` cleared.** `flash_enter_cmd_xip` rewrites
   `xip_ctrl->ctrl`; if WRITABLE_M1 is cleared, PSRAM silently becomes read-only.
3. **M1 window registers / CS1 pad function.** Bootrom programs `m[0]` and
   reconfigures the QSPI pads; almost certainly leaves `m[1].*` alone, but cheap
   to re-arm. Open question: does `connect_internal_flash` reset the PSRAM CS pin
   (`GPIO_FUNC_XIP_CS1`, `picocalc_psram.c:232`)?

### Design response
- **Code placement — two categories (don't conflate them):**
  - **Category A (mandatory):** the flash-op path must be SRAM-resident, because
    XIP is down during erase/program. This covers the LittleFS block-device
    callbacks, `flash_range_*`, `psram_rearm_qmi()`, and `xip_cache_*`. The SDK
    flash funcs and the existing QMI funcs already are (`__no_inline_not_in_flash_func`);
    only the new block-device shim needs `__not_in_flash_func`. Small and required.
  - **Category B (rejected — relocating ISRs to keep interrupts live):** moving
    the keyboard / SD-detect / CYW43 ISRs into SRAM to run *during* a write is
    **not worth it**. It isn't enough to relocate the ISR function — its entire
    transitive call graph + all rodata must be in SRAM **and** must touch neither
    flash **nor PSRAM** (PSRAM shares the QMI bus, and the blob heap / editor
    buffers live there, so an ISR brushing them mid-write corrupts data). The
    CYW43 GPIO IRQ is SDK/library code in flash — invasive to relocate. The payoff
    is only avoiding a bounded, benign stall (keyboard MCU buffers keystrokes;
    SD-detect is a 500 ms timer; the cyw43 background IRQ is on core0 and its chip
    GPIO IRQ is latched, so a few-ms window defers — not drops — its servicing).
- **Instead, run stop-the-world but chunk it.** Erase **one sector at a time,
  re-enabling interrupts between sectors**, so the worst-case interrupts-off
  window is a single ~few-ms sector erase rather than a whole multi-sector commit.
  LittleFS's batched writes touch few sectors per commit, so this stays cheap.
- Factor a **`psram_rearm_qmi()`** out of `psram_configure_qmi()` that re-writes
  only the `m[1]` window registers + re-asserts `XIP_CTRL_WRITABLE_M1`. It must
  **not** re-run `psram_enter_qpi()` (the chip is already in QPI; re-issuing `0x35`
  would be wrong). The detected size and chip-side QPI mode are preserved.
- Single-core (no second core) means **no core-lockout needed**: build with
  `PICO_FLASH_ASSUME_CORE0_SAFE=1` (or bracket in a critical section) rather than
  the full `flash_safe_execute` multicore dance. The cyw43/lwIP background
  servicing is a core0 IRQ, so disabling interrupts for the op inherently pauses
  it; just avoid issuing a write while holding a networking lock.

### Safe-write recipe (per single sector erase / per program)
Operate **one sector at a time** so the interrupts-off window stays bounded; a
multi-sector erase loops over this, re-enabling interrupts between iterations.
1. `save_and_disable_interrupts()` (this also pauses the core0 cyw43/lwIP IRQ).
2. `xip_cache_clean_all()` — write back dirty PSRAM lines (XIP still up).
3. `flash_range_erase` (one sector) / `flash_range_program` (bootrom path, RAM).
4. `psram_rearm_qmi()` — restore `m[1]` regs + WRITABLE_M1.
5. `xip_cache_invalidate_all()`.
6. `restore_interrupts()`.

Per-iteration the interrupts-off window is one flash op: **~47 ms for a sector
erase** (measured, Phase 0), sub-ms for a page program. LittleFS batches writes
and lets us control commit timing, so commits are infrequent and off the hot path.

### Spike acceptance criteria (Phase 0)
1. After `picocalc_psram_init`, a `flash_range_program` to the reserved region
   followed by a cached PSRAM read-back confirms WRITABLE_M1 survived (or that
   step 5 restores it).
2. `m[1]` registers unchanged across the bootrom path (read before/after).
3. PSRAM CS pin function survives `connect_internal_flash`.
4. A single-sector interrupts-off window does not break keyboard / SD-detect
   recovery or drop a CYW43 event; and a multi-sector erase looped with interrupts
   re-enabled between sectors behaves the same.

### Phase 0 results — VALIDATED (2026, SDK 2.2.0, Pico Plus 2 W)
Ran via the `PICOCALC_FLASH_SPIKE` boot self-test (`picocalc_flash.c`). All
criteria passed:
- `[1]` **WRITABLE_M1 survives the raw flash op** (1→1) — the bootrom does **not**
  clear it on this silicon/SDK. Hazard #2 did not materialize.
- `[2]` **M1 window registers unchanged** by the bootrom path. Hazard #3 (regs)
  did not materialize.
- `[3]` **PSRAM CS pin function unchanged** (9 = `GPIO_FUNC_XIP_CS1`) across
  `connect_internal_flash`. Hazard #3 (pin) did not materialize.
- `[1b]` flash program+readback **integrity PASS**; `[1c]` PSRAM **writable after
  the full recipe PASS**.
- `[4]` 8-sector chunked erase returned cleanly; **keyboard stayed responsive, no
  crash**.

**Consequences for the design:**
- The QMI/PSRAM window is **robust to flash writes** — none of the register/pin
  hazards occurred. `psram_rearm_qmi()` is therefore **insurance, not a
  load-bearing fix**: keep it (idempotent, cheap), but correctness no longer
  depends on it. The shared-cache clean/invalidate in the recipe still matters
  (hazard #1) and is retained.
- **Timing correction (important):** a 4 KB sector erase takes **~47 ms**
  (measured 47,140 µs/sector), not "a few ms" as earlier drafts said. The recipe
  holds **interrupts disabled for that full ~47 ms** (the bootrom busy-polls the
  chip with XIP down). Per-sector chunking re-enables interrupts **between**
  sectors, so a multi-sector commit is 47 ms windows with gaps (cyw43/lwIP IRQ
  serviced in the gaps), **worst-case single stall ≈ 47 ms**. Keyboard tolerates
  this (MCU buffers); see decision below on whether to accept it or add an
  interruptible erase.

The core design assumption holds: **flash writes coexist safely with the
configured PSRAM.** Phase 1 may proceed.

## 5. LittleFS block device + configuration

- **Vendor upstream `littlefs`** (`lfs.c`, `lfs.h`, `lfs_util.c/h` — ~4
  self-contained files). This is the pragmatic reading of the "avoid new
  dependencies" rule; flag for explicit sign-off.
- **Block device hooks** map LittleFS read/prog/erase/sync onto the §4 safe-write
  path and direct XIP reads from `0x10000000 + LFS_FLASH_OFFSET`:
  - `read`  → memcpy from the XIP-mapped flash region (cached read, no ceremony).
  - `prog`  → safe-write recipe (program).
  - `erase` → safe-write recipe (sector erase; RP2350 flash erase granularity).
  - `sync`  → no-op (writes are synchronous).
- **Geometry:** `block_size` = flash sector size (4 KB); `block_count` =
  `PICOCALC_FLASH_LFS_SIZE / block_size` (e.g. 8 MB / 4 KB = 2048 blocks on the
  Pico Plus 2 W). RAM buffers don't scale with `block_count`.

### SRAM budget (measured)
From the current `pico+2w` build (`arm-none-eabi-size`):

| Region | Bytes |
| --- | --- |
| `.bss` (incl. the 128 KB `memory_block`, `memory.c:90`) | 421,008 |
| `.data` | 9,792 |
| vector table + stack | ~4,368 |
| **SRAM in use** | **~435 KB of 520 KB** |
| **Free (malloc heap + stack gap)** | **~90 KB** |

Implications:
- **`LOGO_MEMORY_SIZE` is a real SRAM lever** (it is `static uint8_t
  memory_block[LOGO_MEMORY_SIZE]` + a proportional `gc_marks` bitmap,
  `memory.c:1059`). The large blob/editor buffers already live in **PSRAM** (the
  aux region), so this pool is only cons-cells + the atom table.
- **LittleFS does not need that lever.** Its static buffers (`read` + `prog` +
  `lookahead`, plus one cache per open file) total **~0.5–2 KB** — trivial against
  ~90 KB of headroom. So **buffers live in SRAM** and we do **not** shrink
  `LOGO_MEMORY_SIZE` for them (keep it as a reserve lever). Confirm the exact
  footprint in the Phase-0 spike, then fix `cache_size`/`lookahead_size` in
  `core/limits.h`.

## 6. VFS router (LittleFS as root `/`, FAT32 at `/sd`)

Insert a **router `LogoStorageOps`** above two backend ops tables. Routing keys on
the leading path component of the **resolved absolute path**:

- `/sd` or `/sd/...`  → FAT32 backend (existing `fat32_*` ops, lightly renamed).
- everything else     → **LittleFS backend** (new).

Because LittleFS is root, plain `/` paths need no prefix and no special-casing —
the router default is LittleFS, and `/sd` is the only named mount.

- The two backends become two `LogoStorage` instances (or two `LogoStorageOps`);
  the router holds pointers to both and dispatches per call.
- `io.prefix` stays exactly as-is (relative-path sugar); routing is a separate
  concern keyed off the absolute path after `logo_io_resolve_path`.
- The router strips the `/sd` prefix before delegating to the FAT backend (which
  expects SD-absolute paths), and passes other paths unchanged to LittleFS.

### Path edge cases to define
- `/sd` with no trailing slash (the mount root itself) → FAT root listing.
- Listing `/` shows LittleFS entries **plus** a synthetic `sd` entry **only when a
  card is present** (lazy-mount model).
- Reject creating a real `/sd` directory on LittleFS (name reserved for the mount).

## 7. Cross-filesystem copy / move

- **Copy** already works: it is stream read+write at the Logo level, so it crosses
  backends naturally once the router dispatches each path to its backend.
- **Move/rename is the new logic.** The `rename` op currently maps straight to
  `fat32_rename` (`picocalc_storage.c:501`), which only works within one
  filesystem. The router must:
  - same backend  → native rename (`fat32_rename` or `lfs_rename`).
  - cross backend  → **streamed copy + delete** (not a native rename), with
    rollback on failure (don't delete the source if the copy failed).
- **Decided: reject cross-FS directory moves in v1**; allow file moves only.
  A cross-backend directory move returns an error.

## 8. SD hot-swap

Largely already present; keep behind the `/sd` route:
- Keep the 500 ms card-detect timer + lazy `fat32_is_ready()` mount. Inserting a
  new card → next `/sd` access mounts it.
- **Stale handles:** today nothing invalidates a `/sd` file open across a
  hot-swap. Decide: on detected removal, mark open `/sd` streams invalid so reads
  return error instead of reading a different card. Recommend doing this — it is a
  small addition to the existing timer callback.

## 9. Startup / bootstrap changes (LittleFS root)

- `main.c:108-170` currently probes `/Logo` on the SD and loads `startup` from it.
  Change to: default prefix `"/"` (LittleFS root); load `startup` from LittleFS.
- **First-boot:** a freshly flashed board has an **unformatted** LittleFS region.
  On mount failure → `lfs_format` then mount. **No seeding** — the FS starts
  empty; users add files via the editor or by copying from `/sd`.
- **Default save location is LittleFS root `/` directly** (no default subdir).
  `startup` is loaded from `/` if present.
- The "I cannot find the default directory" warning logic is replaced by
  format-on-first-boot.

## 10. Testing strategy

- Host build has no flash/PSRAM. Add a **LittleFS-on-RAM (or on a host file)
  block device** in the mock device so the VFS router, cross-FS copy/move, path
  routing, and bootstrap/format logic are all unit-tested natively
  (`tests/mock_device.*`, per CLAUDE.md).
- Router tests: path dispatch (`/`, `/sd`, `/sd/...`, reserved-name rejection),
  cross-FS move = copy+delete with rollback, same-FS native rename.
- The §4 flash/QMI path is **hardware-only** and validated by the Phase-0 spike on
  device, not by ctest (mirrors how `picocalc_psram.c` is hardware-only).

## 11. Phased plan

- **Phase 0 — PSRAM/QMI flash-write spike (gating). ✅ DONE / PASSED.** Safe-write
  recipe + `psram_rearm_qmi()` implemented (`picocalc_flash.{c,h}`,
  `picocalc_psram.c`); all four §4 criteria validated on hardware. Finding: the
  QMI/PSRAM window is robust (rearm is insurance), but a sector erase is ~47 ms of
  interrupts-off time — see §4 results and the 47 ms decision.
- **Phase 1 — LittleFS block device + format/mount. ✅ DONE / PASSED.** Vendored
  littlefs v2.11.3 (`third_party/littlefs/`); block device over the safe-write
  path + mount-with-format-on-first-boot (`picocalc_lfs.{c,h}`); static SRAM
  buffers (256+256+32 B). Validated on host (RAM bd, format-once + persist) and on
  hardware via the `PICOCALC_LFS_SELFTEST` boot counter: first boot formats,
  follow-up boots persist, and the count **survives a firmware reflash** — the
  reserved region is outside the firmware image as designed.
- **Phase 2 — VFS router + LittleFS as root. ✅ DONE / PASSED.** Storage router
  (`devices/storage_router.{c,h}`) dispatching `/sd[/...]` → FAT, else → the
  device-independent LittleFS backend (`devices/lfs_storage.{c,h}`); `main.c`
  mounts LittleFS as `/`, builds the router over both backends, default prefix
  `/`, loads `startup` from root; `/` listing injects a synthetic `sd` only when a
  card is present. 25 host tests (router + backend over a RAM bd); verified on
  hardware (save/catalog/createdir on `/`, `/sd` lists the card, `sd` appears only
  with a card inserted). Cross-mount rename is rejected here (Phase 3).
- **Phase 3 — cross-FS copy/move + hot-swap hardening. ✅ DONE / PASSED.**
  Router cross-mount rename now does streamed copy+delete (files only; rejects
  directories and a missing source; rolls back a partial destination). FAT
  hot-swap safety via a generation counter bumped on unmount: open FAT handles
  refuse to read/write/flush when it changes (`fat32.c` + `picocalc_storage.c`).
  Also fixed a latent bug exposed here: the FAT `open` left `LogoStream.write_error`
  uninitialised, which made content cross-FS moves spuriously fail. Tests: cross-FS
  move (7, incl. rollback + the write_error regression), FAT generation (1),
  multi-write lfs round-trip (1); full suite 50/50. Verified on hardware:
  content and empty moves both directions, hot-swap does not crash.
- **Phase 4 — polish. ✅ DONE.** New `free` reporter outputting `[free_blocks
  block_size]` for a volume (optional `(free "/sd)`) via a `free_blocks` storage
  op on both backends + router + host/mock; returning the block size lets callers
  convert to bytes and compare across volumes (LittleFS 4 KB blocks vs the FAT
  card's cluster). `setprefix "/sd` with no card now reports "There is no SD
  card" (new `ERR_NO_SD_CARD` + optional `mount_available` storage op) instead of
  "Subdirectory not found". Reference manual updated: File Management overview of
  the `/` + `/sd` layout, default prefix `/` (was `/Logo/`), cross-FS move
  semantics on `rename`, and refreshed `setprefix`/archive examples. Host tests
  for `free`; full suite 50/50.

## 12. Decisions (resolved)

1. **Flash partition size:** **per board, via the `PICOCALC_FLASH_LFS_SIZE` CMake
   cache variable** (set in `CMakePresets.json`): **8 MB** on the Pico Plus 2 W
   (16 MB flash), **2 MB** on a plain Pico 2 / 2 W (4 MB flash). The region is
   anchored to the top of flash (offset = `PICO_FLASH_SIZE_BYTES - size`), so it
   tracks the board's flash size; firmware (~0.73 MB) sits at the bottom with a
   large gap between. `picocalc_flash.c` static-asserts the size is sector-aligned
   and smaller than the board's flash. (Changing the size changes `block_count`,
   so an existing on-device filesystem reformats on the next boot.)
2. **Vendor `littlefs`:** **yes** — copy the ~4 upstream files into the tree.
3. **LittleFS buffer placement:** **SRAM** — buffers are ~0.5–2 KB against ~90 KB
   of headroom (see §5 SRAM budget). `LOGO_MEMORY_SIZE` is *not* reduced for them;
   it stays a reserve lever. Confirm exact `cache_size`/`lookahead` in Phase-0.
4. **First-boot seeding:** **empty** — format and mount only; no seeded files.
5. **`/` listing of `sd`:** show the synthetic `sd` entry **only when a card is
   present** (matches the lazy-mount model).
6. **Cross-FS directory moves:** **rejected in v1** — file moves cross filesystems
   (copy+delete); directory moves across FS error out.
7. **Default save location:** **LittleFS root `/`** directly (no default subdir);
   consistent with "start directory is `/`".
8. **47 ms erase stall (from Phase 0):** **accept for v1.** Worst-case single
   interrupts-off window is one sector erase (~47 ms); chunking re-enables
   interrupts between sectors so cyw43/lwIP is serviced in the gaps. Guidance:
   don't write to flash mid network transfer. Interruptible erase (microsecond
   stalls, but needs RAM-resident ISRs — the fragile "Category B" work) is
   deferred; revisit only if networking-during-save proves a problem.
```
