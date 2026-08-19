//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc full-screen editor interface
//

#pragma once

#include "devices/console.h"
#include <stddef.h>

// Editor function - edit text in a full-screen editor
// buffer: the text to edit (in/out), must be pre-filled with initial content
// buffer_size: maximum size of the buffer
// save/save_ctx: write-back for vi's `:w`, or NULL to have `:w` accept and exit
// Returns: LOGO_EDITOR_ACCEPT if user accepted, LOGO_EDITOR_CANCEL if cancelled
LogoEditorResult picocalc_editor_edit(char *buffer, size_t buffer_size,
                                      LogoEditorSave save, void *save_ctx);

// Select the vi key layer for the next and subsequent edits, from `setvimode`
// (docs/vi-mode-design.md)
void picocalc_editor_set_vi_mode(bool on);

// Lend the editor memory for vi's undo journal, or none (NULL, 0). Which tier a
// board gets is the interpreter's decision, since it owns the aux region
// (docs/vi-mode-design.md §8).
void picocalc_editor_set_undo_store(void *store, size_t size);

// Get the editor operations structure
const LogoConsoleEditor *picocalc_editor_get_ops(void);
