
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __EXAMPLE_GI_ENUM_H__
#define __EXAMPLE_GI_ENUM_H__
/* This is an example of GObject-Introspection's annotations using an Enum type.
 *
 * For more annotations see: https://wiki.gnome.org/GObjectIntrospection/Annotations
 */

#include <glib-object.h>

typedef enum {
  VALUE_1 = 47,
  VALUE_2 = 42
} MyEnumTest;

GType example_gi_my_enum_test_get_type(void);

#endif
