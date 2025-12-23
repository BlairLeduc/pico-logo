//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc history buffer
//

#include <ctype.h>
#include <string.h>
#include "pico/types.h"

#include "history.h"

char history_buffer[HISTORY_SIZE][HISTORY_LINE_LENGTH] = {0};
uint history_head = 0;
uint history_tail = 0;

// Helper to check if a string contains only whitespace
static bool is_blank(const char *str)
{
    while (*str)
    {
        if (!isspace((unsigned char)*str))
        {
            return false;
        }
        str++;
    }
    return true;
}

void history_add(const char *line)
{
    // Don't add empty or blank (whitespace-only) lines to history
    if (line == NULL || line[0] == '\0' || is_blank(line))
    {
        return;
    }

    strncpy(history_buffer[history_head], line, HISTORY_LINE_LENGTH - 1);
    history_buffer[history_head][HISTORY_LINE_LENGTH - 1] = '\0';
    history_head = (history_head + 1) % HISTORY_SIZE;
    if (history_head == history_tail)
    {
        history_tail = (history_tail + 1) % HISTORY_SIZE; // Overwrite oldest
    }
}

void history_get(char *buf, int size, uint index)
{
    if (index >= HISTORY_SIZE)
    {
        buf[0] = '\0'; // Invalid index, return empty string
        return;
    }
    strncpy(buf, history_buffer[index], size - 1);
    buf[size - 1] = '\0'; // Ensure null-termination
}

void history_clear(void)
{
    memset(history_buffer, 0, sizeof(history_buffer));
    history_head = 0;
    history_tail = 0;
}

uint history_get_start_index(void)
{
    return history_head;
}

uint history_prev_index(uint index)
{
    if (index != history_tail)
    {
        // Move to the previous entry, wrapping if needed
        return (index + HISTORY_SIZE - 1) % HISTORY_SIZE;
    }
    return index; // If at the tail, stay at the tail
}

uint history_next_index(uint index)
{
    if (index != history_head)
    {
        // Move to the next entry, wrapping if needed
        return (index + 1) % HISTORY_SIZE;
    }
    return index; // If at the head, stay at the head
}

bool history_is_empty(void)
{
    return history_head == history_tail; // Check if the history is empty
}

bool history_is_end_index(uint index)
{
    return index == history_head; // Check if the index is at the end of the history
}
