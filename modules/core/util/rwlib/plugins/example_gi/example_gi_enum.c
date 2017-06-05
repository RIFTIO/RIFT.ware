
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

/* This is an example of GObject-Introspection's annotations using an Enum type.
 *
 * For more annotations see: https://wiki.gnome.org/GObjectIntrospection/Annotations
 */
#include "example_gi_enum.h"

static const GEnumValue _enum_values[] = {
  { VALUE_1, "VALUE_1", "VALUE_1" },
  { VALUE_2, "VALUE_2", "VALUE_2" },
  { 0, NULL, NULL }
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines example_gi_my_enum_test_get_type()
 */
GType example_gi_my_enum_test_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("MyEnumTest", _enum_values);

  return type;
}
