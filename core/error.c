//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Error message formatting.
//

#include "error.h"
#include <stdio.h>
#include <string.h>

// Error message templates
// (procedure), (symbol), (list) are placeholders
static const char *error_templates[] = {
    [0] = "Unknown error",
    [ERR_ALREADY_DEFINED] = "%s is already defined",
    [ERR_NUMBER_TOO_BIG] = "Number is too big",
    [ERR_NOT_PROCEDURE] = "%s isn't a procedure",
    [ERR_NOT_WORD] = "%s isn't a word",
    [ERR_CANT_USE_TOPLEVEL] = "%s can't be used at toplevel",
    [ERR_IS_PRIMITIVE] = "%s is a primitive",
    [ERR_CANT_FIND_LABEL] = "Can't find label %s",
    [ERR_CANT_FROM_EDITOR] = "Can't %s from the editor",
    [ERR_UNDEFINED] = "%s is undefined",
    [ERR_DIDNT_OUTPUT_TO] = "%s didn't output to %s",
    [ERR_DISK_TROUBLE] = "I'm having trouble with the disk",
    [ERR_DISK_FULL] = "Disk full",
    [ERR_DIVIDE_BY_ZERO] = "Can't divide by zero",
    [ERR_END_OF_DATA] = "End of data",
    [ERR_FILE_EXISTS] = "File already exists",
    [ERR_FILE_NOT_FOUND] = "File not found",
    [ERR_FILE_WRONG_TYPE] = "File is the wrong type",
    [ERR_TOO_FEW_ITEMS] = "Too few items in %s",
    [ERR_NO_FILE_BUFFERS] = "No more file buffers",
    [ERR_NO_CATCH] = "Can't find a catch for %s",
    [ERR_NOT_FOUND] = "%s not found",
    [ERR_OUT_OF_SPACE] = "Out of space",
    [ERR_CANT_USE_PROCEDURE] = "%s can't be used in a procedure",
    [ERR_NOT_BOOL] = "%s is not true or false",
    [ERR_PAUSING] = "Pausing...",
    [ERR_AT_TOPLEVEL] = "You're at toplevel",
    [ERR_STOPPED] = "Stopped!",
    [ERR_NOT_ENOUGH_INPUTS] = "Not enough inputs to %s",
    [ERR_TOO_MANY_INPUTS] = "Too many inputs to %s",
    [ERR_TOO_MUCH_PARENS] = "Too much inside parenthesis",
    [ERR_TOO_FEW_ITEMS_LIST] = "Too few items in %s",
    [ERR_ONLY_IN_PROCEDURE] = "Can only do that inside a procedure",
    [ERR_TURTLE_BOUNDS] = "Turtle out of bounds",
    [ERR_DONT_KNOW_HOW] = "I don't know how to %s",
    [ERR_NO_VALUE] = "%s has no value",
    [ERR_PAREN_MISMATCH] = ") without (",
    [ERR_DONT_KNOW_WHAT] = "I don't know what to do with %s",
    [ERR_BRACKET_MISMATCH] = "] without [",
    [ERR_DISK_PROTECTED] = "Disk is write-protected",
    [ERR_DOESNT_LIKE_INPUT] = "%s doesn't like %s as input",
    [ERR_DIDNT_OUTPUT] = "%s didn't output",
    [ERR_UNSUPPORTED_ON_DEVICE] = "I can't run %s on this device",
    [ERR_NO_FILE_SELECTED] = "No file selected",
    [ERR_FILE_NOT_OPEN] = "File %s is not open",
    [ERR_FILE_ALREADY_OPEN] = "File %s is already open",
    [ERR_FILE_POS_OUT_OF_RANGE] = "File position out of range",
    [ERR_DEVICE_UNAVAILABLE] = "Device unavailable",
    [ERR_ALREADY_DRIBBLING] = "Already dribbling",
    [ERR_DEVICE_IN_USE] = "Device %s is already in use",
    [ERR_FILE_TOO_BIG] = "File %s is too big",
    [ERR_SUBDIR_NOT_FOUND] = "Subdirectory not found for %s",
    [ERR_SUBDIR_NOT_EMPTY] = "Subdirectory %s is not empty",
    [ERR_CANT_OPEN_NETWORK] = "Can't open %s",
    [ERR_LOST_CONNECTION] = "I lost the network connection",
    [ERR_NETWORK_NOT_OPEN] = "Network connection not open",
    [ERR_NETWORK_ERROR] = "Network error occurred",
    [ERR_NOT_FILE_OR_NETWORK] = "Can't use %s as a file or network connection",
    [ERR_INVALID_IP_PORT] = "Invalid IP address or port %s",
    [ERR_NETWORK_ALREADY_OPEN] = "Network connection already open",
    [ERR_CANT_ON_NETWORK] = "Can't %s on a network connection",
    [ERR_NETWORK_TIMEOUT] = "Network timeout occurred",
    [ERR_INVALID_NETWORK_OP] = "Invalid network operation",
};

#define NUM_ERRORS (sizeof(error_templates) / sizeof(error_templates[0]))

const char *error_message(int code)
{
    if (code >= 0 && (size_t)code < NUM_ERRORS && error_templates[code])
    {
        return error_templates[code];
    }
    return error_templates[0];
}

// Helper to append " in <proc>" suffix if caller is present
static void append_caller_suffix(char *buf, size_t bufsize, const char *caller)
{
    if (caller)
    {
        size_t len = strlen(buf);
        snprintf(buf + len, bufsize - len, " in %s", caller);
    }
}

const char *error_format(Result r)
{
    static char buf[256];
    
    if (r.status != RESULT_ERROR)
    {
        return "";
    }
    
    int code = r.error_code;
    const char *tmpl = error_message(code);
    
    // Format based on which context fields are present
    switch (code)
    {
    case ERR_DOESNT_LIKE_INPUT:
        // "proc doesn't like arg as input"
        if (r.error_proc && r.error_arg)
        {
            snprintf(buf, sizeof(buf), "%s doesn't like %s as input",
                     r.error_proc, r.error_arg);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        break;
        
    case ERR_DIDNT_OUTPUT_TO:
        // "proc didn't output to caller"
        if (r.error_proc && r.error_caller)
        {
            snprintf(buf, sizeof(buf), "%s didn't output to %s",
                     r.error_proc, r.error_caller);
            // Note: error_caller is already used in the message itself
            return buf;
        }
        else if (r.error_proc)
        {
            snprintf(buf, sizeof(buf), "%s didn't output", r.error_proc);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        break;
        
    case ERR_TOO_FEW_ITEMS:
    case ERR_TOO_FEW_ITEMS_LIST:
        // "Too few items in [list]"
        if (r.error_arg)
        {
            snprintf(buf, sizeof(buf), "Too few items in %s", r.error_arg);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        break;
        
    case ERR_NO_VALUE:
        // For ERR_NO_VALUE, the variable name is in error_arg
        if (r.error_arg)
        {
            snprintf(buf, sizeof(buf), tmpl, r.error_arg);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        break;
        
    case ERR_DONT_KNOW_HOW:
    case ERR_NOT_PROCEDURE:
    case ERR_UNDEFINED:
    case ERR_NOT_WORD:
    case ERR_IS_PRIMITIVE:
    case ERR_NOT_ENOUGH_INPUTS:
    case ERR_TOO_MANY_INPUTS:
    case ERR_DIDNT_OUTPUT:
    case ERR_NOT_BOOL:
    case ERR_DONT_KNOW_WHAT:
    case ERR_ALREADY_DEFINED:
    case ERR_NO_CATCH:
    case ERR_CANT_FIND_LABEL:
    case ERR_CANT_USE_TOPLEVEL:
    case ERR_CANT_USE_PROCEDURE:
    case ERR_CANT_FROM_EDITOR:
    case ERR_NOT_FOUND:
    case ERR_FILE_NOT_OPEN:
    case ERR_FILE_ALREADY_OPEN:
    case ERR_DEVICE_IN_USE:
    case ERR_UNSUPPORTED_ON_DEVICE:
    case ERR_FILE_TOO_BIG:
    case ERR_SUBDIR_NOT_FOUND:
    case ERR_SUBDIR_NOT_EMPTY:
    case ERR_CANT_ON_NETWORK:
        // Single %s placeholder - use error_proc or error_arg
        if (r.error_proc)
        {
            snprintf(buf, sizeof(buf), tmpl, r.error_proc);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        else if (r.error_arg)
        {
            snprintf(buf, sizeof(buf), tmpl, r.error_arg);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        break;

    case ERR_CANT_OPEN_NETWORK:
    case ERR_NOT_FILE_OR_NETWORK:
    case ERR_INVALID_IP_PORT:
    case ERR_NETWORK_ALREADY_OPEN:
        // Network errors - always use error_arg (the target address)
        if (r.error_arg)
        {
            snprintf(buf, sizeof(buf), tmpl, r.error_arg);
            append_caller_suffix(buf, sizeof(buf), r.error_caller);
            return buf;
        }
        break;
        
    default:
        // No placeholders or context not needed - still append caller if present
        if (r.error_caller)
        {
            snprintf(buf, sizeof(buf), "%s in %s", tmpl, r.error_caller);
            return buf;
        }
        break;
    }
    
    // Fallback: return template as-is (may have unsubstituted %s)
    return tmpl;
}

//==========================================================================
// Caught Error Info (for 'error' primitive)
//==========================================================================

static CaughtError caught_error = {false, 0, "", NULL, NULL};

void error_set_caught(const Result *r)
{
    if (r && r->status == RESULT_ERROR)
    {
        caught_error.valid = true;
        caught_error.code = r->error_code;
        // Use error_format to get the fully formatted error message
        const char *formatted = error_format(*r);
        strncpy(caught_error.message, formatted, sizeof(caught_error.message) - 1);
        caught_error.message[sizeof(caught_error.message) - 1] = '\0';
        caught_error.proc = r->error_proc;
        caught_error.caller = r->error_caller;
    }
}

void error_clear_caught(void)
{
    caught_error.valid = false;
    caught_error.code = 0;
    caught_error.message[0] = '\0';
    caught_error.proc = NULL;
    caught_error.caller = NULL;
}

const CaughtError *error_get_caught(void)
{
    return caught_error.valid ? &caught_error : NULL;
}
