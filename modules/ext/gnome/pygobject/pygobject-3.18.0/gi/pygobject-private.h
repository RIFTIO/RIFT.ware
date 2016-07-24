#ifndef _PYGOBJECT_PRIVATE_H_
#define _PYGOBJECT_PRIVATE_H_

#ifdef _PYGOBJECT_H_
#  error "include pygobject.h or pygobject-private.h, but not both"
#endif

#define _INSIDE_PYGOBJECT_
#include "pygobject.h"

#include "pyglib-python-compat.h"

#define PYGOBJECT_REGISTER_GTYPE(d, type, name, gtype)      \
  {                                                         \
    PyObject *o;					    \
    PYGLIB_REGISTER_TYPE(d, type, name);                    \
    PyDict_SetItemString(type.tp_dict, "__gtype__",         \
			 o=pyg_type_wrapper_new(gtype));    \
    Py_DECREF(o);                                           \
}

/* from gobjectmodule.c */
extern struct _PyGObject_Functions pygobject_api_functions;


#ifndef Py_CLEAR /* since Python 2.4 */
# define Py_CLEAR(op)				\
        do {                            	\
                if (op) {			\
                        PyObject *tmp = (PyObject *)(op);	\
                        (op) = NULL;		\
                        Py_DECREF(tmp);		\
                }				\
        } while (0)
#endif

extern GType PY_TYPE_OBJECT;

extern GQuark pygboxed_type_key;
extern GQuark pygboxed_marshal_key;
extern GQuark pygenum_class_key;
extern GQuark pygflags_class_key;
extern GQuark pyginterface_type_key;
extern GQuark pyginterface_info_key;
extern GQuark pygobject_class_init_key;
extern GQuark pygobject_class_key;
extern GQuark pygobject_wrapper_key;
extern GQuark pygpointer_class_key;
extern GQuark pygobject_has_updated_constructor_key;
extern GQuark pygobject_instance_data_key;
extern GQuark pygobject_custom_key;

void     pygobject_data_free  (PyGObjectData *data);
void     pyg_destroy_notify   (gpointer     user_data);
gboolean pyg_handler_marshal  (gpointer     user_data);
int      pygobject_constructv (PyGObject   *self,
                               guint        n_parameters,
                               GParameter  *parameters);

PyObject *pyg_integer_richcompare(PyObject *v,
                                  PyObject *w,
                                  int op);

void pygobject_ref_float(PyGObject *self);
void pygobject_ref_sink(PyGObject *self);

/* from pygtype.h */
extern PyTypeObject PyGTypeWrapper_Type;

PyObject *pyg_type_wrapper_new (GType type);
GType     pyg_type_from_object_strict (PyObject *obj, gboolean strict);
GType     pyg_type_from_object (PyObject *obj);

gint pyg_enum_get_value  (GType enum_type, PyObject *obj, gint *val);
gint pyg_flags_get_value (GType flag_type, PyObject *obj, guint *val);
int pyg_pyobj_to_unichar_conv (PyObject* py_obj, void* ptr);

GClosure *pyg_closure_new(PyObject *callback, PyObject *extra_args, PyObject *swap_data);
void	  pyg_closure_set_exception_handler(GClosure *closure,
					    PyClosureExceptionHandler handler);
GClosure *pyg_signal_class_closure_get(void);
GClosure *gclosure_from_pyfunc(PyGObject *object, PyObject *func);

PyObject *pyg_object_descr_doc_get(void);
void pygobject_object_register_types(PyObject *d);

extern PyTypeObject *PyGObject_MetaType;

/* from pygobject.h */
extern PyTypeObject PyGObject_Type;
extern PyTypeObject PyGProps_Type;
extern PyTypeObject PyGPropsDescr_Type;
extern PyTypeObject PyGPropsIter_Type;

  /* Data that belongs to the GObject instance, not the Python wrapper */
struct _PyGObjectData {
    PyTypeObject *type; /* wrapper type for this instance */
    GSList *closures;
};

void          pygobject_register_class   (PyObject *dict,
					  const gchar *type_name,
					  GType gtype, PyTypeObject *type,
					  PyObject *bases);
void          pygobject_register_wrapper (PyObject *self);
PyObject *    pygobject_new              (GObject *obj);
PyObject *    pygobject_new_full         (GObject *obj, gboolean steal, gpointer g_class);
void          pygobject_sink             (GObject *obj);
PyTypeObject *pygobject_lookup_class     (GType gtype);
void          pygobject_watch_closure    (PyObject *self, GClosure *closure);
int           pyg_type_register          (PyTypeObject *class,
					  const gchar *type_name);

/* from pygboxed.c */
extern PyTypeObject PyGBoxed_Type;

void       pyg_register_boxed (PyObject *dict, const gchar *class_name,
			       GType boxed_type, PyTypeObject *type);
PyObject * pyg_boxed_new      (GType boxed_type, gpointer boxed,
			       gboolean copy_boxed, gboolean own_ref);

extern PyTypeObject PyGPointer_Type;

void       pyg_register_pointer (PyObject *dict, const gchar *class_name,
				 GType pointer_type, PyTypeObject *type);
PyObject * pyg_pointer_new      (GType pointer_type, gpointer pointer);

const gchar * pyg_constant_strip_prefix(const gchar *name, const gchar *strip_prefix);

/* pygflags */
typedef struct {
    PYGLIB_PyLongObject parent;
    int zero_pad; /* must always be 0 */
    GType gtype;
} PyGFlags;

extern PyTypeObject PyGFlags_Type;

#define PyGFlags_Check(x) (PyObject_IsInstance((PyObject *)x, (PyObject *)&PyGFlags_Type) && g_type_is_a(((PyGFlags*)x)->gtype, G_TYPE_FLAGS))

extern PyObject * pyg_flags_add        (PyObject *   module,
					const char * type_name,
					const char * strip_prefix,
					GType        gtype);
extern PyObject * pyg_flags_from_gtype (GType        gtype,
					guint        value);

/* pygenum */
#define PyGEnum_Check(x) (PyObject_IsInstance((PyObject *)x, (PyObject *)&PyGEnum_Type) && g_type_is_a(((PyGFlags*)x)->gtype, G_TYPE_ENUM))

typedef struct {
    PYGLIB_PyLongObject parent;
    int zero_pad; /* must always be 0 */
    GType gtype;
} PyGEnum;

extern PyTypeObject PyGEnum_Type;

extern PyObject * pyg_enum_add        (PyObject *   module,
				       const char * type_name,
				       const char * strip_prefix,
				       GType        gtype);
extern PyObject * pyg_enum_from_gtype (GType        gtype,
				       int          value);

/* pygtype.c */
extern gboolean pyg_gtype_is_custom (GType gtype);

/* pygobject.c */
extern PyTypeObject PyGObjectWeakRef_Type;

static inline PyGObjectData *
pyg_object_peek_inst_data(GObject *obj)
{
    return ((PyGObjectData *)
            g_object_get_qdata(obj, pygobject_instance_data_key));
}

gboolean        pygobject_prepare_construct_properties  (GObjectClass *class,
                                                         PyObject *kwargs,
                                                         guint *n_params,
                                                         GParameter **params);
/* Defined by PYGLIB_MODULE_START */
extern PyObject *pyglib__gobject_module_create (void);

#endif /*_PYGOBJECT_PRIVATE_H_*/
