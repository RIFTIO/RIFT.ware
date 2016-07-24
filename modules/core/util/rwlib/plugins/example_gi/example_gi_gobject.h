
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
