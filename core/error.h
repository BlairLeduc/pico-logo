//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Error message formatting.
//

#pragma once

#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Error codes from Error_Messages.md
    #define ERR_ALREADY_DEFINED      1
    #define ERR_NUMBER_TOO_BIG       2
    #define ERR_NOT_PROCEDURE        3
    #define ERR_NOT_WORD             4
    #define ERR_CANT_USE_TOPLEVEL    5
    #define ERR_IS_PRIMITIVE         6
    #define ERR_CANT_FIND_LABEL      7
    #define ERR_CANT_FROM_EDITOR     8
    #define ERR_UNDEFINED            9
    #define ERR_DIDNT_OUTPUT_TO      10
    #define ERR_DISK_TROUBLE         11
    #define ERR_DISK_FULL            12
    #define ERR_DIVIDE_BY_ZERO       13
    #define ERR_END_OF_DATA          14
    #define ERR_FILE_EXISTS          15
    #define ERR_FILE_NOT_FOUND       17
    #define ERR_FILE_WRONG_TYPE      18
    #define ERR_TOO_FEW_ITEMS        19
    #define ERR_NO_FILE_BUFFERS      20
    #define ERR_NO_CATCH             21
    #define ERR_NOT_FOUND            22
    #define ERR_OUT_OF_SPACE         23
    #define ERR_CANT_USE_PROCEDURE   24
    #define ERR_NOT_BOOL             25
    #define ERR_PAUSING              26
    #define ERR_AT_TOPLEVEL          27
    #define ERR_STOPPED              28
    #define ERR_NOT_ENOUGH_INPUTS    29
    #define ERR_TOO_MANY_INPUTS      30
    #define ERR_TOO_MUCH_PARENS      31
    #define ERR_TOO_FEW_ITEMS_LIST   32
    #define ERR_ONLY_IN_PROCEDURE    33
    #define ERR_TURTLE_BOUNDS        34
    #define ERR_DONT_KNOW_HOW        35
    #define ERR_NO_VALUE             36
    #define ERR_PAREN_MISMATCH       37
    #define ERR_DONT_KNOW_WHAT       38
    #define ERR_BRACKET_MISMATCH     39
    #define ERR_DISK_PROTECTED       40
    #define ERR_DOESNT_LIKE_INPUT    41
    #define ERR_DIDNT_OUTPUT         42

    // Format an error message from a Result
    // Returns pointer to static buffer - do not free
    const char *error_format(Result r);

    // Get raw error message template by code
    const char *error_message(int code);

#ifdef __cplusplus
}
#endif
