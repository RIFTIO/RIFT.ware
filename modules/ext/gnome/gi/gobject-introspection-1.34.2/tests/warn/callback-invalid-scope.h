#include "common.h"

/**
 * test_callback_invalid:
 * @callback: (scope invalid):
 *
 */
void test_callback_invalid(GCallback *callback, gpointer user_data);

// EXPECT:5: Warning: Test: invalid scope annotation value: 'invalid'

/**
 * test_callback_invalid2:
 * @callback: (scope):
 *
 */
void test_callback_invalid2(GCallback *callback, gpointer user_data);

// EXPECT:14: Warning: Test: scope annotation needs a value

/**
 * test_callback_invalid3:
 * @callback: (scope invalid foo):
 *
 */
void test_callback_invalid3(GCallback *callback, gpointer user_data);

// EXPECT:23: Warning: Test: scope annotation needs one value, not 2

// EXPECT:13: Warning: Test: test_callback_invalid2: argument callback: Missing (scope) annotation for callback without GDestroyNotify (valid: call, async)
// EXPECT:22: Warning: Test: test_callback_invalid3: argument callback: Missing (scope) annotation for callback without GDestroyNotify (valid: call, async)
