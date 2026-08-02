//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tile bank, tile map, and bake primitives (P9 M1/M2,
//  docs/tilemap-scrolling-design.md §5-§6).
//
//  `newtiles` and `snaptile` fill a bank of square tiles by capturing them
//  from the canvas, so a game ships its artwork as the code that draws it.
//  `newmap`, `settile` and `tile` are the world itself -- one byte a cell,
//  O(1) to read, which is what lets a game use the map for collision instead
//  of a parallel Logo list. `stampmap` and `stamptile` bake the map into the
//  canvas: a C loop in place of hundreds of Logo stamps, after which the
//  baked pixels are ordinary canvas that the pen draws over.
//
//  The storage and the sampler live in core/tilemap.c; this file is the Logo
//  surface plus the two device calls the bake needs (capture a canvas region,
//  write a run of canvas pixels).
//

#include "primitives.h"
#include "error.h"
#include "limits.h"
#include "tilemap.h"
#include "devices/io.h"

// Get the console's turtle operations, or NULL if not available
static const LogoConsoleTurtle *get_turtle_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
        return NULL;
    return io->console->turtle;
}

// True when a float names an integer in [lo, hi].
static bool is_int_in(float v, int lo, int hi)
{
    return v == (float)(int)v && (int)v >= lo && (int)v <= hi;
}

static int wrap_mod(int v, int m)
{
    v %= m;
    return (v < 0) ? v + m : v;
}

// The screen rectangle the map is drawn through: the viewport, resolved
// against the device's screen size (a zero width or height means "the whole
// graphics area") and clipped to it. False when nothing is visible.
static bool view_rect(const LogoConsoleTurtle *turtle, int *x, int *y, int *w, int *h)
{
    int screen_w = 0, screen_h = 0;
    bool wrap = false;
    if (turtle->sense_metrics)
    {
        turtle->sense_metrics(&screen_w, &screen_h, &wrap);
    }

    tilemap_get_viewport(x, y, w, h);
    if (*w <= 0) *w = screen_w - *x;
    if (*h <= 0) *h = screen_h - *y;

    if (*x + *w > screen_w) *w = screen_w - *x;
    if (*y + *h > screen_h) *h = screen_h - *y;

    return (*x >= 0 && *y >= 0 && *w > 0 && *h > 0);
}

// Bake a screen rectangle from the map into the canvas, a row at a time.
static void bake_rect(const LogoConsoleTurtle *turtle, int x, int y, int w, int h)
{
    static uint8_t row[TILEMAP_ROW_MAX];

    if (w > TILEMAP_ROW_MAX)
    {
        w = TILEMAP_ROW_MAX;
    }

    uint8_t bg = turtle->get_bg_colour ? turtle->get_bg_colour() : 0;

    for (int r = 0; r < h; r++)
    {
        tilemap_fill_row(row, y + r, x, x + w, bg);
        turtle->canvas_write_row(x, y + r, row, w);
    }
}

// Ready to bake: a bank, a map, and a device that can write the canvas.
static bool bake_ready(const LogoConsoleTurtle *turtle)
{
    return turtle && turtle->canvas_write_row &&
           tilemap_tile_size() > 0 && tilemap_cols() > 0;
}

//==========================================================================
// Bank
//==========================================================================

// newtiles size - Clear the tile bank and set the tile size (8 or 16)
static Result prim_newtiles(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], size);

    if (size != 8.0f && size != 16.0f)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    if (!tilemap_new_tiles((int)size))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    return result_none();
}

// snaptile slot - Capture the tile-sized canvas region centred on the
// (first active) turtle into a bank slot, verbatim
static Result prim_snaptile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], slot);

    // Slot 0 is the background cell, so the usable slots are 1..slots-1.
    // With no bank there are no slots at all, and every input is refused.
    if (!is_int_in(slot, 1, tilemap_bank_slots() - 1))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->canvas_snap)
    {
        turtle_select_first_active();
        if (turtle->canvas_snap((uint8_t)tilemap_tile_size(), tilemap_slot_pixels((int)slot)))
        {
            tilemap_slot_fill_done((int)slot);
        }
    }

    return result_none();
}

//==========================================================================
// Map
//==========================================================================

// newmap cols rows - Allocate and clear a map
static Result prim_newmap(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], cols);
    REQUIRE_NUMBER(args[1], rows);

    if (!is_int_in(cols, 1, 65535))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (!is_int_in(rows, 1, 65535))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }

    // Dimensions are sound, so a refusal here is the tier's capacity.
    if (!tilemap_new_map((int)cols, (int)rows))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    return result_none();
}

// settile col row slot - Write a map cell (1-based, like item)
static Result prim_settile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(3);
    REQUIRE_NUMBER(args[0], col);
    REQUIRE_NUMBER(args[1], row);
    REQUIRE_NUMBER(args[2], slot);

    if (!is_int_in(col, 1, tilemap_cols()))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (!is_int_in(row, 1, tilemap_rows()))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    if (!is_int_in(slot, 0, 255))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));
    }

    tilemap_set_cell((int)col - 1, (int)row - 1, (uint8_t)slot);
    return result_none();
}

// tile col row - Output a map cell (1-based)
static Result prim_tile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], col);
    REQUIRE_NUMBER(args[1], row);

    if (!is_int_in(col, 1, tilemap_cols()))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (!is_int_in(row, 1, tilemap_rows()))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }

    return result_ok(value_number((float)tilemap_cell((int)col - 1, (int)row - 1)));
}

//==========================================================================
// The bake path
//==========================================================================

// stampmap - Render the whole viewport from the map into the canvas
static Result prim_stampmap(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    int x, y, w, h;
    if (bake_ready(turtle) && view_rect(turtle, &x, &y, &w, &h))
    {
        bake_rect(turtle, x, y, w, h);
    }

    return result_none();
}

// stamptile col row - Re-bake one cell wherever it appears on screen
static Result prim_stamptile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], col);
    REQUIRE_NUMBER(args[1], row);

    if (!is_int_in(col, 1, tilemap_cols()))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (!is_int_in(row, 1, tilemap_rows()))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    int vx, vy, vw, vh;
    if (!bake_ready(turtle) || !view_rect(turtle, &vx, &vy, &vw, &vh))
    {
        return result_none();
    }

    int size = tilemap_tile_size();
    int world_w = tilemap_cols() * size;
    int world_h = tilemap_rows() * size;
    int scroll_x, scroll_y;
    tilemap_get_scroll(&scroll_x, &scroll_y);

    // Sampling wraps, so a world smaller than the viewport shows the same
    // cell more than once; re-bake every copy of it that is on screen.
    int first_x = vx + wrap_mod(((int)col - 1) * size - scroll_x, world_w);
    int first_y = vy + wrap_mod(((int)row - 1) * size - scroll_y, world_h);

    for (int sy = first_y; sy < vy + vh; sy += world_h)
    {
        int h = size;
        if (sy + h > vy + vh) h = vy + vh - sy;

        for (int sx = first_x; sx < vx + vw; sx += world_w)
        {
            int w = size;
            if (sx + w > vx + vw) w = vx + vw - sx;

            bake_rect(turtle, sx, sy, w, h);
        }
    }

    return result_none();
}

void primitives_tilemap_init(void)
{
    tilemap_reset();

    primitive_register("newtiles", 1, prim_newtiles);
    primitive_register("snaptile", 1, prim_snaptile);
    primitive_register("newmap", 2, prim_newmap);
    primitive_register("settile", 3, prim_settile);
    primitive_register("tile", 2, prim_tile);
    primitive_register("stampmap", 0, prim_stampmap);
    primitive_register("stamptile", 2, prim_stamptile);
}
