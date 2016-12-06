
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libpeas/peas.h>
#include "yangmodel_plugin.h"
#include "yangmodel_plugin-c.h"
#include "yangmodel.h"

#undef FPRINTF
//#define FPRINTF(file, ...) fprintf(file, ##__VA_ARGS__)
#define FPRINTF(file, ...)

/* Model interface */
static guint64 yangmodel_plugin__model__alloc(YangmodelPluginModel * m) {
    return (guint64)rw_yang_model_create_libncx();
}

static void yangmodel_plugin__model__free(YangmodelPluginModel * m, guint64 model) {
    rw_delete_yang_model((rw_yang_model_t *)model);
}

static guint64 yangmodel_plugin__model__load_module(YangmodelPluginModel * m, guint64 model, const gchar * module_name) {
    return (guint64)rw_yang_model_load_module((rw_yang_model_t *)model, (const char *)module_name);
}

static rw_status_t yangmodel_plugin__model__load_schema_ypbc(YangmodelPluginModel * m, guint64 model, rw_yang_pb_schema_t* schema) {
    return rw_yang_model_load_schema_ypbc((rw_yang_model_t *)model, schema);
}


static guint64 yangmodel_plugin__model__get_root_node(YangmodelPluginModel * m, guint64 model) {
    return (guint64)rw_yang_model_get_root_node((rw_yang_model_t *)model);
}

static guint64 yangmodel_plugin__model__search_module_ns(YangmodelPluginModel * m, guint64 model, const gchar * ns) {
    return (guint64)rw_yang_model_search_module_ns((rw_yang_model_t *)model, (const char *)ns);
}

static guint64 yangmodel_plugin__model__search_module_name_rev(YangmodelPluginModel * m, guint64 model, const gchar * name, const gchar * revision) {
    return (guint64)rw_yang_model_search_module_name_rev((rw_yang_model_t *)model, (const char *)name, (const char *)revision);
}

static guint64 yangmodel_plugin__model__search_module_ypbc(YangmodelPluginModel * m, guint64 model, guint64 ypbc_mod) {
    return (guint64)rw_yang_model_search_module_ypbc((rw_yang_model_t *)model, (const rw_yang_pb_module_t *)ypbc_mod);
}

static guint64 yangmodel_plugin__model__get_first_module(YangmodelPluginModel * m, guint64 model) {
    return (guint64)rw_yang_model_get_first_module((rw_yang_model_t *)model);
}

static guint64 yangmodel_plugin__model__load_and_get_schema(YangmodelPluginModel * m, const gchar * schema_name) {
  return (guint64)rw_yang_model_load_and_get_schema(schema_name);
}

static void yangmodel_plugin__model__set_log_level(YangmodelPluginModel * m, guint64 model, guint32 level) {
    rw_yang_model_set_log_level((rw_yang_model_t *)model, (rw_yang_log_level_t)level);
}

static rw_yang_log_level_t yangmodel_plugin__model__log_level(YangmodelPluginModel * m, guint64 model) {
    return (guint32)rw_yang_model_log_level((rw_yang_model_t *) model);
}

/* Module interface */
static void yangmodel_plugin__module__location(YangmodelPluginModule * m, guint64 module, gchar ** str) {
    rw_yang_module_get_location((rw_yang_module_t *)module, (char **)str);
}

static const gchar * yangmodel_plugin__module__description(YangmodelPluginModule * m, guint64 module) {
    return (const gchar *)rw_yang_module_get_description((rw_yang_module_t *)module);
}

static const gchar * yangmodel_plugin__module__name(YangmodelPluginModule * m, guint64 module) {
    return (const gchar *)rw_yang_module_get_name((rw_yang_module_t *)module);
}

static const gchar * yangmodel_plugin__module__prefix(YangmodelPluginModule * m, guint64 module) {
    return (const gchar *)rw_yang_module_get_prefix((rw_yang_module_t *)module);
}

static const char * yangmodel_plugin__module__ns(YangmodelPluginModule * m, guint64 module) {
    return (const gchar *)rw_yang_module_get_ns((rw_yang_module_t *)module);
}

static guint64 yangmodel_plugin__module__next_module(YangmodelPluginModule * m, guint64 module) {
    return (guint64)rw_yang_module_get_next_module((rw_yang_module_t *)module);
}

static gboolean yangmodel_plugin__module__loaded_explicitly(YangmodelPluginModule * m, guint64 module) {
    return (gboolean)rw_yang_module_was_loaded_explicitly((rw_yang_module_t *)module);
}

static guint64 yangmodel_plugin__module__first_node(YangmodelPluginModule * m, guint64 module) {
    return (guint64)rw_yang_module_get_first_node((rw_yang_module_t *)module);
}

static guint64 yangmodel_plugin__module__first_extension(YangmodelPluginModule * m, guint64 module) {
    return (guint64)rw_yang_module_get_first_extension((rw_yang_module_t *)module);
}

static guint64 yangmodel_plugin__module__first_augment(YangmodelPluginModule * m, guint64 module) {
    return (guint64)rw_yang_module_get_first_augment((rw_yang_module_t *)module);
}

/* Node interface */
static void yangmodel_plugin__node__location(YangmodelPluginNode * n, guint64 node, gchar ** str) {
    rw_yang_node_get_location((rw_yang_node_t *)node, (char **)str);
}

static const gchar * yangmodel_plugin__node__description(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_description((rw_yang_node_t *)node);
}

static const gchar * yangmodel_plugin__node__help_short(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_help_short((rw_yang_node_t *)node);
}

static const gchar * yangmodel_plugin__node__help_full(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_help_full((rw_yang_node_t *)node);
}

static const gchar * yangmodel_plugin__node__name(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_name((rw_yang_node_t *)node);
}

static const gchar * yangmodel_plugin__node__prefix(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_prefix((rw_yang_node_t *)node);
}

static const gchar * yangmodel_plugin__node__ns(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_ns((rw_yang_node_t *)node);
}

static const gchar * yangmodel_plugin__node__default_value(YangmodelPluginNode * n, guint64 node) {
    return (const gchar *)rw_yang_node_get_default_value((rw_yang_node_t *)node);
}

static guint32 yangmodel_plugin__node__max_elements(YangmodelPluginNode * n, guint64 node) {
    return (guint32)rw_yang_node_get_max_elements((rw_yang_node_t *)node);
}

static rw_yang_stmt_type_t yangmodel_plugin__node__stmt_type(YangmodelPluginNode * n, guint64 node) {
    return (rw_yang_stmt_type_t)rw_yang_node_get_stmt_type((rw_yang_node_t *)node);
}

static rw_yang_node_t * yangmodel_plugin__node__leafref_ref(YangmodelPluginNode * n, guint64 node) {
    return (rw_yang_node_t *)rw_yang_node_get_leafref_ref((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_config(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_config((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_leafy(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_leafy((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_listy(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_listy((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_key(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_key((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__has_keys(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_has_keys((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_presence(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_presence((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_mandatory(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_mandatory((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__parent_node(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_parent((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__first_child(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_first_child((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__next_sibling(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_next_sibling((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__search_child(YangmodelPluginNode * n, guint64 node, gchar * name, gchar * ns) {
    return (guint64)rw_yang_node_search_child((rw_yang_node_t *)node, (char *)name, (char *)ns);
}

static guint64 yangmodel_plugin__node__type(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_node_type((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__first_value(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_first_value((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__first_key(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_first_key((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__first_extension(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_first_extension((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__search_extension(YangmodelPluginNode * n, guint64 node, gchar * module, gchar * ext) {
    return (guint64)rw_yang_node_search_extension((rw_yang_node_t *)node, (char *)module, (char *)ext);
}

static guint64 yangmodel_plugin__node__module(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_module((rw_yang_node_t *)node);
}

static guint64 yangmodel_plugin__node__module_orig(YangmodelPluginNode * n, guint64 node) {
    return (guint64)rw_yang_node_get_module_orig((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__matches_prefix(YangmodelPluginNode * n, guint64 node, gchar * prefix) {
    return (gboolean)rw_yang_node_matches_prefix((rw_yang_node_t *)node, (const char *)prefix);
}

static gboolean yangmodel_plugin__node__is_mode_path(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_mode_path((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_rpc(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_rpc((rw_yang_node_t *)node);
}

static gboolean yangmodel_plugin__node__is_rpc_input(YangmodelPluginNode * n, guint64 node) {
    return (gboolean)rw_yang_node_is_rpc_input((rw_yang_node_t *)node);
}

static void yangmodel_plugin__node__to_json_schema(YangmodelPluginNode * n, guint64 node, gchar ** str, gboolean pretty_print) {
    rw_yang_node_to_json_schema((rw_yang_node_t *)node, (char **)str, (bool)pretty_print);
}

/* Key interface */
static guint64 yangmodel_plugin__key__list_node(YangmodelPluginKey * k, guint64 key) {
    return (guint64)rw_yang_key_get_list_node((rw_yang_key_t *)key);
}

static guint64 yangmodel_plugin__key__key_node(YangmodelPluginKey * k, guint64 key) {
    return (guint64)rw_yang_key_get_key_node((rw_yang_key_t *)key);
}

static guint64 yangmodel_plugin__key__next_key(YangmodelPluginKey * k, guint64 key) {
    return (guint64)rw_yang_key_get_next_key((rw_yang_key_t *)key);
}

/* Type interface */
static void yangmodel_plugin__type__location(YangmodelPluginType * t, guint64 type, gchar ** str) {
    rw_yang_type_get_location((rw_yang_type_t *)type, (char **)str);
}

static const gchar * yangmodel_plugin__type__description(YangmodelPluginType * t, guint64 type) {
    return (const gchar *)rw_yang_type_get_description((rw_yang_type_t *)type);
}

static const gchar * yangmodel_plugin__type__help_short(YangmodelPluginType * t, guint64 type) {
    return (const gchar *)rw_yang_type_get_help_short((rw_yang_type_t *)type);
}

static const gchar * yangmodel_plugin__type__help_full(YangmodelPluginType * t, guint64 type) {
    return (const gchar *)rw_yang_type_get_help_full((rw_yang_type_t *)type);
}

static const gchar * yangmodel_plugin__type__prefix(YangmodelPluginType * t, guint64 type) {
    return (const gchar *)rw_yang_type_get_prefix((rw_yang_type_t *)type);
}

static const gchar * yangmodel_plugin__type__ns(YangmodelPluginType * t, guint64 type) {
    return (const gchar *)rw_yang_type_get_ns((rw_yang_type_t *)type);
}

static rw_yang_leaf_type_t yangmodel_plugin__type__leaf_type(YangmodelPluginType * t, guint64 type) {
    return rw_yang_type_get_leaf_type((rw_yang_type_t *)type);
}

static guint64 yangmodel_plugin__type__first_value(YangmodelPluginType * t, guint64 type) {
    return (guint64)rw_yang_type_get_first_value((rw_yang_type_t *)type);
}

static guint32 yangmodel_plugin__type__count_matches(YangmodelPluginType * t, guint64 type, gchar * value_string) {
    return (guint32)rw_yang_type_count_matches((rw_yang_type_t *)type, (char *)value_string);
}

static guint32 yangmodel_plugin__type__count_partials(YangmodelPluginType * t, guint64 type, gchar * value_string) {
    return (guint32)rw_yang_type_count_partials((rw_yang_type_t *)type, (char *)value_string);
}

static guint64 yangmodel_plugin__type__first_extension(YangmodelPluginType * t, guint64 type) {
    return (guint64)rw_yang_type_get_first_extension((rw_yang_type_t *)type);
}

/* Value interface */
static void yangmodel_plugin__value__location(YangmodelPluginValue * v, guint64 value, gchar ** str) {
    rw_yang_value_get_location((rw_yang_value_t *)value, (char **)str);
}

static const gchar * yangmodel_plugin__value__name(YangmodelPluginValue * v, guint64 value) {
    return (const gchar *)rw_yang_value_get_name((rw_yang_value_t *)value);
}

static const gchar * yangmodel_plugin__value__description(YangmodelPluginValue * v, guint64 value) {
    return (const gchar *)rw_yang_value_get_description((rw_yang_value_t *)value);
}

static const gchar * yangmodel_plugin__value__help_short(YangmodelPluginValue * v, guint64 value) {
    return (const gchar *)rw_yang_value_get_help_short((rw_yang_value_t *)value);
}

static const gchar * yangmodel_plugin__value__help_full(YangmodelPluginValue * v, guint64 value) {
    return (const gchar *)rw_yang_value_get_help_full((rw_yang_value_t *)value);
}

static const gchar * yangmodel_plugin__value__prefix(YangmodelPluginValue * v, guint64 value) {
    return (const gchar *)rw_yang_value_get_prefix((rw_yang_value_t *)value);
}

static const gchar * yangmodel_plugin__value__ns(YangmodelPluginValue * v, guint64 value) {
    return (const gchar *)rw_yang_value_get_ns((rw_yang_value_t *)value);
}

static rw_yang_leaf_type_t yangmodel_plugin__value__leaf_type(YangmodelPluginValue * v, guint64 value) {
    return rw_yang_value_get_leaf_type((rw_yang_value_t *)value);
}

static guint64 yangmodel_plugin__value__max_length(YangmodelPluginValue * v, guint64 value) {
    return (guint64)rw_yang_value_get_max_length((rw_yang_value_t *)value);
}

static guint64 yangmodel_plugin__value__next_value(YangmodelPluginValue * v, guint64 value) {
    return (guint64)rw_yang_value_get_next_value((rw_yang_value_t *)value);
}

static guint64 yangmodel_plugin__value__first_extension(YangmodelPluginValue * v, guint64 value) {
    return (guint64)rw_yang_value_get_first_extension((rw_yang_value_t *)value);
}

static guint32 yangmodel_plugin__value__parse_value(YangmodelPluginValue * v, guint64 value, gchar * value_string) {
    return (guint32)rw_yang_value_parse_value((rw_yang_value_t *)value, (char *)value_string);
}

static guint32 yangmodel_plugin__value__parse_partial(YangmodelPluginValue * v, guint64 value, gchar * value_string) {
    return (guint32)rw_yang_value_parse_partial((rw_yang_value_t *)value, (char *)value_string);
}

static gboolean yangmodel_plugin__value__is_keyword(YangmodelPluginValue * v, guint64 value) {
    return (gboolean)rw_yang_value_is_keyword((rw_yang_value_t *)value);
}

/* Extension interface */
static void yangmodel_plugin__extension__location(YangmodelPluginExtension * e, guint64 extension, gchar ** str) {
    rw_yang_extension_get_location((rw_yang_extension_t *)extension, (char **)str);
}

static void yangmodel_plugin__extension__location_ext(YangmodelPluginExtension * e, guint64 extension, gchar ** str) {
    rw_yang_extension_get_location_ext((rw_yang_extension_t *)extension, (char **)str);
}

static const gchar * yangmodel_plugin__extension__value(YangmodelPluginExtension * e, guint64 extension) {
    return (const gchar *)rw_yang_extension_get_value((rw_yang_extension_t *)extension);
}

static const gchar * yangmodel_plugin__extension__description_ext(YangmodelPluginExtension * e, guint64 extension) {
    return (const gchar *)rw_yang_extension_get_description_ext((rw_yang_extension_t *)extension);
}

static const gchar * yangmodel_plugin__extension__name(YangmodelPluginExtension * e, guint64 extension) {
    return (const gchar *)rw_yang_extension_get_name((rw_yang_extension_t *)extension);
}

static const gchar * yangmodel_plugin__extension__module_ext(YangmodelPluginExtension * e, guint64 extension) {
    return (const gchar *)rw_yang_extension_get_module_ext((rw_yang_extension_t *)extension);
}

static const gchar * yangmodel_plugin__extension__prefix(YangmodelPluginExtension * e, guint64 extension) {
    return (const gchar *)rw_yang_extension_get_prefix((rw_yang_extension_t *)extension);
}

static const gchar * yangmodel_plugin__extension__ns(YangmodelPluginExtension * e, guint64 extension) {
    return (const gchar *)rw_yang_extension_get_ns((rw_yang_extension_t *)extension);
}

static guint64 yangmodel_plugin__extension__next_extension(YangmodelPluginExtension * e, guint64 extension) {
    return (guint64)rw_yang_extension_get_next_extension((rw_yang_extension_t *)extension);
}

static guint64 yangmodel_plugin__extension__search(YangmodelPluginExtension * e, guint64 extension, gchar * module, gchar * ext) {
    return (guint64)rw_yang_extension_search((rw_yang_extension_t *)extension, (char *)module, (char *)ext);
}

static gboolean yangmodel_plugin__extension__is_match(YangmodelPluginExtension * e, guint64 extension, gchar * module, gchar * ext) {
    return (gboolean)rw_yang_extension_is_match((rw_yang_extension_t *)extension, (char *)module, (char *)ext);
}

/*****************************************/
static void
yangmodel_plugin_c_extension_init(YangmodelPluginCExtension *plugin)
{
  // This is called once for each extension created
  FPRINTF(stderr, "yangmodel_plugin_c_extension_init() %p called\n",plugin);
}

static void
yangmodel_plugin_c_extension_class_init(YangmodelPluginCExtensionClass *klass)
{
  FPRINTF(stderr, "yangmodel_plugin_c_extension_class_init() %p called\n",klass);
}

static void
yangmodel_plugin_c_extension_class_finalize(YangmodelPluginCExtensionClass *klass)
{
  FPRINTF(stderr, "yangmodel_plugin_c_extension_class_finalize() %p called\n", klass);
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
