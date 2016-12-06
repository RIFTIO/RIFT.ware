
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

#ifndef __EXAMPLE_GI_OBJECT_H__
#define __EXAMPLE_GI_OBJECT_H__
/* This is an example of GObject-Introspection's annotations using a GObject type.
 *
 * For more annotations see: https://wiki.gnome.org/GObjectIntrospection/Annotations
 */

#include <glib-object.h>

#define EXAMPLE_GI_TYPE_OBJECT              (example_gi_object_get_type ())
#define EXAMPLE_GI_OBJECT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_GI_TYPE_OBJECT, ExampleGiObject))
#define EXAMPLE_GI_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_GI_TYPE_OBJECT, ExampleGiObjectClass))
#define EXAMPLE_GI_IS_OBJECT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_GI_TYPE_OBJECT))
#define EXAMPLE_GI_IS_OBJECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EXAMPLE_GI_TYPE_OBJECT))
#define EXAMPLE_GI_OBJECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), EXAMPLE_GI_TYPE_OBJECT, ExampleGiObjectClass))


typedef struct _ExampleGiObject      ExampleGiObject;
typedef struct _ExampleGiObjectClass ExampleGiObjectClass;

struct _ExampleGiObject {
  GObject parent;
};

struct _ExampleGiObjectClass {
  GObjectClass parent_class;

  void (* changed) (ExampleGiObject *gobject);

  /*< private >*/
  /* Avoid ABI breaks */
  gpointer padding[8];
};

GType example_gi_object_get_type(void);

ExampleGiObject *example_gi_object_new(const char *string);

#endif
