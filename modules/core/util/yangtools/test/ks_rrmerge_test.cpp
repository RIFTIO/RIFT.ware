/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file ks_rrmerge_test.cpp
 * @author Sujithra Periasamy
 * @date 2015/04/28
 * @brief Google test cases for reroot, merge schema APIs
 */

#include <limits.h>
#include <stdlib.h>

#include "rwut.h"
#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include "rw_schema.pb-c.h"
#include "bumpy-conversion.pb-c.h"
#include "testy2p-top2.pb-c.h"
#include "test-rwapis.pb-c.h"
#include "company.pb-c.h"
#include "document.pb-c.h"
#include "flat-conversion.pb-c.h"
#include "rw-fpath-d.pb-c.h"
#include "rwdts-data-d.pb-c.h"
#include "test-a-composite.pb-c.h"

#include "test-tag-generation-base.pb-c.h"
#include "test-tag-generation.pb-c.h"
#include "rw_namespace.h"

#define TCORP_NS(v) (((TCORP_NSID & 0xFFF)<<17)+(v))
#define TCORP_NSID rw_namespace_string_to_hash("http://riftio.com/ns/core/util/yangtools/tests/company")

static ProtobufCInstance* pinstance = nullptr;

TEST(KsRRMerge, ReRoot)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company, comp);

  comp.n_employee = 1;
  comp.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)malloc(sizeof(void*)*comp.n_employee);
  comp.employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,comp.employee[0]);
  comp.employee[0]->id = 100;
  comp.employee[0]->has_id = 1;
  comp.employee[0]->name = (char *)malloc(50);
  strcpy(comp.employee[0]->name, "ABCD");
  comp.employee[0]->title = (char *)malloc(50);
  strcpy(comp.employee[0]->title, "Software Eng");
  comp.employee[0]->start_date = (char *)malloc(50);
  strcpy(comp.employee[0]->start_date, "12/12/2012");

  // Create a out keyspec.
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *keyspec_out = nullptr;
  const rw_keyspec_path_t *const_keyspec = comp.employee[0]->base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&keyspec_out));
  ASSERT_TRUE(keyspec_out);

  keyspec_out->dompath.has_path001 = true;
  keyspec_out->dompath.path001.has_key00 = true;
  keyspec_out->dompath.path001.key00.id = 100;

  // Create a in keyspec.
  RWPB_T_PATHSPEC(Company_data_Company) *keyspec_in = nullptr;
  const_keyspec = comp.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&keyspec_in));
  ASSERT_TRUE(keyspec_in);

  // check the deeper case.
  ProtobufCMessage *out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(keyspec_in), nullptr, &comp.base, reinterpret_cast<rw_keyspec_path_t*>(keyspec_out));
  EXPECT_TRUE(out_msg);
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, out_msg, &comp.employee[0]->base));
  protobuf_c_message_free_unpacked(NULL, out_msg);
  out_msg = nullptr;

  // Check for keys mismatch
  keyspec_out->dompath.path001.key00.id = 300;
  out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(keyspec_in), nullptr, &comp.base, reinterpret_cast<rw_keyspec_path_t*>(keyspec_out));
  EXPECT_FALSE(out_msg);

  // Let us print the keyspec.
  char buff[1024];
  size_t buff_len = 1024;
  int ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(keyspec_out), nullptr,
                                                 reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company)), buff, buff_len);
  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/company:company/company:employee[company:id='300']");
  EXPECT_EQ(ret_len, 53);

  ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(keyspec_out), NULL, NULL, buff, buff_len);
  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/company:company/company:employee[company:id='300']");
  EXPECT_EQ(ret_len, 53);

  // Try printing wildcard keys.
  keyspec_out->dompath.path001.has_key00 = false;
  ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(keyspec_out), nullptr,
                                             reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company)), buff, buff_len);
  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/company:company/company:employee");

  ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(keyspec_out), NULL, NULL, buff, buff_len);
  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/company:company/company:employee");

  // Cleanup's
  protobuf_c_message_free_unpacked_usebody(NULL, &comp.base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_out->base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_in->base);
}

TEST(KsRRMerge, ReRootIn)
{
  RWPB_M_MSG_DECL_INIT(FlatConversion_data_Container1_TwoKeys_UnrootedPb,in_msg);
  in_msg.has_unroot_int = true;
  in_msg.unroot_int = 100;

  // Create in keyspec.
  RWPB_T_PATHSPEC(FlatConversion_data_Container1_TwoKeys_UnrootedPb) *ks_in = nullptr;
  const rw_keyspec_path_t *const_keyspec = in_msg.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&ks_in));
  EXPECT_TRUE(ks_in);

  ks_in->dompath.path001.has_key00 = true;
  ks_in->dompath.path001.key00.prim_enum = BASE_CONVERSION_CB_ENUM_SEVENTH;
  ks_in->dompath.path001.has_key01 = true;
  strcpy(ks_in->dompath.path001.key01.sec_string, "Second_key");

  // Create out keyspec.
  RWPB_T_PATHSPEC(FlatConversion_data_Container1) *ks_out = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&ks_out));
  EXPECT_TRUE(ks_out);

  // check the shallower case.
  ProtobufCMessage *out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr, &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(ks_out));
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(FlatConversion_data_Container1));

  auto *top_msg = RWPB_M_MSG_SAFE_CAST(FlatConversion_data_Container1, out_msg);

  EXPECT_EQ(top_msg->n_two_keys, 1);
  EXPECT_EQ(top_msg->two_keys[0].prim_enum, 7);
  EXPECT_STREQ(top_msg->two_keys[0].sec_string, "Second_key");
  EXPECT_TRUE(top_msg->two_keys[0].unrooted_pb.has_unroot_int);
  EXPECT_EQ(top_msg->two_keys[0].unrooted_pb.unroot_int, 100);

  protobuf_c_message_free_unpacked(NULL, out_msg);

  // Let us try using a generic keyspec for in_ks.
  rw_keyspec_path_update_binpath(reinterpret_cast<rw_keyspec_path_t *>(ks_in), nullptr);
  ProtobufCBinaryData data;
  data.data = NULL;
  data.len = 0;

  rw_keyspec_path_get_binpath(reinterpret_cast<rw_keyspec_path_t *>(ks_in), nullptr, const_cast<const uint8_t **>(&data.data), &data.len);

  rw_keyspec_path_t *g_ks = nullptr;
  g_ks = rw_keyspec_path_create_with_dompath_binary_data(nullptr, &data);
  EXPECT_TRUE(g_ks);

  out_msg = rw_keyspec_path_reroot(g_ks, nullptr, &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(ks_out));
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(FlatConversion_data_Container1));

  top_msg = RWPB_M_MSG_SAFE_CAST(FlatConversion_data_Container1, out_msg);

  EXPECT_EQ(top_msg->n_two_keys, 1);
  EXPECT_EQ(top_msg->two_keys[0].prim_enum, 7);
  EXPECT_STREQ(top_msg->two_keys[0].sec_string, "Second_key");
  EXPECT_TRUE(top_msg->two_keys[0].unrooted_pb.has_unroot_int);
  EXPECT_EQ(top_msg->two_keys[0].unrooted_pb.unroot_int, 100);

  protobuf_c_message_free_unpacked(NULL, out_msg);
  rw_keyspec_path_free(g_ks, nullptr);

  // Try print keyspec.
  char buff[1024];
  int buf_len = 1024;
  int ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr,
                                                 reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(FlatConversion)), buff, buf_len);

  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/conv:container-1/conv:two-keys[conv:prim-enum='seventh'][conv:sec-string='Second_key']/conv:unrooted-pb");
  EXPECT_EQ(ret_len, 106);

  ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , NULL, buff, buf_len);

  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/conv:container-1/conv:two-keys[conv:prim-enum='seventh'][conv:sec-string='Second_key']/conv:unrooted-pb");
  EXPECT_EQ(ret_len, 106);

  // Check for the same in and outkey spec.
  out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(ks_in));
  EXPECT_TRUE(out_msg);
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, out_msg, &in_msg.base));
  protobuf_c_message_free_unpacked(NULL, &ks_out->base);
  protobuf_c_message_free_unpacked(NULL, out_msg);

  // Check for a completely unrelated keyspec.
  ks_out = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_ContactInfo));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&ks_out));
  out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr, &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(ks_out));
  EXPECT_FALSE(out_msg);

  // Cleanup
  protobuf_c_message_free_unpacked_usebody(NULL, &in_msg.base);
  protobuf_c_message_free_unpacked(NULL, &ks_out->base);
  protobuf_c_message_free_unpacked(NULL, &ks_in->base);
}

TEST(KsRRMerge, ReRootLong)
{
  //Long keyspecs.
  // Create a in keyspec.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *k_in = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&k_in));

  k_in->dompath.has_path002 = true;
  k_in->dompath.path002.has_key00 = true;
  k_in->dompath.path002.key00.name = (char *)malloc(20);
  strcpy(k_in->dompath.path002.key00.name, "Profile1");

  k_in->dompath.has_path005 = true;
  k_in->dompath.path005.has_key00 = true;
  k_in->dompath.path005.key00.component_name = (char *)malloc(20);
  strcpy(k_in->dompath.path005.key00.component_name, "Component_Name1");

  k_in->dompath.has_path008 = true;
  k_in->dompath.path008.has_key00 = true;
  k_in->dompath.path008.key00.name = (char *)malloc(20);
  strcpy(k_in->dompath.path008.key00.name, "Event1");

  k_in->dompath.has_path009 = true;
  k_in->dompath.path009.has_key00 = true;
  k_in->dompath.path009.key00.name = (char *)malloc(20);
  strcpy(k_in->dompath.path009.key00.name, "Action1");


  // Long keyspec let us print.
  char buff[1024];
  size_t buf_len = 1024;

  int ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(k_in), nullptr,
                                                 reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(TestRwapis)), buff, buf_len);
  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/test-rwapis:config/test-rwapis:manifest/test-rwapis:profile[test-rwapis:name='Profile1']/test-rwapis:init-phase/test-rwapis:rwvcs/test-rwapis:component[test-rwapis:component-name='Component_Name1']/test-rwapis:rwcolony/test-rwapis:event-list/test-rwapis:event[test-rwapis:name='Event1']/test-rwapis:action[test-rwapis:name='Action1']/test-rwapis:start");

  ret_len = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(k_in), NULL, NULL, buff, buf_len);
  EXPECT_NE(ret_len, -1);
  EXPECT_STREQ(buff, "A,/test-rwapis:config/test-rwapis:manifest/test-rwapis:profile[test-rwapis:name='Profile1']/test-rwapis:init-phase/test-rwapis:rwvcs/test-rwapis:component[test-rwapis:component-name='Component_Name1']/test-rwapis:rwcolony/test-rwapis:event-list/test-rwapis:event[test-rwapis:name='Event1']/test-rwapis:action[test-rwapis:name='Action1']/test-rwapis:start");

  // Create in_msg;
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start,in_msg);

  in_msg.component_name = (char *)malloc(20);
  in_msg.instance_id = (char *)malloc(20);
  in_msg.instance_name = (char *)malloc(20);
  strcpy(in_msg.component_name, "Component_Name1");
  strcpy(in_msg.instance_id, "Instance_Id1");
  strcpy(in_msg.instance_name, "Instance_Name1");

  // Create an out keyspec.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile) *k_out = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile));
  rw_keyspec_path_create_dup(const_keyspec, NULL, reinterpret_cast<rw_keyspec_path_t**>(&k_out));

  k_out->dompath.has_path002 = true;
  k_out->dompath.path002.has_key00 = true;
  k_out->dompath.path002.key00.name = (char *)malloc(20);
  strcpy(k_out->dompath.path002.key00.name, "Profile1");

  ProtobufCMessage *out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(k_in), nullptr, &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(k_out));
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(TestRwapis_data_Config_Manifest_Profile));
  auto *out = RWPB_M_MSG_SAFE_CAST(TestRwapis_data_Config_Manifest_Profile, out_msg);

  EXPECT_TRUE(out->init_phase);
  EXPECT_TRUE(out->init_phase->rwvcs);
  EXPECT_EQ(out->init_phase->rwvcs->n_component, 1);
  EXPECT_TRUE(out->init_phase->rwvcs->component);
  EXPECT_STREQ(out->init_phase->rwvcs->component[0]->component_name, "Component_Name1");
  EXPECT_TRUE(out->init_phase->rwvcs->component[0]->rwcolony);
  EXPECT_TRUE(out->init_phase->rwvcs->component[0]->rwcolony->event_list);
  EXPECT_EQ(out->init_phase->rwvcs->component[0]->rwcolony->event_list->n_event, 1);
  EXPECT_TRUE(out->init_phase->rwvcs->component[0]->rwcolony->event_list->event);
  EXPECT_STREQ(out->init_phase->rwvcs->component[0]->rwcolony->event_list->event[0]->name, "Event1");
  EXPECT_EQ(out->init_phase->rwvcs->component[0]->rwcolony->event_list->event[0]->n_action, 1);
  EXPECT_TRUE(out->init_phase->rwvcs->component[0]->rwcolony->event_list->event[0]->action);
  EXPECT_STREQ(out->init_phase->rwvcs->component[0]->rwcolony->event_list->event[0]->action[0]->name, "Action1");
  EXPECT_TRUE(out->init_phase->rwvcs->component[0]->rwcolony->event_list->event[0]->action[0]->start);

  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &in_msg.base, &out->init_phase->rwvcs->component[0]->rwcolony->event_list->event[0]->action[0]->start->base));

  // Cleanup
  protobuf_c_message_free_unpacked_usebody(NULL, &in_msg.base);
  protobuf_c_message_free_unpacked(NULL, &k_out->base);
  protobuf_c_message_free_unpacked(NULL, &k_in->base);
  protobuf_c_message_free_unpacked(NULL, out_msg);
}

TEST(KsRRMerge, ReRootDeeper)
{
  // Deeper case with multiple list messages.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 3;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*3);
  for (unsigned i = 0; i < 3; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->inventory = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory,config.profile[i]->init_phase->inventory);
    config.profile[i]->init_phase->inventory->n_component = 2;
    config.profile[i]->init_phase->inventory->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) **)malloc(sizeof(void*)*2);
    for (unsigned j = 0; j < 2; j++) {
      config.profile[i]->init_phase->inventory->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component,config.profile[i]->init_phase->inventory->component[j]);
      config.profile[i]->init_phase->inventory->component[j]->name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->name, "%s%d", "component", j+1);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet,config.profile[i]->init_phase->inventory->component[j]->rwtasklet);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory = (char *)malloc(20);
      strcpy(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory, "/plugingdir");
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name, "%s%d", "myplugin", i+j+1);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version, "%s%d", "version", i+j+1);
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  // Now populate the keyvalues.
  // profile name
  out_ks->dompath.has_path002 = true;
  out_ks->dompath.path002.has_key00 = true;
  out_ks->dompath.path002.key00.name = (char *)malloc(20);
  sprintf(out_ks->dompath.path002.key00.name, "%s%d", "name", 3);

  //component name.
  out_ks->dompath.has_path005 = true;
  out_ks->dompath.path005.has_key00 = true;
  out_ks->dompath.path005.key00.name = (char *)malloc(20);
  sprintf(out_ks->dompath.path005.key00.name, "%s%d", "component", 2);

  ProtobufCMessage *out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr, &config.base, reinterpret_cast<rw_keyspec_path_t*>(out_ks));
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet));

  auto *msg = RWPB_M_MSG_SAFE_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet, out_msg);

  EXPECT_STREQ(msg->plugin_directory, "/plugingdir");
  EXPECT_STREQ(msg->plugin_name, "myplugin4");
  EXPECT_STREQ(msg->plugin_version, "version4");
  protobuf_c_message_free_unpacked(NULL, out_msg);

  // change the key.
  sprintf(out_ks->dompath.path002.key00.name, "%s%d", "name", 1);
  sprintf(out_ks->dompath.path005.key00.name, "%s%d", "component", 2);

  out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr, &config.base, reinterpret_cast<rw_keyspec_path_t*>(out_ks));
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet));

  msg = RWPB_M_MSG_SAFE_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet, out_msg);

  EXPECT_STREQ(msg->plugin_directory, "/plugingdir");
  EXPECT_STREQ(msg->plugin_name, "myplugin2");
  EXPECT_STREQ(msg->plugin_version, "version2");
  protobuf_c_message_free_unpacked(NULL, out_msg);

  // Give mismatching key.
  sprintf(out_ks->dompath.path005.key00.name, "%s%d", "component", 5);
  out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr, &config.base, reinterpret_cast<rw_keyspec_path_t*>(out_ks));
  EXPECT_FALSE(out_msg);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);
}

TEST(KsRRMerge, ReRootDeeper1)
{
  // inmsg.
  RWPB_M_MSG_DECL_INIT(Testy2pTop1_data_Top1c2, in_msg);

  in_msg.n_l1 = 4;
  for (unsigned i = 0; i < 4; i++) {
    in_msg.l1[i].n_l1 = 4;
    in_msg.l1[i].lf1 = i+1;
    in_msg.l1[i].has_lf1 = 1;
    for (unsigned j = 0; j < 4; j++) {
      sprintf(in_msg.l1[i].l1[j].ln1, "%s%d", "keystr", i+j+1);
      in_msg.l1[i].l1[j].has_ln1 = 1;
      in_msg.l1[i].l1[j].has_ln2 = true;
      in_msg.l1[i].l1[j].ln2 = i+j+1;
    }
  }

  RWPB_T_PATHSPEC(Testy2pTop1_data_Top1c2) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop1_data_Top1c2));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t **>(&in_ks));

  // create outkeyspec.
  RWPB_T_PATHSPEC(Testy2pTop1_data_Top1c2_L1_L1) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop1_data_Top1c2_L1_L1));
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t **>(&out_ks));

  //populate keys.
  out_ks->dompath.has_path001 = true;
  out_ks->dompath.path001.has_key00 = true;
  out_ks->dompath.path001.key00.lf1 = 1;
  out_ks->dompath.has_path002 = true;
  out_ks->dompath.path002.element_id.element_tag = 1000;
  out_ks->dompath.path002.has_key00 = true;
  sprintf(out_ks->dompath.path002.key00.ln1, "%s%d", "keystr", 1);

  ProtobufCMessage *out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr, &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(out_ks));
  EXPECT_TRUE(out_msg);

  auto *msg = RWPB_M_MSG_SAFE_CAST(Testy2pTop1_data_Top1c2_L1_L1, out_msg);

  EXPECT_TRUE(msg->has_ln2);
  EXPECT_STREQ(msg->ln1, "keystr1");
  EXPECT_EQ(msg->ln2, 1);
  protobuf_c_message_free_unpacked(NULL, out_msg);


  //change keys.
  out_ks->dompath.path001.key00.lf1 = 3;
  sprintf(out_ks->dompath.path002.key00.ln1, "%s%d", "keystr", 6);
  out_msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr, &in_msg.base, reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  EXPECT_TRUE(out_msg);

  msg = RWPB_M_MSG_SAFE_CAST(Testy2pTop1_data_Top1c2_L1_L1, out_msg);

  EXPECT_TRUE(msg->has_ln2);
  EXPECT_STREQ(msg->ln1, "keystr6");
  EXPECT_EQ(msg->ln2, 6);

  //cleanup
  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked(NULL, out_msg);
  protobuf_c_message_free_unpacked_usebody(NULL, &in_msg.base);
}

TEST(KsRRMerge, ReRootNeg)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company,comp);

  comp.n_employee = 3;
  comp.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)malloc(sizeof(void*)*comp.n_employee);
  for (unsigned i = 0; i < 3; i++) {
    comp.employee[i] = (RWPB_T_MSG(Company_data_Company_Employee) *)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
    RWPB_F_MSG_INIT(Company_data_Company_Employee,comp.employee[i]);
    comp.employee[i]->id = 100+i;
    comp.employee[i]->has_id = 1;
    comp.employee[i]->name = (char *)malloc(50);
    strcpy(comp.employee[i]->name, "ABCD");
    comp.employee[i]->title = (char *)malloc(50);
    strcpy(comp.employee[i]->title, "Software Eng");
    comp.employee[i]->start_date = (char *)malloc(50);
    strcpy(comp.employee[i]->start_date, "12/12/2012");
  }

  // Create a out keyspec.
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *keyspec_out = nullptr;
  const rw_keyspec_path_t *const_keyspec = comp.employee[0]->base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&keyspec_out));
  ASSERT_TRUE(keyspec_out);

  // Create a in keyspec.
  RWPB_T_PATHSPEC(Company_data_Company) *keyspec_in = nullptr;
  const_keyspec = comp.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr, reinterpret_cast<rw_keyspec_path_t**>(&keyspec_in));
  ASSERT_TRUE(keyspec_in);
  // No keys. Reroot should retun failure for wildcarded keyspec.

  auto *msg = rw_keyspec_path_reroot(reinterpret_cast<rw_keyspec_path_t*>(keyspec_in), nullptr,
                                     &comp.base, reinterpret_cast<rw_keyspec_path_t*>(keyspec_out));
  EXPECT_FALSE(msg);

  protobuf_c_message_free_unpacked_usebody(NULL, &comp.base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_out->base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_in->base);
}

TEST(KsRRMerge, ReRootMerge)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)),
                             nullptr,
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.has_path001 = true;
  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Product) *ks2 = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Product)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.has_path001 = true;
  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 500;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Product,msg2);

  msg2.id = 500;
  msg2.has_id = 1;
  msg2.name = strdup("Prod1");
  msg2.msrp = strdup("100d");

  rw_keyspec_path_t *ks_out = nullptr;
  ProtobufCMessage *out_msg = rw_keyspec_path_reroot_and_merge(nullptr,&ks1->rw_keyspec_path_t, &ks2->rw_keyspec_path_t,
                                                               &msg1.base, &msg2.base, &ks_out);
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(Company_data_Company));

  EXPECT_TRUE(ks_out);
  auto *ko = RWPB_F_PATHSPEC_CAST_OR_CRASH(Company_data_Company, ks_out);

  EXPECT_EQ(ko->base.descriptor, RWPB_G_PATHSPEC_PBCMD(Company_data_Company));
  EXPECT_TRUE(ko->has_dompath);
  EXPECT_EQ(ko->dompath.path000.element_id.element_tag, TCORP_NS(1));

  auto *om = (RWPB_T_MSG(Company_data_Company) *)out_msg;
  EXPECT_FALSE(om->contact_info);
  EXPECT_EQ(om->n_employee, 1);
  EXPECT_TRUE(om->employee);
  EXPECT_TRUE(om->employee[0]);

  EXPECT_EQ(om->employee[0]->id, 100);
  EXPECT_STREQ(om->employee[0]->name, "John");
  EXPECT_STREQ(om->employee[0]->title, "SE");
  EXPECT_STREQ(om->employee[0]->start_date, "14/12");

  EXPECT_EQ(om->n_product, 1);
  EXPECT_TRUE(om->product);
  EXPECT_TRUE(om->product[0]);

  EXPECT_EQ(om->product[0]->id, 500);
  EXPECT_STREQ(om->product[0]->name, "Prod1");
  EXPECT_STREQ(om->product[0]->msrp, "100d");

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(ks_out, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  protobuf_c_message_free_unpacked(NULL, out_msg);
}

TEST(KsRRMerge, ReRootMerge1)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.has_path001 = true;
  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks2 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.has_path001 = true;
  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 200;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,msg2);

  msg2.id = 200;
  msg2.has_id = 1;
  msg2.name = strdup("Alice");
  msg2.title = strdup("SE1");
  msg2.start_date = strdup("14/12");

  rw_keyspec_path_t *ks_out = nullptr;
  ProtobufCMessage *out_msg = rw_keyspec_path_reroot_and_merge(nullptr,&ks1->rw_keyspec_path_t, &ks2->rw_keyspec_path_t,
                                                               &msg1.base, &msg2.base, &ks_out);
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(Company_data_Company));

  EXPECT_TRUE(ks_out);
  auto *ko = RWPB_F_PATHSPEC_CAST_OR_CRASH(Company_data_Company, ks_out);

  EXPECT_EQ(ko->base.descriptor, RWPB_G_PATHSPEC_PBCMD(Company_data_Company));
  EXPECT_TRUE(ko->has_dompath);
  EXPECT_EQ(ko->dompath.path000.element_id.element_tag, TCORP_NS(1));

  auto *om = (RWPB_T_MSG(Company_data_Company) *)out_msg;
  EXPECT_FALSE(om->contact_info);
  EXPECT_EQ(om->n_employee, 2);
  EXPECT_TRUE(om->employee);

  EXPECT_TRUE(om->employee[0]);
  EXPECT_EQ(om->employee[1]->id, 100);
  EXPECT_STREQ(om->employee[1]->name, "John");
  EXPECT_STREQ(om->employee[1]->title, "SE");
  EXPECT_STREQ(om->employee[1]->start_date, "14/12");

  EXPECT_TRUE(om->employee[1]);
  EXPECT_EQ(om->employee[0]->id, 200);
  EXPECT_STREQ(om->employee[0]->name, "Alice");
  EXPECT_STREQ(om->employee[0]->title, "SE1");
  EXPECT_STREQ(om->employee[0]->start_date, "14/12");

  EXPECT_EQ(om->n_product, 0);
  EXPECT_FALSE(om->product);

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(ks_out, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  protobuf_c_message_free_unpacked(NULL, out_msg);
}

TEST(KsRRMerge, ReRootMerge2)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.has_path001 = true;
  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks2 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.has_path001 = true;
  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 200;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,msg2);

  msg2.id = 100;
  msg2.has_id = 1;
  msg2.name = strdup("Alice");

  rw_keyspec_path_t *ks_out = nullptr;
  ProtobufCMessage *out_msg = rw_keyspec_path_reroot_and_merge(nullptr,&ks1->rw_keyspec_path_t,
                                                               &ks2->rw_keyspec_path_t,
                                                               &msg1.base, &msg2.base, &ks_out);
  EXPECT_TRUE(out_msg);
  EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(Company_data_Company));

  EXPECT_TRUE(ks_out);
  auto *ko = RWPB_F_PATHSPEC_CAST_OR_CRASH(Company_data_Company, ks_out);

  EXPECT_EQ(ko->base.descriptor, RWPB_G_PATHSPEC_PBCMD(Company_data_Company));
  EXPECT_TRUE(ko->has_dompath);
  EXPECT_EQ(ko->dompath.path000.element_id.element_tag, TCORP_NS(1));

  auto *om = (RWPB_T_MSG(Company_data_Company) *)out_msg;
  EXPECT_FALSE(om->contact_info);
  EXPECT_EQ(om->n_employee, 1);
  EXPECT_TRUE(om->employee);

  EXPECT_TRUE(om->employee[0]);
  EXPECT_EQ(om->employee[0]->id, 100);
  EXPECT_STREQ(om->employee[0]->name, "Alice");
  EXPECT_STREQ(om->employee[0]->title, "SE");
  EXPECT_STREQ(om->employee[0]->start_date, "14/12");

  EXPECT_EQ(om->n_product, 0);
  EXPECT_FALSE(om->product);

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(ks_out, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  protobuf_c_message_free_unpacked(NULL, out_msg);
}

TEST(KsRRMerge, RerootIter)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company,comp);

  comp.n_employee = 3;
  comp.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)malloc(sizeof(void*)*comp.n_employee);
  for (unsigned i = 0; i < 3; i++) {
    comp.employee[i] = (RWPB_T_MSG(Company_data_Company_Employee) *)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
    RWPB_F_MSG_INIT(Company_data_Company_Employee,comp.employee[i]);
    comp.employee[i]->id = 100+i;
    comp.employee[i]->has_id = 1;
    comp.employee[i]->name = (char *)malloc(50);
    strcpy(comp.employee[i]->name, "ABCD");
    comp.employee[i]->title = (char *)malloc(50);
    strcpy(comp.employee[i]->title, "Software Eng");
    comp.employee[i]->start_date = (char *)malloc(50);
    strcpy(comp.employee[i]->start_date, "12/12/2012");
  }

  // Create a out keyspec.
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *keyspec_out = nullptr;
  const rw_keyspec_path_t *const_keyspec = comp.employee[0]->base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&keyspec_out));
  ASSERT_TRUE(keyspec_out);

  // Create a in keyspec.
  RWPB_T_PATHSPEC(Company_data_Company) *keyspec_in = nullptr;
  const_keyspec = comp.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&keyspec_in));
  ASSERT_TRUE(keyspec_in);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(keyspec_in), nullptr , &state,
                                   &comp.base, reinterpret_cast<rw_keyspec_path_t*>(keyspec_out));

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &comp.employee[i]->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(Company_data_Company_Employee, ks);

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_EQ(out->dompath.path001.key00.id, comp.employee[i]->id);

    i++;
  }
  rw_keyspec_path_reroot_iter_done(&state);

  protobuf_c_message_free_unpacked_usebody(NULL, &comp.base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_out->base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_in->base);
}

TEST(KsRRMerge, RerootIterDeeper)
{
  // Deeper case with multiple list messages.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 3;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*3);
  for (unsigned i = 0; i < 3; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->inventory = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory,config.profile[i]->init_phase->inventory);
    config.profile[i]->init_phase->inventory->n_component = 2;
    config.profile[i]->init_phase->inventory->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) **)malloc(sizeof(void*)*2);
    for (unsigned j = 0; j < 2; j++) {
      config.profile[i]->init_phase->inventory->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component,config.profile[i]->init_phase->inventory->component[j]);
      config.profile[i]->init_phase->inventory->component[j]->name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->name, "%s%d", "component", j+1);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet,config.profile[i]->init_phase->inventory->component[j]->rwtasklet);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory = (char *)malloc(20);
      strcpy(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory, "/plugingdir");
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name, "%s%d", "myplugin", i+j+1);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version, "%s%d", "version", i+j+1);
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0, j = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(out_msg, &config.profile[i]->init_phase->inventory->component[j]->rwtasklet->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet, ks);

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path002.key00.name, config.profile[i]->name));

    EXPECT_TRUE(out->dompath.path005.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path005.key00.name, config.profile[i]->init_phase->inventory->component[j]->name));

    j++; if (j == 2) { j = 0; i++; }
  }

  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(i, 3);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);
}

TEST(KsRRMerge, RerootIterDeeperL)
{
  // Deeper case with multiple list messages.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 4;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*4);
  for (unsigned i = 0; i < 4; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->rwvcs = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs, config.profile[i]->init_phase->rwvcs);
    config.profile[i]->init_phase->rwvcs->n_component = 3;
    config.profile[i]->init_phase->rwvcs->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) **)malloc(sizeof(void*)*3);
    for (unsigned j = 0; j < 3; j++) {
      config.profile[i]->init_phase->rwvcs->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component,config.profile[i]->init_phase->rwvcs->component[j]);
      config.profile[i]->init_phase->rwvcs->component[j]->component_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->rwvcs->component[j]->component_name, "%s%d", "component", j+1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list);
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->n_event = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->name = strdup("Event1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->n_action = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->name = strdup("Action1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->component_name = strdup("abcd");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_id = strdup("id1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_name = strdup("fpath");
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0, j = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(out_msg, &config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, ks); 

    EXPECT_TRUE(out->dompath.has_path002);
    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path002.key00.name, config.profile[i]->name));

    EXPECT_TRUE(out->dompath.has_path005);
    EXPECT_TRUE(out->dompath.path005.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path005.key00.component_name, config.profile[i]->init_phase->rwvcs->component[j]->component_name));

    EXPECT_TRUE(out->dompath.has_path008);
    EXPECT_TRUE(out->dompath.path008.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path008.key00.name, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->name));

    EXPECT_TRUE(out->dompath.has_path009);
    EXPECT_TRUE(out->dompath.path009.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path009.key00.name, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->name));

    j++; if (j == 3) { j = 0; i++; }
  }

  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(i, 4);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);

}

TEST(KsRRMerge, RerootIterDeeper1)
{
  // Deeper case with multiple list messages.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 4;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*4);
  for (unsigned i = 0; i < 4; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->rwvcs = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs, config.profile[i]->init_phase->rwvcs);
    config.profile[i]->init_phase->rwvcs->n_component = 3;
    config.profile[i]->init_phase->rwvcs->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) **)malloc(sizeof(void*)*3);
    for (unsigned j = 0; j < 3; j++) {
      config.profile[i]->init_phase->rwvcs->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component,config.profile[i]->init_phase->rwvcs->component[j]);
      config.profile[i]->init_phase->rwvcs->component[j]->component_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->rwvcs->component[j]->component_name, "%s%d", "component", j+1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list);
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->n_event = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->name = strdup("Event1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->n_action = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->name = strdup("Action1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->component_name = strdup("abcd");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_id = strdup("id1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_name = strdup("fpath");
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  out_ks->dompath.has_path005 = true;
  out_ks->dompath.path005.has_key00 = true;
  out_ks->dompath.path005.key00.component_name = strdup("component2");

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(out_msg, &config.profile[i]->init_phase->rwvcs->component[1]->rwcolony->event_list->event[0]->action[0]->start->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, ks);

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path002.key00.name, config.profile[i]->name));

    EXPECT_TRUE(out->dompath.path005.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path005.key00.component_name, "component2"));

    EXPECT_TRUE(out->dompath.path008.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path008.key00.name, "Event1"));

    EXPECT_TRUE(out->dompath.path009.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path009.key00.name, "Action1"));
    i++;
  }

  EXPECT_EQ(i, 4);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);
}

TEST(KsRRMerge, RerootIterGKs)
{
  // Test generic ks.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 4;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*4);
  for (unsigned i = 0; i < 4; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->rwvcs = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs, config.profile[i]->init_phase->rwvcs);
    config.profile[i]->init_phase->rwvcs->n_component = 3;
    config.profile[i]->init_phase->rwvcs->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) **)malloc(sizeof(void*)*3);
    for (unsigned j = 0; j < 3; j++) {
      config.profile[i]->init_phase->rwvcs->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component,config.profile[i]->init_phase->rwvcs->component[j]);
      config.profile[i]->init_phase->rwvcs->component[j]->component_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->rwvcs->component[j]->component_name, "%s%d", "component", j+1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list);
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->n_event = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->name = strdup("Event1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->n_action = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->name = strdup("Action1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->component_name = strdup("abcd");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_id = strdup("id1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_name = strdup("fpath");
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  out_ks->dompath.has_path005 = true;
  out_ks->dompath.path005.has_key00 = true;
  out_ks->dompath.path005.key00.component_name = strdup("component2");

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t out_ks_dup(
      rw_keyspec_path_create_dup_of_type(&out_ks->rw_keyspec_path_t, NULL, &rw_schema_path_spec__descriptor));

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , &state,
                                   &config.base,
                                   out_ks_dup.get());

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(out_msg, &config.profile[i]->init_phase->rwvcs->component[1]->rwcolony->event_list->event[0]->action[0]->start->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t iter_out_ks(
        rw_keyspec_path_create_dup_of_type(
            ks, nullptr,
            RWPB_G_PATHSPEC_PBCMD(
                TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start)));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, iter_out_ks.get());

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path002.key00.name, config.profile[i]->name));

    EXPECT_TRUE(out->dompath.path005.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path005.key00.component_name, "component2"));

    EXPECT_TRUE(out->dompath.path008.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path008.key00.name, "Event1"));

    EXPECT_TRUE(out->dompath.path009.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path009.key00.name, "Action1"));
    i++;
  }

  EXPECT_EQ(i, 4);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);
}

TEST(KsRRMerge, RerootIter3)
{
  // Deeper case with multiple list messages.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 4;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*4);
  for (unsigned i = 0; i < 4; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->rwvcs = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs, config.profile[i]->init_phase->rwvcs);
    config.profile[i]->init_phase->rwvcs->n_component = 3;
    config.profile[i]->init_phase->rwvcs->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) **)malloc(sizeof(void*)*3);
    for (unsigned j = 0; j < 3; j++) {
      config.profile[i]->init_phase->rwvcs->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component,config.profile[i]->init_phase->rwvcs->component[j]);
      config.profile[i]->init_phase->rwvcs->component[j]->component_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->rwvcs->component[j]->component_name, "%s%d", "component", j+1+i);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list);
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->n_event = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->name = strdup("Event1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->n_action = 1;
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) **) malloc(sizeof(void*)*1);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->name = strdup("Action1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start)));

      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start);

      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->component_name = strdup("abcd");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_id = strdup("id1");
      config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->instance_name = strdup("fpath");
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  out_ks->dompath.has_path005 = true;
  out_ks->dompath.path005.has_key00 = true;
  out_ks->dompath.path005.key00.component_name = strdup("component2");

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0, j = 1;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(out_msg, &config.profile[i]->init_phase->rwvcs->component[j]->rwcolony->event_list->event[0]->action[0]->start->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, ks); 

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path002.key00.name, config.profile[i]->name));

    EXPECT_TRUE(out->dompath.path005.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path005.key00.component_name, "component2"));

    EXPECT_TRUE(out->dompath.path008.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path008.key00.name, "Event1"));

    EXPECT_TRUE(out->dompath.path009.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path009.key00.name, "Action1"));
    i++; j = 0;
  }
  rw_keyspec_path_reroot_iter_done(&state);

  EXPECT_EQ(i, 2);

  strcpy(config.profile[2]->init_phase->rwvcs->component[1]->component_name, "mycomponent");
  free(out_ks->dompath.path005.key00.component_name);
  out_ks->dompath.path005.key00.component_name = strdup("mycomponent");

  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(out_msg, &config.profile[2]->init_phase->rwvcs->component[1]->rwcolony->event_list->event[0]->action[0]->start->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Start, ks);

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path002.key00.name, config.profile[2]->name));

    EXPECT_TRUE(out->dompath.path005.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path005.key00.component_name, "mycomponent"));

    EXPECT_TRUE(out->dompath.path008.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path008.key00.name, "Event1"));

    EXPECT_TRUE(out->dompath.path009.has_key00);
    EXPECT_FALSE(strcmp(out->dompath.path009.key00.name, "Action1"));
    i++;
  }
  rw_keyspec_path_reroot_iter_done(&state);

  EXPECT_EQ(i, 1);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);
}

TEST(KsRRMerge, RerootIter4)
{
  RWPB_M_MSG_DECL_INIT(Document_MainBook, book);

  book.n_chapters = 10;
  for (unsigned i = 0; i < 10; i++) {
    book.chapters[i].number = 100+i;
    book.chapters[i].has_number = 1;
    book.chapters[i].has_title_in_section = true;
    sprintf(book.chapters[i].title_in_section, "Title%d", i+1);
  }

  RWPB_T_PATHSPEC(Document_MainBook) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Document_MainBook));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  RWPB_T_PATHSPEC(Document_MainBook_Chapters) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Document_MainBook_Chapters));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t *>(in_ks), nullptr , &state,
                                   &book.base,
                                   reinterpret_cast<rw_keyspec_path_t *>(out_ks));

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);

    EXPECT_EQ(msg, &book.chapters[i].base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(Document_MainBook_Chapters, ks);

    EXPECT_TRUE(out->dompath.has_path001);
    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_EQ(out->dompath.path001.key00.number, 100+i);

    i++;
  }
  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(i, 10);

  out_ks->dompath.has_path001 = true;
  out_ks->dompath.path001.has_key00 = true;
  out_ks->dompath.path001.key00.number = 102;

  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t *>(in_ks), nullptr , &state,
                                   &book.base,
                                   reinterpret_cast<rw_keyspec_path_t *>(out_ks));

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);

    EXPECT_EQ(msg, &book.chapters[2].base);
    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    //EXPECT_EQ(ks, reinterpret_cast<rw_keyspec_path_t *>(out_ks));
    i++;
  }

  EXPECT_EQ(i, 1);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &book.base);
}

TEST(KsRRMerge, RerootIter5)
{

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony,colony);

  strcpy(colony.name, "colonytrafgen");
  colony.n_network_context_state = 2;

  colony.network_context_state = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState) **)malloc(sizeof(void*)*2);
  for (unsigned i = 0; i < 2; i++) {
    colony.network_context_state[i] = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState) *)malloc(sizeof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState)));

    RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState, colony.network_context_state[i]);

    sprintf(colony.network_context_state[i]->name, "internet%d", i+1);
    colony.network_context_state[i]->has_name = 1;
    colony.network_context_state[i]->has_platform_name = true;
    strcpy(colony.network_context_state[i]->platform_name, "Linux");

    colony.network_context_state[i]->ipv4 = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4) *)malloc(sizeof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4)));
    RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4, colony.network_context_state[i]->ipv4);

    colony.network_context_state[i]->ipv4->has_nexthop_record = true;

    colony.network_context_state[i]->ipv4->nexthop_record.n_nhrec = 5;

    for (unsigned j = 0; j < 5; j++) {

      colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nhrec = j+1;
      colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].has_nhrec = 1;
      colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].n_nexthops = 3;

      for (unsigned k = 0; k < 3; k++) {

        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_type = true;
        strcpy(colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].type, "PP");
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_ifindex = true;
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].ifindex = k+1;
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_gateway = true;
        strcpy(colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].gateway, "default");
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_lportname = true;
        strcpy(colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].lportname, "i2");

      }
    }
  }

  //create in_ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));
  in_ks->dompath.has_path000 = 1;
  in_ks->dompath.path000.has_key00 = 1;
  strcpy(in_ks->dompath.path000.key00.name, "colonytrafgen");

  //create out_ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord_Nhrec_Nexthops) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord_Nhrec_Nexthops));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  rw_keyspec_path_reroot_iter_state_t state;

  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , 
                                   &state,
                                   &colony.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0, j = 0, k = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {

    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(msg, &colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord_Nhrec_Nexthops, ks);

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.name, "colonytrafgen");

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.name, colony.network_context_state[i]->name);

    EXPECT_TRUE(out->dompath.path004.has_key00);
    EXPECT_EQ(out->dompath.path004.key00.nhrec, colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nhrec);


    k++; if (k == 3) { j++; k = 0;}
    if (j == 5) { i++; j = 0; }
    tot_iter++;
  }

  EXPECT_EQ(tot_iter, 30);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &colony.base);
}

TEST(KsRRMerge, RerootIter6)
{

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony,colony);

  strcpy(colony.name, "colonytrafgen");
  colony.n_network_context_state = 2;

  colony.network_context_state = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState) **)malloc(sizeof(void*)*2);
  for (unsigned i = 0; i < 2; i++) {
    colony.network_context_state[i] = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState) *)malloc(sizeof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState)));

    RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState, colony.network_context_state[i]);

    sprintf(colony.network_context_state[i]->name, "internet%d", i+1);
    colony.network_context_state[i]->has_name = 1;
    colony.network_context_state[i]->has_platform_name = true;
    strcpy(colony.network_context_state[i]->platform_name, "Linux");

    colony.network_context_state[i]->ipv4 = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4) *)malloc(sizeof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4)));

    RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4, colony.network_context_state[i]->ipv4);

    colony.network_context_state[i]->ipv4->has_nexthop_record = true;
    colony.network_context_state[i]->ipv4->nexthop_record.n_nhrec = 5;

    for (unsigned j = 0; j < 5; j++) {

      colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nhrec = j+1;
      colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].has_nhrec = 1;
      colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].n_nexthops = 3;

      for (unsigned k = 0; k < 3; k++) {

        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_type = true;
        strcpy(colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].type, "PP");
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_ifindex = true;
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].ifindex = k+1;
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_gateway = true;
        strcpy(colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].gateway, "default");
        colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].has_lportname = true;
        strcpy(colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].lportname, "i2");

      }
    }
  }

  //create in_ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));
  in_ks->dompath.path000.has_key00 = 1;
  strcpy(in_ks->dompath.path000.key00.name, "colonytrafgen");

  // Create a generic ks for in_ks
  rw_keyspec_path_t *in_ks1 = rw_keyspec_path_create_dup_of_type(reinterpret_cast<const rw_keyspec_path_t*>(in_ks), nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_NE(nullptr, in_ks1);

  //create out_ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord_Nhrec_Nexthops) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord_Nhrec_Nexthops));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  rw_keyspec_path_reroot_iter_state_t state;

  rw_keyspec_path_reroot_iter_init(in_ks1, nullptr , 
                                   &state,
                                   &colony.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0, j = 0, k = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {

    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_EQ(msg, &colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nexthops[k].base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord_Nhrec_Nexthops, ks); 

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.name, "colonytrafgen");

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.name, colony.network_context_state[i]->name);

    EXPECT_TRUE(out->dompath.path004.has_key00);
    EXPECT_EQ(out->dompath.path004.key00.nhrec, colony.network_context_state[i]->ipv4->nexthop_record.nhrec[j].nhrec);


    k++; if (k == 3) { j++; k = 0;}
    if (j == 5) { i++; j = 0; }
    tot_iter++;
  }

  EXPECT_EQ(tot_iter, 30);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &colony.base);
  rw_keyspec_path_free(in_ks1, nullptr);
}

TEST(KsRRMerge, RerootIter7)
{
  RWPB_M_MSG_DECL_INIT(Testy2pTop2_data_Topl, topl);

  topl.k0 = strdup("TopKey");
  topl.n_la = 100;

  topl.la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)malloc(sizeof(void*)*100);
  for (unsigned i = 0; i < 100; i++) {
    topl.la[i] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));

    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La, topl.la[i]);

    topl.la[i]->k1 = (char *)malloc(20);
    sprintf(topl.la[i]->k1, "%s%d", "key1", i+1);

    topl.la[i]->k2 = i+1;
    topl.la[i]->has_k2 = 1;

    topl.la[i]->k3 = (char *)malloc(20);
    sprintf(topl.la[i]->k3, "%s%d", "key3", i+1);

    topl.la[i]->n_lb = 200;
    topl.la[i]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)malloc(sizeof(void*)*200);

    for (unsigned j = 0; j < 200; j++) {
      topl.la[i]->lb[j] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));

      RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb, topl.la[i]->lb[j]);

      topl.la[i]->lb[j]->k4 = (char *)malloc(20);
      sprintf(topl.la[i]->lb[j]->k4, "LbKey1%d", j+1);
      topl.la[i]->lb[j]->k5 = j+1;
      topl.la[i]->lb[j]->has_k5 = 1;
    }
  }

  // create in keyspec.
  RWPB_T_PATHSPEC(Testy2pTop2_data_Topl) *in_ks = nullptr;

  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t **>(&in_ks));
  in_ks->dompath.path000.has_key00 = 1;
  in_ks->dompath.path000.key00.k0 = strdup("TopKey");

  rw_keyspec_path_t *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl_La_Lb));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &out_ks);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t *>(in_ks), nullptr , 
                                   &state,
                                   &topl.base,
                                   out_ks);

  unsigned i = 0, j = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &topl.la[i]->lb[j]->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));
    EXPECT_TRUE(ks);

    auto out = RWPB_M_PATHSPEC_CONST_CAST(Testy2pTop2_data_Topl_La_Lb, ks);

    EXPECT_TRUE(out->dompath.has_path000);
    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.k0, "TopKey");

    EXPECT_TRUE(out->dompath.has_path001);
    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.k1, topl.la[i]->k1);

    EXPECT_TRUE(out->dompath.path001.has_key01);
    EXPECT_EQ(out->dompath.path001.key01.k2, topl.la[i]->k2);

    EXPECT_TRUE(out->dompath.path001.has_key02);
    EXPECT_STREQ(out->dompath.path001.key02.k3, topl.la[i]->k3);

    EXPECT_TRUE(out->dompath.has_path002);
    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_STREQ(out->dompath.path002.key00.k4, topl.la[i]->lb[j]->k4);

    EXPECT_TRUE(out->dompath.path002.has_key01);
    EXPECT_EQ(out->dompath.path002.key01.k5, topl.la[i]->lb[j]->k5);

    j++; if (j == 200) { i++; j = 0;}
    tot_iter++;
  }

  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(tot_iter, 20000);

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(in_ks), NULL);
  rw_keyspec_path_free(out_ks, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &topl.base);
}

TEST(KsRRMerge, RerootIter8)
{
  RWPB_M_MSG_DECL_INIT(Testy2pTop2_data_Topl, topl);

  topl.k0 = strdup("TopKey");
  topl.n_la = 10;

  topl.la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)malloc(sizeof(void*)*10);
  for (unsigned i = 0; i < 10; i++) {
    topl.la[i] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));

    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La, topl.la[i]);

    topl.la[i]->k1 = (char *)malloc(20);
    if (i % 2 == 0) {
      strcpy(topl.la[i]->k1, "key1even");
    } else {
      strcpy(topl.la[i]->k1, "key1odd");
    }

    topl.la[i]->k2 = i+1;
    topl.la[i]->has_k2 = 1;

    topl.la[i]->k3 = (char *)malloc(20);
    sprintf(topl.la[i]->k3, "%s%d", "key3", i+1);

    topl.la[i]->n_lb = 20;
    topl.la[i]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)malloc(sizeof(void*)*20);

    for (unsigned j = 0; j < 20; j++) {
      topl.la[i]->lb[j] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));

      RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb, topl.la[i]->lb[j]);

      topl.la[i]->lb[j]->k4 = (char *)malloc(20);
      sprintf(topl.la[i]->lb[j]->k4, "LbKey1%d", j+1);
      topl.la[i]->lb[j]->k5 = j+1;
      topl.la[i]->lb[j]->has_k5 = 1;
    }
  }

  // create in keyspec.
  rw_keyspec_path_t *in_ks = nullptr;

  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &in_ks);

  auto *in = RWPB_F_PATHSPEC_CAST_OR_CRASH(Testy2pTop2_data_Topl, in_ks);
  in->dompath.path000.has_key00 = 1;
  in->dompath.path000.key00.k0 = strdup("TopKey");

  rw_keyspec_path_t *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl_La_Lb));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &out_ks);

  auto *out = RWPB_F_PATHSPEC_CAST_OR_CRASH(Testy2pTop2_data_Topl_La_Lb, out_ks);

  out->dompath.path001.has_key00 = true;
  out->dompath.path001.key00.k1 = strdup("key1odd");

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(in_ks, nullptr , 
                                   &state,
                                   &topl.base,
                                   out_ks);

  unsigned i = 1, j = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &topl.la[i]->lb[j]->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));
    EXPECT_TRUE(ks);

    auto out = RWPB_M_PATHSPEC_CONST_CAST(Testy2pTop2_data_Topl_La_Lb, ks);

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.k0, "TopKey");

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.k1, topl.la[i]->k1);

    EXPECT_TRUE(out->dompath.path001.has_key01);
    EXPECT_EQ(out->dompath.path001.key01.k2, topl.la[i]->k2);

    EXPECT_TRUE(out->dompath.path001.has_key02);
    EXPECT_STREQ(out->dompath.path001.key02.k3, topl.la[i]->k3);

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_STREQ(out->dompath.path002.key00.k4, topl.la[i]->lb[j]->k4);

    EXPECT_TRUE(out->dompath.path002.has_key01);
    EXPECT_EQ(out->dompath.path002.key01.k5, topl.la[i]->lb[j]->k5);

    j++; if (j == 20) { i += 2; j = 0;}
    tot_iter++;
  }

  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(tot_iter, 100);

  rw_keyspec_path_free(in_ks, nullptr);
  rw_keyspec_path_free(out_ks, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &topl.base);
}

TEST(KsRRMerge, RerootIter9)
{
  RWPB_M_MSG_DECL_INIT(Testy2pTop2_data_Topl, topl);

  topl.k0 = strdup("TopKey");
  topl.n_la = 10;

  topl.la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)malloc(sizeof(void*)*10);
  for (unsigned i = 0; i < 10; i++) {
    topl.la[i] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));

    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La, topl.la[i]);

    topl.la[i]->k1 = (char *)malloc(20);
    if (i % 2 == 0) {
      strcpy(topl.la[i]->k1, "key1even");
    } else {
      strcpy(topl.la[i]->k1, "key1odd");
    }

    topl.la[i]->k2 = i+1;
    topl.la[i]->has_k2 = 1;

    topl.la[i]->k3 = (char *)malloc(20);
    sprintf(topl.la[i]->k3, "%s%d", "key3", i+1);

    topl.la[i]->n_lb = 20;
    topl.la[i]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)malloc(sizeof(void*)*20);

    for (unsigned j = 0; j < 20; j++) {
      topl.la[i]->lb[j] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));

      RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb, topl.la[i]->lb[j]);

      topl.la[i]->lb[j]->k4 = (char *)malloc(20);
      sprintf(topl.la[i]->lb[j]->k4, "LbKey1%d", j+1);
      topl.la[i]->lb[j]->k5 = j+1;
      topl.la[i]->lb[j]->has_k5 = 1;
    }
  }

  // create in keyspec.
  rw_keyspec_path_t *in_ks = nullptr;

  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &in_ks);

  auto *in = RWPB_F_PATHSPEC_CAST_OR_CRASH(Testy2pTop2_data_Topl, in_ks);
  in->dompath.path000.has_key00 = 1;
  in->dompath.path000.key00.k0 = strdup("TopKey");

  rw_keyspec_path_t *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl_La_Lb));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &out_ks);

  auto *out = RWPB_F_PATHSPEC_CAST_OR_CRASH(Testy2pTop2_data_Topl_La_Lb, out_ks);

  out->dompath.path001.has_key00 = true;
  out->dompath.path001.key00.k1 = strdup("key1odd");

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(in_ks, nullptr , 
                                   &state,
                                   &topl.base,
                                   out_ks);

  unsigned i = 1, j = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &topl.la[i]->lb[j]->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));
    EXPECT_TRUE(ks);

    auto out = RWPB_M_PATHSPEC_CONST_CAST(Testy2pTop2_data_Topl_La_Lb, ks);

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.k0, "TopKey");

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.k1, topl.la[i]->k1);

    EXPECT_TRUE(out->dompath.path001.has_key01);
    EXPECT_EQ(out->dompath.path001.key01.k2, topl.la[i]->k2);

    EXPECT_TRUE(out->dompath.path001.has_key02);
    EXPECT_STREQ(out->dompath.path001.key02.k3, topl.la[i]->k3);

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_STREQ(out->dompath.path002.key00.k4, topl.la[i]->lb[j]->k4);

    EXPECT_TRUE(out->dompath.path002.has_key01);
    EXPECT_EQ(out->dompath.path002.key01.k5, topl.la[i]->lb[j]->k5);

    j++; if (j == 20) { i += 2; j = 0;}
    tot_iter++;
  }

  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(tot_iter, 100);

  rw_keyspec_path_free(in_ks, nullptr);
  rw_keyspec_path_free(out_ks, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &topl.base);
}

TEST(KsRRMerge, RerootIter10)
{
  RWPB_M_MSG_DECL_INIT(Testy2pTop2_data_Topl, topl);

  topl.k0 = strdup("TopKey");
  topl.n_la = 10;

  topl.la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)malloc(sizeof(void*)*10);
  for (unsigned i = 0; i < 10; i++) {
    topl.la[i] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));

    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La, topl.la[i]);

    topl.la[i]->k1 = (char *)malloc(20);
    if (i % 2 == 0) {
      strcpy(topl.la[i]->k1, "key1even");
    } else {
      strcpy(topl.la[i]->k1, "key1odd");
    }

    topl.la[i]->k2 = i+1;
    topl.la[i]->has_k2 = 1;

    topl.la[i]->k3 = (char *)malloc(20);
    sprintf(topl.la[i]->k3, "%s%d", "key3", i+1);

    topl.la[i]->n_lb = 20;
    topl.la[i]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)malloc(sizeof(void*)*20);

    for (unsigned j = 0; j < 20; j++) {
      topl.la[i]->lb[j] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));

      RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb, topl.la[i]->lb[j]);

      topl.la[i]->lb[j]->k4 = (char *)malloc(20);
      sprintf(topl.la[i]->lb[j]->k4, "LbKey1%d", j+1);
      topl.la[i]->lb[j]->k5 = (j+1)%2;
      topl.la[i]->lb[j]->has_k5 = 1;
    }
  }

  // create in keyspec.
  rw_keyspec_path_t *in_ks = nullptr;

  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &in_ks);

  auto *in = RWPB_F_PATHSPEC_CAST_OR_CRASH(Testy2pTop2_data_Topl, in_ks);
  in->dompath.path000.has_key00 = true;
  in->dompath.path000.key00.k0 = strdup("TopKey");

  rw_keyspec_path_t *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl_La_Lb));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  &out_ks);

  auto *out = RWPB_F_PATHSPEC_CAST_OR_CRASH(Testy2pTop2_data_Topl_La_Lb, out_ks);

  out->dompath.path001.has_key00 = true;
  out->dompath.path001.key00.k1 = strdup("key1odd");

  out->dompath.path002.has_key01 = true;
  out->dompath.path002.key01.k5 = 0;

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(in_ks, nullptr , 
                                   &state,
                                   &topl.base,
                                   out_ks);

  unsigned i = 1, j = 1, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &topl.la[i]->lb[j]->base);

    auto ks =  rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));
    EXPECT_TRUE(ks);

    auto out = RWPB_M_PATHSPEC_CONST_CAST(Testy2pTop2_data_Topl_La_Lb, ks);

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.k0, "TopKey");

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.k1, topl.la[i]->k1);

    EXPECT_TRUE(out->dompath.path001.has_key01);
    EXPECT_EQ(out->dompath.path001.key01.k2, topl.la[i]->k2);

    EXPECT_TRUE(out->dompath.path001.has_key02);
    EXPECT_STREQ(out->dompath.path001.key02.k3, topl.la[i]->k3);

    EXPECT_TRUE(out->dompath.path002.has_key00);
    EXPECT_STREQ(out->dompath.path002.key00.k4, topl.la[i]->lb[j]->k4);

    EXPECT_TRUE(out->dompath.path002.has_key01);
    EXPECT_EQ(out->dompath.path002.key01.k5, topl.la[i]->lb[j]->k5);

    j += 2; if (j > 19) { i += 2; j = 1;}
    tot_iter++;
  }

  rw_keyspec_path_reroot_iter_done(&state);
  EXPECT_EQ(tot_iter, 50);

  rw_keyspec_path_free(in_ks, nullptr);
  rw_keyspec_path_free(out_ks, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &topl.base);
}

TEST(KsRRMerge, RerootIter11)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company,comp);

  comp.n_employee = 1;
  comp.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)malloc(sizeof(void*)*comp.n_employee);
  comp.employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,comp.employee[0]);
  comp.employee[0]->id = 100;
  comp.employee[0]->has_id = 1;
  comp.employee[0]->name = (char *)malloc(50);
  strcpy(comp.employee[0]->name, "ABCD");
  comp.employee[0]->title = (char *)malloc(50);
  strcpy(comp.employee[0]->title, "Software Eng");
  comp.employee[0]->start_date = (char *)malloc(50);
  strcpy(comp.employee[0]->start_date, "12/12/2012");

  // Create a out keyspec.
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *keyspec_out = nullptr;
  const rw_keyspec_path_t *const_keyspec = comp.employee[0]->base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&keyspec_out));
  ASSERT_TRUE(keyspec_out);

  keyspec_out->dompath.path001.has_key00 = true;
  keyspec_out->dompath.path001.key00.id = 100;

  // Create a in keyspec.
  RWPB_T_PATHSPEC(Company_data_Company) *keyspec_in = nullptr;
  const_keyspec = comp.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&keyspec_in));
  ASSERT_TRUE(keyspec_in);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(keyspec_in), nullptr , 
                                   &state,
                                   &comp.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(keyspec_out));

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    auto out_ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(out_ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(out_ks));
    i++;
  }
  EXPECT_EQ(i, 1);
  rw_keyspec_path_reroot_iter_done(&state);

  // Check for keys mismatch
  keyspec_out->dompath.path001.key00.id = 300;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(keyspec_in), nullptr , 
                                   &state,
                                   &comp.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(keyspec_out));

  EXPECT_FALSE(rw_keyspec_path_reroot_iter_next(&state));
  rw_keyspec_path_reroot_iter_done(&state);

  // Cleanup's
  protobuf_c_message_free_unpacked_usebody(NULL, &comp.base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_out->base);
  protobuf_c_message_free_unpacked(NULL, &keyspec_in->base);
}

TEST(KsRRMerge, RerootIterDeep)
{
  // Deeper case with multiple list messages.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest,config);
  config.profile_name = strdup("name1");
  config.n_profile = 3;
  config.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*3);
  for (unsigned i = 0; i < 3; i++) {
    config.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile,config.profile[i]);
    config.profile[i]->name = (char *)malloc(20);
    sprintf(config.profile[i]->name, "%s%d", "name", i+1);
    config.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase,config.profile[i]->init_phase);
    config.profile[i]->init_phase->inventory = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory,config.profile[i]->init_phase->inventory);
    config.profile[i]->init_phase->inventory->n_component = 2;
    config.profile[i]->init_phase->inventory->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) **)malloc(sizeof(void*)*2);
    for (unsigned j = 0; j < 2; j++) {
      config.profile[i]->init_phase->inventory->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component,config.profile[i]->init_phase->inventory->component[j]);
      config.profile[i]->init_phase->inventory->component[j]->name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->name, "%s%d", "component", j+1);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet,config.profile[i]->init_phase->inventory->component[j]->rwtasklet);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory = (char *)malloc(20);
      strcpy(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory, "/plugingdir");
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name, "%s%d", "myplugin", i+j+1);
      config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version = (char *)malloc(20);
      sprintf(config.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version, "%s%d", "version", i+j+1);
    }
  }

  // create in_keyspec for the above message.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *in_ks = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&in_ks));

  // create outkeyspec which is deeper.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *out_ks = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&out_ks));

  // Now populate the keyvalues.
  // profile name
  out_ks->dompath.path002.has_key00 = true;
  out_ks->dompath.path002.key00.name = (char *)malloc(20);
  sprintf(out_ks->dompath.path002.key00.name, "%s%d", "name", 3);

  //component name.
  out_ks->dompath.path005.has_key00 = true;
  out_ks->dompath.path005.key00.name = (char *)malloc(20);
  sprintf(out_ks->dompath.path005.key00.name, "%s%d", "component", 2);

  rw_keyspec_path_reroot_iter_state_t state;

  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , 
                                   &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(out_msg);
    auto out_ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(out_ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(out_ks));
    i++;
  }
  EXPECT_EQ(i, 1);
  rw_keyspec_path_reroot_iter_done(&state);

  // change the key.
  sprintf(out_ks->dompath.path002.key00.name, "%s%d", "name", 1);
  sprintf(out_ks->dompath.path005.key00.name, "%s%d", "component", 2);

  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , 
                                   &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(out_msg);
    i++;
  }
  EXPECT_EQ(i, 1);
  rw_keyspec_path_reroot_iter_done(&state);

  // Give mismatching key.
  sprintf(out_ks->dompath.path005.key00.name, "%s%d", "component", 5);
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(in_ks), nullptr , 
                                   &state,
                                   &config.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(out_ks));

  EXPECT_FALSE(rw_keyspec_path_reroot_iter_next(&state));
  rw_keyspec_path_reroot_iter_done(&state);

  protobuf_c_message_free_unpacked(NULL, &in_ks->base);
  protobuf_c_message_free_unpacked(NULL, &out_ks->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &config.base);
}

TEST(KsRRMerge, RerootIterMKs)
{
  RWPB_M_MSG_DECL_INIT(FlatConversion_data_Container1_TwoKeys_UnrootedPb,in_msg);
  in_msg.has_unroot_int = true;
  in_msg.unroot_int = 100;

  // Create in keyspec.
  RWPB_T_PATHSPEC(FlatConversion_data_Container1_TwoKeys_UnrootedPb) *ks_in = nullptr;
  const rw_keyspec_path_t *const_keyspec = in_msg.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks_in));
  EXPECT_TRUE(ks_in);

  ks_in->dompath.path001.has_key00 = true;
  ks_in->dompath.path001.key00.prim_enum = BASE_CONVERSION_CB_ENUM_SEVENTH;
  ks_in->dompath.path001.has_key01 = true;
  strcpy(ks_in->dompath.path001.key01.sec_string, "Second_key");

  // Create out keyspec.
  RWPB_T_PATHSPEC(FlatConversion_data_Container1) *ks_out = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks_out));
  EXPECT_TRUE(ks_out);

  // check the shallower case.
  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , 
                                   &state,
                                   &in_msg.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(ks_out));

  EXPECT_TRUE(rw_keyspec_path_reroot_iter_next(&state));
  rw_keyspec_path_reroot_iter_done(&state);

  protobuf_c_message_free_unpacked(NULL, &ks_out->base);

  // Check for a completely unrelated keyspec.
  ks_out = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_ContactInfo));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks_out));


  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , 
                                   &state,
                                   &in_msg.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(ks_out));

  EXPECT_FALSE(rw_keyspec_path_reroot_iter_next(&state));
  rw_keyspec_path_reroot_iter_done(&state);

  // Cleanup
  protobuf_c_message_free_unpacked_usebody(NULL, &in_msg.base);
  protobuf_c_message_free_unpacked(NULL, &ks_out->base);
  protobuf_c_message_free_unpacked(NULL, &ks_in->base);
}

TEST(KsRRMerge, RerootIterDts)
{
  RWPB_M_MSG_DECL_INIT(RwdtsDataD_data_Dts, dts);

  dts.n_member = 10;
  dts.member = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member) **) malloc(sizeof(void*)*10);

  for (unsigned i = 0; i < 10; i++) {
    dts.member[i] = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member) *)malloc(sizeof(RWPB_T_MSG(RwdtsDataD_data_Dts_Member)));

    RWPB_F_MSG_INIT(RwdtsDataD_data_Dts_Member, dts.member[i]);
    sprintf(dts.member[i]->name, "Tasklet%d", i+1);
    dts.member[i]->has_name = 1;

    dts.member[i]->state = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State) *)malloc(sizeof(RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State)));

    RWPB_F_MSG_INIT(RwdtsDataD_data_Dts_Member_State, dts.member[i]->state);

    dts.member[i]->state->n_transaction = 5;
    dts.member[i]->state->transaction = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State_Transaction) **)malloc(sizeof(void*)*5);

    for (unsigned j = 0; j < 5; j++) {
      dts.member[i]->state->transaction[j] = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State_Transaction) *)malloc(sizeof(RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State_Transaction)));

      RWPB_F_MSG_INIT(RwdtsDataD_data_Dts_Member_State_Transaction, dts.member[i]->state->transaction[j]);

      sprintf(dts.member[i]->state->transaction[j]->id, "Trans%d", j+1);
      dts.member[i]->state->transaction[j]->has_id = 1;
    }

    dts.member[i]->state->n_registration = 3;
    dts.member[i]->state->registration = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State_Registration) **)malloc(sizeof(void*)*3);

    for (unsigned j = 0; j < 3; j++) {

      dts.member[i]->state->registration[j] = (RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State_Registration) *)malloc(sizeof(RWPB_T_MSG(RwdtsDataD_data_Dts_Member_State_Registration)));

      RWPB_F_MSG_INIT(RwdtsDataD_data_Dts_Member_State_Registration, dts.member[i]->state->registration[j]);

      sprintf(dts.member[i]->state->registration[j]->id, "Reg%d", j+1);
      dts.member[i]->state->registration[j]->has_id = 1;
    }
  }

  dts.n_brokers = 10;
  dts.brokers = (RWPB_T_MSG(RwdtsDataD_data_Dts_Brokers) **)malloc(sizeof(void*)*10);

  for (unsigned i = 0; i < 10; i++) {
    dts.brokers[i] = (RWPB_T_MSG(RwdtsDataD_data_Dts_Brokers) *)malloc(sizeof(RWPB_T_MSG(RwdtsDataD_data_Dts_Brokers)));

    RWPB_F_MSG_INIT(RwdtsDataD_data_Dts_Brokers, dts.brokers[i]);

    dts.brokers[i]->instance_uri = (char *)malloc(30);
    sprintf(dts.brokers[i]->instance_uri, "InstURI%d", i+1);

    dts.brokers[i]->n_stats = 4;
    dts.brokers[i]->stats = (RWPB_T_MSG(RwdtsDataD_data_Dts_Brokers_Stats) **)malloc(sizeof(void*)*4);

    for (unsigned j = 0; j < 4; j++) {
      dts.brokers[i]->stats[j] = (RWPB_T_MSG(RwdtsDataD_data_Dts_Brokers_Stats) *)malloc(sizeof(RWPB_T_MSG(RwdtsDataD_data_Dts_Brokers_Stats)));

      RWPB_F_MSG_INIT(RwdtsDataD_data_Dts_Brokers_Stats, dts.brokers[i]->stats[j]);

      dts.brokers[i]->stats[j]->has_topx_begin = true;
      dts.brokers[i]->stats[j]->topx_begin = 200;
    }
  }

  // create inkeyspec

  rw_keyspec_path_t *in_ks = nullptr;
  const rw_keyspec_path_t *const_ks = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(RwdtsDataD_data_Dts));

  rw_keyspec_path_create_dup(const_ks, nullptr,  &in_ks);
  EXPECT_TRUE(in_ks);

  //create outks
  rw_keyspec_path_t *out_ks = nullptr;
  const_ks = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(RwdtsDataD_data_Dts_Member_State_Registration));
  rw_keyspec_path_create_dup(const_ks, nullptr,  &out_ks);
  EXPECT_TRUE(out_ks);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(in_ks, nullptr , 
                                   &state, &dts.base, out_ks);

  unsigned i = 0, j = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {

    ProtobufCMessage *msg = rw_keyspec_path_reroot_iter_take_msg(&state);
    auto ret = protobuf_c_message_is_equal_deep(pinstance, msg, &dts.member[i]->state->registration[j]->base);
    EXPECT_TRUE(ret);

    protobuf_c_message_free_unpacked(NULL, msg);

    rw_keyspec_path_t *out = rw_keyspec_path_reroot_iter_take_ks(&state);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(out));

    auto *o_ks = RWPB_F_PATHSPEC_UNSAFE_MORPH(RwdtsDataD_data_Dts_Member_State_Registration, out);

    EXPECT_TRUE(o_ks->dompath.path001.has_key00);
    EXPECT_STREQ(o_ks->dompath.path001.key00.name, dts.member[i]->name);

    EXPECT_TRUE(o_ks->dompath.path003.has_key00);
    EXPECT_STREQ(o_ks->dompath.path003.key00.id, dts.member[i]->state->registration[j]->id);

    rw_keyspec_path_free(out, nullptr);

    j++; if (j == 3) { i++; j = 0; }
    tot_iter++;
  }

  EXPECT_EQ(tot_iter, 30);

  // specify some filers.
  rw_keyspec_path_free(out_ks, nullptr);
  out_ks = nullptr;
  const_ks = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(RwdtsDataD_data_Dts_Member_State_Transaction));
  rw_keyspec_path_create_dup(const_ks, nullptr,  &out_ks);
  EXPECT_TRUE(out_ks);

  auto *o_ks = RWPB_F_PATHSPEC_CAST_OR_CRASH(RwdtsDataD_data_Dts_Member_State_Transaction, out_ks);

  o_ks->dompath.path003.has_key00 = true;
  strcpy(o_ks->dompath.path003.key00.id, "Trans1");

  rw_keyspec_path_reroot_iter_init(in_ks, nullptr , 
                                   &state,
                                   &dts.base, out_ks);

  i = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {

    ProtobufCMessage *msg = rw_keyspec_path_reroot_iter_take_msg(&state);
    auto ret = protobuf_c_message_is_equal_deep(pinstance, msg, &dts.member[i]->state->transaction[0]->base);
    EXPECT_TRUE(ret);

    protobuf_c_message_free_unpacked(NULL, msg);

    rw_keyspec_path_t *out = rw_keyspec_path_reroot_iter_take_ks(&state);

    auto *o_ks = RWPB_F_PATHSPEC_UNSAFE_MORPH(RwdtsDataD_data_Dts_Member_State_Registration, out);

    EXPECT_TRUE(o_ks->dompath.path001.has_key00);
    EXPECT_STREQ(o_ks->dompath.path001.key00.name, dts.member[i]->name);

    EXPECT_TRUE(o_ks->dompath.path003.has_key00);
    EXPECT_STREQ(o_ks->dompath.path003.key00.id, dts.member[i]->state->transaction[0]->id);

    rw_keyspec_path_free(out, nullptr);

    i++; tot_iter++;
  }

  EXPECT_EQ(tot_iter, 10);
  rw_keyspec_path_reroot_iter_done(&state);

  rw_keyspec_path_free(out_ks, nullptr);
  out_ks = nullptr;
  const_ks = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(RwdtsDataD_data_Dts_Brokers_Stats));
  rw_keyspec_path_create_dup(const_ks, nullptr,  &out_ks);
  EXPECT_TRUE(out_ks);

  rw_keyspec_path_reroot_iter_init(in_ks, nullptr , 
                                   &state,
                                   &dts.base,
                                   out_ks);

  i = 0, j = 0, tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);

    EXPECT_EQ(msg, &dts.brokers[i]->stats[j]->base);

    j++; if (j == 4) { i++; j = 0; } tot_iter++;
  }

  EXPECT_EQ(tot_iter, 40);
  rw_keyspec_path_reroot_iter_done(&state);

  protobuf_c_message_free_unpacked_usebody(NULL, &dts.base);
  rw_keyspec_path_free(in_ks, nullptr);
  rw_keyspec_path_free(out_ks, nullptr);
}

TEST(KsRRMerge, RerootIter2keys)
{
  RWPB_M_MSG_DECL_INIT(FlatConversion_data_Container1_TwoKeys_UnrootedPb, in_msg);
  in_msg.has_unroot_int = true;
  in_msg.unroot_int = 100;

  // Create in keyspec.
  RWPB_T_PATHSPEC(FlatConversion_data_Container1_TwoKeys_UnrootedPb) *ks_in = nullptr;
  const rw_keyspec_path_t *const_keyspec = in_msg.base.descriptor->ypbc_mdesc->schema_path_value;
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks_in));
  EXPECT_TRUE(ks_in);

  ks_in->dompath.path001.has_key00 = true;
  ks_in->dompath.path001.key00.prim_enum = BASE_CONVERSION_CB_ENUM_SEVENTH;
  ks_in->dompath.path001.has_key01 = true;
  strcpy(ks_in->dompath.path001.key01.sec_string, "Second_key");

  // Create out keyspec.
  RWPB_T_PATHSPEC(FlatConversion_data_Container1) *ks_out = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks_out));
  EXPECT_TRUE(ks_out);

  // check the shallower case for reroot_iter
  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , 
                                   &state,
                                   &in_msg.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(ks_out));

  unsigned tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(out_msg);

    EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(FlatConversion_data_Container1));
    auto top_msg = (const RWPB_T_MSG(FlatConversion_data_Container1) *)out_msg;
    EXPECT_EQ(top_msg->n_two_keys, 1);
    EXPECT_EQ(top_msg->two_keys[0].prim_enum, 7);
    EXPECT_STREQ(top_msg->two_keys[0].sec_string, "Second_key");
    EXPECT_TRUE(top_msg->two_keys[0].unrooted_pb.has_unroot_int);
    EXPECT_EQ(top_msg->two_keys[0].unrooted_pb.unroot_int, 100);

    auto *out_ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(out_ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(out_ks));

    tot_iter++;
  }

  EXPECT_EQ(tot_iter, 1);
  rw_keyspec_path_reroot_iter_done(&state);

  // Let us try using a generic keyspec for in_ks.
  rw_keyspec_path_update_binpath(reinterpret_cast<rw_keyspec_path_t *>(ks_in), NULL);
  ProtobufCBinaryData data;
  data.data = NULL;
  data.len = 0;

  rw_keyspec_path_get_binpath(reinterpret_cast<rw_keyspec_path_t *>(ks_in), NULL, const_cast<const uint8_t **>(&data.data), &data.len);

  rw_keyspec_path_t *g_ks = nullptr;
  g_ks = rw_keyspec_path_create_with_dompath_binary_data(NULL, &data);
  EXPECT_TRUE(g_ks);

  rw_keyspec_path_reroot_iter_init(g_ks, nullptr , 
                                   &state,
                                   &in_msg.base,
                                   reinterpret_cast<rw_keyspec_path_t*>(ks_out));

  tot_iter = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto out_msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(out_msg);

    EXPECT_EQ(out_msg->descriptor, RWPB_G_MSG_PBCMD(FlatConversion_data_Container1));
    auto top_msg = (const RWPB_T_MSG(FlatConversion_data_Container1) *)out_msg;
    EXPECT_EQ(top_msg->n_two_keys, 1);
    EXPECT_EQ(top_msg->two_keys[0].prim_enum, 7);
    EXPECT_STREQ(top_msg->two_keys[0].sec_string, "Second_key");
    EXPECT_TRUE(top_msg->two_keys[0].unrooted_pb.has_unroot_int);
    EXPECT_EQ(top_msg->two_keys[0].unrooted_pb.unroot_int, 100);

    auto out_ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(out_ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(out_ks));

    tot_iter++;
  }

  EXPECT_EQ(tot_iter, 1);
  rw_keyspec_path_reroot_iter_done(&state);

  rw_keyspec_path_free(g_ks, nullptr);

  // Cleanup
  protobuf_c_message_free_unpacked_usebody(NULL, &in_msg.base);
  protobuf_c_message_free_unpacked(NULL, &ks_out->base);
  protobuf_c_message_free_unpacked(NULL, &ks_in->base);
}

TEST(KsRRMerge, RerootJira13456)
{
  RWPB_M_MSG_DECL_INIT(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig, msg);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptrm(&msg.base);

  msg.n_vnf = 3;
  msg.vnf = (RWPB_T_MSG(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf) **)malloc(sizeof(void*) * 3);

  for (unsigned i = 0; i < 3; ++i) {

    msg.vnf[i] = RWPB_F_MSG_ALLOC(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf);
    msg.vnf[i]->name = strdup("pingpong");
    msg.vnf[i]->instance = i+1;
    msg.vnf[i]->has_instance = 1;

    msg.vnf[i]->n_network_context = 5;
    msg.vnf[i]->network_context = (RWPB_T_MSG(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext) **)
        malloc(5 * sizeof(void*));

    for (unsigned n = 0; n < 5; ++n) {

      msg.vnf[i]->network_context[n] = RWPB_F_MSG_ALLOC(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext);
      sprintf(msg.vnf[i]->network_context[n]->name, "nc%d", n+1);
      msg.vnf[i]->network_context[n]->has_name = 1;

      if (n % 2) {

        msg.vnf[i]->network_context[n]->n_vrf = 2;
        msg.vnf[i]->network_context[n]->vrf = (RWPB_T_MSG(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext_Vrf) **)
            malloc(2 * sizeof(void*));

        for (unsigned v = 0; v < 2; ++v) {
          msg.vnf[i]->network_context[n]->vrf[v] = RWPB_F_MSG_ALLOC(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext_Vrf);
          sprintf(msg.vnf[i]->network_context[n]->vrf[v]->name, "vrf%d", v+1);
          msg.vnf[i]->network_context[n]->vrf[v]->has_name = 1;
        }

      } else {
        msg.vnf[i]->network_context[n]->packet_capture = 
            RWPB_F_MSG_ALLOC(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext_PacketCapture);
      }
    }
  }

  RWPB_M_PATHSPEC_DECL_INIT(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig, in_ks);
  RWPB_M_PATHSPEC_DECL_INIT(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext_Vrf, out_ks);

  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptrk1(&out_ks.rw_keyspec_path_t);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks.rw_keyspec_path_t);

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    i++;
  }
  EXPECT_EQ (i, 12);

  out_ks.dompath.path001.has_key00 = true;
  out_ks.dompath.path001.key00.name = strdup("pingpong");
  out_ks.dompath.path001.has_key01 = true;
  out_ks.dompath.path001.key01.instance = 2;

  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks.rw_keyspec_path_t);

  i = 0;
  unsigned vi = 1, ni = 1, vri = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {

    auto res = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(res);

    EXPECT_EQ(res, (ProtobufCMessage *)(msg.vnf[vi]->network_context[ni]->vrf[vri]));
    vri++;
    if (vri == 2) { ni += 2; vri = 0; }

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(ks);

    i++;
  }
  EXPECT_EQ(i, 4);

  out_ks.dompath.path002.has_key00 = true;
  strcpy(out_ks.dompath.path002.key00.name, "nc1");

  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks.rw_keyspec_path_t);

  EXPECT_FALSE (rw_keyspec_path_reroot_iter_next(&state));

  strcpy(out_ks.dompath.path002.key00.name, "nc2");
  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks.rw_keyspec_path_t);

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) { i++; }
  EXPECT_EQ(i, 2);

  RWPB_M_PATHSPEC_DECL_INIT(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext_Vrf, out_ks2);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptrk2(&out_ks2.rw_keyspec_path_t);

  out_ks2.dompath.path003.has_key00 = true;
  strcpy(out_ks2.dompath.path003.key00.name, "vrf2");

  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks2.rw_keyspec_path_t);

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) { i++; }
  EXPECT_EQ(i, 6);

  strcpy(out_ks2.dompath.path003.key00.name, "vrf3");
  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks2.rw_keyspec_path_t);

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) { i++; }
  EXPECT_EQ(i, 0);

  RWPB_M_PATHSPEC_DECL_INIT(TestARwVnfBaseConfig_TestAManoBase_data_VnfConfig_Vnf_NetworkContext_PacketCapture, out_ks1);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptrk3(&out_ks1.rw_keyspec_path_t);

  out_ks1.dompath.path001.has_key00 = true;
  out_ks1.dompath.path001.key00.name = strdup("pingpong");
  out_ks1.dompath.path001.has_key01 = true;
  out_ks1.dompath.path001.key01.instance = 3;

  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks1.rw_keyspec_path_t);

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) { i++; }
  EXPECT_EQ(i, 3);

  out_ks1.dompath.path002.has_key00 = true;
  strcpy(out_ks1.dompath.path002.key00.name, "nc1");

  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks1.rw_keyspec_path_t);

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) { i++; }
  EXPECT_EQ(i, 1);

  out_ks1.dompath.path002.has_key00 = true;
  strcpy(out_ks1.dompath.path002.key00.name, "nc2");

  rw_keyspec_path_reroot_iter_init(
      &in_ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &out_ks1.rw_keyspec_path_t);

  i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) { i++; }
  EXPECT_EQ(i, 0);
}

TEST(KsRRMerge, RerootMergeOpaque)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee, msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Product) *ks2 = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Product)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 500;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Product, msg2);

  msg2.id = 500;
  msg2.has_id = 1;
  msg2.name = strdup("Prod1");
  msg2.msrp = strdup("100d");


  ProtobufCBinaryData p_msg1;
  p_msg1.data = (uint8_t *)malloc(protobuf_c_message_get_packed_size(NULL, &msg1.base));
  p_msg1.len = protobuf_c_message_pack(NULL, &msg1.base, p_msg1.data);

  ProtobufCBinaryData p_msg2;
  p_msg2.data = (uint8_t *)malloc(protobuf_c_message_get_packed_size(NULL, &msg2.base));
  p_msg2.len = protobuf_c_message_pack(NULL, &msg2.base, p_msg2.data);

  rw_keyspec_path_t *ks_out = nullptr;
  ProtobufCBinaryData out_msg;
  out_msg.len = 0;
  out_msg.data = NULL;

  rw_status_t status = rw_keyspec_path_reroot_and_merge_opaque(nullptr,reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                               reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                               &p_msg1, &p_msg2, &ks_out, &out_msg);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(out_msg.data);
  EXPECT_TRUE(out_msg.len);
  EXPECT_TRUE(ks_out);

  ProtobufCMessage *out_pbm = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(Company_data_Company),
                                                        out_msg.len, out_msg.data);
  EXPECT_TRUE(out_pbm);
  auto *ko = RWPB_F_PATHSPEC_CAST_OR_CRASH(Company_data_Company, ks_out);

  EXPECT_EQ(ko->base.descriptor, RWPB_G_PATHSPEC_PBCMD(Company_data_Company));
  EXPECT_TRUE(ko->has_dompath);
  EXPECT_EQ(ko->dompath.path000.element_id.element_tag, TCORP_NS(1));

  auto *om = (RWPB_T_MSG(Company_data_Company) *)out_pbm;
  EXPECT_FALSE(om->contact_info);
  EXPECT_EQ(om->n_employee, 1);
  EXPECT_TRUE(om->employee);
  EXPECT_TRUE(om->employee[0]);

  EXPECT_EQ(om->employee[0]->id, 100);
  EXPECT_STREQ(om->employee[0]->name, "John");
  EXPECT_STREQ(om->employee[0]->title, "SE");
  EXPECT_STREQ(om->employee[0]->start_date, "14/12");

  EXPECT_EQ(om->n_product, 1);
  EXPECT_TRUE(om->product);
  EXPECT_TRUE(om->product[0]);

  EXPECT_EQ(om->product[0]->id, 500);
  EXPECT_STREQ(om->product[0]->name, "Prod1");
  EXPECT_STREQ(om->product[0]->msrp, "100d");

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(ks_out, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  free(p_msg1.data);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  free(p_msg2.data);
  protobuf_c_message_free_unpacked(NULL, out_pbm);
  free(out_msg.data);
}

TEST(KsRRMerge, RerootMergeOpaque1)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee, msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Product) *ks2 = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 500;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee, msg2);

  msg2.id = 500;
  msg2.has_id = 1;
  msg2.name = strdup("Alice");
  msg2.title = strdup("SE");
  msg2.start_date = strdup("31/10");

  ProtobufCBinaryData p_msg1;
  p_msg1.data = (uint8_t *)malloc(protobuf_c_message_get_packed_size(NULL, &msg1.base));
  p_msg1.len = protobuf_c_message_pack(NULL, &msg1.base, p_msg1.data);

  ProtobufCBinaryData p_msg2;
  p_msg2.data = (uint8_t *)malloc(protobuf_c_message_get_packed_size(NULL, &msg2.base));
  p_msg2.len = protobuf_c_message_pack(NULL, &msg2.base, p_msg2.data);

  rw_keyspec_path_t *ks_out = nullptr;
  ProtobufCBinaryData out_msg;
  out_msg.len = 0;
  out_msg.data = NULL;

  rw_status_t status = rw_keyspec_path_reroot_and_merge_opaque(nullptr,reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                               reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                               &p_msg1, &p_msg2, &ks_out, &out_msg);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(out_msg.data);
  EXPECT_TRUE(out_msg.len);
  EXPECT_TRUE(ks_out);

  ProtobufCMessage *out_pbm = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(Company_data_Company),
                                                        out_msg.len, out_msg.data);
  EXPECT_TRUE(out_pbm);
  auto *ko = RWPB_F_PATHSPEC_CAST_OR_CRASH(Company_data_Company, ks_out);

  EXPECT_EQ(ko->base.descriptor, RWPB_G_PATHSPEC_PBCMD(Company_data_Company));
  EXPECT_TRUE(ko->has_dompath);
  EXPECT_EQ(ko->dompath.path000.element_id.element_tag, TCORP_NS(1));

  auto *om = (RWPB_T_MSG(Company_data_Company) *)out_pbm;
  EXPECT_FALSE(om->contact_info);
  EXPECT_EQ(om->n_employee, 2);
  EXPECT_TRUE(om->employee);
  EXPECT_TRUE(om->employee[0]);

  EXPECT_EQ(om->employee[0]->id, 100);
  EXPECT_STREQ(om->employee[0]->name, "John");
  EXPECT_STREQ(om->employee[0]->title, "SE");
  EXPECT_STREQ(om->employee[0]->start_date, "14/12");

  EXPECT_EQ(om->employee[1]->id, 500);
  EXPECT_STREQ(om->employee[1]->name, "Alice");
  EXPECT_STREQ(om->employee[1]->title, "SE");
  EXPECT_STREQ(om->employee[1]->start_date, "31/10");

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(ks_out, nullptr);

  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  free(p_msg1.data);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  free(p_msg2.data);
  protobuf_c_message_free_unpacked(NULL, out_pbm);
  free(out_msg.data);
}

TEST(KsRRMerge, RerootMergeOpaque2)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee, msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create generic keyspec1
  rw_keyspec_path_t *g_ks1 = rw_keyspec_path_create_dup_of_type(reinterpret_cast<const rw_keyspec_path_t*>(ks1), nullptr ,
                                                                &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks1);

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Product) *ks2 = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Product)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 500;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Product, msg2);

  msg2.id = 500;
  msg2.has_id = 1;
  msg2.name = strdup("Prod1");
  msg2.msrp = strdup("100d");

  // Create generic keyspec2.
  rw_keyspec_path_t *g_ks2 = rw_keyspec_path_create_dup_of_type(reinterpret_cast<const rw_keyspec_path_t*>(ks2), nullptr ,
                                                                &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks2);


  ProtobufCBinaryData p_msg1;
  p_msg1.data = (uint8_t *)malloc(protobuf_c_message_get_packed_size(NULL, &msg1.base));
  p_msg1.len = protobuf_c_message_pack(NULL, &msg1.base, p_msg1.data);

  ProtobufCBinaryData p_msg2;
  p_msg2.data = (uint8_t *)malloc(protobuf_c_message_get_packed_size(NULL, &msg2.base));
  p_msg2.len = protobuf_c_message_pack(NULL, &msg2.base, p_msg2.data);

  rw_keyspec_path_t *ks_out = nullptr;
  ProtobufCBinaryData out_msg;
  out_msg.len = 0;
  out_msg.data = NULL;

  rw_status_t status = rw_keyspec_path_reroot_and_merge_opaque(nullptr,g_ks1, g_ks2, &p_msg1, &p_msg2, &ks_out, &out_msg);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(out_msg.data);
  EXPECT_TRUE(out_msg.len);
  EXPECT_TRUE(ks_out);

  ProtobufCMessage *out_pbm = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(Company_data_Company),
                                                        out_msg.len, out_msg.data);
  EXPECT_TRUE(out_pbm);
  auto *ko = reinterpret_cast<RwSchemaPathSpec *>(ks_out);
  EXPECT_TRUE(ko->dompath);
  EXPECT_TRUE(ko->dompath->path000);
  EXPECT_EQ(ko->dompath->path000->element_id->element_tag, TCORP_NS(1));

  auto *om = (RWPB_T_MSG(Company_data_Company) *)out_pbm;
  EXPECT_FALSE(om->contact_info);
  EXPECT_EQ(om->n_employee, 1);
  EXPECT_TRUE(om->employee);
  EXPECT_TRUE(om->employee[0]);

  EXPECT_EQ(om->employee[0]->id, 100);
  EXPECT_STREQ(om->employee[0]->name, "John");
  EXPECT_STREQ(om->employee[0]->title, "SE");
  EXPECT_STREQ(om->employee[0]->start_date, "14/12");

  EXPECT_EQ(om->n_product, 1);
  EXPECT_TRUE(om->product);
  EXPECT_TRUE(om->product[0]);

  EXPECT_EQ(om->product[0]->id, 500);
  EXPECT_STREQ(om->product[0]->name, "Prod1");
  EXPECT_STREQ(om->product[0]->msrp, "100d");

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(g_ks1, nullptr);
  rw_keyspec_path_free(g_ks2, nullptr);
  rw_keyspec_path_free(ks_out, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  free(p_msg1.data);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  free(p_msg2.data);
  protobuf_c_message_free_unpacked(NULL, out_pbm);
  free(out_msg.data);
}

TEST(KsRRMerge, RerootMergeOpaque3)
{
  // msg1.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest, msg1);
  msg1.profile_name = strdup("name1");
  msg1.n_profile = 3;
  msg1.profile = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) **)malloc(sizeof(void*)*3);
  for (unsigned i = 0; i < 3; i++) {
    msg1.profile[i] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile, msg1.profile[i]);
    msg1.profile[i]->name = (char *)malloc(20);
    sprintf(msg1.profile[i]->name, "%s%d", "name", i+1);
    msg1.profile[i]->init_phase = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase, msg1.profile[i]->init_phase);
    msg1.profile[i]->init_phase->inventory = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory)));
    RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory, msg1.profile[i]->init_phase->inventory);
    msg1.profile[i]->init_phase->inventory->n_component = 2;
    msg1.profile[i]->init_phase->inventory->component = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) **)malloc(sizeof(void*)*2);
    for (unsigned j = 0; j < 2; j++) {
      msg1.profile[i]->init_phase->inventory->component[j] = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component, msg1.profile[i]->init_phase->inventory->component[j]);
      msg1.profile[i]->init_phase->inventory->component[j]->name = (char *)malloc(20);
      sprintf(msg1.profile[i]->init_phase->inventory->component[j]->name, "%s%d", "component", j+1);
      msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet)));
      RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_InitPhase_Inventory_Component_Rwtasklet, msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet);
      msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory = (char *)malloc(20);
      strcpy(msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory, "/plugingdir");
      msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name = (char *)malloc(20);
      sprintf(msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name, "%s%d", "myplugin", i+j+1);
      msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version = (char *)malloc(20);
      sprintf(msg1.profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version, "%s%d", "version", i+j+1);
    }
  }

  ProtobufCBinaryData b_msg1;
  b_msg1.len = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  b_msg1.data = (uint8_t *)malloc(b_msg1.len);
  protobuf_c_message_pack(NULL, &msg1.base, b_msg1.data);

  // create keyspec1 for msg1.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest) *ks1 = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  // msg2.
  RWPB_M_MSG_DECL_INIT(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase, msg2);
  msg2.rwtasklet = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtasklet) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtasklet)));
  RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtasklet, msg2.rwtasklet);
  msg2.rwtasklet->plugin_directory = strdup("dir1");
  msg2.rwtasklet->plugin_name = strdup("name1");
  msg2.rwtasklet->plugin_version = strdup("1.1");

  msg2.rwtrace = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtrace) *) malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtrace)));
  RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtrace, msg2.rwtrace);
  msg2.rwtrace->has_enable = true;
  msg2.rwtrace->enable = true;
  msg2.rwtrace->file = strdup("tracefile");
  msg2.rwtrace->has_level = true;
  msg2.rwtrace->level = 10;
  msg2.rwtrace->filter = (RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtrace_Filter) *)malloc(sizeof(RWPB_T_MSG(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtrace_Filter)));
  RWPB_F_MSG_INIT(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase_Rwtrace_Filter, msg2.rwtrace->filter);
  msg2.rwtrace->filter->file = strdup("filterfile");
  msg2.rwtrace->filter->has_line = true;
  msg2.rwtrace->filter->line = 100;

  ProtobufCBinaryData b_msg2;
  b_msg2.len = protobuf_c_message_get_packed_size(NULL, &msg2.base);
  b_msg2.data = (uint8_t *)malloc(b_msg2.len);
  protobuf_c_message_pack(NULL, &msg2.base, b_msg2.data);

  // create keyspec2 for msg2.
  RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase) *ks2 = nullptr;
  const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_BootstrapPhase));
  rw_keyspec_path_create_dup(const_keyspec, nullptr,  reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.path002.has_key00 = true;
  strcpy(ks2->dompath.path002.key00.name, "name1");

  rw_keyspec_path_t *out_ks = nullptr;
  ProtobufCBinaryData out_msg;
  out_msg.len = 0;
  out_msg.data = NULL;
  rw_status_t s = rw_keyspec_path_reroot_and_merge_opaque(nullptr,reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                          reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                          &b_msg1,
                                                          &b_msg2,
                                                          &out_ks,
                                                          &out_msg);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(out_ks);
  EXPECT_TRUE(out_msg.data);
  EXPECT_TRUE(out_msg.len);

  ProtobufCMessage *out_pbm = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(TestRwapis_data_Config_Manifest),
                                                        out_msg.len, out_msg.data);
  EXPECT_TRUE(out_pbm);
  RWPB_T_MSG(TestRwapis_data_Config_Manifest) *om = (RWPB_T_MSG(TestRwapis_data_Config_Manifest) *)out_pbm;

  EXPECT_STREQ(om->profile_name, "name1");
  EXPECT_EQ(om->n_profile, 3);
  EXPECT_TRUE(om->profile);

  for (unsigned i = 0; i < 3; i++) {
    EXPECT_TRUE(om->profile[i]);
    char temp[50];
    sprintf(temp, "%s%d", "name", i+1);
    EXPECT_STREQ(om->profile[i]->name, temp);
    EXPECT_TRUE(om->profile[i]->init_phase);
    EXPECT_TRUE(om->profile[i]->init_phase->inventory);
    EXPECT_EQ(om->profile[i]->init_phase->inventory->n_component, 2);
    EXPECT_TRUE(om->profile[i]->init_phase->inventory->component);

    for (unsigned j = 0; j < 2; j++) {
      EXPECT_TRUE(om->profile[i]->init_phase->inventory->component[j]);
      char temp[50];
      sprintf(temp, "%s%d", "component", j+1);
      EXPECT_STREQ(om->profile[i]->init_phase->inventory->component[j]->name, temp);
      EXPECT_TRUE(om->profile[i]->init_phase->inventory->component[j]->rwtasklet);
      EXPECT_STREQ(om->profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_directory, "/plugingdir");
      sprintf(temp, "%s%d", "myplugin", i+j+1);
      EXPECT_STREQ(om->profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_name, temp);
      sprintf(temp, "%s%d", "version", i+j+1);
      EXPECT_STREQ(om->profile[i]->init_phase->inventory->component[j]->rwtasklet->plugin_version, temp);
    }

    if (!strcmp(om->profile[i]->name, "name1")) {
      EXPECT_TRUE(om->profile[i]->bootstrap_phase);
      EXPECT_TRUE(om->profile[i]->bootstrap_phase->rwtasklet);
      EXPECT_STREQ(om->profile[i]->bootstrap_phase->rwtasklet->plugin_directory, "dir1");
      EXPECT_STREQ(om->profile[i]->bootstrap_phase->rwtasklet->plugin_name, "name1");
      EXPECT_STREQ(om->profile[i]->bootstrap_phase->rwtasklet->plugin_version, "1.1");

      EXPECT_TRUE(om->profile[i]->bootstrap_phase->rwtrace);
      EXPECT_TRUE(om->profile[i]->bootstrap_phase->rwtrace->has_enable);
      EXPECT_TRUE(om->profile[i]->bootstrap_phase->rwtrace->enable);
      EXPECT_STREQ(om->profile[i]->bootstrap_phase->rwtrace->file, "tracefile");
      EXPECT_TRUE(om->profile[i]->bootstrap_phase->rwtrace->has_level);
      EXPECT_EQ(om->profile[i]->bootstrap_phase->rwtrace->level, 10);
      EXPECT_TRUE(om->profile[i]->bootstrap_phase->rwtrace->filter);
      EXPECT_STREQ(om->profile[i]->bootstrap_phase->rwtrace->filter->file, "filterfile");
    }
  }

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  rw_keyspec_path_free(out_ks, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  free(b_msg1.data);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  free(b_msg2.data);
  protobuf_c_message_free_unpacked(NULL, out_pbm);
  free(out_msg.data);
}

TEST(KsRRMerge, RerootMergeOpaque4)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther_Members, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther_Members, msg2);

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther_Members, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther_Members, ks2);

  strcpy(msg1.name, "eth0");
  msg1.has_name = 1;
  msg1.has_lportid = true;
  msg1.lportid =  100;
  msg1.has_admin_state = true;
  msg1.admin_state = RW_FPATH_D_ADMIN_STATE_UP;

  strcpy(msg2.name, "eth1");
  msg2.has_name = 1;
  msg2.has_lportid = true;
  msg2.lportid = 200;
  msg2.has_admin_state = true;
  msg2.admin_state = RW_FPATH_D_ADMIN_STATE_DOWN;

  ks1.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "seagull_client");
  ks1.dompath.path002.has_key00 = true;
  strcpy(ks1.dompath.path002.key00.name, "bundle1");
  ks1.dompath.path003.has_key00 = true;
  strcpy(ks1.dompath.path003.key00.name, "eth0");

  ks2.dompath.path000.has_key00 = true;
  strcpy(ks2.dompath.path000.key00.name, "seagull_client");
  ks2.dompath.path002.has_key00 = true;
  strcpy(ks2.dompath.path002.key00.name, "bundle1");
  ks2.dompath.path003.has_key00 = true;
  strcpy(ks2.dompath.path003.key00.name, "eth1");

  ProtobufCBinaryData data1;
  data1.len = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  data1.data = (uint8_t *)malloc(data1.len);
  protobuf_c_message_pack(NULL, &msg1.base, data1.data);

  ProtobufCBinaryData data2;
  data2.len = protobuf_c_message_get_packed_size(NULL, &msg2.base);
  data2.data = (uint8_t *)malloc(data2.len);
  protobuf_c_message_pack(NULL, &msg2.base, data2.data);

  rw_keyspec_path_t *out_ks = nullptr;
  ProtobufCBinaryData out_msg;
  out_msg.len = 0;
  out_msg.data = NULL;
  rw_status_t status = rw_keyspec_path_reroot_and_merge_opaque(nullptr,&ks1.rw_keyspec_path_t,
                                                               &ks2.rw_keyspec_path_t,
                                                               &data1, &data2, &out_ks, &out_msg);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_TRUE(out_ks);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(out_ks);
  ASSERT_TRUE(out_msg.data);

  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther)* ck = 
      (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther)*) out_ks;
  ASSERT_TRUE(ck);

  EXPECT_EQ(rw_keyspec_path_get_depth(&ck->rw_keyspec_path_t), 3);
  EXPECT_TRUE(ck->has_dompath);
  EXPECT_TRUE(ck->dompath.path000.has_key00);
  EXPECT_STREQ(ck->dompath.path000.key00.name, "seagull_client");
  EXPECT_TRUE(ck->dompath.path002.has_key00);
  EXPECT_STREQ(ck->dompath.path002.key00.name, "bundle1");

  ProtobufCMessage *om = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther), 
                                                   out_msg.len, out_msg.data);
  ASSERT_TRUE(om);
  UniquePtrProtobufCMessage<>::uptr_t uptr2(om);

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther)* cm = 
      (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_BundleState_BundleEther) *)om;
  ASSERT_TRUE(cm);
  EXPECT_STREQ(cm->name, "bundle1");
  EXPECT_EQ(cm->n_members, 2);

  EXPECT_STREQ(cm->members[0].name, "eth0");
  EXPECT_STREQ(cm->members[1].name, "eth1");

  free(data1.data);
  free(data2.data);
  free(out_msg.data);
}

TEST(KsRRMerge, KsMerge)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_Arp, ks2);

  ks1.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "trafgen");
  ks1.dompath.path001.has_key00 = true;
  strcpy(ks1.dompath.path001.key00.name, "nc1");
  ks2.dompath.path003.has_key00 = true;
  strcpy(ks2.dompath.path003.key00.address, "10.0.0.1");

  rw_keyspec_path_t *out = rw_keyspec_path_merge(nullptr, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t);
  ASSERT_TRUE(out);

  auto c_out = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_Arp) *)rw_keyspec_path_create_dup_of_type(out, nullptr , 
                                                                                                                                RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_Arp));
  ASSERT_TRUE(c_out);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(&c_out->rw_keyspec_path_t);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr2(out);

  EXPECT_EQ(rw_keyspec_path_get_depth(&c_out->rw_keyspec_path_t), 4);
  EXPECT_TRUE(c_out->has_dompath);
  EXPECT_TRUE(c_out->dompath.path000.has_key00);
  EXPECT_STREQ(c_out->dompath.path000.key00.name, "trafgen");
  EXPECT_TRUE(c_out->dompath.path001.has_key00);
  EXPECT_STREQ(c_out->dompath.path001.key00.name, "nc1");
  EXPECT_TRUE(c_out->dompath.path003.has_key00);
  EXPECT_STREQ(c_out->dompath.path003.key00.address, "10.0.0.1");
}

TEST(KsRRMerge, KsMerge1)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_PortState_Info_Port, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_PortState_Info_Port_FlowControl, ks2);

  ks1.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "trafgen");
  ks1.dompath.path003.has_key00 = true;
  strcpy(ks1.dompath.path003.key00.name, "port1");

  rw_keyspec_path_t *out = rw_keyspec_path_merge(nullptr, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t);
  ASSERT_TRUE(out);

  auto c_out = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_PortState_Info_Port_FlowControl) *)rw_keyspec_path_create_dup_of_type(out, nullptr , 
                           RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony_PortState_Info_Port_FlowControl));
  ASSERT_TRUE(c_out);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(&c_out->rw_keyspec_path_t);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr2(out);

  EXPECT_EQ(rw_keyspec_path_get_depth(&c_out->rw_keyspec_path_t), 5);
  EXPECT_TRUE(c_out->has_dompath);
  EXPECT_TRUE(c_out->dompath.path000.has_key00);
  EXPECT_STREQ(c_out->dompath.path000.key00.name, "trafgen");
  EXPECT_TRUE(c_out->dompath.path003.has_key00);
  EXPECT_STREQ(c_out->dompath.path003.key00.name, "port1");

  ks2.dompath.path000.has_key00 = true;
  strcpy(ks2.dompath.path000.key00.name, "trafsink");
  out = rw_keyspec_path_merge(nullptr, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t);
  ASSERT_FALSE(out);
}

TEST(KsRRMerge, KsMerge2)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_IpClassifier_Fastpath_Signatures, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony, ks2);

  ks1.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "ltesim");
  ks1.dompath.path002.has_key00 = true;
  ks1.dompath.path002.key00.instance = 1;

  ks1.dompath.path003.has_key00 = true;
  ks1.dompath.path003.key00.signature_id = 23;

  rw_keyspec_path_t *gk = rw_keyspec_path_create_dup_of_type(&ks1.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gk);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(gk);

  rw_keyspec_path_t *out = rw_keyspec_path_merge(nullptr, gk, &ks2.rw_keyspec_path_t);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr2(out);

  auto c_out = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_IpClassifier_Fastpath_Signatures) *)rw_keyspec_path_create_dup_of_type(out, nullptr , 
                                                                                                                                    RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony_IpClassifier_Fastpath_Signatures));
  ASSERT_TRUE(c_out);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr3(&c_out->rw_keyspec_path_t);

  EXPECT_EQ(rw_keyspec_path_get_depth(&c_out->rw_keyspec_path_t), 4);
  EXPECT_TRUE(c_out->has_dompath);
  EXPECT_TRUE(c_out->dompath.path000.has_key00);
  EXPECT_STREQ(c_out->dompath.path000.key00.name, "ltesim");
  EXPECT_TRUE(c_out->dompath.path002.has_key00);
  EXPECT_EQ(c_out->dompath.path002.key00.instance, 1);
  EXPECT_TRUE(c_out->dompath.path003.has_key00);
  EXPECT_EQ(c_out->dompath.path003.key00.signature_id, 23);

  ks2.dompath.path000.has_key00 = true;
  strcpy(ks2.dompath.path000.key00.name, "trafsink");
  out = rw_keyspec_path_merge(nullptr, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t);
  ASSERT_FALSE(out);
}

TEST(KsRRMerge, KsMerge3)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_ExternalAppPlugin_Fastpath_Clients, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_ExternalAppPlugin_Fastpath_Clients, ks2);

  ks1.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "trafgen");
  ks1.dompath.path002.has_key00 = true;
  ks1.dompath.path002.key00.instance = 2;
  ks1.dompath.path003.has_key00 = true;
  ks1.dompath.path003.key00.index = 100;

  rw_keyspec_path_t *gks1 = rw_keyspec_path_create_dup_of_type(&ks1.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks1);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(gks1);

  rw_keyspec_path_t *gks2 = rw_keyspec_path_create_dup_of_type(&ks2.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks2);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr2(gks2);

  rw_keyspec_path_t *out = rw_keyspec_path_merge(nullptr, gks1, gks2);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr3(out);

  auto c_out = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_ExternalAppPlugin_Fastpath_Clients) *)rw_keyspec_path_create_dup_of_type(out, nullptr , 
                                                                                                                                      RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony_ExternalAppPlugin_Fastpath_Clients));
  ASSERT_TRUE(c_out);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr4(&c_out->rw_keyspec_path_t);

  EXPECT_EQ(rw_keyspec_path_get_depth(&c_out->rw_keyspec_path_t), 4);
  EXPECT_TRUE(c_out->has_dompath);
  EXPECT_TRUE(c_out->dompath.path000.has_key00);
  EXPECT_STREQ(c_out->dompath.path000.key00.name, "trafgen");
  EXPECT_EQ(c_out->dompath.path002.key00.instance, 2);
  EXPECT_EQ(c_out->dompath.path003.key00.index, 100);

  ks2.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "trafsink");
  gks2 = rw_keyspec_path_create_dup_of_type(&ks2.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks2);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr5(gks2);

  out = rw_keyspec_path_merge(nullptr, gks1, gks2);
  ASSERT_FALSE(out);
}

TEST(KsRRMerge, KsMerge4)
{
  RWPB_M_PATHSPEC_DECL_INIT(Testy2pTop2_data_Topl_La_Lb, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(Testy2pTop2_data_Topl_La_Lb, ks2);

  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptri(&ks1.rw_keyspec_path_t);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptro(&ks2.rw_keyspec_path_t);

  ks1.dompath.path000.has_key00 = true;
  ks1.dompath.path000.key00.k0 = strdup("key1");

  ks1.dompath.path001.has_key00 = true;
  ks1.dompath.path001.key00.k1 = strdup("key2");

  ks1.dompath.path001.has_key01 = true;
  ks1.dompath.path001.key01.k2 = 100;

  ks1.dompath.path002.has_key00 = true;
  ks1.dompath.path002.key00.k4 = strdup("key4");

  ks2.dompath.path000.has_key00 = true;
  ks2.dompath.path000.key00.k0 = strdup("key1");

  ks2.dompath.path001.has_key02 = true;
  ks2.dompath.path001.key02.k3 = strdup("key3");

  ks2.dompath.path002.has_key01 = true;
  ks2.dompath.path002.key01.k5 = 200;

  rw_keyspec_path_t *gks = rw_keyspec_path_create_dup_of_type(&ks2.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(gks);

  rw_keyspec_path_t *out = rw_keyspec_path_merge(nullptr, &ks1.rw_keyspec_path_t, gks);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr2(out);

  auto c_out = (RWPB_T_PATHSPEC(Testy2pTop2_data_Topl_La_Lb) *)rw_keyspec_path_create_dup_of_type(out, nullptr , 
                                                                                                  RWPB_G_PATHSPEC_PBCMD(Testy2pTop2_data_Topl_La_Lb));
  ASSERT_TRUE(c_out);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr3(&c_out->rw_keyspec_path_t);

  EXPECT_EQ(rw_keyspec_path_get_depth(&c_out->rw_keyspec_path_t), 3);
  EXPECT_TRUE(c_out->has_dompath);

  EXPECT_TRUE(c_out->dompath.path000.has_key00);
  EXPECT_STREQ(c_out->dompath.path000.key00.k0, "key1");

  EXPECT_TRUE(c_out->dompath.path001.has_key00);
  EXPECT_STREQ(c_out->dompath.path001.key00.k1, "key2");

  EXPECT_TRUE(c_out->dompath.path001.has_key01);
  EXPECT_EQ(c_out->dompath.path001.key01.k2, 100);

  EXPECT_TRUE(c_out->dompath.path001.has_key02);
  EXPECT_STREQ(c_out->dompath.path001.key02.k3, "key3");

  EXPECT_TRUE(c_out->dompath.path002.has_key00);
  EXPECT_STREQ(c_out->dompath.path002.key00.k4, "key4");

  EXPECT_TRUE(c_out->dompath.path002.has_key01);
  EXPECT_EQ(c_out->dompath.path002.key01.k5, 200);

  ks2.dompath.path001.has_key00 = true;
  ks2.dompath.path001.key00.k1 = strdup("other");

  out = rw_keyspec_path_merge(nullptr, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t);
  ASSERT_FALSE(out);
}

TEST(KsRRMerge, MergeOpGKs)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther_Vlan, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther_Vlan, msg2);

  msg1.id = 1;
  msg1.has_id = 1;
  msg1.has_open = true;
  msg1.open = true;

  msg2.id = 2;
  msg2.has_id = 1;
  msg2.has_shutdown = true;
  msg2.shutdown = true;

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther_Vlan, ks1);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther_Vlan, ks2);

  ks1.dompath.path000.has_key00 = true;
  strcpy(ks1.dompath.path000.key00.name, "trafgen");
  ks1.dompath.path001.has_key00 = true;
  strcpy(ks1.dompath.path001.key00.name, "bundle1");
  ks1.dompath.path002.has_key00 = true;
  ks1.dompath.path002.key00.id = 1;

  ks2.dompath.path000.has_key00 = true;
  strcpy(ks2.dompath.path000.key00.name, "trafgen");
  ks2.dompath.path001.has_key00 = true;
  strcpy(ks2.dompath.path001.key00.name, "bundle1");
  ks2.dompath.path002.has_key00 = true;
  ks2.dompath.path002.key00.id = 2;

  rw_keyspec_path_t *gks1 = NULL;
  gks1 = rw_keyspec_path_create_dup_of_type(&ks1.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks1);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptrk1(gks1);

  rw_keyspec_path_t *gks2 = NULL;
  gks2 = rw_keyspec_path_create_dup_of_type(&ks2.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks2);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptrk2(gks2);

  ProtobufCBinaryData bmsg1, bmsg2;
  uint8_t smsg1[512];
  bmsg1.len = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  bmsg1.data = &smsg1[0];
  protobuf_c_message_pack(NULL, &msg1.base, bmsg1.data);

  uint8_t smsg2[512];
  bmsg2.len = protobuf_c_message_get_packed_size(NULL, &msg2.base);
  bmsg2.data = &smsg2[0];
  protobuf_c_message_pack(NULL, &msg2.base, bmsg2.data);

  rw_keyspec_path_t *out = NULL;
  ProtobufCBinaryData bout = {0, NULL};
  rw_keyspec_path_reroot_and_merge_opaque(nullptr,gks1, gks2, 
                                                       &bmsg1, &bmsg2, &out, &bout);
  ASSERT_TRUE(out);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr(out);
  ASSERT_TRUE(bout.data);

  auto cout = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_BundleEther)*)rw_keyspec_path_create_dup_of_type(out, nullptr ,
                                                    RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony_BundleEther));
  ASSERT_TRUE(cout);
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(&cout->rw_keyspec_path_t);

  EXPECT_TRUE(cout->dompath.path000.has_key00);
  EXPECT_STREQ(cout->dompath.path000.key00.name, "trafgen");
  EXPECT_TRUE(cout->dompath.path001.has_key00);
  EXPECT_STREQ(cout->dompath.path001.key00.name, "bundle1");

  auto mo = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_BundleEther)*)protobuf_c_message_unpack(NULL,
                                          RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_BundleEther), bout.len,
                                          bout.data);

  ASSERT_TRUE(mo);
  UniquePtrProtobufCMessage<>::uptr_t uptr2(&mo->base);

  EXPECT_STREQ(mo->name, "bundle1");
  EXPECT_EQ(mo->n_vlan, 2);
  EXPECT_EQ(mo->vlan[0].id, 1);
  EXPECT_TRUE(mo->vlan[0].open);
  EXPECT_EQ(mo->vlan[1].id, 2);
  EXPECT_TRUE(mo->vlan[1].shutdown);

  free(bout.data);
  strcpy(ks1.dompath.path000.key00.name, "trafsink");
  gks1 = NULL;
  gks1 = rw_keyspec_path_create_dup_of_type(&ks1.rw_keyspec_path_t, nullptr , &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks1);

  uptrk1.reset(gks1);
  out = NULL;
  bout.data = NULL;

  rw_keyspec_path_reroot_and_merge_opaque(nullptr,gks1, gks2, 
                              &bmsg1, &bmsg2, &out, &bout);
  ASSERT_FALSE(out);
}

TEST(KsRRMerge, RerootCategoryTest)
{
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  ks1->dompath.has_path001 = true;
  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,msg1);

  msg1.id = 100;
  msg1.has_id = 1;
  msg1.name = strdup("John");
  msg1.title = strdup("SE");
  msg1.start_date = strdup("14/12");

  // Create keyspec2
  RWPB_T_PATHSPEC(Company_data_Company_Product) *ks2 = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Product)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&ks2));

  ks2->dompath.has_path001 = true;
  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 500;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Product,msg2);

  msg2.id = 500;
  msg2.has_id = 1;
  msg2.name = strdup("Prod1");
  msg2.msrp = strdup("100d");

  rw_keyspec_path_t *ks_out = nullptr;
  

  /* RW_SCHEMA_CATEGORY_DATA & RW_SCHEMA_CATEGORY_DATA */
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks1), nullptr , RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks2), nullptr , RW_SCHEMA_CATEGORY_DATA);

  ProtobufCMessage *out_msg = rw_keyspec_path_reroot_and_merge(nullptr,reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                               reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                               &msg1.base, &msg2.base, &ks_out);
  ASSERT_TRUE(ks_out);
  EXPECT_EQ(rw_keyspec_path_get_category(reinterpret_cast<rw_keyspec_path_t*>(ks_out)), RW_SCHEMA_CATEGORY_DATA);

  // Free the o/p from merge
  protobuf_c_message_free_unpacked(NULL, out_msg);
  rw_keyspec_path_free(ks_out, nullptr);
  ks_out = NULL;
  /* End of  RW_SCHEMA_CATEGORY_DATA & RW_SCHEMA_CATEGORY_DATA */


  /* RW_SCHEMA_CATEGORY_CONFIG & RW_SCHEMA_CATEGORY_ANY */
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks1), nullptr , RW_SCHEMA_CATEGORY_CONFIG);
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks2), nullptr , RW_SCHEMA_CATEGORY_ANY);

  out_msg = rw_keyspec_path_reroot_and_merge(nullptr,reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                               reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                               &msg1.base, &msg2.base, &ks_out);
  ASSERT_TRUE(ks_out);
  EXPECT_EQ(rw_keyspec_path_get_category(reinterpret_cast<rw_keyspec_path_t*>(ks_out)), RW_SCHEMA_CATEGORY_ANY);

  // Free the o/p from merge
  protobuf_c_message_free_unpacked(NULL, out_msg);
  rw_keyspec_path_free(ks_out, nullptr);
  ks_out = NULL;
  /* End of  RW_SCHEMA_CATEGORY_CONFIG & RW_SCHEMA_CATEGORY_ANY */

  /* RW_SCHEMA_CATEGORY_ANY & RW_SCHEMA_CATEGORY_ANY */
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks1), nullptr , RW_SCHEMA_CATEGORY_ANY);
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks2), nullptr , RW_SCHEMA_CATEGORY_ANY);

  out_msg = rw_keyspec_path_reroot_and_merge(nullptr,reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                               reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                               &msg1.base, &msg2.base, &ks_out);
  ASSERT_TRUE(ks_out);
  EXPECT_EQ(rw_keyspec_path_get_category(reinterpret_cast<rw_keyspec_path_t*>(ks_out)), RW_SCHEMA_CATEGORY_ANY);

  // Free the o/p from merge
  protobuf_c_message_free_unpacked(NULL, out_msg);
  rw_keyspec_path_free(ks_out, nullptr);
  ks_out = NULL;
  /* End of RW_SCHEMA_CATEGORY_ANY & RW_SCHEMA_CATEGORY_ANY */

  /* RW_SCHEMA_CATEGORY_CONFIG & RW_SCHEMA_CATEGORY_DATA */
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks1), nullptr , RW_SCHEMA_CATEGORY_CONFIG);
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t*>(ks2), nullptr, RW_SCHEMA_CATEGORY_DATA);

  out_msg = rw_keyspec_path_reroot_and_merge(nullptr, reinterpret_cast<rw_keyspec_path_t*>(ks1),
                                                               reinterpret_cast<rw_keyspec_path_t*>(ks2),
                                                               &msg1.base, &msg2.base, &ks_out);
  ASSERT_TRUE(ks_out);
  EXPECT_EQ(rw_keyspec_path_get_category(reinterpret_cast<rw_keyspec_path_t*>(ks_out)), RW_SCHEMA_CATEGORY_ANY);

  // Free the o/p from merge
  protobuf_c_message_free_unpacked(NULL, out_msg);
  rw_keyspec_path_free(ks_out, nullptr);
  ks_out = NULL;
  /* End of RW_SCHEMA_CATEGORY_CONFIG & RW_SCHEMA_CATEGORY_DATA */
  
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), NULL);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
}

TEST(KsRRMerge, RootKSReRootDeeper)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data, rks);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, rootm);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&rootm.base);

  rootm.n_colony = 2;
  rootm.colony = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony) **)malloc(sizeof(void*)*2);

  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony, colony1);
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony, colony2);

  strcpy(colony1->name, "trafgen");
  colony1->has_name = 1;
  colony1->n_bundle_ether = 2;
  strcpy(colony1->bundle_ether[0].name, "c1be1");
  strcpy(colony1->bundle_ether[1].name, "c1be2");
  colony1->bundle_ether[0].has_name = 1;
  colony1->bundle_ether[1].has_name = 1;

  strcpy(colony2->name, "trafsink");
  colony2->has_name = 1;
  colony2->n_bundle_ether = 2;
  strcpy(colony2->bundle_ether[0].name, "c2be1");
  strcpy(colony2->bundle_ether[1].name, "c2be2");
  colony2->bundle_ether[0].has_name = 1;
  colony2->bundle_ether[1].has_name = 1;

  rootm.colony[0] = colony1;
  rootm.colony[1] = colony2;

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony, kscol);
  kscol.dompath.path000.has_key00 = true;
  strcpy(kscol.dompath.path000.key00.name, "trafgen");

  ProtobufCMessage* out = rw_keyspec_path_reroot(&rks.rw_keyspec_path_t, NULL, &rootm.base, &kscol.rw_keyspec_path_t);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t uptr(out);

  auto mcol = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*)out;
  EXPECT_EQ(out->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony));
  EXPECT_STREQ(mcol->name, "trafgen");

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, ksbe);
  ksbe.dompath.path000.has_key00 = true;
  strcpy(ksbe.dompath.path000.key00.name, "trafgen");

  ksbe.dompath.path001.has_key00 = true;
  strcpy(ksbe.dompath.path001.key00.name, "c1be2");

  out = rw_keyspec_path_reroot(&rks.rw_keyspec_path_t, NULL, &rootm.base, &ksbe.rw_keyspec_path_t);
  ASSERT_TRUE(out);
  uptr.reset(out);

  auto mbe = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_BundleEther)*)out;
  EXPECT_EQ(out->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_BundleEther));
  EXPECT_STREQ(mbe->name, "c1be2");
}

TEST(KsRRMerge, RootKSReRootShallower)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony, kscol);
  kscol.dompath.path000.has_key00 = true;
  strcpy(kscol.dompath.path000.key00.name, "trafgen");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony1);
  strcpy(colony1.name, "trafgen");
  colony1.has_name = 1;
  colony1.n_bundle_ether = 2;
  strcpy(colony1.bundle_ether[0].name, "c1be1");
  strcpy(colony1.bundle_ether[1].name, "c1be2");
  colony1.bundle_ether[0].has_name = 1;
  colony1.bundle_ether[1].has_name = 1;

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data, rks);

  ProtobufCMessage* out = rw_keyspec_path_reroot(&kscol.rw_keyspec_path_t, NULL, &colony1.base, &rks.rw_keyspec_path_t);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t uptr(out);

  auto mroot = (RWPB_T_MSG(RwFpathD_RwBaseD_data)*)out;
  EXPECT_EQ(out->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data));
  EXPECT_EQ(mroot->n_colony, 1);
  ASSERT_TRUE(mroot->colony);
  EXPECT_STREQ(mroot->colony[0]->name, "trafgen");
  EXPECT_EQ(mroot->colony[0]->n_bundle_ether, 2);
  EXPECT_STREQ(mroot->colony[0]->bundle_ether[0].name, "c1be1");
  EXPECT_STREQ(mroot->colony[0]->bundle_ether[1].name, "c1be2");

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, ksbe);
  ksbe.dompath.path000.has_key00 = true;
  strcpy(ksbe.dompath.path000.key00.name, "trafgen");

  ksbe.dompath.path001.has_key00 = true;
  strcpy(ksbe.dompath.path001.key00.name, "c1be2");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, mbe);
  strcpy(mbe.name, "c1be2");
  mbe.has_name = 1;

  out = rw_keyspec_path_reroot(&ksbe.rw_keyspec_path_t, NULL, &mbe.base, &rks.rw_keyspec_path_t);
  ASSERT_TRUE(out);

  uptr.reset(out);

  mroot = (RWPB_T_MSG(RwFpathD_RwBaseD_data)*)out;
  EXPECT_EQ(out->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data));
  EXPECT_EQ(mroot->n_colony, 1);
  ASSERT_TRUE(mroot->colony);
  EXPECT_STREQ(mroot->colony[0]->name, "trafgen");
  EXPECT_EQ(mroot->colony[0]->n_bundle_ether, 1);
  EXPECT_STREQ(mroot->colony[0]->bundle_ether[0].name, "c1be2");
}

TEST(KsRRMerge, RootKSReRootIterD)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data, rks);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, rootm);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptrm(&rootm.base);

  rootm.n_colony = 2;
  rootm.colony = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony) **)malloc(sizeof(void*)*2);

  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony, colony1);
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony, colony2);

  strcpy(colony1->name, "trafgen");
  colony1->has_name = 1;
  colony1->n_bundle_ether = 2;
  strcpy(colony1->bundle_ether[0].name, "c1be1");
  strcpy(colony1->bundle_ether[1].name, "c1be2");
  colony1->bundle_ether[0].has_name = 1;
  colony1->bundle_ether[1].has_name = 1;

  strcpy(colony2->name, "trafsink");
  colony2->has_name = 1;
  colony2->n_bundle_ether = 2;
  strcpy(colony2->bundle_ether[0].name, "c2be1");
  strcpy(colony2->bundle_ether[1].name, "c2be2");
  colony2->bundle_ether[0].has_name = 1;
  colony2->bundle_ether[1].has_name = 1;

  rootm.colony[0] = colony1;
  rootm.colony[1] = colony2;

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony, kscol);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(&rks.rw_keyspec_path_t, nullptr , &state,
                                   &rootm.base, &kscol.rw_keyspec_path_t);

  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &rootm.colony[i]->base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(RwFpathD_RwBaseD_data_Colony, ks);

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.name, rootm.colony[i]->name);

    i++;
  }
  EXPECT_EQ(i, 2);
  rw_keyspec_path_reroot_iter_done(&state);

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, ksbe);
  rw_keyspec_path_reroot_iter_init(&rks.rw_keyspec_path_t, nullptr , &state,
                                   &rootm.base, &ksbe.rw_keyspec_path_t);

  i = 0;
  unsigned j = 0;

  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);
    EXPECT_EQ(msg, &rootm.colony[i]->bundle_ether[j].base);

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(ks);
    EXPECT_FALSE(rw_keyspec_path_has_wildcards(ks));

    auto out = RWPB_M_PATHSPEC_CONST_CAST(RwFpathD_RwBaseD_data_Colony_BundleEther, ks);

    EXPECT_TRUE(out->dompath.path000.has_key00);
    EXPECT_STREQ(out->dompath.path000.key00.name, rootm.colony[i]->name);

    EXPECT_TRUE(out->dompath.path001.has_key00);
    EXPECT_STREQ(out->dompath.path001.key00.name, rootm.colony[i]->bundle_ether[j].name);

    j++; if (j == 2) { i++; j = 0; }
  }

  EXPECT_EQ(i, 2);
  rw_keyspec_path_reroot_iter_done(&state);
}

TEST(KsRRMerge, RootKSReRootIterS)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony, kscol);
  kscol.dompath.path000.has_key00 = true;
  strcpy(kscol.dompath.path000.key00.name, "trafgen");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony1);
  strcpy(colony1.name, "trafgen");
  colony1.has_name = 1;
  colony1.n_bundle_ether = 2;
  strcpy(colony1.bundle_ether[0].name, "c1be1");
  strcpy(colony1.bundle_ether[1].name, "c1be2");
  colony1.bundle_ether[0].has_name = 1;
  colony1.bundle_ether[1].has_name = 1;

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data, rks);

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(&kscol.rw_keyspec_path_t, NULL, &state,
                                   &colony1.base, &rks.rw_keyspec_path_t);
  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    auto msg = rw_keyspec_path_reroot_iter_get_msg(&state);
    EXPECT_TRUE(msg);

    auto mroot = (RWPB_T_MSG(RwFpathD_RwBaseD_data)*)msg;
    EXPECT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data));
    EXPECT_EQ(mroot->n_colony, 1);
    ASSERT_TRUE(mroot->colony);
    EXPECT_STREQ(mroot->colony[0]->name, "trafgen");
    EXPECT_EQ(mroot->colony[0]->n_bundle_ether, 2);
    EXPECT_STREQ(mroot->colony[0]->bundle_ether[0].name, "c1be1");
    EXPECT_STREQ(mroot->colony[0]->bundle_ether[1].name, "c1be2");

    auto ks = rw_keyspec_path_reroot_iter_get_ks(&state);
    EXPECT_TRUE(ks);
    EXPECT_EQ(ks->base.descriptor, RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data));
    EXPECT_TRUE(rw_keyspec_path_is_module_root(ks));

    i++;
  }

  EXPECT_EQ(i, 1);
  rw_keyspec_path_reroot_iter_done(&state);
}


TEST(KsRRMerge, RerootMergeOpaqueNonKeyList)
{
  struct single_result{
    RwSchemaPathSpec *keyspec;
    ProtobufCBinaryData keybuf;
    ProtobufCBinaryData paybuf;    
  };
  // Create keyspec1
  RWPB_T_PATHSPEC(Company_data_Company_Customer_Department) *queryks = nullptr;
  RWPB_T_PATHSPEC(Company_data_Company_Customer) *appks = nullptr;
  RWPB_T_MSG(Company_data_Company_Customer) *customer = (RWPB_T_MSG(Company_data_Company_Customer) *)NULL;
  unsigned int i;
  unsigned int j;

  RWPB_T_MSG(Company_data_Company_Customer_Department) **dept_s = NULL;
  RWPB_T_MSG(Company_data_Company_Customer_Department_Contact) **contact_s = NULL;
  RWPB_T_MSG(Company_data_Company_Customer_Department) *dept = NULL;
  RWPB_T_MSG(Company_data_Company_Customer_Department_Contact) *contact = NULL;
  rw_keyspec_path_reroot_iter_state_t state;
  //REROOT
  single_result *result_s[40];
  single_result *result;
  unsigned int n_result = 0;
  //MERGEOPAQUE
  ProtobufCBinaryData msg_in;
  ProtobufCBinaryData msg_out;
  rw_keyspec_path_t   *ks_out;
  rw_keyspec_path_t *ks_in = NULL;
  rw_status_t status;
  single_result merged_result;
  ProtobufCMessage *unpacked_msg[64];
  rw_keyspec_path_t *unpacked_keyspec[64];  
  rw_yang_pb_msgdesc_t*       desc_result   = NULL;
  ProtobufCMessage * matchmsg = NULL;
  rw_keyspec_path_t*     matchks  = NULL;

  
  //Create the query keyspec and app returned keyspec just as in the prepare callback in dts
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Customer)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&appks));
  
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Customer_Department)), nullptr, 
                             reinterpret_cast<rw_keyspec_path_t**>(&queryks));
  
  
  customer = RWPB_F_MSG_ALLOC(Company_data_Company_Customer);
  
  appks->dompath.path001.has_key00 = true;
  appks->dompath.path001.key00.name = strdup("Verizon");
  queryks->dompath.path001.has_key00 = true;
  queryks->dompath.path001.key00.name = strdup("Verizon");

  
  //create the protobuf message
  customer->name = strdup(appks->dompath.path001.key00.name);
  dept_s = customer->department = (RWPB_T_MSG(Company_data_Company_Customer_Department)**)RW_MALLOC0(sizeof(*dept_s) * 10);
  customer->n_department = 0;
  for (i = 0; i < 10; i++){
    dept_s[customer->n_department] = dept = (RWPB_T_MSG(Company_data_Company_Customer_Department) *)RW_MALLOC0(sizeof(*dept));
    RWPB_F_MSG_INIT(Company_data_Company_Customer_Department, dept);
    dept->name =(char*) RW_MALLOC0(64);
    sprintf(dept->name, "Dept%d", i);
    dept->contact = contact_s = (RWPB_T_MSG(Company_data_Company_Customer_Department_Contact) **)RW_MALLOC0(sizeof(*contact_s) * 10);
    dept->n_contact = 0;
    customer->n_department++;
    for (j = 0; j < 10; j++){
      contact_s[dept->n_contact] = contact = (RWPB_T_MSG(Company_data_Company_Customer_Department_Contact) *)RW_MALLOC0(sizeof(*contact));
      RWPB_F_MSG_INIT(Company_data_Company_Customer_Department_Contact, contact);

      contact->name= (char*)RW_MALLOC0(64);
      sprintf(contact->name, "Contact%d", j);
      dept->n_contact++;
    }
  }

  /*Reroot it to the query-ks*/
  {
    rw_keyspec_path_reroot_iter_init((const rw_keyspec_path_t*)appks,
                                     NULL,
                                     &state, (const ProtobufCMessage*)customer,
                                     (const rw_keyspec_path_t*)queryks);
    n_result = 0;
    while (rw_keyspec_path_reroot_iter_next(&state)) {
      matchmsg = rw_keyspec_path_reroot_iter_take_msg(&state);
      matchks = rw_keyspec_path_reroot_iter_take_ks(&state);
      EXPECT_TRUE ((!matchmsg ||
                    (matchmsg == (ProtobufCMessage*)customer) ||
                    (matchks == (rw_keyspec_path_t *)appks) ||
                    rw_keyspec_path_has_wildcards(matchks)) == 0);
      result_s[n_result++] = result = (single_result *)RW_MALLOC0(sizeof(*result));
      
      result->paybuf.data  = protobuf_c_message_serialize(NULL, matchmsg,
                                                          &result->paybuf.len);
      rw_keyspec_path_serialize_dompath(matchks,
                                        NULL, &result->keybuf);
      result->keyspec = (RwSchemaPathSpec *)rw_keyspec_path_create_dup_of_type(matchks,
                                                                               NULL,
                                                                               &rw_schema_path_spec__descriptor);
      
      protobuf_c_message_free_unpacked(&protobuf_c_default_instance, matchmsg);
      rw_keyspec_path_free(matchks, NULL);
    }
    rw_keyspec_path_reroot_iter_done(&state);
  }
  

  //Merged the rerooted serialized protobuf using rw_keyspec_path_reroot_and_merge_opaque
  ks_out = NULL;
  msg_out.len = 0;
  msg_out.data = NULL;
  if (!msg_out.data) {
    msg_in.len = 0;
    msg_in.data = NULL;
    ks_out = NULL;
  }
  for (j = 0; j < n_result; j++) {
    result = result_s[j];
    if (result->paybuf.len == 0) {
      continue;
    }
    if (!ks_in && !msg_in.data) {
      msg_in.data = (unsigned char*)RW_MALLOC0(result->paybuf.len);
      msg_in.len = result->paybuf.len;
      memcpy(msg_in.data, result->paybuf.data, msg_in.len);
      rw_keyspec_path_create_dup((rw_keyspec_path_t*)result->keyspec, NULL , &ks_in);
      continue;
    }else if (msg_out.data && msg_out.len) {
      rw_keyspec_path_free(ks_in, NULL);
      ks_in = NULL;
      rw_keyspec_path_create_dup(ks_out, NULL , &ks_in);
      RW_FREE(msg_in.data);
      msg_in.len = msg_out.len;
      msg_in.data = (unsigned char *)RW_MALLOC(msg_in.len);
      memcpy(msg_in.data, msg_out.data, msg_in.len);
    }
    if (ks_out) {
      rw_keyspec_path_free(ks_out, NULL);
      ks_out = NULL;
    }
    if (msg_out.data) {
      RW_FREE(msg_out.data);
    }
    msg_out.len = 0;
    msg_out.data = NULL;
    status = rw_keyspec_path_reroot_and_merge_opaque(NULL,
                                                     ks_in,
                                                     (rw_keyspec_path_t*)result->keyspec,
                                                     &msg_in,
                                                     &result->paybuf,
                                                     &ks_out, &msg_out);
    EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  }
  if (ks_in) {
    rw_keyspec_path_free(ks_in, NULL);
  }
  if (msg_in.data) {
    RW_FREE(msg_in.data);
  }
  
  merged_result.paybuf.len = msg_out.len;
  merged_result.paybuf.data = msg_out.data;
  merged_result.keybuf.len = 0;
  merged_result.keybuf.data = NULL;
  rw_keyspec_path_serialize_dompath(ks_out, NULL, &merged_result.keybuf);
  merged_result.keyspec = (RwSchemaPathSpec *)ks_out;

  //free the results since they are merged into the merged result..
  for (i = 0; i < n_result; i++){
    rw_keyspec_path_free((rw_keyspec_path_t *)result_s[i]->keyspec, NULL);
    RW_FREE(result_s[i]->paybuf.data);
    RW_FREE(result_s[i]->keybuf.data);
    RW_FREE(result_s[i]);
  }



  
  /*Now that we have the merged serialized result, unpack it.*/
  matchmsg = NULL;
  n_result = 0;
  const rw_yang_pb_schema_t* ks_schema = ((ProtobufCMessage*)queryks)->descriptor->ypbc_mdesc->module->schema?
      ((ProtobufCMessage*)queryks)->descriptor->ypbc_mdesc->module->schema:NULL;
  status = rw_keyspec_path_find_msg_desc_schema((rw_keyspec_path_t *)merged_result.keyspec, 
                                                NULL,
                                                ks_schema,
                                                (const rw_yang_pb_msgdesc_t **)&desc_result,
                                                NULL);
  
  matchmsg =  protobuf_c_message_unpack(NULL,
                                        desc_result->u->msg_msgdesc.pbc_mdesc,
                                        merged_result.paybuf.len,
                                        merged_result.paybuf.data);
  
  ks_in = (rw_keyspec_path_t *)merged_result.keyspec;

  rw_keyspec_path_reroot_iter_init(ks_in,
                                   NULL,
                                   &state,
                                   matchmsg, (const rw_keyspec_path_t*)queryks);
  while (rw_keyspec_path_reroot_iter_next(&state)) {
    unpacked_msg[n_result] = rw_keyspec_path_reroot_iter_take_msg(&state);
    if (unpacked_msg[n_result]) {
      unpacked_keyspec[n_result] = rw_keyspec_path_reroot_iter_take_ks(&state);
      n_result++;
    }
  }
  rw_keyspec_path_reroot_iter_done(&state);
  if (matchmsg)
    protobuf_c_message_free_unpacked(NULL, matchmsg);
  
  
  EXPECT_TRUE(n_result == 10);
  for (i = 0; i < n_result; i++){
    dept = (RWPB_T_MSG(Company_data_Company_Customer_Department) *)unpacked_msg[i];
    EXPECT_TRUE(strcmp(dept->name,
                       customer->department[i]->name) == 0);
    EXPECT_TRUE(dept->n_contact == 10);
    for (j = 0; j < dept->n_contact; j++){
      EXPECT_TRUE(strcmp(dept->contact[j]->name,
                         customer->department[i]->contact[j]->name) == 0);
    }
    rw_keyspec_path_free((rw_keyspec_path_t *)unpacked_keyspec[i], NULL);
    protobuf_c_message_free_unpacked(NULL, unpacked_msg[i]);
  }


  
  /*Check if the reroot and then merge came back the same*/
  rw_keyspec_path_free((rw_keyspec_path_t *)merged_result.keyspec, NULL);
  RW_FREE(merged_result.paybuf.data);
  RW_FREE(merged_result.keybuf.data);
  
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(appks), NULL);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(queryks), NULL);  

  protobuf_c_message_free_unpacked(NULL, &customer->base);
}

