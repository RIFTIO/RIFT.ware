
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

/* This is an example of GObject-Introspection's annotations using a Boxed type.
 *
 * For more annotations see: https://wiki.gnome.org/GObjectIntrospection/Annotations
 */
#include <stdio.h>
#include "example_gi_boxed.h"

struct _ExampleGiOpaque {
  int i;
};

G_DEFINE_POINTER_TYPE(ExampleGiOpaque, example_gi_opaque);

ExampleGiOpaque *example_gi_opaque_new(void)
{
  ExampleGiOpaque *boxed = (ExampleGiOpaque *)g_new0(ExampleGiOpaque, 1);
  boxed->i = 1;
  return boxed;
}

static ExampleGiBoxed *_example_gi_boxed_ref(ExampleGiBoxed *boxed);
static void _example_gi_boxed_unref(ExampleGiBoxed *boxed);

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines example_gi_boxed_get_type()
 */
G_DEFINE_BOXED_TYPE(ExampleGiBoxed, 
                    example_gi_boxed, 
                    _example_gi_boxed_ref, 
                    _example_gi_boxed_unref)

static ExampleGiBoxed *_example_gi_boxed_ref(ExampleGiBoxed *boxed)
{
  g_atomic_int_inc(&boxed->ref_count);
  return boxed;
}

static void _example_gi_boxed_unref(ExampleGiBoxed *boxed)
{
  if (!g_atomic_int_dec_and_test(&boxed->ref_count)) {
    return;
  }
  g_free(boxed->integer_array);
  g_free(boxed->string);
  g_strfreev(boxed->string_array);
  g_clear_pointer(&boxed->nested, _example_gi_boxed_unref);
  g_free(boxed);

  /* ... */
}

ExampleGiBoxed *example_gi_boxed_new(void)
{
  ExampleGiBoxed *boxed = (ExampleGiBoxed *)g_new0(ExampleGiBoxed, 1);
  boxed->ref_count = 1;
  return boxed;
}

int example_gi_boxed_get_integer(ExampleGiBoxed *boxed)
{
  printf("example_gi_boxed_get_integer: %d\n", boxed->foo);
  return boxed->integer;
}

void example_gi_boxed_set_integer(ExampleGiBoxed *boxed,
                                  int integer)
{
  boxed->integer = integer;
}

const int *example_gi_boxed_get_integer_array(ExampleGiBoxed *boxed,
                                              int *length)
{
  *length = boxed->integer_array_length;

  return boxed->integer_array;
}

void example_gi_boxed_set_integer_array(ExampleGiBoxed *boxed,
                                        int *integers,
                                        int length)
{
  boxed->integer_array = integers;
  boxed->integer_array_length = length;
}

const gchar *example_gi_boxed_get_string(ExampleGiBoxed *boxed)
{
  return boxed->string;
}

void example_gi_boxed_set_string(ExampleGiBoxed *boxed,
                                 const char *string)
{
  g_free(boxed->string);
  boxed->string = string == NULL ? NULL : g_strdup(string);
}

const char **example_gi_boxed_get_string_array(ExampleGiBoxed *boxed)
{
  return (const char **)boxed->string_array;
}

void example_gi_boxed_set_string_array(ExampleGiBoxed *boxed,
                                       char **strings)
{
  // printf("example_gi_boxed_set_string_array: %p, %p, %s[%p], %s[%p]\n", boxed, strings, strings[0], strings[0], strings[1], strings[1]);
  if (boxed->string_array != NULL) {
    g_strfreev(boxed->string_array);
  }
  boxed->string_array = strings;
}

void example_gi_boxed_string_array_foreach(ExampleGiBoxed *boxed,
                                           ExampleGiBoxedStringArrayForeachFunc callback,
                                           gpointer user_data)
{
  int i;

  for (i = 0; boxed->string_array[i] != NULL; ++i) {
    callback(boxed, boxed->string_array[i], user_data);
  }
}

ExampleGiBoxed *example_gi_boxed_get_nested(ExampleGiBoxed *boxed)
{
  return boxed->nested;
}

void example_gi_boxed_set_nested(ExampleGiBoxed *boxed,
                                 ExampleGiBoxed *nested)
{
  g_clear_pointer(&boxed->nested, _example_gi_boxed_unref);
  boxed->nested = nested == NULL ? NULL : _example_gi_boxed_ref(nested);
}
