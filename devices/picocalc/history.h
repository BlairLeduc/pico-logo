//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc history buffer
//

#pragma once

#include "pico/types.h"

// History buffer size and line length
#define HISTORY_SIZE 20
#define HISTORY_LINE_LENGTH 120

// Function prototypes
void history_add(const char *line);
void history_get(char *buf, int size, uint index);
void history_clear(void);
uint history_get_start_index(void);
uint history_prev_index(uint index);
uint history_next_index(uint index);
bool history_is_empty(void);
bool history_is_end_index(uint index);