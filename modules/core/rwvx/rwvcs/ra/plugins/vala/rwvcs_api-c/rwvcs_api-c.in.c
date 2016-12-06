
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

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include <rwmain.h>
#include <rwvx.h>
#include <rwvcs.h>
#include <rwvcs_rwzk.h>
#include <rw-vcs.pb-c.h>

#include "rwvcs_api-c.h"

// Maximum size in bytes that a RWManifest file can be.
#define RWMANIFEST_MAX_XML_LEN 6553600


RW_CF_TYPE_DEFINE("RW.VCS API Component Type", rwvcs_api_component_ptr_t)
RW_CF_TYPE_DEFINE("RW.VCS API Instance Type", rwvcs_api_instance_ptr_t)


static rw_status_t
rwvcs_api__api__rwmanifest_pb_to_xml(RwvcsApiApi *self,
                                     guint8 *pb_data,
                                     int pbdata_length,
                                     gchar ** xml)
{
  char xml_buffer[RWMANIFEST_MAX_XML_LEN];

  // Unpacket the pbdata into an vcs_manifest protobuf structure
  void *rwmanifest = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(RwManifest_Manifest) ,pbdata_length, (const uint8_t *) pb_data);
  rw_xml_manager_t *xml_mgr = rw_xml_manager_create_xerces();

  rw_yang_model_t *yang_model = rw_xml_manager_get_yang_model(xml_mgr);
  RW_ASSERT(yang_model);

  rw_yang_module_t *rwmanifest_module = rw_yang_model_load_module(yang_model, "rw-manifest");
  if (rwmanifest_module == NULL)
  {
    rw_xml_manager_release(xml_mgr);
    RW_ASSERT(rwmanifest_module);

    return RW_STATUS_FAILURE;
  }

  // Do the conversion from protobuf to XML.
  rw_status_t rc = rw_xml_manager_pb_to_xml(xml_mgr, (void *)rwmanifest, xml_buffer, sizeof(xml_buffer));
  if (rc != 0)
  {
    rw_xml_manager_release(xml_mgr);
    RW_ASSERT(rc == 0);

    return RW_STATUS_FAILURE;
  }

  *xml = strdup(xml_buffer);

  return RW_STATUS_SUCCESS;
}


static void
rwvcs_api_c_extension_init(RwvcsApiCExtension *plugin)
{
}

static void
rwvcs_api_c_extension_class_init(RwvcsApiCExtensionClass *klass)
{
}

static void
rwvcs_api_c_extension_class_finalize(RwvcsApiCExtensionClass *klass)
{
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
