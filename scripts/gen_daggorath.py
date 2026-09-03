#!/usr/bin/env python3
"""Generate the Dungeons of Daggorath maze and architectural vector data.

docs/daggorath-design.md section 7.2: the five mazes are not carved on the
board -- RANDOM.ASM's shift-register RNG costs ~1.75 ms a byte even at 300
MHz, and a level carve needs several thousand of them.  So this script ports
RANDOM.ASM:RANDOX and DGNGEN.ASM:DGNGEN bit-for-bit and writes their result,
once, into logo/games/daggorath itself.  It also flattens the twelve VARC.ASM
architectural outlines, the two VARC.ASM peek-a-boo marks, VERT.ASM's plain
ceiling line (CELINE) and VOBJ.ASM's six object outlines into flat absolute
polylines -- section 11.2's "the generator flattens the encoding" -- for the
forward-view cell walk.

And it reads the object tables out of TOKEN.ASM and DTABAS.ASM rather than
transcribing them: the names come from TOKEN.ASM's packed five-bit strings
and the numbers from DTABAS.ASM's macro calls, cross-checked on the object
class, which both of them carry.  That check is what says the ring the
OBJXXX macro calls HOTH is the ring TOKEN.ASM calls RIME -- see
read_object_tables().

M1-M3's scope: VERT.ASM's LADDER/HOLEUP/HOLEDN (needs VFTTAB, M5) and
D3.ASM/D4.ASM (creatures, M4) are not transcribed here.  The vector-list
decoder below is written to be general over the V$JSR/V$RTS opcodes those
will need, but nothing in this file's RAW table uses them yet.

Every generated maze is gated against docs/DungeonsOfDaggorath/Levels/ --
the published maps of the real dungeon -- cell for cell and door for door,
which is the only external check this port has and the one that caught B83.

Run it (no stdout paste step -- five 32x32 mazes plus vector lists is not
something to hand-copy):

    python3 scripts/gen_daggorath.py

Writes:
    logo/games/daggorath          -- the block between "; BEGIN GENERATED
                                     DATA" and "; END GENERATED DATA" is
                                     rewritten in place. The game ships as
                                     ONE file: whole-row list literals cost
                                     exactly what a separate data file read
                                     with `readlist` cost (measured on the
                                     host: 27,082 free nodes against 27,025),
                                     so there is nothing to buy by splitting
                                     them out again.
    docs/DungeonsOfDaggorath/daggdata-reference.txt
                                  -- checked in for eyeball review, §11.2,
                                     now with the object tables beside the
                                     vector lists
"""

import pathlib
import re

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME_PATH = REPO_ROOT / "logo" / "games" / "daggorath"
REFERENCE_PATH = REPO_ROOT / "docs" / "DungeonsOfDaggorath" / "daggdata-reference.txt"
LEVELS_DIR = REPO_ROOT / "docs" / "DungeonsOfDaggorath" / "Levels"

BEGIN_MARKER = "; BEGIN GENERATED DATA"
END_MARKER = "; END GENERATED DATA"

# ===========================================================================
# RANDOM.ASM:RANDOX -- the 24-bit polynomial-method RNG.
#
# Eight outer shifts.  Each one masks SEED+2 (the byte holding the feedback
# taps) with %11100001, counts how many of those bits are set over eight
# LSLA/BCC/INCB shifts (which is a popcount, since every original bit of the
# masked byte passes through carry exactly once as it's shifted out), takes
# the LSB of that count (its parity) as the feedback bit, and rotates it into
# a 24-bit shift register held as three bytes, low byte first (ROL SEED /
# ROL SEED+1 / ROL SEED+2, carry propagating low-to-high).  The output byte
# is SEED (the low byte) after all eight shifts.
# ===========================================================================

FEEDBACK_MASK = 0xE1


class Random24:
    """RANDOM.ASM:RANDOX, one 24-bit shift-register instance per SEED."""

    def __init__(self, seed3):
        self.seed = list(seed3)  # [SEED, SEED+1, SEED+2] -- low, mid, high

    def next(self):
        for _ in range(8):
            masked = self.seed[2] & FEEDBACK_MASK
            feedback = bin(masked).count("1") & 1  # LSRB's carry-out
            carry = feedback
            for i in range(3):
                old = self.seed[i]
                self.seed[i] = ((old << 1) | carry) & 0xFF
                carry = (old >> 7) & 1
        return self.seed[0]

    def cell(self):
        """RNDCEL: a (row, col) pair, each mod 32.

        The COLUMN is the first draw, the row the second -- RNDCEL masks
        the first random, `TFR A,B` copies it into B (the column), and only
        then draws again into A (the row).  Getting this backwards
        transposes every maze about its diagonal and, because the carve
        walks from the cell it picks, produces a different dungeon
        entirely: 266 of 500 cells right instead of 500 (B83).
        """
        col = self.next() & 31
        row = self.next() & 31
        return row, col


# ===========================================================================
# DGNGEN.ASM:DGNGEN -- the maze carve.
#
# Horizontal maze features, CD.ASM: 00 passage, 01 door, 10 secret door,
# 11 wall, packed N E S W from the low bits up.
# ===========================================================================

HF_PAS, HF_DOR, HF_SDR, HF_WAL = 0, 1, 2, 3
N_WALL, E_WALL, S_WALL, W_WALL = 0b00000011, 0b00001100, 0b00110000, 0b11000000

# STPTAB, CRETUR.ASM: (d_row, d_col) by absolute direction, 0=N 1=E 2=S 3=W.
STPTAB = [(-1, 0), (0, 1), (1, 0), (0, -1)]

# LVLTAB, DGNGEN.ASM.  Seven active bytes (the `;DEBUG`-prefixed $25 ahead of
# them is commented out in the source and never assembled in).  Each level's
# 3-byte SEED is a *sliding window* into this table at a one-byte stride --
# `LDX #LVLTAB; LDB LEVEL; ABX` indexes by LEVEL bytes, not LEVEL*3 -- so
# levels 2-5 each reuse two of the previous level's seed bytes.  This is the
# one place this port disagrees with docs/daggorath-design.md's own
# paraphrase ("$73 $C7 $5D $97 $F3 for the five levels" reads as five
# independent single-byte seeds); the ASM is the source of truth per the
# design doc's own header, and the sliding window is transcribed here.
LVLTAB = [0x73, 0xC7, 0x5D, 0x97, 0xF3, 0x13, 0x87]

CELLS_TO_CARVE = 500
REGULAR_DOORS = 70
SECRET_DOORS = 45

# MSKTAB / DORTAB / SDRTAB, DGNGEN.ASM, indexed by direction 0=N 1=E 2=S 3=W.
MSKTAB = [N_WALL, E_WALL, S_WALL, W_WALL]
DORTAB = [HF_DOR, HF_DOR * 4, HF_DOR * 16, HF_DOR * 64]
SDRTAB = [HF_SDR, HF_SDR * 4, HF_SDR * 16, HF_SDR * 64]
WALLTAB = [N_WALL, E_WALL, S_WALL, W_WALL]


def _neighbor(maze, row, col, d_row, d_col):
    """FRIEND's per-cell read: a cell outside 0..31 reads as $FF (BORDER)."""
    r, c = row + d_row, col + d_col
    if not (0 <= r <= 31 and 0 <= c <= 31):
        return 0xFF
    return maze[r * 32 + c]


def carve_maze(seed3):
    """DGNGEN Phase I + Phase II: carve 500 cells, then wall the rest.

    Returns (maze, rng) -- the rng is handed on to place_doors() so Phase
    III continues the same RNG stream DGNGEN itself runs on, uninterrupted.
    """
    maze = [0xFF] * 1024
    rng = Random24(seed3)

    drow, dcol = rng.cell()
    remaining = CELLS_TO_CARVE
    while remaining > 0:
        direction = rng.next() & 3
        dist = (rng.next() & 7) + 1
        d_row, d_col = STPTAB[direction]
        while True:
            nrow, ncol = drow + d_row, dcol + d_col
            if not (0 <= nrow <= 31 and 0 <= ncol <= 31):
                break  # out of bounds -- pick a new direction/distance
            idx = nrow * 32 + ncol
            if maze[idx] == 0:
                # DGEN20: already-carved cell, just advance through it.
                drow, dcol = nrow, ncol
                dist -= 1
                if dist == 0:
                    break
                continue
            # Virgin ($FF) cell: reject if any 2x2 corner it would complete
            # is already fully carved -- DGNGEN's four BEQ DGEN10 checks.
            nw = _neighbor(maze, nrow, ncol, -1, -1)
            n = _neighbor(maze, nrow, ncol, -1, 0)
            ne = _neighbor(maze, nrow, ncol, -1, 1)
            w = _neighbor(maze, nrow, ncol, 0, -1)
            e = _neighbor(maze, nrow, ncol, 0, 1)
            sw = _neighbor(maze, nrow, ncol, 1, -1)
            s = _neighbor(maze, nrow, ncol, 1, 0)
            se = _neighbor(maze, nrow, ncol, 1, 1)
            if (nw == 0 and n == 0 and w == 0) or \
               (n == 0 and ne == 0 and e == 0) or \
               (e == 0 and se == 0 and s == 0) or \
               (w == 0 and sw == 0 and s == 0):
                break
            maze[idx] = 0
            remaining -= 1
            if remaining == 0:
                break
            drow, dcol = nrow, ncol
            dist -= 1
            if dist == 0:
                break

    # Phase II: wall every side of every carved cell that faces a virgin
    # ($FF) cell or the grid edge (which _neighbor also reads as $FF).
    for row in range(32):
        for col in range(32):
            idx = row * 32 + col
            if maze[idx] == 0xFF:
                continue
            bits = 0
            for direction, (d_row, d_col) in enumerate(STPTAB):
                if _neighbor(maze, row, col, d_row, d_col) == 0xFF:
                    bits |= WALLTAB[direction]
            maze[idx] |= bits

    return maze, rng


def _place_one_door(maze, rng, table):
    """MAKDOR: one door, on the running RNG stream (retries included).

    MDOR10's retry (`BITB A,Y; BNE MDOR10`) rejects back to the *cell* pick,
    not just the direction pick -- a cell whose sides are all already
    walls/doors would spin forever if only the direction were re-rolled.
    """
    while True:
        row, col = rng.cell()
        idx = row * 32 + col
        if maze[idx] == 0xFF:
            continue
        direction = rng.next() & 3
        if maze[idx] & MSKTAB[direction] == 0:
            break
    maze[idx] |= table[direction]

    d_row, d_col = STPTAB[direction]
    nrow, ncol = (row + d_row) % 32, (col + d_col) % 32
    nidx = nrow * 32 + ncol
    opposite = (direction + 2) & 3
    maze[nidx] |= table[opposite]


def place_doors(maze, rng):
    """DGNGEN Phase III: 70 regular doors, then 45 secret doors."""
    for _ in range(REGULAR_DOORS):
        _place_one_door(maze, rng, DORTAB)
    for _ in range(SECRET_DOORS):
        _place_one_door(maze, rng, SDRTAB)


def generate_level(level_index):
    """0-based level_index 0..4 -- displayed level 1..5."""
    seed3 = LVLTAB[level_index:level_index + 3]
    maze, rng = carve_maze(seed3)
    place_doors(maze, rng)
    return maze


# ===========================================================================
# Internal-consistency checks.  These are no longer the only gate: since
# docs/DungeonsOfDaggorath/Levels/ arrived, tests/test_daggorath.c diffs all
# five generated mazes cell-for-cell against the published maps, which is
# what caught B83.  These invariants stay as the cheap check that runs
# inside the generator itself.
# ===========================================================================

def check_maze(maze, start_row=None, start_col=None):
    """Connectivity is checked from the first carved cell unless a start is
    named -- only level 1 has a fixed player start, and CLIMB drops you on a
    random cell on every level below it."""
    open_cells = [i for i, v in enumerate(maze) if v != 0xFF]
    assert len(open_cells) == CELLS_TO_CARVE, \
        f"expected {CELLS_TO_CARVE} open cells, got {len(open_cells)}"

    doors = secret_doors = 0
    for v in maze:
        if v == 0xFF:
            continue
        for shift in (0, 2, 4, 6):
            code = (v >> shift) & 0b11
            if code == HF_DOR:
                doors += 1
            elif code == HF_SDR:
                secret_doors += 1
    assert doors == REGULAR_DOORS * 2, \
        f"expected {REGULAR_DOORS * 2} door bit-pairs (each door touches " \
        f"two cells), got {doors}"
    assert secret_doors == SECRET_DOORS * 2, \
        f"expected {SECRET_DOORS * 2} secret-door bit-pairs, got {secret_doors}"

    # Flood fill from (start_row, start_col) through passages/doors/secret
    # doors (anything but a wall) must reach every open cell -- carving
    # guarantees this by construction, so it is a regression check on the
    # carve/wall port rather than a new requirement.
    if start_row is None:
        start_idx = open_cells[0]
        start_row, start_col = divmod(start_idx, 32)
    else:
        start_idx = start_row * 32 + start_col
    assert maze[start_idx] != 0xFF, \
        f"start cell ({start_row},{start_col}) is not part of the maze"
    seen = {start_idx}
    frontier = [start_idx]
    while frontier:
        idx = frontier.pop()
        row, col = divmod(idx, 32)
        cell = maze[idx]
        for direction, (d_row, d_col) in enumerate(STPTAB):
            code = (cell >> (direction * 2)) & 0b11
            if code == HF_WAL:
                continue
            nrow, ncol = row + d_row, col + d_col
            if not (0 <= nrow <= 31 and 0 <= ncol <= 31):
                continue
            nidx = nrow * 32 + ncol
            if nidx not in seen and maze[nidx] != 0xFF:
                seen.add(nidx)
                frontier.append(nidx)
    assert len(seen) == CELLS_TO_CARVE, \
        f"flood fill from ({start_row},{start_col}) reached {len(seen)} " \
        f"of {CELLS_TO_CARVE} open cells -- maze is not fully connected"


# ===========================================================================
# TOKEN.ASM / DTABAS.ASM -- the object tables, read out of the ROM rather
# than transcribed from it.
#
# M1's LVLTAB and M2's CMDTAB were both cases of this design reading a MACRO
# and believing it was the table.  DTABAS.ASM's OBJXXX macro carries a name
# beside every object -- SUPREME, HOTH, THEWS -- and those names are the
# macro's own first argument, not what the player types.  TOKEN.ASM's ADJTAB
# holds the words, packed five bits to a letter, and one of them disagrees:
# the macro says HOTH and the table says RIME.
#
# So the names come from TOKEN.ASM and the numbers come from DTABAS.ASM,
# both parsed here, and the two are cross-checked on the one field they
# share -- the object class, which TOKEN.ASM carries in every packed string
# and DTABAS.ASM carries in every macro call.  Twenty-five agreements is
# what makes it safe to pair the two tables by POSITION, which is how the
# ROM itself pairs them (P.OCTYP indexes ODBTAB in OCBFIX and ADJTAB in
# OBJNAM, with no name matching anywhere).
#
# The packed string is: 5 bits of length, 5 bits of class, then one 5-bit
# letter each (A = 1), MSB first, spilling across the FCB bytes.  EXPAND.ASM
# unpacks it into STRING+0 (count), STRING+1 (class), STRING+2.. (chars),
# which is why STATUS.ASM:OBJNAM copies from `ADJTAB+1` and PARSER returns
# `STRING+1` as the token class.
# ===========================================================================

TOKEN_PATH = REPO_ROOT / "docs" / "DungeonsOfDaggorath" / "TOKEN.ASM"
DTABAS_PATH = REPO_ROOT / "docs" / "DungeonsOfDaggorath" / "DTABAS.ASM"

_XDEF = re.compile(r"^\s+XDEF\s+(T\.\w+),")
_FCB_BITS = re.compile(r"^\s+FCB\s+%([01]{8})")


def read_token_table(name):
    """One of TOKEN.ASM's four token tables, as [(symbol, word, class), ...].

    The debug commands between `IF DEBFLG` and `ENDIF` are assembled only
    when DEBFLG is set, and it is 0, so they are skipped -- QMAP is not a
    command this game has.
    """
    text = TOKEN_PATH.read_text()
    start = text.index("\n%s  FCB" % name)
    end = text.index("\n%sNUM  EQU" % name[:3], start)
    body = text[start:end]
    out, packed, sym, skipping = [], [], None, False
    for line in body.splitlines():
        stripped = line.strip()
        if stripped.startswith("IF ") and "DEBFLG" in stripped:
            skipping = True
        elif stripped.startswith("ENDIF"):
            skipping = False
        if skipping:
            continue
        m = _XDEF.match(line)
        if m:
            if sym is not None:
                out.append((sym, packed))
            sym, packed = m.group(1), []
            continue
        m = _FCB_BITS.match(line)
        if m and sym is not None:
            packed.append(int(m.group(1), 2))
    if sym is not None:
        out.append((sym, packed))
    return [(s, *_unpack_string(b)) for s, b in out]


def _unpack_string(packed):
    """EXPAND.ASM's five-bit unpacking: count, class, then the letters."""
    bits = "".join(format(b, "08b") for b in packed)
    count = int(bits[0:5], 2)
    cls = int(bits[5:10], 2)
    word = ""
    for i in range(count):
        v = int(bits[10 + 5 * i:15 + 5 * i], 2)
        word += chr(ord("A") + v - 1) if v else " "
    return word, cls


_MACRO_ARGS = re.compile(r"^\s+\\[12]\s+([A-Z0-9$.,]+)\s*$")

CLASS_NUMBER = {"K.FLAS": 0, "K.RING": 1, "K.SCRO": 2,
                "K.SHIE": 3, "K.SWOR": 4, "K.TORC": 5}


def read_dtabas_macro(name):
    """The argument lists inside one of DTABAS.ASM's definition macros."""
    text = DTABAS_PATH.read_text()
    start = text.index("\n%s  MACR" % name)
    end = text.index("ENDM", start)
    rows = []
    for line in text[start:end].splitlines():
        m = _MACRO_ARGS.match(line)
        if m:
            rows.append(m.group(1).split(","))
    return rows


def read_object_tables():
    """ODBTAB, XXXTAB, OMXTAB, OBJWGT and the two token tables, joined.

    Returns (generics, objects, genval): `generics` is the six-entry class
    table [(word, weight), ...], `objects` the twenty-five-entry type table
    [(word, class, reveal, magoff, physoff, level, count, xxx), ...] with
    `xxx` either None or the three special-parameter bytes, and `genval`
    OBIRTH.ASM's per-class translation table resolved to type numbers.
    """
    gentab = read_token_table("GENTAB")
    adjtab = read_token_table("ADJTAB")
    by_symbol = {sym: i for i, (sym, _, _) in enumerate(adjtab)}

    genxxx = read_dtabas_macro("GENXXX")
    assert len(genxxx) == len(gentab) == 6, "GENTAB and GENXXX disagree"
    generics = []
    for i, ((_, word, cls), args) in enumerate(zip(gentab, genxxx)):
        assert cls == i, f"{word}: GENTAB class {cls} is not its own index"
        assert CLASS_NUMBER[args[2]] == i, f"{word}: GENXXX class disagrees"
        generics.append((word, int(args[5])))

    rows = read_dtabas_macro("OBJXXX") + read_dtabas_macro("SPCXXX")
    assert len(rows) == len(adjtab) == 25, "ADJTAB and OBJXXX/SPCXXX disagree"
    objects = []
    for (sym, word, cls), args in zip(adjtab, rows):
        # This is the whole cross-check: the class TOKEN.ASM packed into the
        # word and the class DTABAS.ASM wrote in the macro call, for all
        # twenty-five, pairing the two tables by position the way the ROM
        # does.  It is also what says RIME and HOTH are the same object.
        assert CLASS_NUMBER[args[2]] == cls, (
            f"{word}/{args[0]}: TOKEN.ASM class {cls} against "
            f"DTABAS.ASM {args[2]}")
        reveal, magoff, physoff = (int(a) for a in args[3:6])
        # OBJXXX writes the level as LVL0..LVL5; SPCXXX's own eleven-arg
        # row (DEAD) writes a plain 0 in the same place.
        level = 0
        if len(args) > 6:
            level = int(args[6][3:] if args[6].startswith("LVL") else args[6])
        count = int(args[7]) if len(args) > 7 else 0
        xxx = None
        if len(args) > 8:
            # XXXTAB's three bytes.  For a ring the second is the type it
            # INCANTs into, written as the T.RNxx symbol (PINCAN.ASM reads
            # it out of P.OCXXX+1 and compares it against an ADJTAB match),
            # so it resolves through the same symbol table.
            xxx = [by_symbol[a] if a.startswith("T.") else int(a)
                   for a in args[8:11]]
        objects.append((word, cls, reveal, magoff, physoff, level, count, xxx))

    genval = [-1 if s is None else by_symbol[s] for s in GENVAL_SYMBOLS]
    return generics, objects, genval


def check_command_tables():
    """CMDTAB and DIRTAB, decoded, against what logo/games/daggorath holds.

    Those two tables are hand-written in the game file rather than
    generated -- they are code, one `run`-able instruction to a row -- but
    the WORDS in them are the same packed strings this file already
    decodes, and M2 got them wrong once by reading DTABAS.ASM's CMDXXX
    macro instead (ATTK, INCN, ZSAV are assembler symbols).  So they are
    checked here rather than trusted.
    """
    commands = [word for _, word, _ in read_token_table("CMDTAB")]
    assert commands == [
        "ATTACK", "CLIMB", "DROP", "EXAMINE", "GET", "INCANT", "LOOK",
        "MOVE", "PULL", "REVEAL", "STOW", "TURN", "USE", "ZLOAD", "ZSAVE",
    ], commands
    directions = [word for _, word, _ in read_token_table("DIRTAB")]
    assert directions == [
        "LEFT", "RIGHT", "BACK", "AROUND", "UP", "DOWN",
    ], directions


# OBIRTH.ASM:GENVAL, by class: -1 leaves the object as it was born, anything
# else is the type whose parameters an unrevealed object of that class wears.
# So an unrevealed Mithril shield fights like a Leather one until REVEAL
# gives it its own numbers back (PREVEA.ASM:PREV00's second OCBFIL).
GENVAL_SYMBOLS = [None, None, None, "T.SHI4", "T.SWO3", "T.TOR4"]


# ===========================================================================
# VARC.ASM / VERT.ASM -- the vector-list decoder.
#
# Six control codes, read off VCTLST.ASM's own dispatch table (VCTDIS,
# indexed by byte - V$RTS) and cross-checked against missing-macros.asm's
# SVEND ("SVNEW; FCB V$END" where SVNEW is "FCB V$ABS") -- SVEND's own
# expansion is what pins V$ABS to 0 rather than one of the FA-FF codes.
# ===========================================================================

V_ABS, V_RTS, V_JSR, V_REL, V_JMP, V_END, V_NEW = 0x00, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF


def _decode_relative_byte(byte):
    """VCTLST.ASM:VCTREL's nybble extraction: high nybble dy, low nybble dx,
    each a signed 4-bit two's-complement value doubled (SVECT computes
    nybble = (delta/2) & $F; VCTREL's ASRB x4/LSLB and ANDB/sign-extend/LSLB
    both undo that halving)."""
    hi, lo = (byte >> 4) & 0xF, byte & 0xF
    if hi >= 8:
        hi -= 16
    if lo >= 8:
        lo -= 16
    return hi * 2, lo * 2


def decode(name, raw_table, memo=None):
    """Walk RAW_TABLE[name] into runs: a list of pen-down polylines, each a
    flat list of (y, x) absolute points -- design section 11.2's "flat
    absolute polylines."  V$JMP splices the target list's own runs on the
    end (a door superimposed on its wall); V$NEW starts a new run (a pen
    lift); V$REL walks nybble-encoded deltas until a V$ABS terminator.

    V$JSR/V$RTS (VERT.ASM's LADDER, chained from FLUP/FLDN) are not needed
    by anything in this file's RAW table -- M1 only draws CELINE, the plain
    ceiling line, not the ladder/hole lists (M5, once VFTTAB exists) -- so
    they are left unimplemented rather than guessed at.
    """
    if memo is None:
        memo = {}
    if name in memo:
        return memo[name]
    tokens = raw_table[name]
    runs, cur = [], []
    y = x = 0
    i = 0
    while i < len(tokens):
        t = tokens[i]
        if t == V_END:
            break
        if t == V_NEW:
            if cur:
                runs.append(cur)
                cur = []
            i += 1
        elif t == V_JMP:
            if cur:
                runs.append(cur)
                cur = []
            runs.extend(decode(tokens[i + 1], raw_table, memo))
            break
        elif t == V_JSR or t == V_RTS:
            raise NotImplementedError(
                f"{name}: V$JSR/V$RTS not needed by M1's RAW table")
        elif t == V_REL:
            i += 1
            while tokens[i] != V_ABS:
                dy, dx = _decode_relative_byte(tokens[i])
                y, x = y + dy, x + dx
                cur.append((y, x))
                i += 1
            i += 1  # consume V$ABS
            if cur:
                runs.append(cur)
                cur = []
        else:
            y, x = t, tokens[i + 1]
            cur.append((y, x))
            i += 2
    if cur:
        runs.append(cur)
    memo[name] = runs
    return runs


# VARC.ASM, transcribed verbatim (comments give the source line's own
# gloss).  Grouped left/forward/right x passage/door/secretdoor/wall, the
# same grouping FLATAB.ASM dispatches on, so the Logo loader can read this
# order straight into its own per-direction dispatch lists.
RAW = {
    "LPASAG": [38, 29, 38, 64, 114, 64, 114, 27, V_NEW,          # open rect
               16, 27, 38, 64, V_END],                            # top-wall continuation
    "LDOOR": [128, 40, 65, 40, 68, 56, 119, 56, V_NEW,             # door frame
              92, 48, 93, 52,                                      # doorknob
              V_JMP, "LWALL"],
    "LSDOOR": [128, 40, 66, 50, 117, 58, V_END],                  # triangle
    "LWALL": [16, 27, 38, 64, 114, 64, 136, 27, V_END],

    "FPASAG": [V_END],  # empty: an open passage ahead draws nothing
    "FDOOR": [114, 108, 67, 108, 67, 148, 114, 148, V_NEW,
              94, 126, 94, 130,
              V_JMP, "FWALL"],
    "FSDOOR": [113, 108, 67, 128, 114, 148, V_END],
    "FWALL": [38, 64, 38, 192, V_NEW,
              114, 64, 114, 192, V_END],

    "RPASAG": [38, 229, 38, 192, 114, 192, 114, 229, V_NEW,
               16, 229, 38, 192, V_END],
    "RDOOR": [128, 216, 65, 216, 68, 200, 119, 200, V_NEW,
              92, 208, 93, 204,
              V_JMP, "RWALL"],
    "RSDOOR": [128, 216, 66, 206, 117, 198, V_END],
    "RWALL": [16, 229, 38, 192, 114, 192, 136, 229, V_END],

    # SVORG 100,28 / SVECT ... / SVEND -- transcribed as the encoded bytes
    # the macro would assemble, so decode() exercises the same V$REL path
    # M3/M4's D3.ASM/D4.ASM creature data may need.
    "LPEEK": [100, 28, V_REL, 0x44, 0x2E, 0x42, 0x4C, V_ABS, V_END],
    "RPEEK": [100, 228, V_REL, 0x4C, 0x22, 0x4E, 0x44, V_ABS, V_END],

    # VERT.ASM -- only the plain ceiling line; see the M1 scope note above.
    "CELINE": [28, 47, 28, 210, V_END],

    # VOBJ.ASM -- the six objects seen lying on the floor, indexed by CLASS
    # (VIEWER.ASM:VIEW52 reads FWDOBJ with P.OCCLS, not the type), so there
    # is one outline for every torch and one for every sword.  Transcribed
    # as the bytes the SVORG/SVECT macros assemble: SVECT's nybble is
    # ((delta / 2) & $F), so a -2 step is $F and FTORCH's second vector is
    # the byte $FF -- which is V$NEW everywhere except inside a relative
    # run, where VCTREL tests only for zero (V$ABS).  Writing the absolute
    # points here instead would have hidden that.
    "FFLASK": [110, 162, V_REL, 0x51, 0x0E, 0xB1, V_ABS, V_END],
    "FRING": [122, 60, V_REL, 0x11, 0x1F, 0xFF, 0xF1, V_ABS, V_END],
    "FSCROL": [118, 194, V_REL, 0x1F, 0x34, 0xF1, 0xDC, V_ABS, V_END],
    # FSHIEL's SVORG is not a pen lift: it emits an absolute pair and then
    # V$REL, and DRWFLG is already set, so (128,168) continues the polyline
    # the three absolute pairs started.  One run of six points.
    "FSHIEL": [134, 172, 128, 192, 122, 186,
               128, 168, V_REL, 0x3E, 0x04, V_ABS, V_END],
    "FSWORD": [114, 80, 124, 100, V_NEW,
               118, 82, 114, 86, V_END],
    "FTORCH": [118, 60, V_REL, 0xF7, 0xFF, 0x2A, V_ABS, V_END],
}

# Fixed order: the Logo loader reads this many records, in this order, by
# position -- see logo/games/daggorath's `dagg.load` for the matching read.
VECTOR_LIST_ORDER = [
    "LPASAG", "LDOOR", "LSDOOR", "LWALL",
    "FPASAG", "FDOOR", "FSDOOR", "FWALL",
    "RPASAG", "RDOOR", "RSDOOR", "RWALL",
    "LPEEK", "RPEEK",
    "CELINE",
    # FWDOBJ's own order, which is GENXXX's, which is the class number.
    "FFLASK", "FRING", "FSCROL", "FSHIEL", "FSWORD", "FTORCH",
]

FWDOBJ = ["FFLASK", "FRING", "FSCROL", "FSHIEL", "FSWORD", "FTORCH"]


# ===========================================================================
# Output
# ===========================================================================

def object_table_lines(generics, objects, genval, decoded):
    """The §10.2 tables, as Logo statements, in ROM order.

    Positions are what the ROM uses and what the game reads: an object's
    TYPE is its row in `dagg.adjtab`/`dagg.odb`/`dagg.xxx` (P.OCTYP indexes
    ODBTAB in OCBFIX and ADJTAB in OBJNAM), and its CLASS is its row in
    `dagg.gentab`/`dagg.objwgt`/`dagg.fobj` (P.OCCLS indexes OBJWGT in PGET
    and FWDOBJ in VIEWER).  Both are zero-based in the ROM and one-based
    here, because `item` counts from one.
    """
    lines = []

    names = " ".join("[%s]" % word for word, _ in generics)
    lines.append('make "dagg.gentab [%s]' % names)
    lines.append('make "dagg.objwgt [%s]'
                 % " ".join(str(weight) for _, weight in generics))

    # ADJTAB carries the class beside the word because PARSER returns it
    # (STRING+1), which is how PAROBJ rejects "PINE SWORD".
    lines.append('make "dagg.adjtab []')
    for word, cls, *_ in objects:
        lines.append('make "dagg.adjtab lput [%s %d] :dagg.adjtab' % (word, cls))

    # ODBTAB: class, reveal, magic offense, physical offense -- the four
    # bytes OCBFIX copies into P.OCCLS..P.OCPHO.
    lines.append('make "dagg.odb []')
    for _, cls, reveal, magoff, physoff, _, _, _ in objects:
        lines.append('make "dagg.odb lput [%d %d %d %d] :dagg.odb'
                     % (cls, reveal, magoff, physoff))

    # XXXTAB: torch timer/regular/magic, shield magic/physical filters, ring
    # charges/incantation.  An empty list is a type with no entry, which
    # OCBFIX leaves alone rather than zeroing.
    lines.append('make "dagg.xxx []')
    for *_, xxx in objects:
        body = " ".join(str(v) for v in xxx) if xxx else ""
        lines.append('make "dagg.xxx lput [%s] :dagg.xxx' % body)

    # OMXTAB, the eighteen real objects only -- SPCXXX has no distribution
    # entry (`OBJXXX OMX,OMX`, not SPCXXX), because nothing is born special.
    lines.append('make "dagg.omx []')
    for _, _, _, _, _, level, count, _ in objects:
        if count:
            lines.append('make "dagg.omx lput [%d %d] :dagg.omx' % (level, count))

    lines.append('make "dagg.genval [%s]'
                 % " ".join(str(v) for v in genval))

    def runs_literal(name):
        parts = []
        for run in decoded[name]:
            ys = " ".join(str(y) for y, _ in run)
            xs = " ".join(str(x) for _, x in run)
            parts.append("[[%s] [%s]]" % (ys, xs))
        return " ".join(parts)

    for i, name in enumerate(FWDOBJ):
        lines.append('make "v%d [%s]' % (i, runs_literal(name)))
    lines.append('make "dagg.fobj (list :v0 :v1 :v2 :v3 :v4 :v5)')
    for i in range(6):
        lines.append('make "v%d []' % i)
    return lines


def game_data_lines(mazes, decoded, generics, objects, genval):
    """The Logo statements that put the maze and the vector lists in memory.

    One statement to a line, because `load` only buffers `to ... end`
    blocks -- every other line is lexed and run on its own, so a literal
    that spans lines does not parse.  A maze row is one line: 32 numbers is
    up to 151 characters, well inside LOAD_MAX_LINE (256), and a whole-row
    literal costs exactly what `readlist` used to.  Do NOT be tempted to
    break the rows up to honour the 40-column source rule -- reassembling
    them with `se` retains ~10,900 word-table entries.
    """
    lines = ['make "dagg.mazes []']
    for maze in mazes:
        lines.append('make "rows []')
        for row in range(32):
            cells = " ".join(str(v) for v in maze[row * 32:(row + 1) * 32])
            lines.append(f'make "rows lput [{cells}] :rows')
        lines.append('make "dagg.mazes lput :rows :dagg.mazes')
    lines.append('make "rows []')

    def runs_literal(name):
        """One vector list: runs of parallel ys/xs, the split done here."""
        parts = []
        for run in decoded[name]:
            ys = " ".join(str(y) for y, _ in run)
            xs = " ".join(str(x) for _, x in run)
            parts.append(f"[[{ys}] [{xs}]]")
        return " ".join(parts)

    # FLATAB's own order: each side's four lists are indexed by the wall
    # code (0 passage, 1 door, 2 secret door, 3 wall), so they go into one
    # list of four in exactly that order -- see dagg.side.runs.
    for var, group in (("dagg.left", ("LPASAG", "LDOOR", "LSDOOR", "LWALL")),
                       ("dagg.forward", ("FPASAG", "FDOOR", "FSDOOR", "FWALL")),
                       ("dagg.right", ("RPASAG", "RDOOR", "RSDOOR", "RWALL"))):
        for i, name in enumerate(group):
            lines.append(f'make "v{i} [{runs_literal(name)}]')
        lines.append(f'make "{var} (list :v0 :v1 :v2 :v3)')
    for i in range(4):
        lines.append(f'make "v{i} []')
    for var, name in (("dagg.lpeek", "LPEEK"), ("dagg.rpeek", "RPEEK"),
                      ("dagg.celine", "CELINE")):
        lines.append(f'make "{var} [{runs_literal(name)}]')
    lines.extend(object_table_lines(generics, objects, genval, decoded))
    return lines


def write_game_data(mazes, decoded, tables, path):
    """Rewrite the generated block inside logo/games/daggorath in place."""
    text = path.read_text()
    begin = text.index(BEGIN_MARKER)
    end = text.index(END_MARKER)
    body = "\n".join(game_data_lines(mazes, decoded, *tables))
    new = text[:begin] + BEGIN_MARKER + "\n" + body + "\n" + text[end:]

    longest = max(len(line) for line in body.splitlines())
    assert longest < 256, f"a generated line is {longest} chars, over LOAD_MAX_LINE"
    path.write_text(new)
    return longest


def write_reference(mazes, decoded, tables, path):
    lines = [
        "Generated by scripts/gen_daggorath.py -- do not hand-edit.",
        "Checked in per docs/daggorath-design.md section 11.2: a wrong",
        "nybble or a mistranscribed byte is visible here rather than",
        "merely present in the generated block of logo/games/daggorath.",
        "",
    ]
    for level, maze in enumerate(mazes, start=1):
        open_cells = sum(1 for v in maze if v != 0xFF)
        lines.append(f"level {level}: {open_cells} open cells")
    lines.append("")
    for name in VECTOR_LIST_ORDER:
        runs = decoded[name]
        lines.append(f"{name}: {len(runs)} run(s)")
        for run in runs:
            lines.append("  " + " -> ".join(f"({y},{x})" for y, x in run))

    generics, objects, genval = tables
    lines.append("")
    lines.append("classes (GENTAB word, OBJWGT weight):")
    for i, (word, weight) in enumerate(generics):
        lines.append(f"  {i} {word:8s} weight {weight}")
    lines.append("")
    lines.append("objects (ADJTAB word, ODBTAB, OMXTAB, XXXTAB):")
    header = ("   # word     cls reveal magoff physoff level count "
              "special")
    lines.append(header)
    for i, (word, cls, reveal, magoff, physoff, level, count, xxx) in \
            enumerate(objects):
        special = " ".join(str(v) for v in xxx) if xxx else "-"
        lines.append(f"  {i:2d} {word:8s} {cls:3d} {reveal:6d} {magoff:6d} "
                     f"{physoff:7d} {level:5d} {count:5d} {special}")
    lines.append("")
    lines.append(f"GENVAL (by class): {genval}")
    path.write_text("\n".join(lines) + "\n")


# ===========================================================================
# The published maps -- docs/DungeonsOfDaggorath/Levels/level{1..5}.svg, the
# real dungeon drawn cell by cell.  This is the only *external* check the
# port has: everything else here is the ASM checking itself.
#
# The map's vocabulary is small.  Rock is the grey page; a carved cell is a
# 50x50 white rect at translate(50 + 50*col, 50 + 50*row); and the only
# lines drawn on a cell edge are the doors -- solid green for a regular
# door, dashed red for a secret one.  There is deliberately no line for a
# plain wall, because DGNGEN Phase II only ever walls a side that faces
# uncarved rock, and rock is already visible as grey.  So an edge between
# two carved cells with no line on it is an open passage, and that is
# exactly the assertion below.
#
# This is what caught B83: the port had RNDCEL's row and column the wrong
# way round, which transposed every maze and left the generator emitting
# five plausible dungeons that were not Daggorath's.  The invariants in
# check_maze() all passed happily throughout.
# ===========================================================================

_CELL_GROUP = re.compile(r"<g transform='translate\((\d+),(\d+)\)'>\n(.*?)(?=\n</g>)", re.S)
_EDGE_LINE = re.compile(
    r"<line x1='(\d+)' y1='(\d+)' x2='(\d+)' y2='(\d+)' style='stroke:rgb\(([0-9,]+)\);stroke-width:4")
_MAP_CODE = {"0,255,0": HF_DOR, "255,0,0": HF_SDR}


def read_published_map(path):
    """(carved cells, {(row, col, direction): code}) from one level SVG."""
    carved, edges = set(), {}
    for x, y, body in _CELL_GROUP.findall(path.read_text()):
        x, y = int(x), int(y)
        if x < 50 or y < 50:
            continue
        col, row = (x - 50) // 50, (y - 50) // 50
        if not (0 <= row <= 31 and 0 <= col <= 31):
            continue
        if "fill:rgb(250,250,250)" in body:
            carved.add((row, col))
        for x1, y1, x2, y2, colour in _EDGE_LINE.findall(body):
            x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
            if x1 == x2 == 0:
                direction = 3      # W
            elif x1 == x2 == 50:
                direction = 1      # E
            elif y1 == y2 == 0:
                direction = 0      # N
            elif y1 == y2 == 50:
                direction = 2      # S
            else:
                continue           # a glyph inside the cell, not an edge
            edges[(row, col, direction)] = _MAP_CODE[colour]
    return carved, edges


def check_against_published_map(maze, level_index):
    path = LEVELS_DIR / f"level{level_index + 1}.svg"
    carved, edges = read_published_map(path)

    ours = {(r, c) for r in range(32) for c in range(32) if maze[r * 32 + c] != 0xFF}
    assert ours == carved, (
        f"level {level_index + 1}: {len(ours ^ carved)} cells differ from "
        f"{path.name} ({len(ours)} carved here, {len(carved)} on the map)")

    wrong = []
    for row, col in sorted(carved):
        cell = maze[row * 32 + col]
        for direction, (d_row, d_col) in enumerate(STPTAB):
            if (row + d_row, col + d_col) not in carved:
                continue  # the map draws no line against rock -- nothing to compare
            want = edges.get((row, col, direction), HF_PAS)
            got = (cell >> (direction * 2)) & 0b11
            if got != want:
                wrong.append((row, col, "NESW"[direction], got, want))
    assert not wrong, (
        f"level {level_index + 1}: {len(wrong)} interior edges differ from "
        f"{path.name}, first few {wrong[:5]}")


def main():
    mazes = [generate_level(i) for i in range(5)]
    for level_index, maze in enumerate(mazes):
        check_maze(maze)
        check_against_published_map(maze, level_index)
    # ONCE.ASM:GAME10 -- `LDD #$100B / STD PROW` is row 16, column 11, which
    # is where the published level-1 map draws the player's blue dot.  (The
    # `FCB 12 / FCB 22` in COMDAT.ASM's ONCE-only block is the attract-mode
    # DEMO's position; GAME10 overwrites it before a real game, and (12,22)
    # is not a carved cell in the true level 1 at all.)
    check_maze(mazes[0], start_row=16, start_col=11)

    decoded = {name: decode(name, RAW) for name in VECTOR_LIST_ORDER}
    tables = read_object_tables()
    check_command_tables()

    longest = write_game_data(mazes, decoded, tables, GAME_PATH)
    REFERENCE_PATH.parent.mkdir(parents=True, exist_ok=True)
    write_reference(mazes, decoded, tables, REFERENCE_PATH)
    print(f"rewrote the generated block in {GAME_PATH} "
          f"(longest line {longest} chars)")
    print(f"wrote {REFERENCE_PATH}")


if __name__ == "__main__":
    main()
