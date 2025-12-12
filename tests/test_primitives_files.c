//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for Files primitives: open, openread, openwrite, openappend, openupdate, close, closeall,
//                              setread, setwrite, reader, writer, allopen, readpos, setreadpos,
//                              writepos, setwritepos, filelen, dribble, nodribble, load, save
//

#include "test_scaffold.h"
#include <stdlib.h>

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}


//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    
    return UNITY_END();
}
