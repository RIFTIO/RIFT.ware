
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
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
