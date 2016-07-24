
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

/* This is an example of GObject-Introspection's annotations using a GObject type.
 *
 * For more annotations see: https://wiki.gnome.org/GObjectIntrospection/Annotations
 */
#include "example_gi_gobject.h"

typedef struct {
  int integer;
  char *string;
} ExampleGiObjectPrivate;

/* Registers the Object resulting in a GType.
 *
 * This defines (among others):
 *   example_gi_object_parent_class
 *   example_gi_object_get_type()
 *   example_gi_object_get_instance_private()
 */
G_DEFINE_TYPE_WITH_PRIVATE(ExampleGiObject,
                           example_gi_object,
                           G_TYPE_OBJECT)

/* Signals */
enum {
  CHANGED,
  LAST_SIGNAL
};

/* Properties */
enum {
  PROP_0,
  PROP_INTEGER,
  PROP_STRING,
  N_PROPERTIES
};

static unsigned int signals[LAST_SIGNAL];
static GParamSpec *properties[N_PROPERTIES] = { NULL };

static void example_gi_object_get_property(GObject *object,
                                            unsigned int prop_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
  ExampleGiObject *gobject = EXAMPLE_GI_OBJECT(object);
  ExampleGiObjectPrivate *priv = example_gi_object_get_instance_private(gobject);

  switch (prop_id)
    {
    case PROP_INTEGER:
      g_value_set_int(value, priv->integer);
      break;
    case PROP_STRING:
      g_value_set_string(value, priv->string);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    }

  /* Never chain up! */
}

static void example_gi_object_set_property(GObject *object,
                                            unsigned int prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
  ExampleGiObject *gobject = EXAMPLE_GI_OBJECT(object);
  ExampleGiObjectPrivate *priv = example_gi_object_get_instance_private(gobject);

  switch (prop_id)
    {
    case PROP_INTEGER:
      priv->integer = g_value_get_int(value);
      break;
    case PROP_STRING:
      priv->string = g_value_dup_string(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    }

  /* Never chain up! */
}

static void example_gi_object_constructed(GObject *object)
{
  /* Runs after init() and after the properties given to g_object_new() have been set */

  G_OBJECT_CLASS(example_gi_object_parent_class)->constructed(object);
}
  

static void example_gi_object_dispose(GObject *object)
{
  /* For anything with a reference count, including: GObjects and Boxed types.
   *
   * NOTE: This is meant to break cycles and can be called multiple times.
   */

  /* g_clear_object(&priv->some_object);
   * g_clear_pointer(&priv->boxed, example_gi_boxed_unref);
   */

  G_OBJECT_CLASS(example_gi_object_parent_class)->dispose(object);
}

static void example_gi_object_finalize(GObject *object)
{
  ExampleGiObject *gobject = EXAMPLE_GI_OBJECT(object);
  ExampleGiObjectPrivate *priv = example_gi_object_get_instance_private(gobject);

  /* This will be called only once, free everything not freed in dispose() */

  g_free(priv->string);

  G_OBJECT_CLASS(example_gi_object_parent_class)->finalize(object);
}


static void example_gi_object_class_init(ExampleGiObjectClass *klass)
{
  GType the_type = G_TYPE_FROM_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = example_gi_object_get_property;
  object_class->set_property = example_gi_object_set_property;
  object_class->constructed = example_gi_object_constructed;
  object_class->dispose = example_gi_object_dispose;
  object_class->finalize = example_gi_object_finalize;

  properties[PROP_INTEGER] =
    g_param_spec_int("integer", "Integer", "An Integer",
                     /* Valid value range */
                     G_MININT, G_MAXINT,
                     0,
                     G_PARAM_READWRITE |
                     G_PARAM_STATIC_STRINGS);

  properties[PROP_STRING] =
    g_param_spec_string("string", "String", "A String",
                        NULL,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  signals[CHANGED] =
    g_signal_new("changed", the_type, G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(ExampleGiObjectClass, changed),
                 NULL, NULL, g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  g_object_class_install_properties(object_class, N_PROPERTIES, properties);
}

static void example_gi_object_init(ExampleGiObject *gobject)
{
  ExampleGiObjectPrivate *priv = example_gi_object_get_instance_private(gobject);

  /* Called before the properties given to g_object_new() are set */

  priv->integer = 47;
  priv->string = "replaced by string given to example_gi_object_new()";
}

ExampleGiObject *example_gi_object_new(const char *string)
{
  return EXAMPLE_GI_OBJECT(g_object_new(EXAMPLE_GI_TYPE_OBJECT,
                                         "string", string,
                                         NULL));
}
