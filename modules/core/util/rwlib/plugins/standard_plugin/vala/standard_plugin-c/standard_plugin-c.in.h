
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

#ifndef __STANDARD_PLUGIN_C_H__
#define __STANDARD_PLUGIN_C_H__

#include <libpeas/peas.h>

G_BEGIN_DECLS

#define COMMON_INTERFACE_C_EXTENSION_TYPE (common_interface_c_extension_get_type ())

typedef struct _CommonInterfaceCExtension CommonInterfaceCExtension;
typedef struct _CommonInterfaceCExtensionClass CommonInterfaceCExtensionClass;

struct _CommonInterfaceCExtension {
  PeasExtensionBase parent_instance;
};

struct _CommonInterfaceCExtensionClass {
  PeasExtensionBaseClass parent_class;
};

GType common_interface_c_extension_get_type (void) G_GNUC_CONST;

G_END_DECLS

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

/* Don't modify the code below, it is autogenerated */

G_BEGIN_DECLS

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

#define STANDARD_PLUGIN_C_EXTENSION_TYPE (standard_plugin_c_extension_get_type())

typedef struct _StandardPluginCExtension       StandardPluginCExtension;
typedef struct _StandardPluginCExtensionClass StandardPluginCExtensionClass;

struct _StandardPluginCExtension {
  PeasExtensionBase parent_instance;
};

struct _StandardPluginCExtensionClass {
  PeasExtensionBaseClass parent_class;
};

GType standard_plugin_c_extension_get_type (void) G_GNUC_CONST;

//
//StandardPlugin.Api
//
#define STANDARD_PLUGIN_API_C_EXTENSION_TYPE (standard_plugin_api_c_extension_get_type())

typedef struct _StandardPluginApiCExtension       StandardPluginApiCExtension;
typedef struct _StandardPluginApiCExtensionClass StandardPluginApiCExtensionClass;

struct _StandardPluginApiCExtension {
  PeasExtensionBase parent_instance;
};

struct _StandardPluginApiCExtensionClass {
  PeasExtensionBaseClass parent_class;
};

GType standard_plugin_api_c_extension_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif //VAPI_TO_C_AUTOGEN

#endif //__STANDARD_PLUGIN_C_H__