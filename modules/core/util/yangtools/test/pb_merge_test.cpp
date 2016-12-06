
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
 */


/**
 * @file pb_merge_test.cpp
 * @author Sujithra Periasamy
 * @date 2015/04/28
 * @brief Google test cases for testing protobuf merge functionality
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include "rwut.h"
#include "rwlib.h"
#include "test-yang-desc.pb-c.h"
#include "enum.pb-c.h"
#include "pbcopy.pb-c.h"
#include <string.h>
#include <stdio.h>
#include "company.pb-c.h"
#include "testy2p-top1.pb-c.h"
#include "testy2p-top2.pb-c.h"
#include "rw-fpath-d.pb-c.h"
#include "rwmsg-data-d.pb-c.h"
#include "oneof_test.pb-c.h"
#include "test-tag-generation-base.pb-c.h"
#include "test-tag-generation.pb-c.h"
#include "test-ydom-top.pb-c.h"

static ProtobufCInstance* pinstance = nullptr;

static ProtobufCMessage* pack_merge_and_unpack_msgs(ProtobufCMessage* msg1, ProtobufCMessage* msg2)
{
  size_t size1 = 0;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, msg1, &size1);

  size_t size2 = 0;
  uint8_t* data2 = protobuf_c_message_serialize(NULL, msg2, &size2);

  uint8_t res[size1+size2];
  memcpy(res, data1, size1);
  memcpy(res+size1, data2, size2);

  ProtobufCMessage* out = protobuf_c_message_unpack(NULL, msg1->descriptor, size1+size2, res);
  free(data1);
  free(data2);
  return out;
}

static void init_comp_msg(RWPB_T_MSG(Company_data_Company)* msg, uint32_t nume, uint32_t nump) 
{
  msg->contact_info = (RWPB_T_MSG(Company_data_Company_ContactInfo) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_ContactInfo)));
  RWPB_F_MSG_INIT(Company_data_Company_ContactInfo,msg->contact_info); 
  msg->contact_info->name = strdup("abc"); 
  msg->contact_info->address = strdup("bglr"); 
  msg->contact_info->phone_number = strdup("1234");
  msg->n_employee = nume;
  msg->employee = (RWPB_T_MSG(Company_data_Company_Employee) **)calloc(nume, sizeof(void*));
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
  msg->n_product = nump;
  msg->product = (RWPB_T_MSG(Company_data_Company_Product) **)calloc(nump, sizeof(void*));
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

static void init_test_msg(RWPB_T_MSG(Testy2pTop1_data_Top1c2) *msg)
{
  msg->has_ls1 = true;
  strcpy(msg->ls1, "RiftIO");
  msg->has_ln1 = true;
  msg->ln1 = 20000;
  msg->has_ln2 = true;
  msg->ln2 = 500;
  msg->has_c1 = true;
  msg->c1.has_c1 = true;
  msg->c1.c1.has_ln1 = true;
  msg->c1.c1.ln1 = 10;
  msg->c1.c1.has_ln2 = true;
  msg->c1.c1.ln2 = 100;
  msg->n_l1 = 2;
  msg->l1[0].n_l1 = 2;
  msg->l1[0].lf1 = 14;
  msg->l1[0].has_lf1 = 1;
  strcpy(msg->l1[0].l1[0].ln1, "Key1");
  msg->l1[0].l1[0].has_ln1 = 1;
  msg->l1[0].l1[0].has_ln2 = true;
  msg->l1[0].l1[0].ln2 = 100;
  strcpy(msg->l1[0].l1[1].ln1, "Key2");
  msg->l1[0].l1[1].has_ln1 = 1;
  msg->l1[0].l1[1].has_ln2 = true;
  msg->l1[0].l1[1].ln2 = 200;
  msg->l1[1].n_l1 = 2;
  msg->l1[1].lf1 = 31;
  msg->l1[1].has_lf1 = 1;
  strcpy(msg->l1[1].l1[0].ln1, "Key3");
  msg->l1[1].l1[0].has_ln1 = 1;
  msg->l1[1].l1[0].has_ln2 = true;
  msg->l1[1].l1[0].ln2 = 300;
  strcpy(msg->l1[1].l1[1].ln1, "Key4");
  msg->l1[1].l1[1].has_ln1 = 1;
  msg->l1[1].l1[1].has_ln2 = true;
  msg->l1[1].l1[1].ln2 = 400;
}

static void init_multi_key_msg(RWPB_T_LISTONLY(Testy2pTop2_data_Topl) *msg)
{
  msg->n_topl = 2; 
  msg->topl = (RWPB_T_MSG(Testy2pTop2_data_Topl)**)calloc(2, sizeof(void*)); 
  msg->topl[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl))); 
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg->topl[0]); 
  msg->topl[0]->k0 = strdup("Key1"); 
  msg->topl[0]->n_la = 2; 
  msg->topl[0]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*)); 
  msg->topl[0]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La))); 
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg->topl[0]->la[0]); 
  msg->topl[0]->la[0]->k1 = strdup("SubKey1"); 
  msg->topl[0]->la[0]->k2 = 100;
  msg->topl[0]->la[0]->has_k2 = 1;
  msg->topl[0]->la[0]->k3 = strdup("SubKey2");
  msg->topl[0]->la[0]->n_lb = 2;
  msg->topl[0]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*)); 
  msg->topl[0]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[0]->la[0]->lb[0]); 
  msg->topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1");
  msg->topl[0]->la[0]->lb[0]->k5 = 500;
  msg->topl[0]->la[0]->lb[0]->has_k5 = 1;
  msg->topl[0]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[0]->la[0]->lb[1]); 
  msg->topl[0]->la[0]->lb[1]->k4 = strdup("InnerKey2");
  msg->topl[0]->la[0]->lb[1]->k5 = 600;
  msg->topl[0]->la[0]->lb[1]->has_k5 = 1;
  msg->topl[0]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg->topl[0]->la[1]); 
  msg->topl[0]->la[1]->k1 = strdup("SubKey3");
  msg->topl[0]->la[1]->k2 = 200;
  msg->topl[0]->la[1]->has_k2 = 1;
  msg->topl[0]->la[1]->k3 = strdup("SubKey4");
  msg->topl[0]->la[1]->n_lb = 2;
  msg->topl[0]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg->topl[0]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[0]->la[1]->lb[0]); 
  msg->topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3");
  msg->topl[0]->la[1]->lb[0]->k5 = 700;
  msg->topl[0]->la[1]->lb[0]->has_k5 = 1;
  msg->topl[0]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[0]->la[1]->lb[1]); 
  msg->topl[0]->la[1]->lb[1]->k4 = strdup("InnerKey4");
  msg->topl[0]->la[1]->lb[1]->k5 = 800;
  msg->topl[0]->la[1]->lb[1]->has_k5 = 1;
  msg->topl[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg->topl[1]); 
  msg->topl[1]->k0 = strdup("Key2");
  msg->topl[1]->n_la = 2;
  msg->topl[1]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg->topl[1]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg->topl[1]->la[0]); 
  msg->topl[1]->la[0]->k1 = strdup("SubKey5");
  msg->topl[1]->la[0]->k2 = 300;
  msg->topl[1]->la[0]->has_k2 = 1;
  msg->topl[1]->la[0]->k3 = strdup("SubKey6");
  msg->topl[1]->la[0]->n_lb = 2;
  msg->topl[1]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg->topl[1]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[1]->la[0]->lb[0]); 
  msg->topl[1]->la[0]->lb[0]->k4 = strdup("InnerKey5");
  msg->topl[1]->la[0]->lb[0]->k5 = 900;
  msg->topl[1]->la[0]->lb[0]->has_k5 = 1;
  msg->topl[1]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[1]->la[0]->lb[1]); 
  msg->topl[1]->la[0]->lb[1]->k4 = strdup("InnerKey6");
  msg->topl[1]->la[0]->lb[1]->k5 = 1000;
  msg->topl[1]->la[0]->lb[1]->has_k5 = 1;
  msg->topl[1]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg->topl[1]->la[1]); 
  msg->topl[1]->la[1]->k1 = strdup("SubKey7");
  msg->topl[1]->la[1]->k2 = 400;
  msg->topl[1]->la[1]->has_k2 = 1;
  msg->topl[1]->la[1]->k3 = strdup("SubKey8");
  msg->topl[1]->la[1]->n_lb = 2;
  msg->topl[1]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg->topl[1]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[1]->la[1]->lb[0]); 
  msg->topl[1]->la[1]->lb[0]->k4 = strdup("InnerKey7");
  msg->topl[1]->la[1]->lb[0]->k5 = 1100;
  msg->topl[1]->la[1]->lb[0]->has_k5 = 1;
  msg->topl[1]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg->topl[1]->la[1]->lb[1]); 
  msg->topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8");
  msg->topl[1]->la[1]->lb[1]->k5 = 1200;
  msg->topl[1]->la[1]->lb[1]->has_k5 = 1;
}

static void init_nc_msgs(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* nc1,
                         RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* nc2)
{
  strcpy(nc1->name, "trafgen");
  nc1->n_interface = 2;
  nc1->interface = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)**)malloc(2*sizeof(void*));

  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if1);
  strcpy(if1->name, "if1");
  if1->has_name = 1;
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if2);
  strcpy(if2->name, "if2");
  if2->has_name = 1;

  nc1->interface[0] = if1;
  nc1->interface[1] = if2;

  nc1->n_load_balancer = 2;
  strcpy(nc1->load_balancer[0].name, "loadbl1");
  strcpy(nc1->load_balancer[1].name, "loadbl2");
  nc1->load_balancer[0].has_name = 1;
  nc1->load_balancer[1].has_name = 1;

  nc1->n_ip_receiver_application = 2;
  strcpy(nc1->ip_receiver_application[0].name, "iprecvapp1");
  strcpy(nc1->ip_receiver_application[1].name, "iprecvapp2");
  nc1-> ip_receiver_application[0].has_name = 1;
  nc1->ip_receiver_application[1].has_name = 1;

  strcpy(nc2->name, "trafgen");
  nc2->has_name = 1;
  nc2->n_interface = 2;
  nc2->interface = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)**)malloc(2*sizeof(void*));

  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if3);
  strcpy(if3->name, "if3");
  if3->has_name = 1;
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if4);
  strcpy(if4->name, "if4");
  if4->has_name = 1;

  nc2->interface[0] = if3;
  nc2->interface[1] = if4;

  nc2->n_lb_profile = 2;
  strcpy(nc2->lb_profile[0].name, "lbpr3");
  strcpy(nc2->lb_profile[1].name, "lbpr4");
  nc2->lb_profile[0].has_name = 1;
  nc2->lb_profile[1].has_name = 1;

  nc2->n_scriptable_lb = 1;
  strcpy(nc2->scriptable_lb[0].name, "slb1");
  nc2->scriptable_lb[0].has_name = 1;
}

TEST(RWProtobuf, MergeMsgsPointy)
{
  TEST_DESCRIPTION("Test merge protobuf messages");

  RWPB_T_MSG(Company_data_Company) msg1;
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);

  init_comp_msg(&msg1, 2, 2);

  RWPB_M_MSG_DECL_INIT(Company_data_Company, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  RWPB_T_MSG(Company_data_Company)* out = 
      (RWPB_T_MSG(Company_data_Company)*)pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t result(&out->base);

  ASSERT_TRUE(out->contact_info);
  ASSERT_TRUE(out->contact_info->name);
  EXPECT_FALSE(strcmp(out->contact_info->name, "abc"));
  ASSERT_TRUE(out->contact_info->address);
  EXPECT_FALSE(strcmp(out->contact_info->address, "bglr"));
  ASSERT_TRUE(out->contact_info->phone_number);
  EXPECT_FALSE(strcmp(out->contact_info->phone_number, "1234"));
  ASSERT_EQ(out->n_employee, 2);
  ASSERT_TRUE(out->employee);
  ASSERT_EQ(out->n_product, 2);
  EXPECT_TRUE(out->product);

  ASSERT_TRUE(out->employee[0]);
  EXPECT_EQ(out->employee[0]->id, 100);
  EXPECT_FALSE(strcmp(out->employee[0]->name, "Emp1"));
  EXPECT_FALSE(strcmp(out->employee[0]->title, "Title"));
  EXPECT_FALSE(strcmp(out->employee[0]->start_date, "1234"));

  ASSERT_TRUE(out->employee[1]);
  EXPECT_EQ(out->employee[1]->id, 200);
  EXPECT_FALSE(strcmp(out->employee[1]->name, "Emp2"));
  EXPECT_FALSE(strcmp(out->employee[1]->title, "Title"));
  EXPECT_FALSE(strcmp(out->employee[1]->start_date, "1234"));

  ASSERT_TRUE(out->product[0]);
  EXPECT_EQ(out->product[0]->id, 1000);
  EXPECT_FALSE(strcmp(out->product[0]->name, "nfv"));
  EXPECT_FALSE(strcmp(out->product[0]->msrp, "1000"));

  ASSERT_TRUE(out->product[1]);
  EXPECT_EQ(out->product[1]->id, 2000);
  EXPECT_FALSE(strcmp(out->product[1]->name, "cloud"));
  EXPECT_FALSE(strcmp(out->product[1]->msrp, "2000"));

  uptr1.reset();
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);

  init_comp_msg(&msg1, 3, 2);
  uptr1.reset(&msg1.base);

  msg1.n_employee = 3;
  msg1.employee[2] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg1.employee[2]);
  msg1.employee[2]->id = 300;
  msg1.employee[2]->has_id = 1;
  msg1.employee[2]->name = strdup("Emp3");
  msg1.employee[2]->title = strdup("SWE");
  msg1.employee[2]->start_date = strdup("1431");

  uptr2.reset();
  RWPB_F_MSG_INIT(Company_data_Company, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_employee = 3;
  msg2.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)calloc(3, sizeof(void*));
  msg2.employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[0]);
  msg2.employee[0]->id = 100;
  msg2.employee[0]->has_id = 1;
  msg2.employee[0]->name = strdup("Emp3");
  msg2.employee[0]->title = strdup("Title");

  msg2.employee[1] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[1]);
  msg2.employee[1]->id = 200;
  msg2.employee[1]->has_id = 1;

  msg2.employee[2] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[2]);
  msg2.employee[2]->id = 800;
  msg2.employee[2]->has_id = 1;

  out = (RWPB_T_MSG(Company_data_Company) *)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);

  ASSERT_TRUE(out);
  result.reset(&out->base);

  ASSERT_EQ((out)->n_employee, 4);
  EXPECT_EQ((out)->employee[0]->id, 100);
  EXPECT_FALSE(strcmp((out)->employee[0]->name, "Emp3"));
  EXPECT_FALSE(strcmp((out)->employee[0]->title, "Title"));
  EXPECT_TRUE((out)->employee[0]->start_date);
  EXPECT_FALSE(strcmp((out)->employee[0]->start_date, "1234"));
  EXPECT_EQ((out)->employee[1]->id, 200);
  EXPECT_TRUE((out)->employee[1]->name);
  EXPECT_FALSE(strcmp((out)->employee[1]->name, "Emp2"));
  EXPECT_TRUE((out)->employee[1]->title);
  EXPECT_FALSE(strcmp((out)->employee[1]->title, "Title"));
  EXPECT_TRUE((out)->employee[1]->start_date);
  EXPECT_FALSE(strcmp((out)->employee[1]->start_date, "1234"));
  EXPECT_EQ((out)->employee[3]->id, 800);
  EXPECT_FALSE((out)->employee[3]->name);
  EXPECT_FALSE((out)->employee[3]->title);
  EXPECT_FALSE((out)->employee[3]->start_date);
  EXPECT_EQ((out)->employee[2]->id, 300);
  EXPECT_FALSE(strcmp((out)->employee[2]->name, "Emp3"));
  EXPECT_FALSE(strcmp((out)->employee[2]->title, "SWE"));
  EXPECT_FALSE(strcmp((out)->employee[2]->start_date, "1431"));
  ASSERT_EQ((out)->n_product, 2);
  EXPECT_EQ((out)->product[0]->id, 1000);
  EXPECT_FALSE(strcmp((out)->product[0]->name, "nfv"));
  EXPECT_FALSE(strcmp((out)->product[0]->msrp, "1000"));
  EXPECT_EQ((out)->product[1]->id, 2000);
  EXPECT_FALSE(strcmp((out)->product[1]->name, "cloud"));
  EXPECT_FALSE(strcmp((out)->product[1]->msrp, "2000"));
}

TEST(RWProtobuf, MergeMsgsInline)
{
  // Inline repeated messages.
  RWPB_T_MSG(Testy2pTop1_data_Top1c2) msg1;
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);

  init_test_msg(&msg1);

  RWPB_M_MSG_DECL_INIT(Testy2pTop1_data_Top1c2, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  auto out = (RWPB_T_MSG(Testy2pTop1_data_Top1c2)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t uptr3(&out->base);

  EXPECT_TRUE((out)->has_ls1);
  EXPECT_FALSE(strcmp((out)->ls1, "RiftIO"));
  EXPECT_TRUE((out)->has_ln1);
  EXPECT_EQ((out)->ln1, 20000);
  EXPECT_TRUE((out)->has_ln2);
  EXPECT_EQ((out)->ln2, 500);
  EXPECT_TRUE((out)->has_c1);
  EXPECT_TRUE((out)->c1.has_c1);
  EXPECT_TRUE((out)->c1.c1.has_ln1);
  EXPECT_EQ((out)->c1.c1.ln1, 10);
  EXPECT_TRUE((out)->c1.c1.has_ln2);
  EXPECT_EQ((out)->c1.c1.ln2, 100);
  ASSERT_EQ((out)->n_l1, 2);
  ASSERT_EQ((out)->l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ((out)->l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp((out)->l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ((out)->l1[0].l1[1].ln2, 200);
  ASSERT_EQ((out)->l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ((out)->l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp((out)->l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ((out)->l1[1].l1[1].ln2, 400);

  uptr1.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);
  init_test_msg(&msg1);
  uptr1.reset(&msg1.base);

  uptr2.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg2);
  uptr2.reset(&msg2.base);

  msg2.has_ls1 = true;
  strcpy(msg2.ls1, "Super RiftIO");
  msg2.has_c1 = true;
  msg2.c1.has_c1 = true;
  msg2.c1.c1.has_ln1 = true;
  msg2.c1.c1.ln1 = 20;
  msg2.c1.c1.has_ln2 = true;
  msg2.c1.c1.ln2 = 200;

  msg2.n_l1 = 2;
  msg2.l1[0].lf1 = 14;
  msg2.l1[0].has_lf1 = 1;
  msg2.l1[0].n_l1 = 2;
  strcpy(msg2.l1[0].l1[0].ln1, "Key1");
  msg2.l1[0].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[0].l1[1].ln1, "Key2");
  msg2.l1[0].l1[1].has_ln1 = 1;

  msg2.l1[1].lf1 = 31;
  msg2.l1[1].has_lf1 = 1;
  msg2.l1[1].n_l1 = 2;
  strcpy(msg2.l1[1].l1[0].ln1, "Key3");
  msg2.l1[1].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[1].l1[1].ln1, "Key4");
  msg2.l1[1].l1[1].has_ln1 = 1;

  out = (RWPB_T_MSG(Testy2pTop1_data_Top1c2)*)pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  uptr3.reset(&out->base);

  EXPECT_TRUE((out)->has_ls1);
  EXPECT_FALSE(strcmp((out)->ls1, "Super RiftIO"));
  EXPECT_TRUE((out)->has_ln1);
  EXPECT_EQ((out)->ln1, 20000);
  EXPECT_TRUE((out)->has_ln2);
  EXPECT_EQ((out)->ln2, 500);
  EXPECT_TRUE((out)->has_c1);
  EXPECT_TRUE((out)->c1.has_c1);
  EXPECT_TRUE((out)->c1.c1.has_ln1);
  EXPECT_EQ((out)->c1.c1.ln1, 20);
  EXPECT_TRUE((out)->c1.c1.has_ln2);
  EXPECT_EQ((out)->c1.c1.ln2, 200);
  ASSERT_EQ((out)->n_l1, 2);
  ASSERT_EQ((out)->l1[0].n_l1, 2); 
  EXPECT_FALSE(strcmp((out)->l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ((out)->l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp((out)->l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ((out)->l1[0].l1[1].ln2, 200);
  ASSERT_EQ((out)->l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ((out)->l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp((out)->l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ((out)->l1[1].l1[1].ln2, 400);

  uptr1.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);
  init_test_msg(&msg1);
  uptr1.reset(&msg1.base);

  uptr2.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg2);
  uptr2.reset(&msg2.base);

  msg2.has_ls1 = true;
  strcpy(msg2.ls1, "Super RiftIO");
  msg2.has_c1 = true;
  msg2.c1.has_c1 = true;
  msg2.c1.c1.has_ln1 = true;
  msg2.c1.c1.ln1 = 20;
  msg2.c1.c1.has_ln2 = true;
  msg2.c1.c1.ln2 = 200;

  msg2.n_l1 = 3;
  msg2.l1[0].lf1 = 14;
  msg2.l1[0].has_lf1 = 1;
  msg2.l1[0].n_l1 = 2;
  strcpy(msg2.l1[0].l1[0].ln1, "Key1");
  msg2.l1[0].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[0].l1[1].ln1, "Key2");
  msg2.l1[0].l1[1].has_ln1 = 1;

  msg2.l1[1].lf1 = 9;
  msg2.l1[1].has_lf1 = 1;
  msg2.l1[1].n_l1 = 2;
  strcpy(msg2.l1[1].l1[0].ln1, "Key3");
  msg2.l1[1].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[1].l1[1].ln1, "Key4");
  msg2.l1[1].l1[1].has_ln1 = 1;

  msg2.l1[2].lf1 = 10;
  msg2.l1[2].has_lf1 = 1;
  msg2.l1[2].n_l1 = 2;
  strcpy(msg2.l1[2].l1[0].ln1, "Key5");
  msg2.l1[2].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[2].l1[1].ln1, "Key6");
  msg2.l1[2].l1[1].has_ln1 = 1;

  out = (RWPB_T_MSG(Testy2pTop1_data_Top1c2)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  uptr3.reset(&out->base);

  EXPECT_TRUE((out)->has_ls1);
  EXPECT_FALSE(strcmp((out)->ls1, "Super RiftIO"));
  EXPECT_TRUE((out)->has_ln1);
  EXPECT_EQ((out)->ln1, 20000);
  EXPECT_TRUE((out)->has_ln2);
  EXPECT_EQ((out)->ln2, 500);
  EXPECT_TRUE((out)->has_c1);
  EXPECT_TRUE((out)->c1.has_c1);
  EXPECT_TRUE((out)->c1.c1.has_ln1);
  EXPECT_EQ((out)->c1.c1.ln1, 20);
  EXPECT_TRUE((out)->c1.c1.has_ln2);
  EXPECT_EQ((out)->c1.c1.ln2, 200);
  ASSERT_EQ((out)->n_l1, 4);
  EXPECT_EQ((out)->l1[0].lf1, 14);
  ASSERT_EQ((out)->l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ((out)->l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp((out)->l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ((out)->l1[0].l1[1].ln2, 200);
  EXPECT_EQ((out)->l1[2].lf1, 9);
  ASSERT_EQ((out)->l1[2].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[2].l1[0].ln1, "Key3"));
  EXPECT_FALSE((out)->l1[2].l1[0].has_ln2);
  EXPECT_FALSE(strcmp((out)->l1[2].l1[1].ln1, "Key4"));
  EXPECT_FALSE((out)->l1[2].l1[1].has_ln2);
  EXPECT_EQ((out)->l1[3].lf1, 10);
  ASSERT_EQ((out)->l1[3].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[3].l1[0].ln1, "Key5"));
  EXPECT_FALSE((out)->l1[3].l1[0].has_ln2);
  EXPECT_FALSE(strcmp((out)->l1[3].l1[1].ln1, "Key6"));
  EXPECT_FALSE((out)->l1[3].l1[1].has_ln2);
  EXPECT_EQ((out)->l1[1].lf1, 31);
  ASSERT_EQ((out)->l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp((out)->l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ((out)->l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp((out)->l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ((out)->l1[1].l1[1].ln2, 400);
}

TEST(RWProtobuf, MergeMsgsMKeys)
{
  // Multiple keys.
  RWPB_T_LISTONLY(Testy2pTop2_data_Topl) msg1;
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg1);

  init_multi_key_msg(&msg1);

  RWPB_M_LISTONLY_DECL_INIT(Testy2pTop2_data_Topl, msg2);

  auto out = (RWPB_T_LISTONLY(Testy2pTop2_data_Topl)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);
  UniquePtrProtobufCMessage<>::uptr_t uptr3(&out->base);

  ASSERT_EQ((out)->n_topl, 2); 
  EXPECT_TRUE((out)->topl); 
  EXPECT_TRUE((out)->topl[0]); 
  EXPECT_TRUE((out)->topl[0]->k0); 
  EXPECT_FALSE(strcmp((out)->topl[0]->k0,"Key1")); 
  ASSERT_EQ((out)->topl[0]->n_la, 2); 
  EXPECT_TRUE((out)->topl[0]->la); 
  EXPECT_TRUE((out)->topl[0]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->k1, "SubKey1")); 
  EXPECT_EQ((out)->topl[0]->la[0]->k2, 100); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->k3, "SubKey2")); 
  ASSERT_EQ((out)->topl[0]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->lb[0]->k4, "InnerKey1")); 
  EXPECT_EQ((out)->topl[0]->la[0]->lb[0]->k5, 500); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->lb[1]->k4, "InnerKey2")); 
  EXPECT_EQ((out)->topl[0]->la[0]->lb[1]->k5, 600); 
  EXPECT_TRUE((out)->topl[0]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->k1, "SubKey3")); 
  EXPECT_EQ((out)->topl[0]->la[1]->k2, 200); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->k3, "SubKey4")); 
  ASSERT_EQ((out)->topl[0]->la[1]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->lb[0]->k4, "InnerKey3")); 
  EXPECT_EQ((out)->topl[0]->la[1]->lb[0]->k5, 700); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->lb[1]->k4, "InnerKey4")); 
  EXPECT_EQ((out)->topl[0]->la[1]->lb[1]->k5, 800); 
  EXPECT_TRUE((out)->topl[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->k0, "Key2")); 
  ASSERT_EQ((out)->topl[1]->n_la, 2); 
  EXPECT_TRUE((out)->topl[1]->la); 
  EXPECT_TRUE((out)->topl[1]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->k1, "SubKey5")); 
  EXPECT_EQ((out)->topl[1]->la[0]->k2, 300); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->k3, "SubKey6")); 
  ASSERT_EQ((out)->topl[1]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->lb[0]->k4, "InnerKey5")); 
  EXPECT_EQ((out)->topl[1]->la[0]->lb[0]->k5, 900); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->lb[1]->k4, "InnerKey6")); 
  EXPECT_EQ((out)->topl[1]->la[0]->lb[1]->k5, 1000); 
  EXPECT_TRUE((out)->topl[1]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->k1,"SubKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->k2, 400); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->k3, "SubKey8")); 
  ASSERT_EQ((out)->topl[1]->la[1]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[0]->k4, "InnerKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[0]->k5, 1100); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[1]->k4, "InnerKey8")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[1]->k5, 1200);

  uptr1.reset();
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg1);
  init_multi_key_msg(&msg1);
  uptr1.reset(&msg1.base);

  uptr2.reset();
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_topl = 2;
  msg2.topl = (RWPB_T_MSG(Testy2pTop2_data_Topl)**)calloc(2, sizeof(void*));
  msg2.topl[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[0]);
  msg2.topl[0]->k0 = strdup("Key1");
  msg2.topl[0]->n_la = 2;
  msg2.topl[0]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[0]);
  msg2.topl[0]->la[0]->k1 = strdup("SubKey1");
  msg2.topl[0]->la[0]->k2 = 100;
  msg2.topl[0]->la[0]->has_k2 = 1;
  msg2.topl[0]->la[0]->k3 = strdup("SubKey2");
  msg2.topl[0]->la[0]->n_lb = 2;
  msg2.topl[0]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[0]);
  msg2.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1");
  msg2.topl[0]->la[0]->lb[0]->k5 = 500;
  msg2.topl[0]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[1]);
  msg2.topl[0]->la[0]->lb[1]->k4 = strdup("InnerKey2");
  msg2.topl[0]->la[0]->lb[1]->k5 = 600;
  msg2.topl[0]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[0]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[1]);
  msg2.topl[0]->la[1]->k1 = strdup("SubKey3");
  msg2.topl[0]->la[1]->k2 = 200;
  msg2.topl[0]->la[1]->has_k2 = 1;
  msg2.topl[0]->la[1]->k3 = strdup("SubKey4");
  msg2.topl[0]->la[1]->n_lb = 2;
  msg2.topl[0]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[0]);
  msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3");
  msg2.topl[0]->la[1]->lb[0]->k5 = 700;
  msg2.topl[0]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[1]);
  msg2.topl[0]->la[1]->lb[1]->k4 = strdup("InnerKey4");
  msg2.topl[0]->la[1]->lb[1]->k5 = 800;
  msg2.topl[0]->la[1]->lb[1]->has_k5 = 1;

  msg2.topl[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[1]);
  msg2.topl[1]->k0 = strdup("Key2");
  msg2.topl[1]->n_la = 2;
  msg2.topl[1]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[0]);
  msg2.topl[1]->la[0]->k1 = strdup("SubKey5");
  msg2.topl[1]->la[0]->k2 = 300;
  msg2.topl[1]->la[0]->has_k2 = 1;
  msg2.topl[1]->la[0]->k3 = strdup("SubKey6");
  msg2.topl[1]->la[0]->n_lb = 2;
  msg2.topl[1]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[0]);
  msg2.topl[1]->la[0]->lb[0]->k4 = strdup("InnerKey5");
  msg2.topl[1]->la[0]->lb[0]->k5 = 900;
  msg2.topl[1]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[1]);
  msg2.topl[1]->la[0]->lb[1]->k4 = strdup("InnerKey6");
  msg2.topl[1]->la[0]->lb[1]->k5 = 1000;
  msg2.topl[1]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[1]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[1]);
  msg2.topl[1]->la[1]->k1 = strdup("SubKey7");
  msg2.topl[1]->la[1]->k2 = 400;
  msg2.topl[1]->la[1]->has_k2 = 1;
  msg2.topl[1]->la[1]->k3 = strdup("SubKey8");
  msg2.topl[1]->la[1]->n_lb = 2;
  msg2.topl[1]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[0]);
  msg2.topl[1]->la[1]->lb[0]->k4 = strdup("InnerKey7");
  msg2.topl[1]->la[1]->lb[0]->k5 = 1100;
  msg2.topl[1]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[1]);
  msg2.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8");
  msg2.topl[1]->la[1]->lb[1]->k5 = 1200;
  msg2.topl[1]->la[1]->lb[1]->has_k5 = 1;

  out = (RWPB_T_LISTONLY(Testy2pTop2_data_Topl)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  uptr3.reset(&out->base);

  ASSERT_EQ((out)->n_topl, 2); 
  EXPECT_TRUE((out)->topl); 
  EXPECT_TRUE((out)->topl[0]); 
  EXPECT_TRUE((out)->topl[0]->k0); 
  EXPECT_FALSE(strcmp((out)->topl[0]->k0,"Key1")); 
  ASSERT_EQ((out)->topl[0]->n_la, 2); 
  EXPECT_TRUE((out)->topl[0]->la); 
  EXPECT_TRUE((out)->topl[0]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->k1, "SubKey1")); 
  EXPECT_EQ((out)->topl[0]->la[0]->k2, 100); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->k3, "SubKey2")); 
  ASSERT_EQ((out)->topl[0]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->lb[0]->k4, "InnerKey1")); 
  EXPECT_EQ((out)->topl[0]->la[0]->lb[0]->k5, 500); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->lb[1]->k4, "InnerKey2")); 
  EXPECT_EQ((out)->topl[0]->la[0]->lb[1]->k5, 600); 
  EXPECT_TRUE((out)->topl[0]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->k1, "SubKey3")); 
  EXPECT_EQ((out)->topl[0]->la[1]->k2, 200); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->k3, "SubKey4")); 
  ASSERT_EQ((out)->topl[0]->la[1]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->lb[0]->k4, "InnerKey3")); 
  EXPECT_EQ((out)->topl[0]->la[1]->lb[0]->k5, 700); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->lb[1]->k4, "InnerKey4")); 
  EXPECT_EQ((out)->topl[0]->la[1]->lb[1]->k5, 800); 
  EXPECT_TRUE((out)->topl[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->k0, "Key2")); 
  ASSERT_EQ((out)->topl[1]->n_la, 2); 
  EXPECT_TRUE((out)->topl[1]->la); 
  EXPECT_TRUE((out)->topl[1]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->k1, "SubKey5")); 
  EXPECT_EQ((out)->topl[1]->la[0]->k2, 300); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->k3, "SubKey6")); 
  ASSERT_EQ((out)->topl[1]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->lb[0]->k4, "InnerKey5")); 
  EXPECT_EQ((out)->topl[1]->la[0]->lb[0]->k5, 900); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->lb[1]->k4, "InnerKey6")); 
  EXPECT_EQ((out)->topl[1]->la[0]->lb[1]->k5, 1000); 
  EXPECT_TRUE((out)->topl[1]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->k1,"SubKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->k2, 400); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->k3, "SubKey8")); 
  ASSERT_EQ((out)->topl[1]->la[1]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[0]->k4, "InnerKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[0]->k5, 1100); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[1]->k4, "InnerKey8")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[1]->k5, 1200); 

  uptr1.reset();
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg1);
  init_multi_key_msg(&msg1);
  uptr1.reset(&msg1.base);

  uptr2.reset();
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_topl = 2;
  msg2.topl = (RWPB_T_MSG(Testy2pTop2_data_Topl)**)calloc(2, sizeof(void*));
  msg2.topl[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[0]);
  msg2.topl[0]->k0 = strdup("NewKey1");
  msg2.topl[0]->n_la = 2;
  msg2.topl[0]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[0]);
  msg2.topl[0]->la[0]->k1 = strdup("SubKey1");
  msg2.topl[0]->la[0]->k2 = 100;
  msg2.topl[0]->la[0]->has_k2 = 1;
  msg2.topl[0]->la[0]->k3 = strdup("SubKey2");
  msg2.topl[0]->la[0]->n_lb = 2;
  msg2.topl[0]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[0]);
  msg2.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1");
  msg2.topl[0]->la[0]->lb[0]->k5 = 500;
  msg2.topl[0]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[1]);
  msg2.topl[0]->la[0]->lb[1]->k4 = strdup("InnerKey2");
  msg2.topl[0]->la[0]->lb[1]->k5 = 600;
  msg2.topl[0]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[0]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[1]);
  msg2.topl[0]->la[1]->k1 = strdup("SubKey3");
  msg2.topl[0]->la[1]->k2 = 200;
  msg2.topl[0]->la[1]->has_k2 = 1;
  msg2.topl[0]->la[1]->k3 = strdup("SubKey4");
  msg2.topl[0]->la[1]->n_lb = 2;
  msg2.topl[0]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[0]);
  msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3");
  msg2.topl[0]->la[1]->lb[0]->k5 = 700;
  msg2.topl[0]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[1]);
  msg2.topl[0]->la[1]->lb[1]->k4 = strdup("InnerKey4");
  msg2.topl[0]->la[1]->lb[1]->k5 = 800;
  msg2.topl[0]->la[1]->lb[1]->has_k5 = 1;

  msg2.topl[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[1]);
  msg2.topl[1]->k0 = strdup("Key2");
  msg2.topl[1]->n_la = 2;
  msg2.topl[1]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[0]);
  msg2.topl[1]->la[0]->k1 = strdup("NewSubKey5");
  msg2.topl[1]->la[0]->k2 = 300;
  msg2.topl[1]->la[0]->has_k2 = 1;
  msg2.topl[1]->la[0]->k3 = strdup("SubKey6");
  msg2.topl[1]->la[0]->n_lb = 2;
  msg2.topl[1]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[0]);
  msg2.topl[1]->la[0]->lb[0]->k4 = strdup("InnerKey5");
  msg2.topl[1]->la[0]->lb[0]->k5 = 900;
  msg2.topl[1]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[1]);
  msg2.topl[1]->la[0]->lb[1]->k4 = strdup("InnerKey6");
  msg2.topl[1]->la[0]->lb[1]->k5 = 1000;
  msg2.topl[1]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[1]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[1]);
  msg2.topl[1]->la[1]->k1 = strdup("SubKey7");
  msg2.topl[1]->la[1]->k2 = 400;
  msg2.topl[1]->la[1]->has_k2 = 1;
  msg2.topl[1]->la[1]->k3 = strdup("SubKey8");
  msg2.topl[1]->la[1]->n_lb = 2;
  msg2.topl[1]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[0]);
  msg2.topl[1]->la[1]->lb[0]->k4 = strdup("NewInnerKey7");
  msg2.topl[1]->la[1]->lb[0]->k5 = 1100;
  msg2.topl[1]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[1]);
  msg2.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8");
  msg2.topl[1]->la[1]->lb[1]->k5 = 1200;
  msg2.topl[1]->la[1]->lb[1]->has_k5 = 1;

  out = (RWPB_T_LISTONLY(Testy2pTop2_data_Topl)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  uptr3.reset(&out->base);

  ASSERT_EQ((out)->n_topl, 3); 
  EXPECT_TRUE((out)->topl); 
  EXPECT_TRUE((out)->topl[2]); 
  EXPECT_TRUE((out)->topl[2]->k0); 
  EXPECT_FALSE(strcmp((out)->topl[2]->k0,"NewKey1")); 
  ASSERT_EQ((out)->topl[2]->n_la, 2); 
  EXPECT_TRUE((out)->topl[2]->la); 
  EXPECT_TRUE((out)->topl[2]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[0]->k1, "SubKey1")); 
  EXPECT_EQ((out)->topl[2]->la[0]->k2, 100); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[0]->k3, "SubKey2")); 
  ASSERT_EQ((out)->topl[2]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[2]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[2]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[0]->lb[0]->k4, "InnerKey1")); 
  EXPECT_EQ((out)->topl[2]->la[0]->lb[0]->k5, 500); 
  EXPECT_TRUE((out)->topl[2]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[0]->lb[1]->k4, "InnerKey2")); 
  EXPECT_EQ((out)->topl[2]->la[0]->lb[1]->k5, 600); 
  EXPECT_TRUE((out)->topl[2]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[1]->k1, "SubKey3")); 
  EXPECT_EQ((out)->topl[2]->la[1]->k2, 200); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[1]->k3, "SubKey4")); 
  ASSERT_EQ((out)->topl[2]->la[1]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[2]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[2]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[1]->lb[0]->k4, "InnerKey3")); 
  EXPECT_EQ((out)->topl[2]->la[1]->lb[0]->k5, 700); 
  EXPECT_TRUE((out)->topl[2]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[2]->la[1]->lb[1]->k4, "InnerKey4")); 
  EXPECT_EQ((out)->topl[2]->la[1]->lb[1]->k5, 800); 
  EXPECT_TRUE((out)->topl[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->k0, "Key2")); 
  ASSERT_EQ((out)->topl[1]->n_la, 3); 
  EXPECT_TRUE((out)->topl[1]->la); 
  EXPECT_TRUE((out)->topl[1]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->k1, "NewSubKey5")); 
  EXPECT_EQ((out)->topl[1]->la[0]->k2, 300); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->k3, "SubKey6")); 
  ASSERT_EQ((out)->topl[1]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->lb[0]->k4, "InnerKey5")); 
  EXPECT_EQ((out)->topl[1]->la[0]->lb[0]->k5, 900); 
  EXPECT_TRUE((out)->topl[1]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[0]->lb[1]->k4, "InnerKey6")); 
  EXPECT_EQ((out)->topl[1]->la[0]->lb[1]->k5, 1000); 
  EXPECT_TRUE((out)->topl[1]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->k1,"SubKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->k2, 400); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->k3, "SubKey8")); 
  ASSERT_EQ((out)->topl[1]->la[1]->n_lb, 3); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[0]->k4, "NewInnerKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[0]->k5, 1100); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[1]->k4, "InnerKey8")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[1]->k5, 1200); 
  EXPECT_TRUE((out)->topl[1]->la[1]->lb[2]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[1]->lb[2]->k4, "InnerKey7")); 
  EXPECT_EQ((out)->topl[1]->la[1]->lb[2]->k5, 1100); 
  EXPECT_TRUE((out)->topl[1]->la[2]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[2]->k1, "SubKey5")); 
  EXPECT_EQ((out)->topl[1]->la[2]->k2, 300); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[2]->k3, "SubKey6")); 
  ASSERT_EQ((out)->topl[1]->la[2]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[1]->la[2]->lb); 
  EXPECT_TRUE((out)->topl[1]->la[2]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[2]->lb[0]->k4, "InnerKey5")); 
  EXPECT_EQ((out)->topl[1]->la[2]->lb[0]->k5, 900); 
  EXPECT_TRUE((out)->topl[1]->la[2]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[1]->la[2]->lb[1]->k4, "InnerKey6")); 
  EXPECT_EQ((out)->topl[1]->la[2]->lb[1]->k5, 1000); 
  EXPECT_TRUE((out)->topl[0]); 
  EXPECT_TRUE((out)->topl[0]->k0); 
  EXPECT_FALSE(strcmp((out)->topl[0]->k0,"Key1")); 
  ASSERT_EQ((out)->topl[0]->n_la, 2); 
  EXPECT_TRUE((out)->topl[0]->la); 
  EXPECT_TRUE((out)->topl[0]->la[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->k1, "SubKey1")); 
  EXPECT_EQ((out)->topl[0]->la[0]->k2, 100); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->k3, "SubKey2")); 
  ASSERT_EQ((out)->topl[0]->la[0]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->lb[0]->k4, "InnerKey1")); 
  EXPECT_EQ((out)->topl[0]->la[0]->lb[0]->k5, 500); 
  EXPECT_TRUE((out)->topl[0]->la[0]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[0]->lb[1]->k4, "InnerKey2")); 
  EXPECT_EQ((out)->topl[0]->la[0]->lb[1]->k5, 600); 
  EXPECT_TRUE((out)->topl[0]->la[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->k1, "SubKey3")); 
  EXPECT_EQ((out)->topl[0]->la[1]->k2, 200); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->k3, "SubKey4")); 
  ASSERT_EQ((out)->topl[0]->la[1]->n_lb, 2); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb[0]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->lb[0]->k4, "InnerKey3")); 
  EXPECT_EQ((out)->topl[0]->la[1]->lb[0]->k5, 700); 
  EXPECT_TRUE((out)->topl[0]->la[1]->lb[1]); 
  EXPECT_FALSE(strcmp((out)->topl[0]->la[1]->lb[1]->k4, "InnerKey4")); 
  EXPECT_EQ((out)->topl[0]->la[1]->lb[1]->k5, 800); 
}

TEST(RWProtobuf, MergeMsgsUnF)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc1);
  init_nc_msgs(&nc, &nc1);

  UniquePtrProtobufCMessageUseBody<>::uptr_t ncup1(&nc.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t ncup2(&nc1.base);

  size_t s1;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, &nc.base, &s1);
  ASSERT_TRUE(data1);

  ProtobufCMessage* out1 = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony_NetworkContext), s1, data1);
  ASSERT_TRUE(out1);
  UniquePtrProtobufCMessage<>::uptr_t uptm1(out1);

  ASSERT_EQ(protobuf_c_message_unknown_get_count(out1), 6);

  size_t s2;
  uint8_t* data2 = protobuf_c_message_serialize(NULL, &nc1.base, &s2);
  ASSERT_TRUE(data2);

  ProtobufCMessage* out2 = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony_NetworkContext), s2, data2);
  ASSERT_TRUE(out2);
  UniquePtrProtobufCMessage<>::uptr_t uptm2(out2);

  ASSERT_EQ(protobuf_c_message_unknown_get_count(out2), 5);

  protobuf_c_boolean ok = protobuf_c_message_merge(NULL, out1, out2);
  ASSERT_TRUE(ok);

  ASSERT_EQ(protobuf_c_message_unknown_get_count(out2), 11);

  const ProtobufCMessageUnknownField* unk_field = NULL;
  unk_field = protobuf_c_message_unknown_get_index(out2, 0);
  ASSERT_TRUE(unk_field);

  unsigned if_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)->pb_element_tag;
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 1);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unsigned lb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_LbProfile)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 2);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, lb_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 3);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, lb_tag);

  unsigned slb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 4);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, slb_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 5);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 6);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unsigned loadb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_LoadBalancer)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 7);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, loadb_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 8);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, loadb_tag);

  unsigned ipr_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_IpReceiverApplication)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 9);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, ipr_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 10);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, ipr_tag);

  size_t fs = 0;
  uint8_t* fmsg = protobuf_c_message_serialize(NULL, out2, &fs);
  ASSERT_TRUE(fmsg);

  ProtobufCMessage* res = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext), fs, fmsg);
  ASSERT_TRUE(res);
  UniquePtrProtobufCMessage<>::uptr_t uptr4(res);

  EXPECT_EQ(protobuf_c_message_unknown_get_count(res), 0);

  auto resm = RWPB_M_MSG_CAST(RwFpathD_RwBaseD_data_Colony_NetworkContext, res);
  EXPECT_EQ(resm->n_interface, 4);
  EXPECT_EQ(resm->n_lb_profile, 2);
  EXPECT_EQ(resm->n_load_balancer, 2);
  EXPECT_EQ(resm->n_ip_receiver_application, 2);
  EXPECT_EQ(resm->n_scriptable_lb, 1);

  free(data1);
  free(data2);
  free(fmsg);
}

TEST(RWProtobuf, MergeMsgsUnFNew)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc1);

  init_nc_msgs(&nc, &nc1);
  UniquePtrProtobufCMessageUseBody<>::uptr_t ncup1(&nc.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t ncup2(&nc1.base);

  size_t s1;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, &nc.base, &s1);
  ASSERT_TRUE(data1);

  ProtobufCMessage* out1 = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony_NetworkContext), s1, data1);
  ASSERT_TRUE(out1);
  UniquePtrProtobufCMessage<>::uptr_t uptm1(out1);

  ASSERT_EQ(protobuf_c_message_unknown_get_count(out1), 6);

  size_t s2;
  uint8_t* data2 = protobuf_c_message_serialize(NULL, &nc1.base, &s2);
  ASSERT_TRUE(data2);

  ProtobufCMessage* out2 = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony_NetworkContext), s2, data2);
  ASSERT_TRUE(out2);
  UniquePtrProtobufCMessage<>::uptr_t uptm2(out2);

  ASSERT_EQ(protobuf_c_message_unknown_get_count(out2), 5);

  protobuf_c_boolean ok = protobuf_c_message_merge_new(NULL, out1, out2);
  ASSERT_TRUE(ok);

  ASSERT_EQ(protobuf_c_message_unknown_get_count(out2), 11);

  const ProtobufCMessageUnknownField* unk_field = NULL;
  unk_field = protobuf_c_message_unknown_get_index(out2, 0);
  ASSERT_TRUE(unk_field);

  unsigned if_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)->pb_element_tag;
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 1);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unsigned loadb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_LoadBalancer)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 2);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, loadb_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 3);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, loadb_tag);

  unsigned ipr_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_IpReceiverApplication)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 4);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, ipr_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 5);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, ipr_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 6);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 7);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, if_tag);

  unsigned lb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_LbProfile)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 8);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, lb_tag);

  unk_field = protobuf_c_message_unknown_get_index(out2, 9);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, lb_tag);

  unsigned slb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)->pb_element_tag;
  unk_field = protobuf_c_message_unknown_get_index(out2, 10);
  ASSERT_TRUE(unk_field);
  EXPECT_EQ(unk_field->base_.tag, slb_tag);

  size_t fs = 0;
  uint8_t* fmsg = protobuf_c_message_serialize(NULL, out2, &fs);
  ASSERT_TRUE(fmsg);

  ProtobufCMessage* res = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext), fs, fmsg);
  ASSERT_TRUE(res);
  UniquePtrProtobufCMessage<>::uptr_t uptr4(res);

  EXPECT_EQ(protobuf_c_message_unknown_get_count(res), 0);

  auto resm = RWPB_M_MSG_CAST(RwFpathD_RwBaseD_data_Colony_NetworkContext, res);
  EXPECT_EQ(resm->n_interface, 4);
  EXPECT_EQ(resm->n_lb_profile, 2);
  EXPECT_EQ(resm->n_load_balancer, 2);
  EXPECT_EQ(resm->n_ip_receiver_application, 2);
  EXPECT_EQ(resm->n_scriptable_lb, 1);

  free(data1);
  free(data2);
  free(fmsg);
}

TEST(RWProtobuf, MergeUnFUnpack)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc1);

  init_nc_msgs(&nc, &nc1);

  UniquePtrProtobufCMessageUseBody<>::uptr_t ncup1(&nc.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t ncup2(&nc1.base);

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony2);

  strcpy(colony1.name, "colony");
  strcpy(colony2.name, "colony");

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* ncp1, *ncp2;
  ncp1 = &nc;
  ncp2 = &nc1;
  colony1.n_network_context = 1;
  colony1.network_context = &ncp1;

  colony2.n_network_context = 1;
  colony2.network_context = &ncp2;

  size_t s1 = 0;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, &colony1.base, &s1);

  size_t s2 = 0;
  uint8_t* data2 = protobuf_c_message_serialize(NULL, &colony2.base, &s2);

  uint8_t res[s1+s2];
  memcpy(res, data1, s1);
  memcpy(res+s1, data2, s2);

  ProtobufCMessage* out = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony), s1+s2, res);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t upto(out);
  auto col = RWPB_M_MSG_CAST(RwBaseD_data_Colony, out);
  EXPECT_EQ(col->n_network_context, 1);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&col->network_context[0]->base), 11);

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc2);
  strcpy(nc2.name, "trafgen");

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* ncp3;
  ncp3 = &nc2;
  colony2.network_context = &ncp3;

  free(data2);
  data2 = protobuf_c_message_serialize(NULL, &colony2.base, &s2);
  memcpy(res+s1, data2, s2);

  out = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony), s1+s2, res);
  ASSERT_TRUE(out);
  upto.reset(out);

  col = RWPB_M_MSG_CAST(RwBaseD_data_Colony, out);
  EXPECT_EQ(col->n_network_context, 1);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&col->network_context[0]->base), 6);

  free(data1);
  free(data2);
}

TEST(RWProtobuf, MergeFDs)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr3(&msg2.base);

  strcpy(msg1.name, "slb1");
  strcpy(msg2.name, "slb1");
  msg1.has_receive_function = 1;
  msg1.receive_function.has_builtin_basic_lb = 1;
  msg1.receive_function.builtin_basic_lb.has_address = 1;
  strcpy(msg1.receive_function.builtin_basic_lb.address, "127.0.0.1");
  msg1.receive_function.builtin_basic_lb.has_domain_name = 1;
  strcpy(msg1.receive_function.builtin_basic_lb.domain_name, "bslb1");
  msg1.receive_function.builtin_basic_lb.has_ip_proto = 1;
  msg1.receive_function.builtin_basic_lb.ip_proto = RW_FPATH_D_IP_PROTO_PROTO_UDP;
  msg1.receive_function.builtin_basic_lb.has_port = 1;
  msg1.receive_function.builtin_basic_lb.port = 5060;

  auto out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t uptr1(&out->base);

  EXPECT_STREQ((out)->name, "slb1"); 
  EXPECT_TRUE((out)->has_receive_function); 
  EXPECT_TRUE((out)->receive_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_address); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.address, "127.0.0.1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_domain_name); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.domain_name, "bslb1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_ip_proto); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_port); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.port, 5060);

  msg1.has_receive_function = 1;
  msg1.receive_function.has_receive_script = 1;
  msg1.receive_function.receive_script.has_script_name = 1;
  strcpy(msg1.receive_function.receive_script.script_name, "script1");

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_STREQ((out)->name, "slb1"); 
  EXPECT_TRUE((out)->has_receive_function); 
  EXPECT_TRUE((out)->receive_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_address); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.address, "127.0.0.1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_domain_name); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.domain_name, "bslb1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_ip_proto); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_port); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.port, 5060); 
  EXPECT_TRUE((out)->receive_function.has_receive_script); 
  EXPECT_TRUE((out)->receive_function.receive_script.has_script_name); 
  EXPECT_STREQ((out)->receive_function.receive_script.script_name, "script1");

  msg1.has_classify_function = 1;
  msg1.classify_function.has_builtin_basic_lb = 1;
  msg1.classify_function.builtin_basic_lb.has_key_type = 1;
  msg1.classify_function.builtin_basic_lb.key_type = RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE;

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_STREQ((out)->name, "slb1"); 
  EXPECT_TRUE((out)->has_receive_function); 
  EXPECT_TRUE((out)->receive_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_address); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.address, "127.0.0.1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_domain_name); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.domain_name, "bslb1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_ip_proto); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_port); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.port, 5060); 
  EXPECT_TRUE((out)->receive_function.has_receive_script); 
  EXPECT_TRUE((out)->receive_function.receive_script.has_script_name); 
  EXPECT_STREQ((out)->receive_function.receive_script.script_name, "script1"); 
  EXPECT_TRUE((out)->has_classify_function); 
  EXPECT_TRUE((out)->classify_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->classify_function.builtin_basic_lb.has_key_type); 
  EXPECT_EQ((out)->classify_function.builtin_basic_lb.key_type, RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE);

  msg1.has_classify_function = 1;
  msg1.classify_function.has_classify_script = 1;
  msg1.classify_function.classify_script.has_script_name = 1;
  strcpy(msg1.classify_function.classify_script.script_name, "classifyscript");

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_STREQ((out)->name, "slb1"); 
  EXPECT_TRUE((out)->has_receive_function); 
  EXPECT_TRUE((out)->receive_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_address); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.address, "127.0.0.1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_domain_name); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.domain_name, "bslb1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_ip_proto); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_port); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.port, 5060); 
  EXPECT_TRUE((out)->receive_function.has_receive_script); 
  EXPECT_TRUE((out)->receive_function.receive_script.has_script_name); 
  EXPECT_STREQ((out)->receive_function.receive_script.script_name, "script1"); 
  EXPECT_TRUE((out)->has_classify_function); 
  EXPECT_TRUE((out)->classify_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->classify_function.builtin_basic_lb.has_key_type); 
  EXPECT_EQ((out)->classify_function.builtin_basic_lb.key_type, RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE); 
  EXPECT_TRUE((out)->classify_function.has_classify_script); 
  EXPECT_TRUE((out)->classify_function.classify_script.has_script_name); 
  EXPECT_STREQ((out)->classify_function.classify_script.script_name, "classifyscript");

  msg1.has_server_selection_function = 1;
  msg1.server_selection_function.n_server_group = 1;
  strcpy(msg1.server_selection_function.server_group[0].name, "ServerGName");
  msg1.server_selection_function.server_group[0].n_server = 1;
  strcpy(msg1.server_selection_function.server_group[0].server[0].address, "Server1");
  msg1.server_selection_function.server_group[0].server[0].has_port = 1;
  msg1.server_selection_function.server_group[0].server[0].port = 5061;
  msg1.server_selection_function.server_group[0].has_nat_address = 1;
  msg1.server_selection_function.server_group[0].nat_address.has_src_address = 1;
  strcpy(msg1.server_selection_function.server_group[0].nat_address.src_address, "192.168.1.2");
  msg1.server_selection_function.server_group[0].nat_address.has_src_port = 1;
  msg1.server_selection_function.server_group[0].nat_address.src_port = 6060;

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_TRUE(out->has_server_selection_function);
  EXPECT_TRUE(out->server_selection_function.n_server_group);
  EXPECT_STREQ(out->server_selection_function.server_group[0].name, "ServerGName");
  EXPECT_TRUE(out->server_selection_function.server_group[0].n_server);
  EXPECT_STREQ(out->server_selection_function.server_group[0].server[0].address, "Server1");
  EXPECT_TRUE(out->server_selection_function.server_group[0].server[0].has_port);
  EXPECT_EQ(out->server_selection_function.server_group[0].server[0].port, 5061);
  EXPECT_TRUE(out->server_selection_function.server_group[0].has_nat_address);
  EXPECT_TRUE(out->server_selection_function.server_group[0].nat_address.has_src_address);
  EXPECT_STREQ(out->server_selection_function.server_group[0].nat_address.src_address, "192.168.1.2");
  EXPECT_TRUE(out->server_selection_function.server_group[0].nat_address.has_src_port);
  EXPECT_EQ(out->server_selection_function.server_group[0].nat_address.src_port, 6060);

  msg1.has_server_selection_function = 1;
  msg1.server_selection_function.has_builtin_basic_lb = 1;
  msg1.server_selection_function.builtin_basic_lb.has_selection_type = 1;
  msg1.server_selection_function.builtin_basic_lb.selection_type = RW_FPATH_D_SELECTION_TYPE_BASIC_ROUND_ROBIN;
  msg1.server_selection_function.has_server_selection_script = 1;
  msg1.server_selection_function.server_selection_script.has_script_name = 1;
  strcpy(msg1.server_selection_function.server_selection_script.script_name, "SSS");
  msg1.server_selection_function.has_selection_criteria = 1;
  msg1.server_selection_function.selection_criteria.has_dns_lb = 1;
  msg1.server_selection_function.selection_criteria.dns_lb.n_match_rule = 1;
  strcpy(msg1.server_selection_function.selection_criteria.dns_lb.match_rule[0].domain, "domain1");
  msg1.server_selection_function.selection_criteria.dns_lb.match_rule[0].has_server_group = 1;
  strcpy(msg1.server_selection_function.selection_criteria.dns_lb.match_rule[0].server_group, "Group1");

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_TRUE(out->server_selection_function.has_builtin_basic_lb);
  EXPECT_TRUE(out->server_selection_function.builtin_basic_lb.has_selection_type);
  EXPECT_EQ(out->server_selection_function.builtin_basic_lb.selection_type, RW_FPATH_D_SELECTION_TYPE_BASIC_ROUND_ROBIN);
  EXPECT_TRUE(out->server_selection_function.has_server_selection_script);
  EXPECT_TRUE(out->server_selection_function.server_selection_script.has_script_name);
  EXPECT_STREQ(out->server_selection_function.server_selection_script.script_name, "SSS");
  EXPECT_TRUE(out->server_selection_function.has_selection_criteria);
  EXPECT_TRUE(out->server_selection_function.selection_criteria.has_dns_lb);
  EXPECT_TRUE(out->server_selection_function.selection_criteria.dns_lb.n_match_rule);
  EXPECT_STREQ(out->server_selection_function.selection_criteria.dns_lb.match_rule[0].domain, "domain1");

  msg1.has_transform_function = 1;
  msg1.transform_function.has_builtin_transform = 1;
  msg1.transform_function.builtin_transform.has_transform_type = 1;
  msg1.transform_function.builtin_transform.transform_type = RW_FPATH_D_TRANSFORM_TYPE_DOUBLE_NAT;

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_TRUE(out->has_transform_function);
  EXPECT_TRUE(out->transform_function.has_builtin_transform);
  EXPECT_TRUE(out->transform_function.builtin_transform.has_transform_type);
  EXPECT_EQ(out->transform_function.builtin_transform.transform_type, RW_FPATH_D_TRANSFORM_TYPE_DOUBLE_NAT);

  msg1.has_transform_function = 1;
  msg1.transform_function.has_transform_script = 1;
  msg1.transform_function.transform_script.has_script_name = 1;
  strcpy(msg1.transform_function.transform_script.script_name, "TSCN");

  out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)*)
      pack_merge_and_unpack_msgs(&msg1.base, &msg2.base);
  ASSERT_TRUE(out);
  uptr1.reset(&out->base);

  EXPECT_STREQ((out)->name, "slb1"); 
  EXPECT_TRUE((out)->has_receive_function); 
  EXPECT_TRUE((out)->receive_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_address); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.address, "127.0.0.1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_domain_name); 
  EXPECT_STREQ((out)->receive_function.builtin_basic_lb.domain_name, "bslb1"); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_ip_proto); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP); 
  EXPECT_TRUE((out)->receive_function.builtin_basic_lb.has_port); 
  EXPECT_EQ((out)->receive_function.builtin_basic_lb.port, 5060); 
  EXPECT_TRUE((out)->receive_function.has_receive_script); 
  EXPECT_TRUE((out)->receive_function.receive_script.has_script_name); 
  EXPECT_STREQ((out)->receive_function.receive_script.script_name, "script1"); 
  EXPECT_TRUE((out)->has_classify_function); 
  EXPECT_TRUE((out)->classify_function.has_builtin_basic_lb); 
  EXPECT_TRUE((out)->classify_function.builtin_basic_lb.has_key_type); 
  EXPECT_EQ((out)->classify_function.builtin_basic_lb.key_type, RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE); 
  EXPECT_TRUE((out)->classify_function.has_classify_script); 
  EXPECT_TRUE((out)->classify_function.classify_script.has_script_name); 
  EXPECT_STREQ((out)->classify_function.classify_script.script_name, "classifyscript"); 
  EXPECT_TRUE((out)->transform_function.has_transform_script); 
  EXPECT_TRUE((out)->transform_function.transform_script.has_script_name); 
  EXPECT_STREQ((out)->transform_function.transform_script.script_name, "TSCN"); 
}

TEST(RWProtobuf, MergeMessagesNew)
{
  TEST_DESCRIPTION("Test merge protobuf messages");

  RWPB_T_MSG(Company_data_Company) msg1;
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);

  init_comp_msg(&msg1, 2, 2);

  RWPB_M_MSG_DECL_INIT(Company_data_Company, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.contact_info);
  EXPECT_TRUE(msg1.contact_info);

  EXPECT_TRUE(msg2.contact_info->name);
  EXPECT_FALSE(strcmp(msg2.contact_info->name, "abc"));
  EXPECT_TRUE(msg2.contact_info->address);
  EXPECT_FALSE(strcmp(msg2.contact_info->address, "bglr"));
  EXPECT_TRUE(msg2.contact_info->phone_number);
  EXPECT_FALSE(strcmp(msg2.contact_info->phone_number, "1234"));

  ASSERT_EQ(msg2.n_employee, 2);
  ASSERT_EQ(msg1.n_employee, 2);

  EXPECT_TRUE(msg2.employee);
  EXPECT_TRUE(msg1.employee);

  ASSERT_EQ(msg2.n_product, 2);
  ASSERT_EQ(msg1.n_product, 2);

  EXPECT_TRUE(msg2.product);
  EXPECT_TRUE(msg1.product);

  EXPECT_EQ(msg2.employee[0]->id, 100);
  EXPECT_FALSE(strcmp(msg2.employee[0]->name, "Emp1"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->title, "Title"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->start_date, "1234"));

  EXPECT_EQ(msg2.employee[1]->id, 200);
  EXPECT_FALSE(strcmp(msg2.employee[1]->name, "Emp2"));
  EXPECT_FALSE(strcmp(msg2.employee[1]->title, "Title"));
  EXPECT_FALSE(strcmp(msg2.employee[1]->start_date, "1234"));

  EXPECT_EQ(msg2.product[0]->id, 1000);
  EXPECT_FALSE(strcmp(msg2.product[0]->name, "nfv"));
  EXPECT_FALSE(strcmp(msg2.product[0]->msrp, "1000"));

  EXPECT_EQ(msg2.product[1]->id, 2000);
  EXPECT_FALSE(strcmp(msg2.product[1]->name, "cloud"));
  EXPECT_FALSE(strcmp(msg2.product[1]->msrp, "2000"));

  uptr1.reset();
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);

  init_comp_msg(&msg1, 3, 2);
  uptr1.reset(&msg1.base);

  msg1.n_employee = 3;
  msg1.employee[2] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg1.employee[2]);
  msg1.employee[2]->id = 300;
  msg1.employee[2]->has_id = 1;
  msg1.employee[2]->name = strdup("Emp3");
  msg1.employee[2]->title = strdup("SWE");
  msg1.employee[2]->start_date = strdup("1431");


  uptr2.reset();
  RWPB_F_MSG_INIT(Company_data_Company, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_employee = 3;
  msg2.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)calloc(3, sizeof(void*));
  msg2.employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[0]);
  msg2.employee[0]->id = 100;
  msg2.employee[0]->has_id = 1;
  msg2.employee[0]->name = strdup("Emp3");
  msg2.employee[0]->title = strdup("Title");

  msg2.employee[1] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[1]);
  msg2.employee[1]->id = 200;
  msg2.employee[1]->has_id = 1;

  msg2.employee[2] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[2]);
  msg2.employee[2]->id = 800;
  msg2.employee[2]->has_id = 1;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  ASSERT_EQ(msg2.n_employee, 4);
  ASSERT_EQ(msg1.n_employee, 3);
  EXPECT_TRUE(msg1.employee);

  EXPECT_EQ(msg2.employee[0]->id, 100);
  EXPECT_FALSE(strcmp(msg2.employee[0]->name, "Emp3"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->title, "Title"));
  EXPECT_TRUE(msg2.employee[0]->start_date);
  EXPECT_FALSE(strcmp(msg2.employee[0]->start_date, "1234"));

  EXPECT_EQ(msg2.employee[1]->id, 200);
  EXPECT_TRUE(msg2.employee[1]->name);
  EXPECT_FALSE(strcmp(msg2.employee[1]->name, "Emp2"));
  EXPECT_TRUE(msg2.employee[1]->title);
  EXPECT_FALSE(strcmp(msg2.employee[1]->title, "Title"));
  EXPECT_TRUE(msg2.employee[1]->start_date);
  EXPECT_FALSE(strcmp(msg2.employee[1]->start_date, "1234"));

  EXPECT_EQ(msg2.employee[2]->id, 800);
  EXPECT_FALSE(msg2.employee[2]->name);
  EXPECT_FALSE(msg2.employee[2]->title);
  EXPECT_FALSE(msg2.employee[2]->start_date);

  EXPECT_EQ(msg2.employee[3]->id, 300);
  EXPECT_FALSE(strcmp(msg2.employee[3]->name, "Emp3"));
  EXPECT_FALSE(strcmp(msg2.employee[3]->title, "SWE"));
  EXPECT_FALSE(strcmp(msg2.employee[3]->start_date, "1431"));

  ASSERT_EQ(msg1.n_employee, 3);
  EXPECT_TRUE(msg1.employee);

  ASSERT_EQ(msg2.n_product, 2);
  EXPECT_EQ(msg2.product[0]->id, 1000);
  EXPECT_FALSE(strcmp(msg2.product[0]->name, "nfv"));
  EXPECT_FALSE(strcmp(msg2.product[0]->msrp, "1000"));

  EXPECT_EQ(msg2.product[1]->id, 2000);
  EXPECT_FALSE(strcmp(msg2.product[1]->name, "cloud"));
  EXPECT_FALSE(strcmp(msg2.product[1]->msrp, "2000"));

  ASSERT_EQ(msg1.n_product, 2);
  EXPECT_TRUE(msg1.product);
}

TEST(RWProtobuf, MergeMsgsNewInline)
{
  // Inline repeated messages.
  RWPB_T_MSG(Testy2pTop1_data_Top1c2) msg1;
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);

  init_test_msg(&msg1);

  RWPB_M_MSG_DECL_INIT(Testy2pTop1_data_Top1c2, msg2);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  EXPECT_TRUE(msg2.has_ls1);
  EXPECT_FALSE(strcmp(msg2.ls1, "RiftIO"));
  EXPECT_TRUE(msg2.has_ln1);
  EXPECT_EQ(msg2.ln1, 20000);
  EXPECT_TRUE(msg2.has_ln2);
  EXPECT_EQ(msg2.ln2, 500);
  EXPECT_TRUE(msg2.has_c1);
  EXPECT_TRUE(msg2.c1.has_c1);
  EXPECT_TRUE(msg2.c1.c1.has_ln1);
  EXPECT_EQ(msg2.c1.c1.ln1, 10);
  EXPECT_TRUE(msg2.c1.c1.has_ln2);
  EXPECT_EQ(msg2.c1.c1.ln2, 100);

  ASSERT_EQ(msg2.n_l1, 2);
  ASSERT_EQ(msg2.l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ(msg2.l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ(msg2.l1[0].l1[1].ln2, 200);

  ASSERT_EQ(msg2.l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ(msg2.l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ(msg2.l1[1].l1[1].ln2, 400);

  uptr2.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg2);
  uptr2.reset(&msg2.base);

  msg2.has_ls1 = true;
  strcpy(msg2.ls1, "Super RiftIO");
  msg2.has_c1 = true;
  msg2.c1.has_c1 = true;
  msg2.c1.c1.has_ln1 = true;
  msg2.c1.c1.ln1 = 20;
  msg2.c1.c1.has_ln2 = true;
  msg2.c1.c1.ln2 = 200;

  msg2.n_l1 = 2;
  msg2.l1[0].lf1 = 14;
  msg2.l1[0].has_lf1 = 1;
  msg2.l1[0].n_l1 = 2;
  strcpy(msg2.l1[0].l1[0].ln1, "Key1");
  msg2.l1[0].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[0].l1[1].ln1, "Key2");
  msg2.l1[0].l1[1].has_ln1 = 1;

  msg2.l1[1].lf1 = 31;
  msg2.l1[1].has_lf1 = 1;
  msg2.l1[1].n_l1 = 2;
  strcpy(msg2.l1[1].l1[0].ln1, "Key3");
  msg2.l1[1].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[1].l1[1].ln1, "Key4");
  msg2.l1[1].l1[1].has_ln1 = 1;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.has_ls1);
  EXPECT_FALSE(strcmp(msg2.ls1, "Super RiftIO"));
  EXPECT_TRUE(msg2.has_ln1);
  EXPECT_EQ(msg2.ln1, 20000);
  EXPECT_TRUE(msg2.has_ln2);
  EXPECT_EQ(msg2.ln2, 500);
  EXPECT_TRUE(msg2.has_c1);
  EXPECT_TRUE(msg2.c1.has_c1);
  EXPECT_TRUE(msg2.c1.c1.has_ln1);
  EXPECT_EQ(msg2.c1.c1.ln1, 20);
  EXPECT_TRUE(msg2.c1.c1.has_ln2);
  EXPECT_EQ(msg2.c1.c1.ln2, 200);

  ASSERT_EQ(msg2.n_l1, 2);
  ASSERT_EQ(msg2.l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ(msg2.l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ(msg2.l1[0].l1[1].ln2, 200);

  ASSERT_EQ(msg2.l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ(msg2.l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ(msg2.l1[1].l1[1].ln2, 400);

  uptr2.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg2);
  uptr2.reset(&msg2.base);

  msg2.has_ls1 = true;
  strcpy(msg2.ls1, "Super RiftIO");
  msg2.has_c1 = true;
  msg2.c1.has_c1 = true;
  msg2.c1.c1.has_ln1 = true;
  msg2.c1.c1.ln1 = 20;
  msg2.c1.c1.has_ln2 = true;
  msg2.c1.c1.ln2 = 200;

  msg2.n_l1 = 3;
  msg2.l1[0].lf1 = 14;
  msg2.l1[0].has_lf1 = 1;
  msg2.l1[0].n_l1 = 2;
  strcpy(msg2.l1[0].l1[0].ln1, "Key1");
  msg2.l1[0].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[0].l1[1].ln1, "Key2");
  msg2.l1[0].l1[1].has_ln1 = 1;

  msg2.l1[1].lf1 = 9;
  msg2.l1[1].has_lf1 = 1;
  msg2.l1[1].n_l1 = 2;
  strcpy(msg2.l1[1].l1[0].ln1, "Key3");
  msg2.l1[1].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[1].l1[1].ln1, "Key4");
  msg2.l1[1].l1[1].has_ln1 = 1;

  msg2.l1[2].lf1 = 10;
  msg2.l1[2].has_lf1 = 1;
  msg2.l1[2].n_l1 = 2;
  strcpy(msg2.l1[2].l1[0].ln1, "Key5");
  msg2.l1[2].l1[0].has_ln1 = 1;
  strcpy(msg2.l1[2].l1[1].ln1, "Key6");
  msg2.l1[2].l1[1].has_ln1 = 1;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.has_ls1);
  EXPECT_FALSE(strcmp(msg2.ls1, "Super RiftIO"));
  EXPECT_TRUE(msg2.has_ln1);
  EXPECT_EQ(msg2.ln1, 20000);
  EXPECT_TRUE(msg2.has_ln2);
  EXPECT_EQ(msg2.ln2, 500);
  EXPECT_TRUE(msg2.has_c1);
  EXPECT_TRUE(msg2.c1.has_c1);
  EXPECT_TRUE(msg2.c1.c1.has_ln1);
  EXPECT_EQ(msg2.c1.c1.ln1, 20);
  EXPECT_TRUE(msg2.c1.c1.has_ln2);
  EXPECT_EQ(msg2.c1.c1.ln2, 200);

  ASSERT_EQ(msg2.n_l1, 4);
  EXPECT_EQ(msg2.l1[0].lf1, 14);
  ASSERT_EQ(msg2.l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ(msg2.l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ(msg2.l1[0].l1[1].ln2, 200);

  EXPECT_EQ(msg2.l1[1].lf1, 9);
  ASSERT_EQ(msg2.l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[0].ln1, "Key3"));
  EXPECT_FALSE(msg2.l1[1].l1[0].has_ln2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[1].ln1, "Key4"));
  EXPECT_FALSE(msg2.l1[1].l1[1].has_ln2);

  EXPECT_EQ(msg2.l1[2].lf1, 10);
  ASSERT_EQ(msg2.l1[2].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[2].l1[0].ln1, "Key5"));
  EXPECT_FALSE(msg2.l1[2].l1[0].has_ln2);
  EXPECT_FALSE(strcmp(msg2.l1[2].l1[1].ln1, "Key6"));
  EXPECT_FALSE(msg2.l1[2].l1[1].has_ln2);

  EXPECT_EQ(msg2.l1[3].lf1, 31);
  ASSERT_EQ(msg2.l1[3].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[3].l1[0].ln1, "Key3"));
  EXPECT_EQ(msg2.l1[3].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp(msg2.l1[3].l1[1].ln1, "Key4"));
  EXPECT_EQ(msg2.l1[3].l1[1].ln2, 400);
}

TEST(RWProtobuf, MergeMsgsNewMKeys)
{
  // Multiple keys.
  
  RWPB_T_LISTONLY(Testy2pTop2_data_Topl) msg1;
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg1);

  init_multi_key_msg(&msg1);

  RWPB_M_LISTONLY_DECL_INIT(Testy2pTop2_data_Topl,msg2);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  ASSERT_EQ(msg2.n_topl, 2);
  ASSERT_EQ(msg1.n_topl, 2);
  EXPECT_TRUE(msg2.topl);
  EXPECT_TRUE(msg1.topl);
  EXPECT_TRUE(msg2.topl[0]);
  EXPECT_TRUE(msg2.topl[0]->k0);
  EXPECT_FALSE(strcmp(msg2.topl[0]->k0,"Key1"));
  ASSERT_EQ(msg2.topl[0]->n_la, 2);
  EXPECT_TRUE(msg2.topl[0]->la);
  EXPECT_TRUE(msg2.topl[0]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->k1, "SubKey1"));
  EXPECT_EQ(msg2.topl[0]->la[0]->k2, 100);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->k3, "SubKey2"));
  ASSERT_EQ(msg2.topl[0]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->lb[0]->k4, "InnerKey1"));
  EXPECT_EQ(msg2.topl[0]->la[0]->lb[0]->k5, 500);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->lb[1]->k4, "InnerKey2"));
  EXPECT_EQ(msg2.topl[0]->la[0]->lb[1]->k5, 600);
  EXPECT_TRUE(msg2.topl[0]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->k1, "SubKey3"));
  EXPECT_EQ(msg2.topl[0]->la[1]->k2, 200);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->k3, "SubKey4"));
  ASSERT_EQ(msg2.topl[0]->la[1]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->lb[0]->k4, "InnerKey3"));
  EXPECT_EQ(msg2.topl[0]->la[1]->lb[0]->k5, 700);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->lb[1]->k4, "InnerKey4"));
  EXPECT_EQ(msg2.topl[0]->la[1]->lb[1]->k5, 800);

  EXPECT_TRUE(msg2.topl[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->k0, "Key2"));
  ASSERT_EQ(msg2.topl[1]->n_la, 2);
  EXPECT_TRUE(msg2.topl[1]->la);
  EXPECT_TRUE(msg2.topl[1]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->k1, "SubKey5"));
  EXPECT_EQ(msg2.topl[1]->la[0]->k2, 300);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->k3, "SubKey6"));
  ASSERT_EQ(msg2.topl[1]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->lb[0]->k4, "InnerKey5"));
  EXPECT_EQ(msg2.topl[1]->la[0]->lb[0]->k5, 900);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->lb[1]->k4, "InnerKey6"));
  EXPECT_EQ(msg2.topl[1]->la[0]->lb[1]->k5, 1000);
  EXPECT_TRUE(msg2.topl[1]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->k1,"SubKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->k2, 400);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->k3, "SubKey8"));
  ASSERT_EQ(msg2.topl[1]->la[1]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[0]->k4, "InnerKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[0]->k5, 1100);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[1]->k4, "InnerKey8"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[1]->k5, 1200);

  uptr2.reset();
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_topl = 2;
  msg2.topl = (RWPB_T_MSG(Testy2pTop2_data_Topl)**)calloc(2, sizeof(void*));
  msg2.topl[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[0]);
  msg2.topl[0]->k0 = strdup("Key1");
  msg2.topl[0]->n_la = 2;
  msg2.topl[0]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[0]);
  msg2.topl[0]->la[0]->k1 = strdup("SubKey1");
  msg2.topl[0]->la[0]->k2 = 100;
  msg2.topl[0]->la[0]->has_k2 = 1;
  msg2.topl[0]->la[0]->k3 = strdup("SubKey2");
  msg2.topl[0]->la[0]->n_lb = 2;
  msg2.topl[0]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[0]);
  msg2.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1");
  msg2.topl[0]->la[0]->lb[0]->k5 = 500;
  msg2.topl[0]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[1]);
  msg2.topl[0]->la[0]->lb[1]->k4 = strdup("InnerKey2");
  msg2.topl[0]->la[0]->lb[1]->k5 = 600;
  msg2.topl[0]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[0]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[1]);
  msg2.topl[0]->la[1]->k1 = strdup("SubKey3");
  msg2.topl[0]->la[1]->k2 = 200;
  msg2.topl[0]->la[1]->has_k2 = 1;
  msg2.topl[0]->la[1]->k3 = strdup("SubKey4");
  msg2.topl[0]->la[1]->n_lb = 2;
  msg2.topl[0]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[0]);
  msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3");
  msg2.topl[0]->la[1]->lb[0]->k5 = 700;
  msg2.topl[0]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[1]);
  msg2.topl[0]->la[1]->lb[1]->k4 = strdup("InnerKey4");
  msg2.topl[0]->la[1]->lb[1]->k5 = 800;
  msg2.topl[0]->la[1]->lb[1]->has_k5 = 1;

  msg2.topl[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[1]);
  msg2.topl[1]->k0 = strdup("Key2");
  msg2.topl[1]->n_la = 2;
  msg2.topl[1]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[0]);
  msg2.topl[1]->la[0]->k1 = strdup("SubKey5");
  msg2.topl[1]->la[0]->k2 = 300;
  msg2.topl[1]->la[0]->has_k2 = 1;
  msg2.topl[1]->la[0]->k3 = strdup("SubKey6");
  msg2.topl[1]->la[0]->n_lb = 2;
  msg2.topl[1]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[0]);
  msg2.topl[1]->la[0]->lb[0]->k4 = strdup("InnerKey5");
  msg2.topl[1]->la[0]->lb[0]->k5 = 900;
  msg2.topl[1]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[1]);
  msg2.topl[1]->la[0]->lb[1]->k4 = strdup("InnerKey6");
  msg2.topl[1]->la[0]->lb[1]->k5 = 1000;
  msg2.topl[1]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[1]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[1]);
  msg2.topl[1]->la[1]->k1 = strdup("SubKey7");
  msg2.topl[1]->la[1]->k2 = 400;
  msg2.topl[1]->la[1]->has_k2 = 1;
  msg2.topl[1]->la[1]->k3 = strdup("SubKey8");
  msg2.topl[1]->la[1]->n_lb = 2;
  msg2.topl[1]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[0]);
  msg2.topl[1]->la[1]->lb[0]->k4 = strdup("InnerKey7");
  msg2.topl[1]->la[1]->lb[0]->k5 = 1100;
  msg2.topl[1]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[1]);
  msg2.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8");
  msg2.topl[1]->la[1]->lb[1]->k5 = 1200;
  msg2.topl[1]->la[1]->lb[1]->has_k5 = 1;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  ASSERT_EQ(msg2.n_topl, 2);
  ASSERT_EQ(msg1.n_topl, 2);
  EXPECT_TRUE(msg2.topl);
  EXPECT_TRUE(msg1.topl);
  EXPECT_TRUE(msg2.topl[0]);
  EXPECT_TRUE(msg2.topl[0]->k0);
  EXPECT_FALSE(strcmp(msg2.topl[0]->k0,"Key1"));
  ASSERT_EQ(msg2.topl[0]->n_la, 2);
  EXPECT_TRUE(msg2.topl[0]->la);
  EXPECT_TRUE(msg2.topl[0]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->k1, "SubKey1"));
  EXPECT_EQ(msg2.topl[0]->la[0]->k2, 100);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->k3, "SubKey2"));
  ASSERT_EQ(msg2.topl[0]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->lb[0]->k4, "InnerKey1"));
  EXPECT_EQ(msg2.topl[0]->la[0]->lb[0]->k5, 500);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->lb[1]->k4, "InnerKey2"));
  EXPECT_EQ(msg2.topl[0]->la[0]->lb[1]->k5, 600);
  EXPECT_TRUE(msg2.topl[0]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->k1, "SubKey3"));
  EXPECT_EQ(msg2.topl[0]->la[1]->k2, 200);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->k3, "SubKey4"));
  ASSERT_EQ(msg2.topl[0]->la[1]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->lb[0]->k4, "InnerKey3"));
  EXPECT_EQ(msg2.topl[0]->la[1]->lb[0]->k5, 700);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->lb[1]->k4, "InnerKey4"));
  EXPECT_EQ(msg2.topl[0]->la[1]->lb[1]->k5, 800);

  EXPECT_TRUE(msg2.topl[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->k0, "Key2"));
  ASSERT_EQ(msg2.topl[1]->n_la, 2);
  EXPECT_TRUE(msg2.topl[1]->la);
  EXPECT_TRUE(msg2.topl[1]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->k1, "SubKey5"));
  EXPECT_EQ(msg2.topl[1]->la[0]->k2, 300);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->k3, "SubKey6"));
  ASSERT_EQ(msg2.topl[1]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->lb[0]->k4, "InnerKey5"));
  EXPECT_EQ(msg2.topl[1]->la[0]->lb[0]->k5, 900);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->lb[1]->k4, "InnerKey6"));
  EXPECT_EQ(msg2.topl[1]->la[0]->lb[1]->k5, 1000);
  EXPECT_TRUE(msg2.topl[1]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->k1,"SubKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->k2, 400);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->k3, "SubKey8"));
  ASSERT_EQ(msg2.topl[1]->la[1]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[0]->k4, "InnerKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[0]->k5, 1100);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[1]->k4, "InnerKey8"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[1]->k5, 1200);

  uptr2.reset();
  RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_topl = 2;
  msg2.topl = (RWPB_T_MSG(Testy2pTop2_data_Topl)**)calloc(2, sizeof(void*));
  msg2.topl[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[0]);
  msg2.topl[0]->k0 = strdup("NewKey1");
  msg2.topl[0]->n_la = 2;
  msg2.topl[0]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[0]);
  msg2.topl[0]->la[0]->k1 = strdup("SubKey1");
  msg2.topl[0]->la[0]->k2 = 100;
  msg2.topl[0]->la[0]->has_k2 = 1;
  msg2.topl[0]->la[0]->k3 = strdup("SubKey2");
  msg2.topl[0]->la[0]->n_lb = 2;
  msg2.topl[0]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[0]);
  msg2.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1");
  msg2.topl[0]->la[0]->lb[0]->k5 = 500;
  msg2.topl[0]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[0]->lb[1]);
  msg2.topl[0]->la[0]->lb[1]->k4 = strdup("InnerKey2");
  msg2.topl[0]->la[0]->lb[1]->k5 = 600;
  msg2.topl[0]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[0]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[0]->la[1]);
  msg2.topl[0]->la[1]->k1 = strdup("SubKey3");
  msg2.topl[0]->la[1]->k2 = 200;
  msg2.topl[0]->la[1]->has_k2 = 1;
  msg2.topl[0]->la[1]->k3 = strdup("SubKey4");
  msg2.topl[0]->la[1]->n_lb = 2;
  msg2.topl[0]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
  msg2.topl[0]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[0]);
  msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3");
  msg2.topl[0]->la[1]->lb[0]->k5 = 700;
  msg2.topl[0]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[0]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[0]->la[1]->lb[1]);
  msg2.topl[0]->la[1]->lb[1]->k4 = strdup("InnerKey4");
  msg2.topl[0]->la[1]->lb[1]->k5 = 800;
  msg2.topl[0]->la[1]->lb[1]->has_k5 = 1;

  msg2.topl[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg2.topl[1]);
  msg2.topl[1]->k0 = strdup("Key2");
  msg2.topl[1]->n_la = 2;
  msg2.topl[1]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[0]);
  msg2.topl[1]->la[0]->k1 = strdup("NewSubKey5");
  msg2.topl[1]->la[0]->k2 = 300;
  msg2.topl[1]->la[0]->has_k2 = 1;
  msg2.topl[1]->la[0]->k3 = strdup("SubKey6");
  msg2.topl[1]->la[0]->n_lb = 2;
  msg2.topl[1]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[0]);
  msg2.topl[1]->la[0]->lb[0]->k4 = strdup("InnerKey5");
  msg2.topl[1]->la[0]->lb[0]->k5 = 900;
  msg2.topl[1]->la[0]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[0]->lb[1]);
  msg2.topl[1]->la[0]->lb[1]->k4 = strdup("InnerKey6");
  msg2.topl[1]->la[0]->lb[1]->k5 = 1000;
  msg2.topl[1]->la[0]->lb[1]->has_k5 = 1;
  msg2.topl[1]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg2.topl[1]->la[1]);
  msg2.topl[1]->la[1]->k1 = strdup("SubKey7");
  msg2.topl[1]->la[1]->k2 = 400;
  msg2.topl[1]->la[1]->has_k2 = 1;
  msg2.topl[1]->la[1]->k3 = strdup("SubKey8");
  msg2.topl[1]->la[1]->n_lb = 2;
  msg2.topl[1]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
  msg2.topl[1]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[0]);
  msg2.topl[1]->la[1]->lb[0]->k4 = strdup("NewInnerKey7");
  msg2.topl[1]->la[1]->lb[0]->k5 = 1100;
  msg2.topl[1]->la[1]->lb[0]->has_k5 = 1;
  msg2.topl[1]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg2.topl[1]->la[1]->lb[1]);
  msg2.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8");
  msg2.topl[1]->la[1]->lb[1]->k5 = 1200;
  msg2.topl[1]->la[1]->lb[1]->has_k5 = 1;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  ASSERT_EQ(msg2.n_topl, 3);
  ASSERT_EQ(msg1.n_topl, 2);
  EXPECT_TRUE(msg2.topl);
  EXPECT_TRUE(msg1.topl);
  EXPECT_TRUE(msg2.topl[0]);
  EXPECT_TRUE(msg2.topl[0]->k0);
  EXPECT_FALSE(strcmp(msg2.topl[0]->k0,"NewKey1"));
  ASSERT_EQ(msg2.topl[0]->n_la, 2);
  EXPECT_TRUE(msg2.topl[0]->la);
  EXPECT_TRUE(msg2.topl[0]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->k1, "SubKey1"));
  EXPECT_EQ(msg2.topl[0]->la[0]->k2, 100);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->k3, "SubKey2"));
  ASSERT_EQ(msg2.topl[0]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->lb[0]->k4, "InnerKey1"));
  EXPECT_EQ(msg2.topl[0]->la[0]->lb[0]->k5, 500);
  EXPECT_TRUE(msg2.topl[0]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[0]->lb[1]->k4, "InnerKey2"));
  EXPECT_EQ(msg2.topl[0]->la[0]->lb[1]->k5, 600);
  EXPECT_TRUE(msg2.topl[0]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->k1, "SubKey3"));
  EXPECT_EQ(msg2.topl[0]->la[1]->k2, 200);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->k3, "SubKey4"));
  ASSERT_EQ(msg2.topl[0]->la[1]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->lb[0]->k4, "InnerKey3"));
  EXPECT_EQ(msg2.topl[0]->la[1]->lb[0]->k5, 700);
  EXPECT_TRUE(msg2.topl[0]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[0]->la[1]->lb[1]->k4, "InnerKey4"));
  EXPECT_EQ(msg2.topl[0]->la[1]->lb[1]->k5, 800);

  EXPECT_TRUE(msg2.topl[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->k0, "Key2"));
  ASSERT_EQ(msg2.topl[1]->n_la, 3);
  EXPECT_TRUE(msg2.topl[1]->la);
  EXPECT_TRUE(msg2.topl[1]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->k1, "NewSubKey5"));
  EXPECT_EQ(msg2.topl[1]->la[0]->k2, 300);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->k3, "SubKey6"));
  ASSERT_EQ(msg2.topl[1]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->lb[0]->k4, "InnerKey5"));
  EXPECT_EQ(msg2.topl[1]->la[0]->lb[0]->k5, 900);
  EXPECT_TRUE(msg2.topl[1]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[0]->lb[1]->k4, "InnerKey6"));
  EXPECT_EQ(msg2.topl[1]->la[0]->lb[1]->k5, 1000);
  EXPECT_TRUE(msg2.topl[1]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->k1,"SubKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->k2, 400);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->k3, "SubKey8"));
  ASSERT_EQ(msg2.topl[1]->la[1]->n_lb, 3);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[0]->k4, "NewInnerKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[0]->k5, 1100);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[1]->k4, "InnerKey8"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[1]->k5, 1200);
  EXPECT_TRUE(msg2.topl[1]->la[1]->lb[2]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[1]->lb[2]->k4, "InnerKey7"));
  EXPECT_EQ(msg2.topl[1]->la[1]->lb[2]->k5, 1100);

  EXPECT_TRUE(msg2.topl[1]->la[2]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[2]->k1, "SubKey5"));
  EXPECT_EQ(msg2.topl[1]->la[2]->k2, 300);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[2]->k3, "SubKey6"));
  ASSERT_EQ(msg2.topl[1]->la[2]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[1]->la[2]->lb);
  EXPECT_TRUE(msg2.topl[1]->la[2]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[2]->lb[0]->k4, "InnerKey5"));
  EXPECT_EQ(msg2.topl[1]->la[2]->lb[0]->k5, 900);
  EXPECT_TRUE(msg2.topl[1]->la[2]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[1]->la[2]->lb[1]->k4, "InnerKey6"));
  EXPECT_EQ(msg2.topl[1]->la[2]->lb[1]->k5, 1000);

  EXPECT_TRUE(msg2.topl[2]);
  EXPECT_TRUE(msg2.topl[2]->k0);
  EXPECT_FALSE(strcmp(msg2.topl[2]->k0,"Key1"));
  ASSERT_EQ(msg2.topl[2]->n_la, 2);
  EXPECT_TRUE(msg2.topl[2]->la);
  EXPECT_TRUE(msg2.topl[2]->la[0]);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[0]->k1, "SubKey1"));
  EXPECT_EQ(msg2.topl[2]->la[0]->k2, 100);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[0]->k3, "SubKey2"));
  ASSERT_EQ(msg2.topl[2]->la[0]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[2]->la[0]->lb);
  EXPECT_TRUE(msg2.topl[2]->la[0]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[0]->lb[0]->k4, "InnerKey1"));
  EXPECT_EQ(msg2.topl[2]->la[0]->lb[0]->k5, 500);
  EXPECT_TRUE(msg2.topl[2]->la[0]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[0]->lb[1]->k4, "InnerKey2"));
  EXPECT_EQ(msg2.topl[2]->la[0]->lb[1]->k5, 600);
  EXPECT_TRUE(msg2.topl[2]->la[1]);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[1]->k1, "SubKey3"));
  EXPECT_EQ(msg2.topl[2]->la[1]->k2, 200);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[1]->k3, "SubKey4"));
  ASSERT_EQ(msg2.topl[2]->la[1]->n_lb, 2);
  EXPECT_TRUE(msg2.topl[2]->la[1]->lb);
  EXPECT_TRUE(msg2.topl[2]->la[1]->lb[0]);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[1]->lb[0]->k4, "InnerKey3"));
  EXPECT_EQ(msg2.topl[2]->la[1]->lb[0]->k5, 700);
  EXPECT_TRUE(msg2.topl[2]->la[1]->lb[1]);
  EXPECT_FALSE(strcmp(msg2.topl[2]->la[1]->lb[1]->k4, "InnerKey4"));
  EXPECT_EQ(msg2.topl[2]->la[1]->lb[1]->k5, 800);
}

TEST(RWProtobuf, MergMsgNewUF)
{
  // Test merging of unknown fields.
  RWPB_T_MSG(Company_data_Company) msg1;
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);
  init_comp_msg(&msg1, 2, 2);

  // Add unknown fields.
  auto ok = protobuf_c_message_append_unknown_serialized(nullptr,
      &msg1.base, 500, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED, (uint8_t*)"Hello", 5);
  ASSERT_TRUE(ok);

  ok = protobuf_c_message_append_unknown_serialized(nullptr,
      &msg1.base, 509, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED, (uint8_t*)"World", 5);
  ASSERT_TRUE(ok);

  RWPB_M_MSG_DECL_INIT(Company_data_Company,msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  ok = protobuf_c_message_append_unknown_serialized(nullptr,
      &msg2.base, 700, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED, (uint8_t*)"Happy", 5);
  ASSERT_TRUE(ok);

  ok = protobuf_c_message_append_unknown_serialized(nullptr,
      &msg2.base, 709, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED, (uint8_t*)"Computing", 9);
  ASSERT_TRUE(ok);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.contact_info);
  EXPECT_TRUE(msg1.contact_info);

  EXPECT_TRUE(msg2.contact_info->name);
  EXPECT_FALSE(strcmp(msg2.contact_info->name, "abc"));
  EXPECT_TRUE(msg2.contact_info->address);
  EXPECT_FALSE(strcmp(msg2.contact_info->address, "bglr"));
  EXPECT_TRUE(msg2.contact_info->phone_number);
  EXPECT_FALSE(strcmp(msg2.contact_info->phone_number, "1234"));

  ASSERT_EQ(msg2.n_employee, 2);
  ASSERT_EQ(msg2.n_employee, 2);

  EXPECT_TRUE(msg2.employee);
  EXPECT_TRUE(msg1.employee);

  ASSERT_EQ(msg2.n_product, 2);
  ASSERT_EQ(msg1.n_product, 2);

  EXPECT_TRUE(msg2.product);
  EXPECT_TRUE(msg1.product);

  EXPECT_EQ(msg2.employee[0]->id, 100);
  EXPECT_FALSE(strcmp(msg2.employee[0]->name, "Emp1"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->title, "Title"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->start_date, "1234"));

  EXPECT_EQ(msg2.employee[1]->id, 200);
  EXPECT_FALSE(strcmp(msg2.employee[1]->name, "Emp2"));
  EXPECT_FALSE(strcmp(msg2.employee[1]->title, "Title"));
  EXPECT_FALSE(strcmp(msg2.employee[1]->start_date, "1234"));

  EXPECT_EQ(msg2.product[0]->id, 1000);
  EXPECT_FALSE(strcmp(msg2.product[0]->name, "nfv"));
  EXPECT_FALSE(strcmp(msg2.product[0]->msrp, "1000"));

  EXPECT_EQ(msg2.product[1]->id, 2000);
  EXPECT_FALSE(strcmp(msg2.product[1]->name, "cloud"));
  EXPECT_FALSE(strcmp(msg2.product[1]->msrp, "2000"));

  ASSERT_EQ(protobuf_c_message_unknown_get_count(&msg2.base), 4);

  const ProtobufCMessageUnknownField* unf = protobuf_c_message_unknown_get_index(&msg2.base, 2);
  ASSERT_TRUE(unf);

  EXPECT_EQ(unf->base_.tag, 700);
  EXPECT_EQ(unf->serialized.wire_type, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(unf->serialized.len, 5);
  EXPECT_FALSE(memcmp(unf->serialized.data, "Happy", 5));

  unf = protobuf_c_message_unknown_get_index(&msg2.base, 3);
  ASSERT_TRUE(unf);

  EXPECT_EQ(unf->base_.tag, 709);
  EXPECT_EQ(unf->serialized.wire_type, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(unf->serialized.len, 9);
  EXPECT_FALSE(memcmp(unf->serialized.data, "Computing", 9));

  unf = protobuf_c_message_unknown_get_index(&msg2.base, 0);
  ASSERT_TRUE(unf);

  EXPECT_EQ(unf->base_.tag, 500);
  EXPECT_EQ(unf->serialized.wire_type, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(unf->serialized.len, 5);
  EXPECT_FALSE(memcmp(unf->serialized.data, "Hello", 5));

  unf = protobuf_c_message_unknown_get_index(&msg2.base, 1);
  ASSERT_TRUE(unf);

  EXPECT_EQ(unf->base_.tag, 509);
  EXPECT_EQ(unf->serialized.wire_type, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(unf->serialized.len, 5);
  EXPECT_FALSE(memcmp(unf->serialized.data, "World", 5));

  ASSERT_EQ(protobuf_c_message_unknown_get_count(&msg1.base), 2);
  unf = protobuf_c_message_unknown_get_index(&msg1.base, 0);
  ASSERT_TRUE(unf);

  EXPECT_EQ(unf->base_.tag, 500);
  EXPECT_EQ(unf->serialized.wire_type, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(unf->serialized.len, 5);
  EXPECT_FALSE(memcmp(unf->serialized.data, "Hello", 5));

  unf = protobuf_c_message_unknown_get_index(&msg1.base, 1);
  ASSERT_TRUE(unf);

  EXPECT_EQ(unf->base_.tag, 509);
  EXPECT_EQ(unf->serialized.wire_type, PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(unf->serialized.len, 5);
  EXPECT_FALSE(memcmp(unf->serialized.data, "World", 5));

}

TEST(RWProtobuf, MergMsgFDs)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb, msg2);

  strcpy(msg1.name, "slb1");
  strcpy(msg2.name, "slb1");
  msg1.has_name = 1;
  msg2.has_name = 1;
  msg1.has_receive_function = 1;
  msg1.receive_function.has_builtin_basic_lb = 1;
  msg1.receive_function.builtin_basic_lb.has_address = 1;
  strcpy(msg1.receive_function.builtin_basic_lb.address, "127.0.0.1");
  msg1.receive_function.builtin_basic_lb.has_domain_name = 1;
  strcpy(msg1.receive_function.builtin_basic_lb.domain_name, "bslb1");
  msg1.receive_function.builtin_basic_lb.has_ip_proto = 1;
  msg1.receive_function.builtin_basic_lb.ip_proto = RW_FPATH_D_IP_PROTO_PROTO_UDP;
  msg1.receive_function.builtin_basic_lb.has_port = 1;
  msg1.receive_function.builtin_basic_lb.port = 5060;

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_STREQ(msg2.name, "slb1");
  EXPECT_TRUE(msg2.has_receive_function);
  EXPECT_TRUE(msg2.receive_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_address);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.address, "127.0.0.1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_domain_name);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.domain_name, "bslb1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_ip_proto);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_port);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.port, 5060);

  msg1.receive_function.has_receive_script = 1;
  msg1.receive_function.receive_script.has_script_name = 1;
  strcpy(msg1.receive_function.receive_script.script_name, "script1");

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_STREQ(msg2.name, "slb1");
  EXPECT_TRUE(msg2.has_receive_function);
  EXPECT_TRUE(msg2.receive_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_address);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.address, "127.0.0.1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_domain_name);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.domain_name, "bslb1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_ip_proto);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_port);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.port, 5060);
  EXPECT_TRUE(msg2.receive_function.has_receive_script);
  EXPECT_TRUE(msg2.receive_function.receive_script.has_script_name);
  EXPECT_STREQ(msg2.receive_function.receive_script.script_name, "script1");

  msg1.has_classify_function = 1;
  msg1.classify_function.has_builtin_basic_lb = 1;
  msg1.classify_function.builtin_basic_lb.has_key_type = 1;
  msg1.classify_function.builtin_basic_lb.key_type = RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_STREQ(msg2.name, "slb1");
  EXPECT_TRUE(msg2.has_receive_function);
  EXPECT_TRUE(msg2.receive_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_address);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.address, "127.0.0.1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_domain_name);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.domain_name, "bslb1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_ip_proto);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_port);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.port, 5060);
  EXPECT_TRUE(msg2.receive_function.has_receive_script);
  EXPECT_TRUE(msg2.receive_function.receive_script.has_script_name);
  EXPECT_STREQ(msg2.receive_function.receive_script.script_name, "script1");
  EXPECT_TRUE(msg2.has_classify_function);
  EXPECT_TRUE(msg2.classify_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.classify_function.builtin_basic_lb.has_key_type);
  EXPECT_EQ(msg2.classify_function.builtin_basic_lb.key_type, RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE);


  msg1.classify_function.has_classify_script = 1;
  msg1.classify_function.classify_script.has_script_name = 1;
  strcpy(msg1.classify_function.classify_script.script_name, "classifyscript");

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_STREQ(msg2.name, "slb1");
  EXPECT_TRUE(msg2.has_receive_function);
  EXPECT_TRUE(msg2.receive_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_address);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.address, "127.0.0.1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_domain_name);
  EXPECT_STREQ(msg2.receive_function.builtin_basic_lb.domain_name, "bslb1");
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_ip_proto);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.ip_proto, RW_FPATH_D_IP_PROTO_PROTO_UDP);
  EXPECT_TRUE(msg2.receive_function.builtin_basic_lb.has_port);
  EXPECT_EQ(msg2.receive_function.builtin_basic_lb.port, 5060);
  EXPECT_TRUE(msg2.receive_function.has_receive_script);
  EXPECT_TRUE(msg2.receive_function.receive_script.has_script_name);
  EXPECT_STREQ(msg2.receive_function.receive_script.script_name, "script1");
  EXPECT_TRUE(msg2.has_classify_function);
  EXPECT_TRUE(msg2.classify_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.classify_function.builtin_basic_lb.has_key_type);
  EXPECT_EQ(msg2.classify_function.builtin_basic_lb.key_type, RW_FPATH_D_KEY_TYPE_BASIC_5TUPLE);
  EXPECT_TRUE(msg2.classify_function.has_classify_script);
  EXPECT_TRUE(msg2.classify_function.classify_script.has_script_name);
  EXPECT_STREQ(msg2.classify_function.classify_script.script_name, "classifyscript");

  msg1.has_server_selection_function = 1;
  msg1.server_selection_function.n_server_group = 1;
  strcpy(msg1.server_selection_function.server_group[0].name, "ServerGName");
  msg1.server_selection_function.server_group[0].has_name = 1;
  msg1.server_selection_function.server_group[0].n_server = 1;
  strcpy(msg1.server_selection_function.server_group[0].server[0].address, "Server1");
  msg1.server_selection_function.server_group[0].server[0].has_address = 1;
  msg1.server_selection_function.server_group[0].server[0].has_port = 1;
  msg1.server_selection_function.server_group[0].server[0].port = 5061;
  msg1.server_selection_function.server_group[0].has_nat_address = 1;
  msg1.server_selection_function.server_group[0].nat_address.has_src_address = 1;
  strcpy(msg1.server_selection_function.server_group[0].nat_address.src_address, "192.168.1.2");
  msg1.server_selection_function.server_group[0].nat_address.has_src_port = 1;
  msg1.server_selection_function.server_group[0].nat_address.src_port = 6060;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.has_server_selection_function);
  EXPECT_TRUE(msg2.server_selection_function.n_server_group);
  EXPECT_STREQ(msg2.server_selection_function.server_group[0].name, "ServerGName");
  msg2.server_selection_function.server_group[0].has_name = 1;
  EXPECT_TRUE(msg2.server_selection_function.server_group[0].n_server);
  EXPECT_STREQ(msg2.server_selection_function.server_group[0].server[0].address, "Server1");
  msg2.server_selection_function.server_group[0].server[0].has_address = 1;
  EXPECT_TRUE(msg2.server_selection_function.server_group[0].server[0].has_port);
  EXPECT_EQ(msg2.server_selection_function.server_group[0].server[0].port, 5061);
  EXPECT_TRUE(msg2.server_selection_function.server_group[0].has_nat_address);
  EXPECT_TRUE(msg2.server_selection_function.server_group[0].nat_address.has_src_address);
  EXPECT_STREQ(msg2.server_selection_function.server_group[0].nat_address.src_address, "192.168.1.2");
  EXPECT_TRUE(msg2.server_selection_function.server_group[0].nat_address.has_src_port);
  EXPECT_EQ(msg2.server_selection_function.server_group[0].nat_address.src_port, 6060);

  msg1.server_selection_function.has_builtin_basic_lb = 1;
  msg1.server_selection_function.builtin_basic_lb.has_selection_type = 1;
  msg1.server_selection_function.builtin_basic_lb.selection_type = RW_FPATH_D_SELECTION_TYPE_BASIC_ROUND_ROBIN;
  msg1.server_selection_function.has_server_selection_script = 1;
  msg1.server_selection_function.server_selection_script.has_script_name = 1;
  strcpy(msg1.server_selection_function.server_selection_script.script_name, "SSS");
  msg1.server_selection_function.has_selection_criteria = 1;
  msg1.server_selection_function.selection_criteria.has_dns_lb = 1;
  msg1.server_selection_function.selection_criteria.dns_lb.n_match_rule = 1;
  strcpy(msg1.server_selection_function.selection_criteria.dns_lb.match_rule[0].domain, "domain1");
  msg1.server_selection_function.selection_criteria.dns_lb.match_rule[0].has_server_group = 1;
  strcpy(msg1.server_selection_function.selection_criteria.dns_lb.match_rule[0].server_group, "Group1");

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.server_selection_function.has_builtin_basic_lb);
  EXPECT_TRUE(msg2.server_selection_function.builtin_basic_lb.has_selection_type);
  EXPECT_EQ(msg2.server_selection_function.builtin_basic_lb.selection_type, RW_FPATH_D_SELECTION_TYPE_BASIC_ROUND_ROBIN);
  EXPECT_TRUE(msg2.server_selection_function.has_server_selection_script);
  EXPECT_TRUE(msg2.server_selection_function.server_selection_script.has_script_name);
  EXPECT_STREQ(msg2.server_selection_function.server_selection_script.script_name, "SSS");
  EXPECT_TRUE(msg2.server_selection_function.has_selection_criteria);
  EXPECT_TRUE(msg2.server_selection_function.selection_criteria.has_dns_lb);
  EXPECT_TRUE(msg2.server_selection_function.selection_criteria.dns_lb.n_match_rule);
  EXPECT_STREQ(msg2.server_selection_function.selection_criteria.dns_lb.match_rule[0].domain, "domain1");

  msg1.has_transform_function = 1;
  msg1.transform_function.has_builtin_transform = 1;
  msg1.transform_function.builtin_transform.has_transform_type = 1;
  msg1.transform_function.builtin_transform.transform_type = RW_FPATH_D_TRANSFORM_TYPE_DOUBLE_NAT;

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.has_transform_function);
  EXPECT_TRUE(msg2.transform_function.has_builtin_transform);
  EXPECT_TRUE(msg2.transform_function.builtin_transform.has_transform_type);
  EXPECT_EQ(msg2.transform_function.builtin_transform.transform_type, RW_FPATH_D_TRANSFORM_TYPE_DOUBLE_NAT);

  msg1.transform_function.has_transform_script = 1;
  msg1.transform_function.transform_script.has_script_name = 1;
  strcpy(msg1.transform_function.transform_script.script_name, "TSCN");

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  EXPECT_TRUE(ret);

  EXPECT_TRUE(msg2.transform_function.has_transform_script);
  EXPECT_TRUE(msg2.transform_function.transform_script.has_script_name);
  EXPECT_STREQ(msg2.transform_function.transform_script.script_name, "TSCN");

  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &msg1.base, &msg2.base));
}


TEST(RWProtobuf, MergMsgNewListyIn)
{
  // Lists without keys (inline)
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_Ipv4_NexthopRecord, msg2);

  msg1.n_nhrec = 2;
  for (unsigned i = 0; i < 2; i++) {
    msg1.nhrec[i].nhrec = i+1;
    msg1.nhrec[i].has_nhrec = 1;
    msg1.nhrec[i].n_nexthops = 3;
    for (unsigned j = 0; j < 3; j++) {
      msg1.nhrec[i].nexthops[j].has_type = true;
      strcpy(msg1.nhrec[i].nexthops[j].type, "10.0.0.1");
      msg1.nhrec[i].nexthops[j].has_ifindex = true;
      msg1.nhrec[i].nexthops[j].ifindex = 10;
      msg1.nhrec[i].nexthops[j].has_gateway = true;
      strcpy(msg1.nhrec[i].nexthops[j].gateway, "10.0.0.20");
      msg1.nhrec[i].nexthops[j].has_lportname = true;
      strcpy(msg1.nhrec[i].nexthops[j].lportname, "port0");
    }
  }

  msg2.n_nhrec = 2;
  for (unsigned i = 0; i < 2; i++) {
    msg2.nhrec[i].nhrec = i+1;
    msg2.nhrec[i].has_nhrec = 1;
    msg2.nhrec[i].n_nexthops = 3;
    for (unsigned j = 0; j < 3; j++) {
      msg2.nhrec[i].nexthops[j].has_type = true;
      strcpy(msg2.nhrec[i].nexthops[j].type, "10.0.0.2");
      msg2.nhrec[i].nexthops[j].has_ifindex = true;
      msg2.nhrec[i].nexthops[j].ifindex = 20;
      msg2.nhrec[i].nexthops[j].has_gateway = true;
      strcpy(msg2.nhrec[i].nexthops[j].gateway, "10.0.0.30");
      msg2.nhrec[i].nexthops[j].has_lportname = true;
      strcpy(msg2.nhrec[i].nexthops[j].lportname, "port1");
    }
  }

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  EXPECT_EQ(msg2.n_nhrec, 2);
  for (unsigned i = 0; i < 2; i++) {
    EXPECT_EQ(msg2.nhrec[i].nhrec, i+1);
    EXPECT_EQ(msg2.nhrec[i].n_nexthops, 6);
    for (unsigned j = 0; j < 3; j++) {
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_type);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].type, "10.0.0.1");
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_ifindex);
      EXPECT_EQ(msg2.nhrec[i].nexthops[j].ifindex, 10);
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_gateway);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].gateway, "10.0.0.20");
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_lportname);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].lportname, "port0");
    }

    for (unsigned j = 3; j < 6; j++) {
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_type);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].type, "10.0.0.2");
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_ifindex);
      EXPECT_EQ(msg2.nhrec[i].nexthops[j].ifindex, 20);
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_gateway);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].gateway, "10.0.0.30");
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_lportname);
      strcpy(msg2.nhrec[i].nexthops[j].lportname, "port1");
    }
  }

  msg2.nhrec[0].n_nexthops = 0;
  msg2.nhrec[1].n_nexthops = 0;
  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  EXPECT_EQ(msg2.n_nhrec, 2);
  for (unsigned i = 0; i < 2; i++) {
    EXPECT_EQ(msg2.nhrec[i].nhrec, i+1);
    EXPECT_EQ(msg2.nhrec[i].n_nexthops, 3);

    for (unsigned j = 0; j < 3; j++) {
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_type);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].type, "10.0.0.1");
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_ifindex);
      EXPECT_EQ(msg2.nhrec[i].nexthops[j].ifindex, 10);
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_gateway);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].gateway, "10.0.0.20");
      EXPECT_TRUE(msg2.nhrec[i].nexthops[j].has_lportname);
      EXPECT_STREQ(msg2.nhrec[i].nexthops[j].lportname, "port0");
    }
  }
}

TEST(RWProtobuf, MergeMsgsNewListyNI)
{
  // Lists without keys not-inline
  RWPB_M_MSG_DECL_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg, msg1);
  RWPB_M_MSG_DECL_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg, msg2);

  msg1.n_brokers = 4;
  msg1.brokers = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers) **)malloc(sizeof(void*)*4);
  for (unsigned i = 0; i < 4; i++) {
    msg1.brokers[i] = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers) *)malloc(sizeof(RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers)));
    RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers, msg1.brokers[i]);
    msg1.brokers[i]->instance_uri = strdup("/BROKER/1");
    msg1.brokers[i]->n_channels = 1;
    msg1.brokers[i]->channels = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels) **)malloc(sizeof(void*)*1);
    msg1.brokers[i]->channels[0] = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels) *)malloc(sizeof(RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels)));
    RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels, msg1.brokers[i]->channels[0]);
    msg1.brokers[i]->channels[0]->has_type = true;
    msg1.brokers[i]->channels[0]->type = RWMSG_DATA_D_RWMSG_CHANNEL_YENUM_SRV;
    msg1.brokers[i]->channels[0]->has_clict = true;
    msg1.brokers[i]->channels[0]->clict = 100;
    msg1.brokers[i]->channels[0]->has_id = true;
    msg1.brokers[i]->channels[0]->id = 101;
    msg1.brokers[i]->channels[0]->has_peer_id = true;
    msg1.brokers[i]->channels[0]->peer_id = 102;
    msg1.brokers[i]->channels[0]->has_peer_ip = true;
    strcpy(msg1.brokers[i]->channels[0]->peer_ip, "10.0.0.1");
    msg1.brokers[i]->channels[0]->n_methbindings = 1;
    msg1.brokers[i]->channels[0]->methbindings = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings) **)malloc(sizeof(void*)*1);
    msg1.brokers[i]->channels[0]->methbindings[0] = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings) *)malloc(sizeof(RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings)));
    RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings, msg1.brokers[i]->channels[0]->methbindings[0]);
    msg1.brokers[i]->channels[0]->methbindings[0]->has_btype = true;
    msg1.brokers[i]->channels[0]->methbindings[0]->btype = RWMSG_DATA_D_RWMSG_METHB_YENUM_BROSRVCHAN;
    msg1.brokers[i]->channels[0]->methbindings[0]->has_path = true;
    strcpy(msg1.brokers[i]->channels[0]->methbindings[0]->path, "/tmp");
    msg1.brokers[i]->channels[0]->methbindings[0]->has_pathhash = true;
    msg1.brokers[i]->channels[0]->methbindings[0]->pathhash = 1000;
  }

  msg2.n_brokers = 10;
  msg2.brokers = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers) **)malloc(sizeof(void*)*10);
  for (unsigned i = 0; i < 10; i++) {
    msg2.brokers[i] = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers) *)malloc(sizeof(RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers)));
    RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers, msg2.brokers[i]);
    msg2.brokers[i]->instance_uri = strdup("/BROKER/2");
    msg2.brokers[i]->n_channels = 1;
    msg2.brokers[i]->channels = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels) **)malloc(sizeof(void*)*1);
    msg2.brokers[i]->channels[0] = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels) *)malloc(sizeof(RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels)));
    RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels, msg2.brokers[i]->channels[0]);
    msg2.brokers[i]->channels[0]->has_type = true;
    msg2.brokers[i]->channels[0]->type = RWMSG_DATA_D_RWMSG_CHANNEL_YENUM_SRV;
    msg2.brokers[i]->channels[0]->has_clict = true;
    msg2.brokers[i]->channels[0]->clict = 100;
    msg2.brokers[i]->channels[0]->has_id = true;
    msg2.brokers[i]->channels[0]->id = 101;
    msg2.brokers[i]->channels[0]->has_peer_id = true;
    msg2.brokers[i]->channels[0]->peer_id = 102;
    msg2.brokers[i]->channels[0]->has_peer_ip = true;
    strcpy(msg2.brokers[i]->channels[0]->peer_ip, "10.0.0.1");
    msg2.brokers[i]->channels[0]->n_methbindings = 1;
    msg2.brokers[i]->channels[0]->methbindings = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings) **)malloc(sizeof(void*)*1);
    msg2.brokers[i]->channels[0]->methbindings[0] = (RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings) *)malloc(sizeof(RWPB_T_MSG(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings)));
    RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg_Brokers_Channels_Methbindings, msg2.brokers[i]->channels[0]->methbindings[0]);
    msg2.brokers[i]->channels[0]->methbindings[0]->has_btype = true;
    msg2.brokers[i]->channels[0]->methbindings[0]->btype = RWMSG_DATA_D_RWMSG_METHB_YENUM_BROSRVCHAN;
    msg2.brokers[i]->channels[0]->methbindings[0]->has_path = true;
    strcpy(msg2.brokers[i]->channels[0]->methbindings[0]->path, "/tmp");
    msg2.brokers[i]->channels[0]->methbindings[0]->has_pathhash = true;
    msg2.brokers[i]->channels[0]->methbindings[0]->pathhash = 1000;
  }

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);
  EXPECT_EQ(msg2.n_brokers, 14);

  for (unsigned i = 0; i < 4; i++) {
    EXPECT_STREQ(msg2.brokers[i]->instance_uri, "/BROKER/1");
  }

  for (unsigned i = 4; i < 14; i++) {
    EXPECT_STREQ(msg2.brokers[i]->instance_uri, "/BROKER/2");
  }

  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
  RWPB_F_MSG_INIT(RwmsgDataD_RwBaseD_data_Messaging_Rwmsg, &msg2);

  ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);
  EXPECT_EQ(msg2.n_brokers, 4);
  for (unsigned i = 0; i < 4; i++) {
    EXPECT_STREQ(msg2.brokers[i]->instance_uri, "/BROKER/1");
  }
}

TEST(RWProtobuf, MergeLeafLists)
{
  //leaf-lists (string & binary) (not-inline)
  RWPB_M_MSG_DECL_INIT(TestYdomTop_data_Top_ApisTest, msg1);
  RWPB_M_MSG_DECL_INIT(TestYdomTop_data_Top_ApisTest, msg2);

  msg1.has_lint8 = true;
  msg1.lint8 = 100;
  msg1.has_luint64 = true;
  msg1.luint64 = 200;
  msg1.n_leaflist = 4;
  msg1.leaflist = (char **)malloc(sizeof(void*)*4);
  msg1.leaflist[0] = strdup("Hello");
  msg1.leaflist[1] = strdup("World");
  msg1.leaflist[2] = strdup("Happy");
  msg1.leaflist[3] = strdup("Computing");
  msg1.n_primlist = 10;
  msg1.primlist = (int32_t *)malloc(sizeof(int32_t)*10);
  for (unsigned i = 0; i < 10; i++) {
    msg1.primlist[i] = i+1;
  }
  msg1.binary_test = (RWPB_T_MSG(TestYdomTop_data_Top_ApisTest_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestYdomTop_data_Top_ApisTest_BinaryTest)));
  RWPB_F_MSG_INIT(TestYdomTop_data_Top_ApisTest_BinaryTest, msg1.binary_test);
  msg1.binary_test->n_binary_list = 2;
  msg1.binary_test->binary_list = (ProtobufCBinaryData *)malloc(sizeof(ProtobufCBinaryData)*2);
  msg1.binary_test->binary_list[0].data = (uint8_t *)strdup("Aim for");
  msg1.binary_test->binary_list[0].len = 8;
  msg1.binary_test->binary_list[1].data = (uint8_t *)strdup("Bug free code");
  msg1.binary_test->binary_list[1].len = 14;

  msg2.lstring = strdup("Rift");
  msg2.has_lenum = true;
  msg2.lenum = TEST_YDOM_TOP_ENUM1_RWCAT_E_A;
  msg2.n_leaflist = 3;
  msg2.leaflist = (char **)malloc(sizeof(void*)*3);
  msg2.leaflist[0] = strdup("Cloud");
  msg2.leaflist[1] = strdup("Computing");
  msg2.leaflist[2] = strdup("RiftIo");
  msg2.n_primlist = 5;
  msg2.primlist = (int32_t *)malloc(sizeof(int32_t)*5);
  for (unsigned i = 0; i < 5; i++) {
    msg2.primlist[i] = 101+i;
  }
  msg2.binary_test = (RWPB_T_MSG(TestYdomTop_data_Top_ApisTest_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestYdomTop_data_Top_ApisTest_BinaryTest)));
  RWPB_F_MSG_INIT(TestYdomTop_data_Top_ApisTest_BinaryTest, msg2.binary_test);
  msg2.binary_test->n_binary_list = 3;
  msg2.binary_test->binary_list = (ProtobufCBinaryData *)malloc(sizeof(ProtobufCBinaryData)*3);
  msg2.binary_test->binary_list[0].data = (uint8_t *)strdup("Distributed");
  msg2.binary_test->binary_list[0].len = 12;
  msg2.binary_test->binary_list[1].data = (uint8_t *)strdup("Systems");
  msg2.binary_test->binary_list[1].len = 8;
  msg2.binary_test->binary_list[2].data = (uint8_t *)strdup("World");
  msg2.binary_test->binary_list[2].len = 6;

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  EXPECT_TRUE(msg2.has_lint8);
  EXPECT_EQ(msg2.lint8, 100);
  EXPECT_TRUE(msg2.has_luint64);
  EXPECT_EQ(msg2.luint64, 200);
  EXPECT_STREQ(msg2.lstring, "Rift");
  EXPECT_TRUE(msg2.has_lenum);
  EXPECT_EQ(msg2.lenum, TEST_YDOM_TOP_ENUM1_RWCAT_E_A);
  EXPECT_EQ(msg2.n_leaflist, 7);

  EXPECT_STREQ(msg2.leaflist[0], "Hello");
  EXPECT_STREQ(msg2.leaflist[1], "World");
  EXPECT_STREQ(msg2.leaflist[2], "Happy");
  EXPECT_STREQ(msg2.leaflist[3], "Computing");
  EXPECT_STREQ(msg2.leaflist[4], "Cloud");
  EXPECT_STREQ(msg2.leaflist[5], "Computing");
  EXPECT_STREQ(msg2.leaflist[6], "RiftIo");

  EXPECT_EQ(msg2.n_primlist, 15);
  for (unsigned i = 0; i < 10; i++) {
    EXPECT_EQ(msg2.primlist[i], i+1);
  }
  for (unsigned i = 10; i < 15; i++) {
    EXPECT_EQ(msg2.primlist[i], 101+i-10);
  }

  EXPECT_TRUE(msg2.binary_test);
  EXPECT_EQ(msg2.binary_test->n_binary_list, 5);
  EXPECT_STREQ((char *)msg2.binary_test->binary_list[0].data, "Aim for");
  EXPECT_EQ(msg2.binary_test->binary_list[0].len, 8);
  EXPECT_STREQ((char *)msg2.binary_test->binary_list[1].data, "Bug free code");
  EXPECT_EQ(msg2.binary_test->binary_list[1].len, 14);
  EXPECT_STREQ((char *)msg2.binary_test->binary_list[2].data, "Distributed");
  EXPECT_EQ(msg2.binary_test->binary_list[2].len, 12);
  EXPECT_STREQ((char *)msg2.binary_test->binary_list[3].data, "Systems");
  EXPECT_EQ(msg2.binary_test->binary_list[3].len, 8);
  EXPECT_STREQ((char *)msg2.binary_test->binary_list[4].data, "World");
  EXPECT_EQ(msg2.binary_test->binary_list[4].len, 6);
}

TEST(RWProtobuf, MergeLeafListsNL)
{
  //leaf-lists (inline).
  RWPB_M_MSG_DECL_INIT(TestYdomTop_data_Top_A_ContInA, msg1);
  RWPB_M_MSG_DECL_INIT(TestYdomTop_data_Top_A_ContInA, msg2);

  msg1.n_ls = 4;
  for (unsigned i = 0; i < 4; i++) {
    sprintf(msg1.ls[i], "String %d", i);
  }
  msg1.n_ll = 10;
  for (unsigned i = 0; i < 10; i++) {
    msg1.ll[i] = TEST_YDOM_TOP_ENUM1_RWCAT_E_E;
  }

  msg2.n_ls = 6;
  for (unsigned i = 0; i < 6; i++) {
    sprintf(msg2.ls[i], "Msg %d", i);
  }
  msg2.n_ll = 30;
  for (unsigned i = 0; i < 30; i++) {
    msg2.ll[i] = TEST_YDOM_TOP_ENUM1_RWCAT_E_C;
  }

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  protobuf_c_boolean ret = protobuf_c_message_merge_new(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  EXPECT_EQ(msg2.n_ls, 10);
  for (unsigned i = 0; i < 4; i++) {
    char temp[64];
    sprintf(temp, "String %d", i);
    EXPECT_STREQ(msg2.ls[i], temp);
  }
  for (unsigned i = 4; i < 10; i++) {
    char temp[64];
    sprintf(temp, "Msg %d", i-4);
    EXPECT_STREQ(msg2.ls[i], temp);
  }

  EXPECT_EQ(msg2.n_ll, 40);
  for (unsigned i = 0; i < 10; i++) {
    EXPECT_EQ(msg2.ll[i], TEST_YDOM_TOP_ENUM1_RWCAT_E_E);
  }
  for (unsigned i = 11; i < 40; i++) {
    EXPECT_EQ(msg2.ll[i], TEST_YDOM_TOP_ENUM1_RWCAT_E_C);
  }
}

TEST(RWProtobuf, UnpackMerge)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company, msg1);
  RWPB_M_MSG_DECL_INIT(Company_data_Company, msg2);

  msg1.contact_info = (RWPB_T_MSG(Company_data_Company_ContactInfo)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_ContactInfo)));
  RWPB_F_MSG_INIT(Company_data_Company_ContactInfo, msg1.contact_info);

  msg1.contact_info->name = strdup("RiftIO");
  msg1.contact_info->address = strdup("Burlington");
  msg1.contact_info->phone_number = strdup("123456");

  msg1.n_employee = 2;
  msg1.employee = (RWPB_T_MSG(Company_data_Company_Employee)**)malloc(sizeof(void*)*2);
  msg1.employee[0] =  (RWPB_T_MSG(Company_data_Company_Employee)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee, msg1.employee[0]);
  msg1.employee[0]->id = 100;
  msg1.employee[0]->has_id = 1;
  msg1.employee[0]->name = strdup("ABCD");
  msg1.employee[0]->title = strdup("MTS");

  msg1.employee[1] =  (RWPB_T_MSG(Company_data_Company_Employee)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee, msg1.employee[1]);
  msg1.employee[1]->id = 200;
  msg1.employee[1]->has_id = 1;
  msg1.employee[1]->name = strdup("EFGH");
  msg1.employee[1]->title = strdup("MTS");

  msg1.n_product = 3;
  msg1.product = (RWPB_T_MSG(Company_data_Company_Product)**)malloc(sizeof(void*)*3);
  for (unsigned i = 0; i < 3; i++) {
    msg1.product[i] = (RWPB_T_MSG(Company_data_Company_Product)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Product)));
    RWPB_F_MSG_INIT(Company_data_Company_Product, msg1.product[i]);
    msg1.product[i]->id = i+1;
    msg1.product[i]->has_id = 1;
    msg1.product[i]->name = strdup("CloudPF");
  }

  msg2.n_employee = 2;
  msg2.employee = (RWPB_T_MSG(Company_data_Company_Employee)**)malloc(sizeof(void*)*2);
  msg2.employee[0] =  (RWPB_T_MSG(Company_data_Company_Employee)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee, msg2.employee[0]);
  msg2.employee[0]->id = 300;
  msg2.employee[0]->has_id = 1;
  msg2.employee[0]->name = strdup("ABCD");
  msg2.employee[0]->title = strdup("MTS");

  msg2.employee[1] =  (RWPB_T_MSG(Company_data_Company_Employee)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee, msg2.employee[1]);
  msg2.employee[1]->id = 200;
  msg2.employee[1]->has_id = 1;
  msg2.employee[1]->name = strdup("EFGH");
  msg2.employee[1]->title = strdup("PMTS");

  msg2.n_product = 3;
  msg2.product = (RWPB_T_MSG(Company_data_Company_Product)**)malloc(sizeof(void*)*3);
  for (unsigned i = 0; i < 3; i++) {
    msg2.product[i] = (RWPB_T_MSG(Company_data_Company_Product)*)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Product)));
    RWPB_F_MSG_INIT(Company_data_Company_Product, msg2.product[i]);
    msg2.product[i]->id = i+1;
    msg2.product[i]->has_id = 1;
    msg2.product[i]->name = strdup("RiftWare");
    msg2.product[i]->msrp = strdup("UL");
  }

  uint8_t buff[2048];
  size_t len1 = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  size_t len2 = protobuf_c_message_get_packed_size(NULL, &msg2.base);

  protobuf_c_message_pack(NULL, &msg1.base, buff);
  protobuf_c_message_pack(NULL, &msg2.base, buff+len1);

  auto out = (RWPB_T_MSG(Company_data_Company)*)protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(Company_data_Company), len1+len2, buff);
  ASSERT_TRUE(out);

  ASSERT_TRUE(out->contact_info);
  EXPECT_STREQ(out->contact_info->name, "RiftIO");
  EXPECT_STREQ(out->contact_info->address, "Burlington");
  EXPECT_STREQ(out->contact_info->phone_number, "123456");

  EXPECT_EQ(out->n_employee, 3);
  ASSERT_TRUE(out->employee);
  ASSERT_TRUE(out->employee[0]);
  EXPECT_EQ(out->employee[0]->id, 100);
  EXPECT_STREQ(out->employee[0]->name, "ABCD");
  EXPECT_STREQ(out->employee[0]->title, "MTS");

  ASSERT_TRUE(out->employee[1]);
  EXPECT_EQ(out->employee[1]->id, 200);
  EXPECT_STREQ(out->employee[1]->name, "EFGH");
  EXPECT_STREQ(out->employee[1]->title, "PMTS");

  ASSERT_TRUE(out->employee[2]);
  EXPECT_EQ(out->employee[2]->id, 300);
  EXPECT_STREQ(out->employee[2]->name, "ABCD");
  EXPECT_STREQ(out->employee[2]->title, "MTS");

  EXPECT_EQ(out->n_product, 3);
  ASSERT_TRUE(out->product);
  for (unsigned i = 0; i < 3; i++) {
    ASSERT_TRUE(out->product[i]);
    EXPECT_EQ(out->product[i]->id, i+1);
    EXPECT_STREQ(out->product[i]->name, "RiftWare");
    EXPECT_STREQ(out->product[i]->msrp, "UL");
  }

  protobuf_c_message_free_unpacked(NULL, &out->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
}
 
TEST(RWProtobuf, UnpackMergeInline)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, msg1);
  strcpy(msg1.name, "trafgen");
  msg1.n_bundle_ether = 2;
  strcpy(msg1.bundle_ether[0].name, "bundle1");
  msg1.bundle_ether[0].has_name = 1;
  msg1.bundle_ether[0].has_lacp = true;
  msg1.bundle_ether[0].lacp.has_system = true;
  msg1.bundle_ether[0].lacp.system.has_priority = true;
  msg1.bundle_ether[0].lacp.system.priority = 2;
  msg1.bundle_ether[0].lacp.system.has_mac = true;
  strcpy(msg1.bundle_ether[0].lacp.system.mac, "AA:AA:AA:AA:AA:AA");
  msg1.bundle_ether[0].has_bundle = true;
  msg1.bundle_ether[0].bundle.has_minimum_active = true;
  msg1.bundle_ether[0].bundle.minimum_active.has_links = true;
  msg1.bundle_ether[0].bundle.minimum_active.links = 20;
  msg1.bundle_ether[0].bundle.has_maximum_active = true;
  msg1.bundle_ether[0].bundle.maximum_active.has_links = true;
  msg1.bundle_ether[0].bundle.maximum_active.links = 100;
  msg1.bundle_ether[0].bundle.has_load_balance = true;
  msg1.bundle_ether[0].bundle.load_balance = RW_FPATH_D_LAG_LB_MODE_MAC_IP;

  strcpy(msg1.bundle_ether[1].name, "bundle2");
  msg1.bundle_ether[1].has_name = 1;
  msg1.bundle_ether[1].has_lacp = true;
  msg1.bundle_ether[1].lacp.has_system = true;
  msg1.bundle_ether[1].lacp.system.has_priority = true;
  msg1.bundle_ether[1].lacp.system.priority = 1;
  msg1.bundle_ether[1].lacp.system.has_mac = true;
  strcpy(msg1.bundle_ether[1].lacp.system.mac, "BA:AA:AA:AA:AA:AA");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, msg2);
  strcpy(msg2.name, "trafgen");
  msg2.n_bundle_ether = 2;
  strcpy(msg2.bundle_ether[0].name, "bundle1");
  msg2.bundle_ether[0].has_name = 1;
  msg2.bundle_ether[0].has_lacp = true;
  msg2.bundle_ether[0].lacp.has_system = true;
  msg2.bundle_ether[0].lacp.system.has_priority = true;
  msg2.bundle_ether[0].lacp.system.priority = 3;
  msg2.bundle_ether[0].lacp.system.has_mac = true;
  strcpy(msg2.bundle_ether[0].lacp.system.mac, "CC:AA:AA:AA:AA:AA");
  msg2.bundle_ether[0].has_bundle = true;
  msg2.bundle_ether[0].bundle.has_minimum_active = true;
  msg2.bundle_ether[0].bundle.minimum_active.has_links = true;
  msg2.bundle_ether[0].bundle.minimum_active.links = 10;
  msg2.bundle_ether[0].bundle.has_mac_mode = true;
  msg2.bundle_ether[0].bundle.mac_mode = RW_FPATH_D_MAC_MODE_ACTIVE;
  strcpy(msg2.bundle_ether[1].name, "bundle3");
  msg2.bundle_ether[1].has_lacp = msg2.bundle_ether[1].lacp.has_system = msg2.bundle_ether[1].lacp.system.has_priority = true;
  msg2.bundle_ether[1].lacp.system.priority = 55;

  uint8_t buff[2048];
  size_t len1 = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  size_t len2 = protobuf_c_message_get_packed_size(NULL, &msg2.base);

  protobuf_c_message_pack(NULL, &msg1.base, buff);
  protobuf_c_message_pack(NULL, &msg2.base, buff+len1);

  auto out = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*)protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony), len1+len2, buff);
  ASSERT_TRUE(out);

  EXPECT_STREQ(out->name, "trafgen");
  EXPECT_EQ(out->n_bundle_ether, 3);

  EXPECT_STREQ(out->bundle_ether[0].name, "bundle1");
  EXPECT_EQ(out->bundle_ether[0].lacp.system.priority, 3);
  EXPECT_STREQ(out->bundle_ether[0].lacp.system.mac, "CC:AA:AA:AA:AA:AA");
  EXPECT_EQ(out->bundle_ether[0].bundle.minimum_active.links, 10);
  EXPECT_EQ(out->bundle_ether[0].bundle.maximum_active.links, 100);
  EXPECT_EQ(out->bundle_ether[0].bundle.load_balance, RW_FPATH_D_LAG_LB_MODE_MAC_IP);
  EXPECT_EQ(out->bundle_ether[0].bundle.mac_mode, RW_FPATH_D_MAC_MODE_ACTIVE);

  EXPECT_STREQ(out->bundle_ether[1].name, "bundle2");
  EXPECT_EQ(out->bundle_ether[1].lacp.system.priority, 1);
  EXPECT_STREQ(out->bundle_ether[1].lacp.system.mac, "BA:AA:AA:AA:AA:AA");

  EXPECT_STREQ(out->bundle_ether[2].name, "bundle3");
  EXPECT_EQ(out->bundle_ether[2].lacp.system.priority, 55);

  protobuf_c_message_free_unpacked(NULL, &out->base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg1.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg2.base);
}

TEST(RWProtobuf, BinaryKeys)
{
  RWPB_M_LISTONLY_DECL_INIT(Testy2pTop2_data_Bkll, msg1);
  RWPB_M_LISTONLY_DECL_INIT(Testy2pTop2_data_Bkll, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  msg1.n_bkll = 1;
  msg1.bkll = (RWPB_T_MSG(Testy2pTop2_data_Bkll) **)malloc(sizeof(void*)*1);
  msg1.bkll[0] = (RWPB_T_MSG(Testy2pTop2_data_Bkll) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Bkll)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Bkll, msg1.bkll[0]);

  msg1.bkll[0]->k.len = strlen("BinaryKey1")+1;
  strcpy((char*)msg1.bkll[0]->k.data, "BinaryKey1");
  msg1.bkll[0]->has_k = 1;
  msg1.bkll[0]->has_myl = true;
  msg1.bkll[0]->myl = 15;
  msg1.bkll[0]->myls = strdup("hello");

  msg2.n_bkll = 1;
  msg2.bkll = (RWPB_T_MSG(Testy2pTop2_data_Bkll) **)malloc(sizeof(void*)*1);
  msg2.bkll[0] = (RWPB_T_MSG(Testy2pTop2_data_Bkll) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Bkll)));
  RWPB_F_MSG_INIT(Testy2pTop2_data_Bkll, msg2.bkll[0]);

  msg2.bkll[0]->k.len = strlen("BinaryKey2")+1;
  strcpy((char*)msg2.bkll[0]->k.data, "BinaryKey2");
  msg2.bkll[0]->has_k = 1;
  msg2.bkll[0]->has_myl = true;
  msg2.bkll[0]->myl = 20;
  msg2.bkll[0]->myls = strdup("world");

  uint8_t buff[1024];
  size_t len1 = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  size_t len2 = protobuf_c_message_get_packed_size(NULL, &msg2.base);

  protobuf_c_message_pack(NULL, &msg1.base, buff);
  protobuf_c_message_pack(NULL, &msg2.base, buff+len1);

  auto out = (RWPB_T_LISTONLY(Testy2pTop2_data_Bkll)*)protobuf_c_message_unpack(NULL, RWPB_G_LISTONLY_PBCMD(Testy2pTop2_data_Bkll), len1+len2, buff);
  ASSERT_TRUE(out);

  UniquePtrProtobufCMessage<>::uptr_t uptr3(&out->base);

  EXPECT_EQ(out->n_bkll, 2);
  ASSERT_TRUE(out->bkll);
  ASSERT_TRUE(out->bkll[0]);
  EXPECT_EQ(out->bkll[0]->k.len, 11);
  EXPECT_STREQ((char*)out->bkll[0]->k.data, "BinaryKey1");
  EXPECT_EQ(out->bkll[0]->myl, 15);
  EXPECT_STREQ(out->bkll[0]->myls, "hello");

  ASSERT_TRUE(out->bkll[1]);
  EXPECT_EQ(out->bkll[1]->k.len, 11);
  EXPECT_STREQ((char*)out->bkll[1]->k.data, "BinaryKey2");
  EXPECT_EQ(out->bkll[1]->myl, 20);
  EXPECT_STREQ(out->bkll[1]->myls, "world");

  strcpy((char*)msg2.bkll[0]->k.data, "BinaryKey1");
  msg2.bkll[0]->has_myl = false;

  len1 = protobuf_c_message_get_packed_size(NULL, &msg1.base);
  len2 = protobuf_c_message_get_packed_size(NULL, &msg2.base);

  protobuf_c_message_pack(NULL, &msg1.base, buff);
  protobuf_c_message_pack(NULL, &msg2.base, buff+len1);

  out = (RWPB_T_LISTONLY(Testy2pTop2_data_Bkll)*)protobuf_c_message_unpack(NULL, RWPB_G_LISTONLY_PBCMD(Testy2pTop2_data_Bkll), len1+len2, buff);
  ASSERT_TRUE(out);

  uptr3.reset(&out->base);

  EXPECT_EQ(out->n_bkll, 1);
  ASSERT_TRUE(out->bkll);
  ASSERT_TRUE(out->bkll[0]);
  EXPECT_EQ(out->bkll[0]->k.len, 11);
  EXPECT_STREQ((char*)out->bkll[0]->k.data, "BinaryKey1");
  EXPECT_EQ(out->bkll[0]->myl, 15);
  EXPECT_STREQ(out->bkll[0]->myls, "world");
}


TEST(RWProtobuf, UnpackInlineMaxMerge)
{
  TEST_DESCRIPTION("Test unpack merging at inline_max");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, msg0);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, msg2);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, msg3);

  UniquePtrProtobufCMessageUseBody<>::uptr_t umsg0(&msg0.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t umsg1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t umsg2(&msg2.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t umsg3(&msg3.base);

  strncpy(msg0.name, "msg0", sizeof(msg0.name));
  strncpy(msg1.name, "msg1", sizeof(msg1.name));
  strncpy(msg2.name, "msg2", sizeof(msg2.name));
  strncpy(msg3.name, "msg3", sizeof(msg3.name));
  msg0.has_name = 1;
  msg1.has_name = 1;
  msg2.has_name = 1;
  msg3.has_name = 1;

  strncpy(msg0.scriptable_lb[0].name, "name_a", sizeof(msg0.scriptable_lb[0].name));
  msg0.scriptable_lb[0].has_name = 1;
  msg0.n_scriptable_lb = 1;
  strncpy(msg1.scriptable_lb[0].name, "name_a", sizeof(msg1.scriptable_lb[0].name));
  msg1.scriptable_lb[0].has_name = 1;
  msg1.n_scriptable_lb = 1;
  strncpy(msg2.scriptable_lb[0].name, "name_b", sizeof(msg2.scriptable_lb[0].name));
  msg2.scriptable_lb[0].has_name = 1;
  msg2.n_scriptable_lb = 1;
  // msg3 has none

  ProtobufCMessage* msgs[] = {
    &msg0.base,
    &msg1.base,
    &msg2.base,
    &msg3.base,
  };

  struct {
    unsigned a;
    unsigned b;
    bool success;
    const char* mname;
    const char* lbname;
  } tests[] = {
    { .a = 0, .b = 1, .success = true,  .mname = "msg1", .lbname = "name_a", },
    { .a = 0, .b = 2, .success = false,                                      },
    { .a = 0, .b = 3, .success = true,  .mname = "msg3", .lbname = "name_a", },
    { .a = 1, .b = 0, .success = true,  .mname = "msg0", .lbname = "name_a", },
    { .a = 1, .b = 2, .success = false,                                      },
    { .a = 1, .b = 3, .success = true,  .mname = "msg3", .lbname = "name_a", },
    { .a = 2, .b = 0, .success = false,                                      },
    { .a = 2, .b = 1, .success = false,                                      },
    { .a = 2, .b = 3, .success = true,  .mname = "msg3", .lbname = "name_b", },
    { .a = 3, .b = 0, .success = true,  .mname = "msg0", .lbname = "name_a", },
    { .a = 3, .b = 1, .success = true,  .mname = "msg1", .lbname = "name_a", },
    { .a = 3, .b = 2, .success = true,  .mname = "msg2", .lbname = "name_b", },
  };

  // Now, test all merge permutations
  for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); ++i) {
    // Pack the two messages
    size_t size_sma = 0;
    UniquePtrProtobufCFree<>::uptr_t sma(protobuf_c_message_serialize(nullptr, msgs[tests[i].a], &size_sma));
    ASSERT_NE(nullptr, sma.get());
    ASSERT_NE(0, size_sma);

    size_t size_smb = 0;
    UniquePtrProtobufCFree<>::uptr_t smb(protobuf_c_message_serialize(nullptr, msgs[tests[i].b], &size_smb));
    ASSERT_NE(nullptr, smb.get());
    ASSERT_NE(0, size_smb);

    // Concatenate the messages
    uint8_t merged[size_sma+size_smb];
    memcpy(merged, sma.get(), size_sma);
    memcpy(merged+size_sma, smb.get(), size_smb);

    // Attempt to unpack
    UniquePtrProtobufCMessage<>::uptr_t out(
      protobuf_c_message_unpack(
        nullptr,
        msg0.base.descriptor,
        size_sma+size_smb,
        &merged[0]));
    bool success = out.get();
    if (success != tests[i].success) {
      ADD_FAILURE() << "Unexpected pack status: " << success << ", i=" << i
                    << ", a=" << tests[i].a << ", b=" << tests[i].b;
      continue;
    }

    if (!tests[i].success) {
      continue;
    }
    ASSERT_NE(nullptr, tests[i].mname);
    ASSERT_NE(nullptr, tests[i].lbname);

    auto m = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)*)(out.get());
    ASSERT_NE(nullptr, m);
    EXPECT_STREQ(tests[i].mname, m->name)
      << "    info: i=" << i<< ", a=" << tests[i].a << ", b=" << tests[i].b;
    EXPECT_STREQ(tests[i].lbname, m->scriptable_lb[0].name)
      << "    info: i=" << i<< ", a=" << tests[i].a << ", b=" << tests[i].b;
  }
}

TEST(RWProtobuf, IntutiveMergePointy)
{
  TEST_DESCRIPTION("Test merge protobuf messages");

  RWPB_T_MSG(Company_data_Company) msg1;
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);

  init_comp_msg(&msg1, 2, 2);

  RWPB_M_MSG_DECL_INIT(Company_data_Company, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  size_t size1 = 0;
  uint8_t *pdata1 = protobuf_c_message_serialize(NULL, &msg1.base, &size1);
  ASSERT_TRUE(pdata1);

  protobuf_c_boolean ret = protobuf_c_message_merge(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  size_t size2 = 0;
  uint8_t *pdata2 = protobuf_c_message_serialize(NULL, &msg1.base, &size2);
  ASSERT_TRUE(pdata2);

  // There should not ba any change to the input message
  ASSERT_EQ(size1, size2);
  ASSERT_FALSE(memcmp(pdata1, pdata2, size1));
  free(pdata1); free(pdata2);

  ASSERT_TRUE(msg2.contact_info);
  ASSERT_TRUE(msg2.contact_info->name);
  EXPECT_FALSE(strcmp(msg2.contact_info->name, "abc"));
  ASSERT_TRUE(msg2.contact_info->address);
  EXPECT_FALSE(strcmp(msg2.contact_info->address, "bglr"));
  ASSERT_TRUE(msg2.contact_info->phone_number);
  EXPECT_FALSE(strcmp(msg2.contact_info->phone_number, "1234"));

  ASSERT_EQ(msg2.n_employee, 2);
  ASSERT_TRUE(msg2.employee);
  ASSERT_EQ(msg2.n_product, 2);
  EXPECT_TRUE(msg2.product);

  ASSERT_TRUE(msg2.employee[0]);
  EXPECT_EQ(msg2.employee[0]->id, 100);
  EXPECT_FALSE(strcmp(msg2.employee[0]->name, "Emp1"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->title, "Title"));
  EXPECT_FALSE(strcmp(msg2.employee[0]->start_date, "1234"));

  ASSERT_TRUE(msg2.employee[1]);
  EXPECT_EQ(msg2.employee[1]->id, 200);
  EXPECT_FALSE(strcmp(msg2.employee[1]->name, "Emp2"));
  EXPECT_FALSE(strcmp(msg2.employee[1]->title, "Title"));
  EXPECT_FALSE(strcmp(msg2.employee[1]->start_date, "1234"));

  ASSERT_TRUE(msg2.product[0]);
  EXPECT_EQ(msg2.product[0]->id, 1000);
  EXPECT_FALSE(strcmp(msg2.product[0]->name, "nfv"));
  EXPECT_FALSE(strcmp(msg2.product[0]->msrp, "1000"));

  ASSERT_TRUE(msg2.product[1]);
  EXPECT_EQ(msg2.product[1]->id, 2000);
  EXPECT_FALSE(strcmp(msg2.product[1]->name, "cloud"));
  EXPECT_FALSE(strcmp(msg2.product[1]->msrp, "2000"));

  uptr1.reset();
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);

  init_comp_msg(&msg1, 3, 2);
  uptr1.reset(&msg1.base);

  msg1.n_employee = 3;
  msg1.employee[2] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg1.employee[2]);
  msg1.employee[2]->id = 300;
  msg1.employee[2]->has_id = 1;
  msg1.employee[2]->name = strdup("Emp3");
  msg1.employee[2]->title = strdup("SWE");
  msg1.employee[2]->start_date = strdup("1431");

  uptr2.reset();
  RWPB_F_MSG_INIT(Company_data_Company, &msg2);
  uptr2.reset(&msg2.base);

  msg2.n_employee = 3;
  msg2.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)calloc(3, sizeof(void*));
  msg2.employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[0]);
  msg2.employee[0]->id = 100;
  msg2.employee[0]->has_id = 1;
  msg2.employee[0]->name = strdup("Emp3");
  msg2.employee[0]->title = strdup("Title");

  msg2.employee[1] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[1]);
  msg2.employee[1]->id = 200;
  msg2.employee[1]->has_id = 1;
  msg2.employee[1]->name = strdup("Emp4");

  msg2.employee[2] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg2.employee[2]);
  msg2.employee[2]->id = 800;
  msg2.employee[2]->has_id = 1;
  msg2.employee[2]->name = strdup("Emp7");

  size1 = 0;
  pdata1 = protobuf_c_message_serialize(NULL, &msg1.base, &size1);
  ASSERT_TRUE(pdata1);

  ret = protobuf_c_message_merge(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  size2 = 0;
  pdata2 = protobuf_c_message_serialize(NULL, &msg1.base, &size2);
  ASSERT_TRUE(pdata2);

  // There should not ba any change to the input message
  ASSERT_EQ(size1, size2);
  ASSERT_FALSE(memcmp(pdata1, pdata2, size1));
  free(pdata1); free(pdata2);

  ASSERT_EQ(msg2.n_employee, 4); 
  EXPECT_EQ(msg2.employee[0]->id, 100); 
  EXPECT_FALSE(strcmp(msg2.employee[0]->name, "Emp1")); 
  EXPECT_FALSE(strcmp(msg2.employee[0]->title, "Title")); 
  EXPECT_TRUE(msg2.employee[0]->start_date); 
  EXPECT_FALSE(strcmp(msg2.employee[0]->start_date, "1234")); 
  EXPECT_EQ(msg2.employee[1]->id, 200); 
  EXPECT_TRUE(msg2.employee[1]->name); 
  EXPECT_FALSE(strcmp(msg2.employee[1]->name, "Emp2")); 
  EXPECT_TRUE(msg2.employee[1]->title); 
  EXPECT_FALSE(strcmp(msg2.employee[1]->title, "Title")); 
  EXPECT_TRUE(msg2.employee[1]->start_date); 
  EXPECT_FALSE(strcmp(msg2.employee[1]->start_date, "1234")); 
  EXPECT_EQ(msg2.employee[2]->id, 800); 
  EXPECT_TRUE(msg2.employee[2]->name); 
  EXPECT_FALSE(strcmp(msg2.employee[2]->name, "Emp7")); 
  EXPECT_FALSE(msg2.employee[2]->title); 
  EXPECT_FALSE(msg2.employee[2]->start_date); 
  EXPECT_EQ(msg2.employee[3]->id, 300); 
  EXPECT_FALSE(strcmp(msg2.employee[3]->name, "Emp3")); 
  EXPECT_FALSE(strcmp(msg2.employee[3]->title, "SWE")); 
  EXPECT_FALSE(strcmp(msg2.employee[3]->start_date, "1431")); 
  ASSERT_EQ(msg2.n_product, 2); 
  EXPECT_EQ(msg2.product[0]->id, 1000); 
  EXPECT_FALSE(strcmp(msg2.product[0]->name, "nfv")); 
  EXPECT_FALSE(strcmp(msg2.product[0]->msrp, "1000")); 
  EXPECT_EQ(msg2.product[1]->id, 2000); 
  EXPECT_FALSE(strcmp(msg2.product[1]->name, "cloud")); 
  EXPECT_FALSE(strcmp(msg2.product[1]->msrp, "2000"));
}

TEST(RWProtobuf, IntutiveMergeInline)
{
  // Inline repeated messages.
  RWPB_T_MSG(Testy2pTop1_data_Top1c2) msg1;
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);

  init_test_msg(&msg1);

  RWPB_M_MSG_DECL_INIT(Testy2pTop1_data_Top1c2, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  size_t size1 = 0;
  uint8_t *pdata1 = protobuf_c_message_serialize(NULL, &msg1.base, &size1);
  ASSERT_TRUE(pdata1);

  protobuf_c_boolean ret = protobuf_c_message_merge(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  size_t size2 = 0;
  uint8_t *pdata2 = protobuf_c_message_serialize(NULL, &msg1.base, &size2);
  ASSERT_TRUE(pdata2);

  // There should not ba any change to the input message
  ASSERT_EQ(size1, size2);
  ASSERT_FALSE(memcmp(pdata1, pdata2, size1));
  free(pdata1); free(pdata2);

  EXPECT_TRUE(msg2.has_ls1); 
  EXPECT_FALSE(strcmp(msg2.ls1, "RiftIO"));
  EXPECT_TRUE(msg2.has_ln1);
  EXPECT_EQ(msg2.ln1, 20000);
  EXPECT_TRUE(msg2.has_ln2);
  EXPECT_EQ(msg2.ln2, 500);
  EXPECT_TRUE(msg2.has_c1);
  EXPECT_TRUE(msg2.c1.has_c1);
  EXPECT_TRUE(msg2.c1.c1.has_ln1);
  EXPECT_EQ(msg2.c1.c1.ln1, 10);
  EXPECT_TRUE(msg2.c1.c1.has_ln2);
  EXPECT_EQ(msg2.c1.c1.ln2, 100);
  ASSERT_EQ(msg2.n_l1, 2);
  ASSERT_EQ(msg2.l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ(msg2.l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ(msg2.l1[0].l1[1].ln2, 200);
  ASSERT_EQ(msg2.l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ(msg2.l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ(msg2.l1[1].l1[1].ln2, 400);

  uptr1.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);
  init_test_msg(&msg1);
  uptr1.reset(&msg1.base);

  uptr2.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg2);
  uptr2.reset(&msg2.base);

  msg2.has_ls1 = true;
  strcpy(msg2.ls1, "Super RiftIO");
  msg2.has_c1 = true;
  msg2.c1.has_c1 = true;
  msg2.c1.c1.has_ln1 = true;
  msg2.c1.c1.ln1 = 20;
  msg2.c1.c1.has_ln2 = true;
  msg2.c1.c1.ln2 = 200;

  msg2.n_l1 = 2;
  msg2.l1[0].lf1 = 14;
  msg2.l1[0].has_lf1 = 1;
  msg2.l1[0].n_l1 = 2;
  strcpy(msg2.l1[0].l1[0].ln1, "Key1");
  strcpy(msg2.l1[0].l1[1].ln1, "Key2");
  msg2.l1[0].l1[0].has_ln1 = 1;
  msg2.l1[0].l1[1].has_ln1 = 1;

  msg2.l1[1].lf1 = 31;
  msg2.l1[1].has_lf1 = 1;
  msg2.l1[1].n_l1 = 2;
  strcpy(msg2.l1[1].l1[0].ln1, "Key3");
  strcpy(msg2.l1[1].l1[1].ln1, "Key4");
  msg2.l1[1].l1[0].has_ln1 = 1;
  msg2.l1[1].l1[1].has_ln1 = 1;

  size1 = 0;
  pdata1 = protobuf_c_message_serialize(NULL, &msg1.base, &size1);
  ASSERT_TRUE(pdata1);

  ret = protobuf_c_message_merge(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  size2 = 0;
  pdata2 = protobuf_c_message_serialize(NULL, &msg1.base, &size2);
  ASSERT_TRUE(pdata2);

  // There should not ba any change to the input message
  ASSERT_EQ(size1, size2);
  ASSERT_FALSE(memcmp(pdata1, pdata2, size1));
  free(pdata1); free(pdata2);

  EXPECT_TRUE(msg2.has_ls1);
  EXPECT_FALSE(strcmp(msg2.ls1, "RiftIO")); 
  EXPECT_TRUE(msg2.has_ln1);
  EXPECT_EQ(msg2.ln1, 20000);
  EXPECT_TRUE(msg2.has_ln2);
  EXPECT_EQ(msg2.ln2, 500);
  EXPECT_TRUE(msg2.has_c1);
  EXPECT_TRUE(msg2.c1.has_c1);
  EXPECT_TRUE(msg2.c1.c1.has_ln1);
  EXPECT_EQ(msg2.c1.c1.ln1, 10);
  EXPECT_TRUE(msg2.c1.c1.has_ln2);
  EXPECT_EQ(msg2.c1.c1.ln2, 100);
  ASSERT_EQ(msg2.n_l1, 2); 
  ASSERT_EQ(msg2.l1[0].n_l1, 2); 
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[0].ln1, "Key1")); 
  EXPECT_EQ(msg2.l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ(msg2.l1[0].l1[1].ln2, 200);
  ASSERT_EQ(msg2.l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[0].ln1, "Key3"));
  EXPECT_EQ(msg2.l1[1].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[1].ln1, "Key4"));
  EXPECT_EQ(msg2.l1[1].l1[1].ln2, 400);

  uptr1.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg1);
  init_test_msg(&msg1);
  uptr1.reset(&msg1.base);

  uptr2.reset();
  RWPB_F_MSG_INIT(Testy2pTop1_data_Top1c2, &msg2);
  uptr2.reset(&msg2.base);

  msg2.has_ls1 = true;
  strcpy(msg2.ls1, "Super RiftIO");
  msg2.has_c1 = true;
  msg2.c1.has_c1 = true;
  msg2.c1.c1.has_ln1 = true;
  msg2.c1.c1.ln1 = 20;
  msg2.c1.c1.has_ln2 = true;
  msg2.c1.c1.ln2 = 200;

  msg2.n_l1 = 3;
  msg2.l1[0].lf1 = 14;
  msg2.l1[0].has_lf1 = 1;
  msg2.l1[0].n_l1 = 2;
  strcpy(msg2.l1[0].l1[0].ln1, "Key1");
  strcpy(msg2.l1[0].l1[1].ln1, "Key2");
  msg2.l1[0].l1[0].has_ln1 = 1;
  msg2.l1[0].l1[1].has_ln1 = 1;

  msg2.l1[1].lf1 = 9;
  msg2.l1[1].has_lf1 = 1;
  msg2.l1[1].n_l1 = 2;
  strcpy(msg2.l1[1].l1[0].ln1, "Key3");
  strcpy(msg2.l1[1].l1[1].ln1, "Key4");
  msg2.l1[1].l1[0].has_ln1 = 1;
  msg2.l1[1].l1[1].has_ln1 = 1;

  msg2.l1[2].lf1 = 10;
  msg2.l1[2].has_lf1 = 1;
  msg2.l1[2].n_l1 = 2;
  strcpy(msg2.l1[2].l1[0].ln1, "Key5");
  strcpy(msg2.l1[2].l1[1].ln1, "Key6");
  msg2.l1[2].l1[0].has_ln1 = 1;
  msg2.l1[2].l1[1].has_ln1 = 1;

  size1 = 0;
  pdata1 = protobuf_c_message_serialize(NULL, &msg1.base, &size1);
  ASSERT_TRUE(pdata1);

  ret = protobuf_c_message_merge(NULL, &msg1.base, &msg2.base);
  ASSERT_TRUE(ret);

  size2 = 0;
  pdata2 = protobuf_c_message_serialize(NULL, &msg1.base, &size2);
  ASSERT_TRUE(pdata2);

  // There should not ba any change to the input message
  ASSERT_EQ(size1, size2);
  ASSERT_FALSE(memcmp(pdata1, pdata2, size1));
  free(pdata1); free(pdata2);

  EXPECT_TRUE(msg2.has_ls1);
  EXPECT_FALSE(strcmp(msg2.ls1, "RiftIO"));
  EXPECT_TRUE(msg2.has_ln1);
  EXPECT_EQ(msg2.ln1, 20000);
  EXPECT_TRUE(msg2.has_ln2);
  EXPECT_EQ(msg2.ln2, 500);
  EXPECT_TRUE(msg2.has_c1);
  EXPECT_TRUE(msg2.c1.has_c1);
  EXPECT_TRUE(msg2.c1.c1.has_ln1);
  EXPECT_EQ(msg2.c1.c1.ln1, 10);
  EXPECT_TRUE(msg2.c1.c1.has_ln2);
  EXPECT_EQ(msg2.c1.c1.ln2, 100);
  ASSERT_EQ(msg2.n_l1, 4);
  EXPECT_EQ(msg2.l1[0].lf1, 14);
  ASSERT_EQ(msg2.l1[0].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[0].ln1, "Key1"));
  EXPECT_EQ(msg2.l1[0].l1[0].ln2, 100);
  EXPECT_FALSE(strcmp(msg2.l1[0].l1[1].ln1, "Key2"));
  EXPECT_EQ(msg2.l1[0].l1[1].ln2, 200);
  EXPECT_EQ(msg2.l1[1].lf1, 9);
  ASSERT_EQ(msg2.l1[1].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[0].ln1, "Key3"));
  EXPECT_FALSE(msg2.l1[1].l1[0].has_ln2);
  EXPECT_FALSE(strcmp(msg2.l1[1].l1[1].ln1, "Key4"));
  EXPECT_FALSE(msg2.l1[1].l1[1].has_ln2);
  EXPECT_EQ(msg2.l1[2].lf1, 10);
  ASSERT_EQ(msg2.l1[2].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[2].l1[0].ln1, "Key5"));
  EXPECT_FALSE(msg2.l1[2].l1[0].has_ln2);
  EXPECT_FALSE(strcmp(msg2.l1[2].l1[1].ln1, "Key6"));
  EXPECT_FALSE(msg2.l1[2].l1[1].has_ln2);
  EXPECT_EQ(msg2.l1[3].lf1, 31);
  ASSERT_EQ(msg2.l1[3].n_l1, 2);
  EXPECT_FALSE(strcmp(msg2.l1[3].l1[0].ln1, "Key3"));
  EXPECT_EQ(msg2.l1[3].l1[0].ln2, 300);
  EXPECT_FALSE(strcmp(msg2.l1[3].l1[1].ln1, "Key4"));
  EXPECT_EQ(msg2.l1[3].l1[1].ln2, 400);
}

TEST(RWProtobuf, ConfigUpdate)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, crdu);

  // Fill initial config.
  strcpy(crdu.name, "trafgen");
  crdu.has_name = 1;
  crdu.n_bundle_ether = 1;
  strcpy(crdu.bundle_ether[0].name, "be1");
  crdu.bundle_ether[0].has_name = 1;
  crdu.bundle_ether[0].has_descr_string = true;
  strcpy(crdu.bundle_ether[0].descr_string, "desc1");
  crdu.bundle_ether[0].has_mtu = true;
  crdu.bundle_ether[0].mtu = 1000;

  crdu.n_network_context = 1;
  crdu.network_context = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)**)malloc(sizeof(void*));
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_NetworkContext, ncp);
  strcpy(ncp->name, "nc1");
  crdu.network_context[0] = ncp;

  protobuf_c_boolean res = protobuf_c_message_merge(NULL, &crdu.base, &colony.base);
  ASSERT_TRUE(res);

  res = protobuf_c_message_is_equal_deep(NULL, &crdu.base, &colony.base);
  ASSERT_TRUE(res);

  // Updates
  protobuf_c_message_free_unpacked_usebody(NULL, &crdu.base);
  RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony, &crdu);

  strcpy(crdu.name, "trafgen");
  crdu.has_name = 1;
  crdu.n_bundle_ether = 1;
  strcpy(crdu.bundle_ether[0].name, "be1");
  crdu.bundle_ether[0].has_name = 1;
  crdu.bundle_ether[0].has_descr_string = true;
  strcpy(crdu.bundle_ether[0].descr_string, "desc2");
  crdu.bundle_ether[0].has_mtu = true;
  crdu.bundle_ether[0].mtu = 2000;
  crdu.bundle_ether[0].n_vlan = 1;
  crdu.bundle_ether[0].vlan[0].id = 100;

  res = protobuf_c_message_merge(NULL, &crdu.base, &colony.base);
  ASSERT_TRUE(res);

  EXPECT_EQ(colony.n_bundle_ether, 1);
  EXPECT_EQ(colony.bundle_ether[0].mtu, 2000);
  EXPECT_STREQ(colony.bundle_ether[0].descr_string, "desc2");
  EXPECT_EQ(colony.bundle_ether[0].n_vlan, 1);
  EXPECT_EQ(colony.n_network_context, 1);

  protobuf_c_message_free_unpacked_usebody(NULL, &crdu.base);
  RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony, &crdu);

  strcpy(crdu.name, "trafgen");
  crdu.has_name = 1;
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_ScriptableLb, slb);
  crdu.scriptable_lb = slb;

  slb->n_fastpath = 1;
  slb->fastpath[0].instance = 12;
  slb->fastpath[0].has_instance = 1;
  slb->fastpath[0].n_service = 1;
  strcpy(slb->fastpath[0].service[0].service_name, "sv1");
  slb->fastpath[0].service[0].has_service_name = 1;
  slb->fastpath[0].service[0].n_fwd_sessions = 1;
  slb->fastpath[0].service[0].fwd_sessions[0].sessionid = 1;
  slb->fastpath[0].service[0].fwd_sessions[0].has_sessionid = 1;
  slb->fastpath[0].service[0].fwd_sessions[0].has_slb_session = 1;
  slb->fastpath[0].service[0].fwd_sessions[0].slb_session.has_proto_specific = 1;
  slb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.has_proto_info = 1;

  uint8_t bytes[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA };
  slb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.len = sizeof(bytes);
  memcpy(slb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.data, bytes, sizeof(bytes));

  res = protobuf_c_message_merge(NULL, &crdu.base, &colony.base);
  ASSERT_TRUE(res);

  EXPECT_EQ(colony.n_bundle_ether, 1);
  EXPECT_EQ(colony.bundle_ether[0].mtu, 2000);
  EXPECT_STREQ(colony.bundle_ether[0].descr_string, "desc2");
  EXPECT_EQ(colony.bundle_ether[0].n_vlan, 1);
  EXPECT_EQ(colony.n_network_context, 1);
  ASSERT_TRUE(colony.scriptable_lb);
  EXPECT_EQ(colony.scriptable_lb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.len, 10);

  res = protobuf_c_message_is_equal_deep(NULL, &crdu.scriptable_lb->base, &colony.scriptable_lb->base);
  ASSERT_TRUE(res);

  uint8_t bytes1[] = { 0xA, 0xB, 0xC, 0xD };
  slb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.len = sizeof(bytes1);
  memcpy(slb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.data, bytes1, sizeof(bytes1));

  res = protobuf_c_message_merge(NULL, &crdu.base, &colony.base);
  ASSERT_TRUE(res);

  EXPECT_EQ(colony.n_bundle_ether, 1);
  EXPECT_EQ(colony.bundle_ether[0].mtu, 2000);
  EXPECT_STREQ(colony.bundle_ether[0].descr_string, "desc2");
  EXPECT_EQ(colony.bundle_ether[0].n_vlan, 1);
  EXPECT_EQ(colony.n_network_context, 1);
  ASSERT_TRUE(colony.scriptable_lb);
  EXPECT_EQ(colony.scriptable_lb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.len, 4);
  EXPECT_FALSE(memcmp(bytes1, 
                      colony.scriptable_lb->fastpath[0].service[0].fwd_sessions[0].slb_session.proto_specific.proto_info.data,
                      4));

  res = protobuf_c_message_is_equal_deep(NULL, &crdu.scriptable_lb->base, &colony.scriptable_lb->base);
  ASSERT_TRUE(res);

  protobuf_c_message_free_unpacked_usebody(NULL, &crdu.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &colony.base);
}
