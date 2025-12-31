//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control reset helper - consolidates test state reset for testing
//

#include "primitives.h"
#include "error.h"
#include "variables.h"

// Combined reset function for testing purposes
// Resets both test state (in variables) and caught error state
void primitives_control_reset_test_state(void)
{
    var_reset_test_state();
    error_clear_caught();
}
