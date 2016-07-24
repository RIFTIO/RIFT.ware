
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __EXAMPLE_GI_BOXED_H__
#define __EXAMPLE_GI_BOXED_H__
/* This is an example of GObject-Introspection's annotations using a Boxed type.
 *
 * For more annotations see: https://wiki.gnome.org/GObjectIntrospection/Annotations
 */

#include <glib-object.h>

typedef struct _ExampleGiOpaque ExampleGiOpaque;
GType example_gi_opaque_get_type(void);

/**
 * example_gi_opaque_new:
 * Returns: (transfer none)
 */
ExampleGiOpaque *example_gi_opaque_new(void);
// typedef struct _ExampleGiBoxed ExampleGiBoxed;

typedef struct _ExampleGiBoxed {
  int foo;
#ifndef __GI_SACNNER__
  gint ref_count;
  int integer;
  int *integer_array;
  int integer_array_length;
  char *string;
  char **string_array;
  struct _ExampleGiBoxed *nested;
  /* ... */
#endif //__GI_SACNNER__
} ExampleGiBoxed;

/**
 * ExampleGiBoxedStringArrayForeachFunc:
 * @boxed:
 * @string:
 * @data:
 */
typedef void (*ExampleGiBoxedStringArrayForeachFunc) (ExampleGiBoxed *boxed,
                                                      const char *string,
                                                      gpointer user_data);

GType example_gi_boxed_get_type(void);

ExampleGiBoxed *example_gi_boxed_new(void);

int example_gi_boxed_get_integer(ExampleGiBoxed *boxed);

void example_gi_boxed_set_integer(ExampleGiBoxed *boxed,
                                  int integer);

/**
 * example_gi_boxed_get_integer_array:
 * @boxed:
 * @length: (out):
 *
 * Returns: (transfer none) (array length=length):
 */
const int * example_gi_boxed_get_integer_array(ExampleGiBoxed *boxed,
                                               int *length);

/**
 * example_gi_boxed_set_integer_array:
 * @boxed:
 * @integers: (transfer full) (array length=length) (allow-none):
 * @length:
 *
 */
void example_gi_boxed_set_integer_array(ExampleGiBoxed *boxed,
                                        int *integers,
                                        int length);

const char * example_gi_boxed_get_string(ExampleGiBoxed *boxed);

/**
 * example_gi_boxed_set_string:
 * @boxed:
 * @string: (allow-none):
 *
 */
void example_gi_boxed_set_string(ExampleGiBoxed *boxed,
                                 const char *string);

/**
 * example_gi_boxed_get_string_array:
 * @boxed:
 *
 * Returns: (transfer none) (array zero-terminated=1) (allow-none):
 */
const char **example_gi_boxed_get_string_array(ExampleGiBoxed *boxed);

/**
 * example_gi_boxed_set_string_array:
 * @boxed:
 * @strings: (transfer full) (array zero-terminated=1) (allow-none):
 *
 */
void example_gi_boxed_set_string_array(ExampleGiBoxed *boxed,
                                       char **strings);

/**
 * example_gi_boxed_string_array_foreach:
 * @boxed:
 * @callback: (scope call):
 * @user_data:
 *
 */
void example_gi_boxed_string_array_foreach(ExampleGiBoxed *boxed,
                                           ExampleGiBoxedStringArrayForeachFunc callback,
                                           gpointer user_data);

/**
 * example_gi_boxed_get_nested:
 * @boxed:
 *
 * Returns: (transfer none):
 */
ExampleGiBoxed *example_gi_boxed_get_nested(ExampleGiBoxed *boxed);

/**
 * example_gi_boxed_set_nested:
 * @boxed:
 * @nested: (transfer none) (allow-none):
 *
 */
void example_gi_boxed_set_nested(ExampleGiBoxed *boxed,
                                 ExampleGiBoxed *nested);

#endif
