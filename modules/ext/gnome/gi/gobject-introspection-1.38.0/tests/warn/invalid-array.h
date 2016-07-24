#include "common.h"

/**
 * test_invalid_array:
 * @out1: (array foobar):
 **/
void
test_invalid_array (char ***out1);

// EXPECT:5: Warning: Test: invalid array annotation value: 'foobar'

/**
 * test_invalid_array_zero_terminated:
 * @out1: (array zero-terminated):
 * @out2: (array zero-terminated=foobar):
 **/
void
test_invalid_array_zero_terminated (char ***out1,
                                    char ***out2);

// EXPECT:14: Warning: Test: array option zero-terminated needs a value
// EXPECT:15: Warning: Test: invalid array zero-terminated option value 'foobar', must be an integer

/**
 * test_invalid_array_fixed_size:
 * @out1: (array fixed-size):
 * @out2: (array fixed-size=foobar):
 **/
void
test_invalid_array_fixed_size (char ***out1,
                               char ***out2);

// EXPECT:26: Warning: Test: array option fixed-size needs a value
// EXPECT:27: Warning: Test: invalid array fixed-size option value 'foobar', must be an integer

/**
 * test_invalid_array_length:
 * @out1: (array length):
 **/
void
test_invalid_array_length (char ***out1,
                           char ***out2);

// EXPECT:38: Warning: Test: array option length needs a value
