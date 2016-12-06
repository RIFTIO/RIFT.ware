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
 * Creation Date: 3/24/16
 * 
 */

#include "rw_keyspec.h"
#include "rw_pb_schema.h"
#include "yangtest_common.hpp"
#include "rw-composite-d.pb-c.h"
#include "rift-cli-test.pb-c.h"

void create_dynamic_composite_d(const rw_yang_pb_schema_t** dynamic)
{
  // 1. rw-base-d and rw-fpath-d
  const rw_yang_pb_schema_t* base  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwBaseD);
  const rw_yang_pb_schema_t* fpath = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathD);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, base, fpath);
  ASSERT_TRUE(unified);
  ASSERT_EQ(unified, fpath);

  // 2. 1 plus rwuagent-cli-d
  const rw_yang_pb_schema_t* clid = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwuagentCliD);
  const rw_yang_pb_schema_t* unified2 = rw_schema_merge(NULL, unified, clid);

  ASSERT_TRUE(unified2);
  ASSERT_NE(unified2, unified);
  ASSERT_NE(unified2, clid);

  ASSERT_TRUE(unified2->modules);
  ASSERT_EQ(unified2->module_count, 11);

  ASSERT_TRUE(unified2->schema_module);

  // 3. 2 plus rw-appmgr-d
  const rw_yang_pb_schema_t* appmgr = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwAppmgrD);
  const rw_yang_pb_schema_t* unified3 = rw_schema_merge(NULL, unified2, appmgr);

  ASSERT_TRUE(unified3);
  ASSERT_NE(unified3, unified2);
  ASSERT_NE(unified3, appmgr);

  ASSERT_TRUE(unified3->modules);
  ASSERT_EQ(unified3->module_count, 13);

  ASSERT_TRUE(unified3->schema_module);


  //4. 3 plus rw-appmgr-log-d
  const rw_yang_pb_schema_t* appmgr_logd = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwAppmgrLogD);
  const rw_yang_pb_schema_t* unified4 = rw_schema_merge(NULL, unified3, appmgr_logd);

  ASSERT_TRUE(unified4);
  ASSERT_NE(unified4, unified3);
  ASSERT_NE(unified4, appmgr_logd);

  ASSERT_TRUE(unified4->modules);
  ASSERT_EQ(unified4->module_count, 14);

  ASSERT_TRUE(unified4->schema_module);

  //5. 4 plus rwmanifest-d
  const rw_yang_pb_schema_t* manif = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwmanifestD);
  const rw_yang_pb_schema_t* unified5 = rw_schema_merge(NULL, unified4, manif);

  ASSERT_TRUE(unified5);
  ASSERT_NE(unified5, unified4);
  ASSERT_NE(unified5, manif);

  ASSERT_TRUE(unified5->modules);
  ASSERT_EQ(unified5->module_count, 15);

  ASSERT_TRUE(unified5->schema_module);

  //6. 5 plus rw-vcs-d
  const rw_yang_pb_schema_t* vcs = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwVcsD);
  const rw_yang_pb_schema_t* unified6 = rw_schema_merge(NULL, unified5, vcs);

  ASSERT_TRUE(unified6);
  ASSERT_NE(unified6, unified5);
  ASSERT_NE(unified6, manif);

  ASSERT_TRUE(unified6->modules);
  ASSERT_EQ(unified6->module_count, 16);

  ASSERT_TRUE(unified6->schema_module);

  //7. 6 plus rwlog-mgmt-d
  const rw_yang_pb_schema_t* logmg = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwlogMgmtD);
  const rw_yang_pb_schema_t* unified7 = rw_schema_merge(NULL, unified6, logmg);

  ASSERT_TRUE(unified7);
  ASSERT_NE(unified7, unified6);
  ASSERT_NE(unified7, logmg);

  ASSERT_TRUE(unified7->modules);
  ASSERT_EQ(unified7->module_count, 17);

  ASSERT_TRUE(unified7->schema_module);

  //8. 7 plus rw-3gpp-types-d
  const rw_yang_pb_schema_t* gpp = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(Rw3gppTypesD);
  const rw_yang_pb_schema_t* unified8 = rw_schema_merge(NULL, unified7, gpp);

  ASSERT_TRUE(unified8);
  ASSERT_EQ(unified8, unified7);

  //9. 8 plus rwdts-data-d
  const rw_yang_pb_schema_t* rwdts = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwdtsDataD);
  const rw_yang_pb_schema_t* unified9 = rw_schema_merge(NULL, unified8, rwdts);

  ASSERT_TRUE(unified9);
  ASSERT_NE(unified9, unified8);
  ASSERT_NE(unified9, rwdts);

  ASSERT_TRUE(unified9->modules);
  ASSERT_EQ(unified9->module_count, 18);

  //10. 9 plus rw-dts-api-log-d
  const rw_yang_pb_schema_t* rwdtsa = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwDtsApiLogD);
  const rw_yang_pb_schema_t* unified10 = rw_schema_merge(NULL, unified9, rwdtsa);

  ASSERT_TRUE(unified10);
  ASSERT_NE(unified10, unified9);
  ASSERT_NE(unified10, rwdtsa);

  ASSERT_TRUE(unified10->modules);
  ASSERT_EQ(unified10->module_count, 19);

  ASSERT_TRUE(unified10->schema_module);

  //11. 10 plus rw-dts-router-log-d
  const rw_yang_pb_schema_t* rwdtsr = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwDtsRouterLogD);
  const rw_yang_pb_schema_t* unified11 = rw_schema_merge(NULL, unified10, rwdtsr);

  ASSERT_TRUE(unified11);
  ASSERT_NE(unified11, unified10);
  ASSERT_NE(unified10, rwdtsr);

  ASSERT_TRUE(unified11->modules);
  ASSERT_EQ(unified11->module_count, 20);

  ASSERT_TRUE(unified11->schema_module);

  //12. 11 plus rwmsg-data-d
  const rw_yang_pb_schema_t* rwmsg = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwmsgDataD);
  const rw_yang_pb_schema_t* unified12 = rw_schema_merge(NULL, unified11, rwmsg);

  ASSERT_TRUE(unified12);
  ASSERT_NE(unified12, unified11);
  ASSERT_NE(unified12, rwmsg);

  ASSERT_TRUE(unified12->modules);
  ASSERT_EQ(unified12->module_count, 21);

  ASSERT_TRUE(unified12->schema_module);

  //13. 12 plus rw-fpath-log-d
  const rw_yang_pb_schema_t* fpathl = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathLogD);
  const rw_yang_pb_schema_t* unified13 =  rw_schema_merge(NULL, unified12, fpathl);

  ASSERT_TRUE(unified13);
  ASSERT_NE(unified13, unified12);
  ASSERT_NE(unified13, fpathl);

  ASSERT_TRUE(unified13->modules);
  ASSERT_EQ(unified13->module_count, 22);

  ASSERT_TRUE(unified13->schema_module);

  //14. 13 plus rw-ipsec-d
  const rw_yang_pb_schema_t* ipsec = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwIpsecD);
  const rw_yang_pb_schema_t* unified14 = rw_schema_merge(NULL, unified13, ipsec);

  ASSERT_TRUE(unified14);
  ASSERT_NE(unified14, unified13);
  ASSERT_NE(unified14, ipsec);

  ASSERT_TRUE(unified14->modules);
  ASSERT_EQ(unified14->module_count, 23);

  ASSERT_TRUE(unified14->schema_module);

  *dynamic = unified14;
}

RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *get_general_containers(int num_list_entries)
{
  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *gc =
      (RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *)
      malloc (sizeof (RWPB_T_MSG(RiftCliTest_data_GeneralContainer)));

  RWPB_F_MSG_INIT(RiftCliTest_data_GeneralContainer,gc);

  gc->n_g_list = num_list_entries;
  gc->g_list = (RWPB_T_MSG(RiftCliTest_data_GeneralContainer_GList) **)
      malloc (sizeof (RWPB_T_MSG(RiftCliTest_data_GeneralContainer_GList) *) *
              gc->n_g_list);

  for (size_t i = 0; i < gc->n_g_list; i++) {
    gc->g_list[i] =
        (RWPB_T_MSG(RiftCliTest_data_GeneralContainer_GList)* )
        malloc (sizeof (RWPB_T_MSG(RiftCliTest_data_GeneralContainer_GList)));
    RWPB_F_MSG_INIT(RiftCliTest_data_GeneralContainer_GList,gc->g_list[i]);
    gc->g_list[i]->index = i * 10;
    gc->g_list[i]->has_index = 1;
    gc->g_list[i]->gcl_container = (RWPB_T_MSG(RiftCliTest_data_GeneralContainer_GList_GclContainer) *)
        malloc (sizeof (RWPB_T_MSG(RiftCliTest_data_GeneralContainer_GList_GclContainer)));
    RWPB_F_MSG_INIT(RiftCliTest_data_GeneralContainer_GList_GclContainer, gc->g_list[i]->gcl_container);
    if (i%2) {
      gc->g_list[i]->gcl_container->has_gclc_empty = 1;
      gc->g_list[i]->gcl_container->gclc_empty = 1;
    } else {
      gc->g_list[i]->gcl_container->has_having_a_bool = 1;
      gc->g_list[i]->gcl_container->having_a_bool = 1;
    }
  }
  return gc;
}

std::string get_rift_root()
{
  const char* rift_root = getenv("RIFT_ROOT");
  if (nullptr == rift_root) {
    std::cerr << "Unable to find $RIFT_ROOT" << std::endl;
    throw std::exception();
  }
  return std::string(rift_root);
}
