/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Author(s): Tim Mortsolf
 * Creation Date: 8/29/2013
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <libpeas/peas.h>

#include <stdio.h>
#include <math.h>

#include "feature_plugin-c.h"

static void
feature_plugin__api__example1a(FeaturePluginApi *api,
                               gint32 *number,
                               gint32 delta,
                               gint32 *negative)
{
  static int print_done = 0;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example1a\n");
    printf("========================================\n");
    print_done = 1;
  }

  //Take a number (in/out) then add a number to it (in)
  *number += delta;

  // output the negative of it (out)
  *negative = -1 * (*number);
}

static gint32
feature_plugin__api__example1b(FeaturePluginApi *api,
                               gint32 *number,
                               gint32 delta,
                               gint32 *negative)
{
  static int print_done = 0;
  ((void)api);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example1b\n");
    printf("========================================\n");
    print_done = 1;
  }

  //Take a number (in/out) then add a number to it (in)
  *number += delta;

  // output the negative of it (out)
  *negative = -1 * (*number);

  // Return the sum of all 3 of these
  return *number + delta + *negative;
}

static void
feature_plugin__api__example1c(FeaturePluginApi *api,
                               gint32 number1,
                               gint32 number2,
                               gint32 number3,
                               gint32 *negative1,
                               gint32 *negative2,
                               gint32 *negative3)
{
  static int print_done = 0;
  ((void)api);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example1c\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Take 3 numbers and return the negative of each of them
  *negative1 = -number1;
  *negative2 = -number2;
  *negative3 = -number3;
}

static gint32
feature_plugin__api__example1d(FeaturePluginApi *api,
                               gint32 number1,
                               gint32 number2,
                               gint32 number3,
                               gint32 *negative1,
                               gint32 *negative2,
                               gint32 *negative3)
{
  static int print_done = 0;
  ((void)api);
  int result;
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example1d\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Take 3 numbers and return the negative of each of them
  *negative1 = -number1;
  *negative2 = -number2;
  *negative3 = -number3;

  // Return the sum of the square of all 6 of these
  result = 2 * (number1 * number1 + number2 * number2 + number3 * number3);
  return result;
}

static gint32
feature_plugin__api__example1e(FeaturePluginApi *api,
                               gchar **value,
                               gchar *delta,
                               gchar **reverse)
{
  static int print_done = 0;
  ((void)api);
  int result;
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example1e\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Take a string (in/out) then append a String to it (in), and also output the reverse of it (out)
  *value = g_strconcat(*value, delta, NULL);
  *reverse = g_strconcat(*value, NULL);
  g_strreverse(*reverse);

  // Return the length of the string
  result = strlen(*reverse);

  return result;
}

static gint32
feature_plugin__api__example1f(FeaturePluginApi *api,
                               gint **value,
                               int *value_len,
                               gint *delta,
                               int delta_len,
                               gint **reflection,
                               int *reflection_len)
{
  static int print_done = 0;
  int result;
  ((void)api);
  ((void)value_len);
  ((void)delta_len);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example1f\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Add the delta (in) to the value (inout)
  (*value)[0] += delta[0];
  (*value)[1] += delta[1];
  (*value)[2] += delta[2];

  // Output the origin reflection of it (out)
  *reflection = g_new(gint, 3);
  *reflection_len = 3;

  (*reflection)[0] = -(*value)[0];
  (*reflection)[1] = -(*value)[1];
  (*reflection)[2] = -(*value)[2];
  
  // Return the sum of the reflection elements
  result = (*reflection)[0] + (*reflection)[1] + (*reflection)[2];

  return result;
}

static gint32
feature_plugin__api__example2a(FeaturePluginApi *instance,
                               FeaturePlugin_ComplexNumber *complex)
{
  static int print_done = 0;
  int modulus;
  ((void)instance);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example2a\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Compute the modulus of a complex number
  modulus = sqrt(complex->real * complex->real + complex->imag * complex->imag);

  return modulus;
}

static void
feature_plugin__api__example2b(FeaturePluginApi *instance,
                               FeaturePlugin_ComplexNumber **complex)
{
  static int print_done = 0;
  ((void)instance);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example2b\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Allocate a GObject for the output parameter
  *complex = g_object_new(FEATURE_PLUGIN_TYPE__COMPLEXNUMBER, 0);

  // Initialize a complex number to (0, -i)
  (*complex)->real = 0;
  (*complex)->imag = -1;
}

static gint32
feature_plugin__api__example2c(FeaturePluginApi *instance,
                               MathCommon_ComplexNumber *complex)
{
  static int print_done = 0;
  int modulus;
  ((void)instance);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example2c\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Compute the modulus of a complex number
  modulus = sqrt(complex->real * complex->real + complex->imag * complex->imag);

  return modulus;
}

static void
feature_plugin__api__example2d(FeaturePluginApi *instance,
                               MathCommon_ComplexNumber **complex)
{
  static int print_done = 0;
  ((void)instance);
  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example2d\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Allocate a GObject for the output parameter
  *complex = g_object_new(FEATURE_PLUGIN_TYPE__COMPLEXNUMBER, 0);

  // Initialize a complex number to (0, -i)
  (*complex)->real = 0;
  (*complex)->imag = -1;
}

static void
feature_plugin__api__example4a(FeaturePluginApi *instance,
                               MathCommon_ComplexNumber** number,
                               MathCommon_ComplexNumber* delta,
                               MathCommon_ComplexNumber** negative)
{

  ((void)instance);
  ((void)number);
  ((void)delta);
  ((void)negative);
}

MathCommon_ComplexNumber *
feature_plugin__api__example4b(FeaturePluginApi *instance,
                               MathCommon_ComplexNumber** number,
                               MathCommon_ComplexNumber* delta)
{
  ((void)instance);
  ((void)number);
  ((void)delta);
  return NULL;
}

MathCommon_ComplexNumber *
feature_plugin__api__example4c(FeaturePluginApi *instance,
                               MathCommon_ComplexNumber* number,
                               MathCommon_ComplexNumber* delta)
{
  ((void)instance);
  ((void)number);
  ((void)delta);
  return NULL;
}

static gint
feature_plugin__api__example6a(FeaturePluginApi *api,
                               FeaturePlugin_VectorInt **value,
                               FeaturePlugin_VectorInt *delta,
                               FeaturePlugin_VectorInt **reflection)
{
  static int print_done = 0;
  int result;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example6a\n");
    printf("========================================\n");
    print_done = 1;
  }

  return result = 0;
}

static gint
feature_plugin__api__example6b(FeaturePluginApi *api,
                               FeaturePlugin_VectorInt **value,
                               FeaturePlugin_VectorInt *delta,
                               FeaturePlugin_VectorInt **reflection)
{
  static int print_done = 0;
  int result;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example6b\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Add the delta (in) to the value (inout)
  (*value)->_sarrayint->_storage[0] += delta->_sarrayint->_storage[0];
  (*value)->_sarrayint->_storage[1] += delta->_sarrayint->_storage[1];
  (*value)->_sarrayint->_storage[2] += delta->_sarrayint->_storage[2];

  // Output the origin reflection of it (out)
  (*reflection)->_sarrayint->_storage = g_new(gint, 3);
  (*reflection)->_sarrayint->_storage[0] = -(*value)->_sarrayint->_storage[0];
  (*reflection)->_sarrayint->_storage[1] = -(*value)->_sarrayint->_storage[1];
  (*reflection)->_sarrayint->_storage[2] = -(*value)->_sarrayint->_storage[2];
  
  // Return the sum of the reflection elements
  result =
      (*reflection)->_sarrayint->_storage[0] +
      (*reflection)->_sarrayint->_storage[1] +
      (*reflection)->_sarrayint->_storage[2];

  return result;
}

static gint
feature_plugin__api__example6c(FeaturePluginApi *api,
                               FeaturePlugin_VectorInt **value,
                               FeaturePlugin_VectorInt *delta,
                               FeaturePlugin_VectorInt **reflection)
{
  static int print_done = 0;
  int result;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example6c\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Add the delta (in) to the value (inout)
  ((gint *) (*value)->_sarrayint2->_storage)[0] += ((gint *) delta->_sarrayint2->_storage)[0];
  ((gint *) (*value)->_sarrayint2->_storage)[1] += ((gint *) delta->_sarrayint2->_storage)[1];
  ((gint *) (*value)->_sarrayint2->_storage)[2] += ((gint *) delta->_sarrayint2->_storage)[2];

  // Output the origin reflection of it (out)
  (*reflection)->_sarrayint2->_storage = (gpointer *)g_new(gint, 3);
  ((gint *) (*reflection)->_sarrayint2->_storage)[0] = -((gint *) (*value)->_sarrayint2->_storage)[0];
  ((gint *) (*reflection)->_sarrayint2->_storage)[1] = -((gint *) (*value)->_sarrayint2->_storage)[1];
  ((gint *) (*reflection)->_sarrayint2->_storage)[2] = -((gint *) (*value)->_sarrayint2->_storage)[2];
  
  // Return the sum of the reflection elements
  result =
      ((gint *) (*reflection)->_sarrayint2->_storage)[0] +
      ((gint *) (*reflection)->_sarrayint2->_storage)[1] +
      ((gint *) (*reflection)->_sarrayint2->_storage)[2];

  return result;
}

static gint
feature_plugin__api__example7a(FeaturePluginApi *api,
                               FeaturePlugin_VectorInt **value,
                               FeaturePlugin_VectorInt *delta,
                               FeaturePlugin_VectorInt **reflection)
{
  static int print_done = 0;
  int result;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example7a\n");
    printf("========================================\n");
    print_done = 1;
  }

  return result = 0;
}

static int
feature_plugin__api__example7b(FeaturePluginApi *api,
                               FeaturePlugin_VectorInt **value,
                               FeaturePlugin_VectorInt *delta,
                               FeaturePlugin_VectorInt **reflection)
{
  static int print_done = 0;
  int result;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example7b\n");
    printf("========================================\n");
    print_done = 1;
  }

  // Add the delta (in) to the value (inout)
  g_array_index((*value)->_darrayint->_storage, int, 0) += g_array_index(delta->_darrayint->_storage, int, 0);
  g_array_index((*value)->_darrayint->_storage, int, 1) += g_array_index(delta->_darrayint->_storage, int, 1);
  g_array_index((*value)->_darrayint->_storage, int, 2) += g_array_index(delta->_darrayint->_storage, int, 2);

  // Output the origin reflection of it (out)
  (*reflection)->_darrayint->_storage = g_array_new(FALSE, FALSE, sizeof(gint));
  g_array_set_size((*reflection)->_darrayint->_storage, 3);
  g_array_index((*reflection)->_darrayint->_storage, int, 0) =
      -g_array_index((*value)->_darrayint->_storage, int, 0);
  g_array_index((*reflection)->_darrayint->_storage, int, 1) =
      -g_array_index((*value)->_darrayint->_storage, int, 1);
  g_array_index((*reflection)->_darrayint->_storage, int, 2) =
      -g_array_index((*value)->_darrayint->_storage, int, 2);
  
  // Return the sum of the reflection elements
  result =
      g_array_index((*reflection)->_darrayint->_storage, int, 0) +
      g_array_index((*reflection)->_darrayint->_storage, int, 1) +
      g_array_index((*reflection)->_darrayint->_storage, int, 2);

  return result;
}

static int
feature_plugin__api__example7c(FeaturePluginApi *api,
                               FeaturePlugin_VectorInt **value,
                               FeaturePlugin_VectorInt *delta,
                               FeaturePlugin_VectorInt **reflection)
{
  static int print_done = 0;
  int result;
  ((void)api);

  if (print_done == 0) {
    printf("========================================\n");
    printf("This is a call to the C plugin example7c\n");
    printf("========================================\n");
    print_done = 1;
  }

  return result = 0;
}

static void
feature_plugin_c_extension_init (FeaturePluginCExtension *plugin)
{
  ((void)plugin);
}

static void
feature_plugin_c_extension_class_init (FeaturePluginCExtensionClass *klass)
{
  ((void)klass);
}

static void
feature_plugin_c_extension_class_finalize (FeaturePluginCExtensionClass *klass)
{
  ((void)klass);
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN
#endif //VAPI_TO_C_AUTOGEN
