//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc full-screen editor interface
//

#pragma once

#include "devices/console.h"
#include <stddef.h>

// Editor function - edit text in a full-screen editor
// buffer: the text to edit (in/out), must be pre-filled with initial content
// buffer_size: maximum size of the buffer
// Returns: LOGO_EDITOR_ACCEPT if user accepted, LOGO_EDITOR_CANCEL if cancelled
LogoEditorResult picocalc_editor_edit(char *buffer, size_t buffer_size);

// Get the editor operations structure
const LogoConsoleEditor *picocalc_editor_get_ops(void);
