/* STANDARD_RIFT_IO_COPYRIGHT */
/*
 * @file pbc_delta_test.cpp
 * @author Arun Muralidharan
 * @date 2015/07/16
 * @brief Google test cases for testing protobuf delta message functionality
 */

#include <limits.h>
#include <stdlib.h>
#include <sstream>

#include "rwut.h"
#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include "rw_json.h"
#include "rw_schema.pb-c.h"
#include "bumpy-conversion.pb-c.h"
#include "test-rwapis.pb-c.h"
#include "company.pb-c.h"
#include "document.pb-c.h"
#include "flat-conversion.pb-c.h"
#include "test-ydom-top.pb-c.h"
#include "test-ydom-aug.pb-c.h"
#include "test-field-merge-b.pb-c.h"
#include "rw-fpath-d.pb-c.h"
#include "rwdts-data-d.pb-c.h"
#include "ctype-test.pb-c.h"

#include "test-tag-generation-base.pb-c.h"
#include "test-tag-generation.pb-c.h"

#include "rw_namespace.h"
#include <protobuf-c/rift/rw_pb_delta.h>

void init_comp_msg(RWPB_T_MSG(Company_data_Company)* msg, int nume_, int nump_)
{
      msg->contact_info = (RWPB_T_MSG(Company_data_Company_ContactInfo) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_ContactInfo)));
      RWPB_F_MSG_INIT(Company_data_Company_ContactInfo, msg->contact_info); 
      msg->contact_info->name = strdup("abc"); 
      msg->contact_info->address = strdup("bglr");
      msg->contact_info->phone_number = strdup("1234");
      msg->n_employee = nume_;
      msg->employee = (RWPB_T_MSG(Company_data_Company_Employee) **)calloc(nume_, sizeof(void*));
      msg->employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
      RWPB_F_MSG_INIT(Company_data_Company_Employee,msg->employee[0]); 
      msg->employee[0]->id = 100;
      msg->employee[0]->has_id = 1;
      msg->employee[0]->name = strdup("Emp1");
      msg->employee[0]->title = strdup("Title");
      msg->employee[0]->start_date = strdup("1234");
      msg->employee[1] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
      RWPB_F_MSG_INIT(Company_data_Company_Employee,msg->employee[1]);
      msg->employee[1]->id = 200;
      msg->employee[1]->has_id = 1;
      msg->employee[1]->name = strdup("Emp2");
      msg->employee[1]->title = strdup("Title");
      msg->employee[1]->start_date = strdup("1234");
      msg->n_product = nump_;
      msg->product = (RWPB_T_MSG(Company_data_Company_Product) **)calloc(nump_, sizeof(void*));
      msg->product[0] =  (RWPB_T_MSG(Company_data_Company_Product) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Product)));
      RWPB_F_MSG_INIT(Company_data_Company_Product,msg->product[0]);
      msg->product[0]->id = 1000;
      msg->product[0]->has_id = 1;
      msg->product[0]->name = strdup("nfv");
      msg->product[0]->msrp = strdup("1000");
      msg->product[1] = (RWPB_T_MSG(Company_data_Company_Product) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Product)));
      RWPB_F_MSG_INIT(Company_data_Company_Product,msg->product[1]);
      msg->product[1]->id = 2000;
      msg->product[1]->has_id = 1;
      msg->product[1]->name = strdup("cloud");
      msg->product[1]->msrp = strdup("2000");
}

TEST(PbDelta, APITest)
{
  TEST_DESCRIPTION("Test the protobuf-c API protobuf_c_unknown_field_cast_message");

  ASSERT_GE(sizeof(ProtobufCUnknownUnionMax), sizeof(ProtobufCUnknownSerialized));

  ASSERT_GE(sizeof(ProtobufCUnknownUnionMax), sizeof(ProtobufCUnknownMessageCast));

  RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont1 =
              (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
  EXPECT_TRUE(tscont1);

  RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont1);

  tscont1->n_inventory = 3;

  tscont1->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont1->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        tscont1->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)));
        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont1->inventory[i]);

        std::ostringstream oss;
        oss << "Batman " << i;

        tscont1->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory[i]->item_id = static_cast<uint32_t>(i);
    }


    tscont1->n_inventory_2 = 3;
    tscont1->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                            calloc(tscont1->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont1->n_inventory_2; i++) {
        tscont1->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                                  calloc(1, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont1->inventory_2[i]);

        std::ostringstream oss;
        oss << "Ironman " << i;

        tscont1->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory_2[i]->item_id = static_cast<uint32_t>(i); 
        tscont1->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module);
    module.n_all_shops = 3;
    module.all_shops = (char**)calloc(module.n_all_shops, sizeof(char*));

    module.all_shops[0] = strdup("ComicHouse-1");
    module.all_shops[1] = strdup("ComicHouse-2");
    module.all_shops[2] = strdup("ComicHouse-3"); 

    // set the shop
    module.shop = tscont1;
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&module.base);

    // create a message for unknown field
    RWPB_T_MSG(Company_data_Company) msg1;
    RWPB_F_MSG_INIT(Company_data_Company, &msg1);
    init_comp_msg(&msg1, 2, 2);

    //UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg1.base);

    EXPECT_TRUE(msg1.base.descriptor);
    const auto *fdesc = &msg1.base.descriptor->fields[0];
    const auto *submsg = protobuf_c_message_get_sub_message(&msg1.base, 0, 0);
    EXPECT_TRUE(submsg);

    // Add 1st unknown field
    auto ret = protobuf_c_message_append_unknown_cast_message(nullptr, &module.base, 60019, 
                      fdesc, submsg);
    EXPECT_TRUE(ret);

    // Now get the unknown field
    auto *unkn_fld = protobuf_c_message_unknown_get_index(&module.base, 0);
    ASSERT_TRUE(unkn_fld);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE), 
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_MESSAGE_CAST);

    auto *msg_cast = protobuf_c_unknown_field_cast_message(nullptr, 
        const_cast<ProtobufCMessageUnknownField*>(unkn_fld), fdesc);

    EXPECT_TRUE(msg_cast);

    ret = protobuf_c_message_append_unknown_cast_message_dup(nullptr, &module.base, 500,
                fdesc, submsg);
    EXPECT_TRUE(ret);

    unkn_fld = protobuf_c_message_unknown_get_index(&module.base, 1);
    ASSERT_TRUE(unkn_fld);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_MESSAGE_CAST);

    msg_cast = protobuf_c_unknown_field_cast_message(nullptr, 
        const_cast<ProtobufCMessageUnknownField*>(unkn_fld), fdesc);
    EXPECT_TRUE(msg_cast);

    EXPECT_NE(msg_cast, submsg);


    // Serialize the second submsg
    auto *user = protobuf_c_unknown_field_cast_serialized(nullptr, 
        const_cast<ProtobufCMessageUnknownField*>(unkn_fld));
    EXPECT_TRUE(user);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);


    // pack the protobuf message
    uint8_t buff[2048];
    auto len = protobuf_c_message_get_packed_size(nullptr, &module.base);
    ASSERT_TRUE(sizeof(buff) > len);

    // Pack should change the internal union repr
    // to serialized from message_cast
    len = protobuf_c_message_pack(nullptr, &module.base, buff);

    auto *out = (RWPB_T_MSG(TestFieldMergeB_data)*)protobuf_c_message_unpack(nullptr, RWPB_G_MSG_PBCMD(TestFieldMergeB_data), 
        len, buff);
    ASSERT_TRUE(out);

    UniquePtrProtobufCMessage<>::uptr_t uptr3(&out->base);

    EXPECT_EQ(protobuf_c_message_unknown_get_count(&out->base), 2);

    unkn_fld = protobuf_c_message_unknown_get_index(&out->base, 0);
    ASSERT_TRUE(unkn_fld);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

    // Cast it to message_cast
    auto *cmsg1 = protobuf_c_unknown_field_cast_message(nullptr, const_cast<ProtobufCMessageUnknownField*>(unkn_fld), fdesc);
    EXPECT_TRUE(cmsg1);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_MESSAGE_CAST);

    unkn_fld = protobuf_c_message_unknown_get_index(&out->base, 1);
    ASSERT_TRUE(unkn_fld);
    auto *cmsg2 = protobuf_c_unknown_field_cast_message(nullptr, const_cast<ProtobufCMessageUnknownField*>(unkn_fld), fdesc);

    EXPECT_STREQ(cmsg1->descriptor->name, "company.YangData.Company.Company.ContactInfo");
    EXPECT_STREQ(cmsg2->descriptor->name, "company.YangData.Company.Company.ContactInfo");

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_MESSAGE_CAST);

    user = protobuf_c_unknown_field_cast_serialized(nullptr,
        const_cast<ProtobufCMessageUnknownField*>(unkn_fld));
    EXPECT_TRUE(user);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

    // Re pack it
    len = protobuf_c_message_get_packed_size(nullptr, &out->base);
    ASSERT_TRUE(sizeof(buff) > len);
    
    // Pack should change the internal union repr
    // to serialized from message_cast
    size_t len1 = protobuf_c_message_pack(nullptr, &out->base, buff);
    ASSERT_TRUE(len == len1);
   
    out = (RWPB_T_MSG(TestFieldMergeB_data)*)protobuf_c_message_unpack(nullptr, RWPB_G_MSG_PBCMD(TestFieldMergeB_data),
    len1, buff);
    ASSERT_TRUE(out);
    uptr3.reset(&out->base);

    unkn_fld = protobuf_c_message_unknown_get_index(&out->base, 1);
    ASSERT_TRUE(unkn_fld);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

    cmsg2 = protobuf_c_unknown_field_cast_message(nullptr, const_cast<ProtobufCMessageUnknownField*>(unkn_fld), fdesc);
    EXPECT_STREQ(cmsg2->descriptor->name, "company.YangData.Company.Company.ContactInfo");

    unkn_fld = protobuf_c_message_unknown_get_index(&out->base, 0);
    ASSERT_TRUE(unkn_fld);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

    cmsg1 = protobuf_c_unknown_field_cast_message(nullptr, const_cast<ProtobufCMessageUnknownField*>(unkn_fld), fdesc);
    EXPECT_STREQ(cmsg1->descriptor->name, "company.YangData.Company.Company.ContactInfo");

    const auto *fdesc1 = &msg1.base.descriptor->fields[1];
    const auto *submsg1 = protobuf_c_message_get_sub_message(&msg1.base, 1, 0);
    ASSERT_TRUE(submsg1);

    ret = protobuf_c_message_append_unknown_cast_message(nullptr, &out->base, 60020,
        fdesc1, submsg1);
    EXPECT_TRUE(ret);
    
    // Now get the unknown field
    auto *unkn_fld2 = protobuf_c_message_unknown_get_index(&out->base, 2);
    ASSERT_TRUE(unkn_fld2);
    
    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld2->base_.unknown_flags, UNKNOWN_UNION_TYPE),
      PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_MESSAGE_CAST);

    user = protobuf_c_unknown_field_cast_serialized(nullptr,
        const_cast<ProtobufCMessageUnknownField*>(unkn_fld2));
    EXPECT_TRUE(user);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld2->base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

    EXPECT_EQ(protobuf_c_message_unknown_get_count(&out->base), 3);

    auto *cmsg3 = protobuf_c_unknown_field_cast_message(nullptr, const_cast<ProtobufCMessageUnknownField*>(unkn_fld2), fdesc1);
    EXPECT_TRUE(cmsg3);

    EXPECT_EQ(PROTOBUF_C_FLAG_GET(unkn_fld2->cmessage.base_.unknown_flags, UNKNOWN_UNION_TYPE),
        PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_MESSAGE_CAST);

    EXPECT_STREQ(cmsg3->descriptor->name, "company.YangData.Company.Company.Employee");


    // Add tests for augmented yang fields
}

TEST(PbDelta, DeleteDeltaList)
{
  // 1. Create a registration ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext) reg_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext));

  reg_ks.dompath.path000.has_key00 = true;
  strcpy(reg_ks.dompath.path000.key00.name, "trafgen");

  // 2. Create a delete ks deep inside nc
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_Ip) del_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_Ip));

  del_ks.dompath.path000.has_key00 = true;
  strcpy(del_ks.dompath.path000.key00.name, "trafgen");

  del_ks.dompath.path001.has_key00 = true;
  strcpy(del_ks.dompath.path001.key00.name, "nc1");

  del_ks.dompath.path002.has_key00 = true;
  strcpy(del_ks.dompath.path002.key00.name, "int1");

  del_ks.dompath.path003.has_key00 = true;
  strcpy(del_ks.dompath.path003.key00.address, "10.0.1.0");

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext));

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* delete_delta =
      (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)*) msg;
  ASSERT_EQ(delete_delta->n_interface, 1);
  ASSERT_TRUE(delete_delta->interface);
  ASSERT_EQ(delete_delta->interface[0]->n_ip, 1);

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "interface");

  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->interface[0]->base, "ip");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));

  EXPECT_STREQ(delete_delta->interface[0]->ip[0].address, "10.0.1.0");

  char* json = nullptr;
  rw_json_pbcm_to_json(msg, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  // Test dup of delta's
  ProtobufCMessage* out = protobuf_c_message_duplicate_allow_deltas(NULL, msg, msg->descriptor);
  ASSERT_TRUE(out);
  uptr.reset(out);

  json = nullptr;
  rw_json_pbcm_to_json(out, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  ASSERT_EQ(out->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext));
  delete_delta = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)*) out;

  EXPECT_FALSE(out->delete_delta);

  ProtobufCFieldReference fref1 = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref1, out, "interface");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref1));

  ASSERT_TRUE(delete_delta->interface[0]->base.delete_delta);
  ASSERT_FALSE(delete_delta->interface[0]->base.delete_delta->me);

  protobuf_c_field_ref_goto_proto_name(&fref1, &delete_delta->interface[0]->base, "ip");
  ASSERT_EQ(delete_delta->interface[0]->base.delete_delta->child_tag, fref1.fdesc->id);

  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref1));
  EXPECT_STREQ(delete_delta->interface[0]->ip[0].address, "10.0.1.0");

  EXPECT_TRUE(delete_delta->interface[0]->ip[0].base.delete_delta);
  EXPECT_TRUE(delete_delta->interface[0]->ip[0].base.delete_delta->me);
}

TEST(PbDelta, DeleteDeltaContainer)
{
  // 1. Create a registration ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony) reg_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony));

  reg_ks.dompath.path000.has_key00 = true;
  strcpy(reg_ks.dompath.path000.key00.name, "trafgen");

  // 2. Create a delete ks deep inside nc
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_InterfaceType) del_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_InterfaceType));

  del_ks.dompath.path000.has_key00 = true;
  strcpy(del_ks.dompath.path000.key00.name, "trafgen");

  del_ks.dompath.path001.has_key00 = true;
  strcpy(del_ks.dompath.path001.key00.name, "nc1");

  del_ks.dompath.path002.has_key00 = true;
  strcpy(del_ks.dompath.path002.key00.name, "int1");

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony));

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)* delete_delta =
      (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*) msg;
  ASSERT_EQ(delete_delta->n_network_context, 1);
  ASSERT_TRUE(delete_delta->network_context[0]);
  ASSERT_EQ(delete_delta->network_context[0]->n_interface, 1);
  ASSERT_TRUE(delete_delta->network_context[0]->interface);

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "network_context");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));
  EXPECT_STREQ(delete_delta->network_context[0]->name, "nc1");

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->network_context[0]->base, "interface");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));
  EXPECT_STREQ(delete_delta->network_context[0]->interface[0]->name, "int1");

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->network_context[0]->interface[0]->base, "interface_type");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_FALSE(protobuf_c_field_ref_is_field_present(&fref));

  char* json = nullptr;
  rw_json_pbcm_to_json(msg, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  // Test Dup of deltas
  ProtobufCMessage* out = protobuf_c_message_duplicate_allow_deltas(NULL, msg, msg->descriptor);
  ASSERT_TRUE(out);
  uptr.reset(out);

  json = nullptr;
  rw_json_pbcm_to_json(out, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  delete_delta = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*) out;
  EXPECT_FALSE(delete_delta->base.delete_delta);
  EXPECT_FALSE(delete_delta->network_context[0]->base.delete_delta);
  ASSERT_TRUE(delete_delta->network_context[0]->interface[0]->base.delete_delta);
  EXPECT_FALSE(delete_delta->network_context[0]->interface[0]->base.delete_delta->me);
  EXPECT_EQ(delete_delta->network_context[0]->interface[0]->base.delete_delta->child_tag, fref.fdesc->id);
}

TEST(PbDelta, DeleteDeltaLeaf)
{
  // 1. Create a registration ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony) reg_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony));

  reg_ks.dompath.path000.has_key00 = true;
  strcpy(reg_ks.dompath.path000.key00.name, "trafgen");

  // 2. Create a delete ks deep inside nc
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_InterfaceType)* del_ks = nullptr;
  rw_keyspec_path_create_dup(
      &(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_InterfaceType))->rw_keyspec_path_t,
      NULL,
      (rw_keyspec_path_t **)(&del_ks));
  ASSERT_TRUE(del_ks);

  rw_status_t s = rw_keyspec_path_create_leaf_entry(
      &del_ks->rw_keyspec_path_t, NULL, 
      (const rw_yang_pb_msgdesc_t *)RWPB_G_MSG_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_InterfaceType), "loopback");
  ASSERT_EQ(s, RW_STATUS_SUCCESS);

  UniquePtrKeySpecPath::uptr_t ksuptr(&del_ks->rw_keyspec_path_t);

  del_ks->dompath.path000.has_key00 = true;
  strcpy(del_ks->dompath.path000.key00.name, "trafgen");

  del_ks->dompath.path001.has_key00 = true;
  strcpy(del_ks->dompath.path001.key00.name, "nc1");

  del_ks->dompath.path002.has_key00 = true;
  strcpy(del_ks->dompath.path002.key00.name, "int1");

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks->rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony));

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)* delete_delta =
      (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*) msg;
  ASSERT_EQ(delete_delta->n_network_context, 1);
  ASSERT_TRUE(delete_delta->network_context[0]);
  ASSERT_EQ(delete_delta->network_context[0]->n_interface, 1);
  ASSERT_TRUE(delete_delta->network_context[0]->interface);

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "network_context");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));
  EXPECT_STREQ(delete_delta->network_context[0]->name, "nc1");

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->network_context[0]->base, "interface");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));
  EXPECT_STREQ(delete_delta->network_context[0]->interface[0]->name, "int1");

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->network_context[0]->interface[0]->base, "interface_type");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->network_context[0]->interface[0]->interface_type.base, "loopback");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_FALSE(protobuf_c_field_ref_is_field_present(&fref));

  char* json = nullptr;
  rw_json_pbcm_to_json(msg, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  // Test delta dup
  ProtobufCMessage *out = protobuf_c_message_duplicate_allow_deltas(NULL, msg, msg->descriptor);
  ASSERT_TRUE(out);
  uptr.reset(out);

  delete_delta = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*) out;
  EXPECT_FALSE(delete_delta->base.delete_delta);
  EXPECT_FALSE(delete_delta->network_context[0]->base.delete_delta);
  EXPECT_FALSE(delete_delta->network_context[0]->interface[0]->base.delete_delta);
  ASSERT_TRUE(delete_delta->network_context[0]->interface[0]->interface_type.base.delete_delta);
  EXPECT_FALSE(delete_delta->network_context[0]->interface[0]->interface_type.base.delete_delta->me);
  EXPECT_EQ(delete_delta->network_context[0]->interface[0]->interface_type.base.delete_delta->child_tag, fref.fdesc->id);
}

TEST(PbDelta, DeleteDeltaLeafList)
{
  // 1. Create a registration ks.
  RWPB_T_PATHSPEC(RwdtsDataD_data_Dts) reg_ks = *(RWPB_G_PATHSPEC_VALUE(RwdtsDataD_data_Dts));

  // 2. Create a delete ks deep inside nc
  RWPB_T_PATHSPEC(RwdtsDataD_data_Dts_Brokers_Stats_TopxMemberHisto) del_ks = *(RWPB_G_PATHSPEC_VALUE(RwdtsDataD_data_Dts_Brokers_Stats_TopxMemberHisto));

  del_ks.dompath.path003.has_key00 = true;
  del_ks.dompath.path003.key00.topx_member_histo = 1234;

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(RwdtsDataD_data_Dts));

  RWPB_T_MSG(RwdtsDataD_data_Dts)* delete_delta =
      (RWPB_T_MSG(RwdtsDataD_data_Dts)*) msg;

  ASSERT_EQ(delete_delta->n_brokers, 1);
  ASSERT_TRUE(delete_delta->brokers[0]);
  ASSERT_EQ(delete_delta->brokers[0]->n_stats, 1);
  ASSERT_TRUE(delete_delta->brokers[0]->stats);
  ASSERT_EQ(delete_delta->brokers[0]->stats[0]->n_topx_member_histo, 1);
  ASSERT_TRUE(delete_delta->brokers[0]->stats[0]->topx_member_histo);

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "brokers");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));

  EXPECT_TRUE(PROTOBUF_C_FIELD_REF_IS(&fref, delete_delta, brokers, PRESENT)); 

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->brokers[0]->base, "stats");
  EXPECT_FALSE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_TRUE(protobuf_c_field_ref_is_field_present(&fref));

  EXPECT_TRUE(PROTOBUF_C_FIELD_REF_IS(&fref, delete_delta->brokers[0], stats, PRESENT)); 

  protobuf_c_field_ref_goto_proto_name(&fref, &delete_delta->brokers[0]->stats[0]->base, "topx_member_histo");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));
  EXPECT_EQ(delete_delta->brokers[0]->stats[0]->topx_member_histo[0], 1234);

  EXPECT_TRUE(PROTOBUF_C_FIELD_REF_IS(&fref, delete_delta->brokers[0]->stats[0], topx_member_histo, DELETED));

  char* json = nullptr;
  rw_json_pbcm_to_json(msg, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  // Test delta dup
  ProtobufCMessage *out = protobuf_c_message_duplicate_allow_deltas(NULL, msg, msg->descriptor);
  ASSERT_TRUE(out);
  uptr.reset(out);

  json = nullptr;
  rw_json_pbcm_to_json(out, NULL, &json);
  std::cout << json << std::endl;
  free(json);
}

TEST(PbDelta, DeleteDeltaSelf)
{
  // 1. Create a registration ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext) reg_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext));

  reg_ks.dompath.path000.has_key00 = true;
  strcpy(reg_ks.dompath.path000.key00.name, "trafgen");

  // 2. Create a delete ks at same level as reg ks.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext) del_ks = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext));

  del_ks.dompath.path000.has_key00 = true;
  strcpy(del_ks.dompath.path000.key00.name, "trafgen");

  del_ks.dompath.path001.has_key00 = true;
  strcpy(del_ks.dompath.path001.key00.name, "nc1");

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext));

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* delete_delta =
      (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)*) msg;

  EXPECT_STREQ(delete_delta->name, "nc1");

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_whole_message(&fref, msg);
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));

  char* json = nullptr;
  rw_json_pbcm_to_json(msg, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  // Test delta dup
  ProtobufCMessage *out = protobuf_c_message_duplicate_allow_deltas(NULL, msg, msg->descriptor);
  ASSERT_TRUE(out);
  uptr.reset(out);

  json = nullptr;
  rw_json_pbcm_to_json(out, NULL, &json);
  std::cout << json << std::endl;
  free(json);

  ASSERT_TRUE(out->delete_delta);
  EXPECT_TRUE(out->delete_delta->me);
}

TEST(PbDelta, DeleteCTypeKey)
{
  //1. Create a registration ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeFlatList, reg_ks);

  //2. Create a delete ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeFlatList_Ip, del_ks);
  del_ks.dompath.path001.has_key00 = true;
  rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(&del_ks.dompath.path001.key00.ip, "192.168.1.0", strlen("192.168.1.0"));

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(CtypeTest_data_CtypeFlatList));

  RWPB_T_MSG(CtypeTest_data_CtypeFlatList) *delete_delta = 
      (RWPB_T_MSG(CtypeTest_data_CtypeFlatList)*) msg;

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "ip");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));

  EXPECT_EQ(delete_delta->n_ip, 1);
  EXPECT_FALSE(rw_ip_addr_cmp(&delete_delta->ip[0]->ip, &del_ks.dompath.path001.key00.ip));

  // BumpyList
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeBumpyList, reg_ks1);

  // 2. Delete ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeBumpyList_Ip, del_ks1);
  del_ks1.dompath.path001.has_key00 = true;

  rw_ip_addr_t local_ip = {};
  del_ks1.dompath.path001.key00.ip = &local_ip;
  rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(del_ks1.dompath.path001.key00.ip, "192.168.1.0", strlen("192.168.1.0"));

  msg = rw_keyspec_path_create_delete_delta(&del_ks1.rw_keyspec_path_t, NULL, &reg_ks1.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  uptr.reset(msg);

  RWPB_T_MSG(CtypeTest_data_CtypeBumpyList) *delete_delta1 = 
      (RWPB_T_MSG(CtypeTest_data_CtypeBumpyList)*) msg;

  protobuf_c_field_ref_goto_proto_name(&fref, msg, "ip");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));

  EXPECT_EQ(delete_delta1->n_ip, 1);
  EXPECT_FALSE(rw_ip_addr_cmp(delete_delta1->ip[0]->ip, del_ks1.dompath.path001.key00.ip));
}

TEST(PbDelta, DeleteCtypeField)
{
  // 1. Create a registration ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeBumpyLeaf, reg_ks);

  // 2. Create a delete ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeBumpyLeaf, del_ks);
  rw_status_t s = rw_keyspec_path_create_leaf_entry(&del_ks.rw_keyspec_path_t, NULL, 
                                                    (const rw_yang_pb_msgdesc_t *)RWPB_G_MSG_YPBCD(CtypeTest_data_CtypeBumpyLeaf), "ip");

  ASSERT_EQ(s, RW_STATUS_SUCCESS);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t ksuptr(&del_ks.rw_keyspec_path_t);

  ProtobufCMessage* msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(CtypeTest_data_CtypeBumpyLeaf));

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "ip");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));
}

TEST(PbDelta, DeleteCtypeLL)
{
  // 1. Create registration ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeBumpyLeafList, reg_ks);

  // 2. Create a delete ks.
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeBumpyLeafList_Ip, del_ks);
  del_ks.dompath.path001.has_key00 = true;

  rw_ip_addr_t local_ip = {};
  del_ks.dompath.path001.key00.ip = &local_ip;
  rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(del_ks.dompath.path001.key00.ip, "10.0.1.10", sizeof("10.0.1.10"));

  ProtobufCMessage *msg = rw_keyspec_path_create_delete_delta(&del_ks.rw_keyspec_path_t, NULL, &reg_ks.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  UniquePtrProtobufCMessage<>::uptr_t uptr(msg);

  ASSERT_EQ(msg->descriptor, RWPB_G_MSG_PBCMD(CtypeTest_data_CtypeBumpyLeafList));

  RWPB_T_MSG(CtypeTest_data_CtypeBumpyLeafList) *delete_delta = 
      (RWPB_T_MSG(CtypeTest_data_CtypeBumpyLeafList)*) msg;

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  protobuf_c_field_ref_goto_proto_name(&fref, msg, "ip");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));

  EXPECT_EQ(delete_delta->n_ip, 1);
  EXPECT_FALSE(rw_ip_addr_cmp(delete_delta->ip[0], del_ks.dompath.path001.key00.ip));

  //ctype-flat-leaf-list
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeFlatLeafList, reg_ks1);
  RWPB_M_PATHSPEC_DECL_INIT(CtypeTest_data_CtypeFlatLeafList_Ip, del_ks1);

  del_ks1.dompath.path001.has_key00 = true;
  rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(&del_ks1.dompath.path001.key00.ip, "172.13.2.1", sizeof("172.13.2.0"));

  msg = rw_keyspec_path_create_delete_delta(&del_ks1.rw_keyspec_path_t, NULL, &reg_ks1.rw_keyspec_path_t);
  ASSERT_TRUE(msg);
  uptr.reset(msg);

  RWPB_T_MSG(CtypeTest_data_CtypeFlatLeafList) *delete_delta1 = 
      (RWPB_T_MSG(CtypeTest_data_CtypeFlatLeafList)*) msg;

  protobuf_c_field_ref_goto_proto_name(&fref, msg, "ip");
  EXPECT_TRUE(protobuf_c_field_ref_is_field_deleted(&fref));

  EXPECT_EQ(delete_delta1->n_ip, 1);
  EXPECT_FALSE(rw_ip_addr_cmp(&delete_delta1->ip[0], &del_ks1.dompath.path001.key00.ip));
}
