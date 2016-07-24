
/* 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 * */

/**
 * @file test_pb_to_cli.cpp
 * @author Shaji Radhakrishnan (shaji.radhakrishnan@riftio.com)
 * @date 2015/03/05
 * @brief Tests for pb_to_cli
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"

#include "rw_app_data.hpp"
#include "yangcli.hpp"
#include "yangncx.hpp"
#include "rw_keyspec.h"
#include "rw-base-d.pb-c.h"

using namespace rw_yang;

TEST (PBtoCLI, PbToCli)
{
  TEST_DESCRIPTION("Test Pb to CLI");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);
  YangModule* test_ncx = model->load_module("rw-base-c");
  ASSERT_TRUE(test_ncx);

  RwBaseD__YangData__RwBaseD__Colony protobuf;
  RwBaseD__YangData__RwBaseD__Colony__NetworkContext *nwcontexts[1];
  RwBaseD__YangData__RwBaseD__Colony__NetworkContext nwcontext;

  rw_base_d__yang_data__rw_base_d__colony__init(&protobuf);
  rw_base_d__yang_data__rw_base_d__colony__network_context__init(&nwcontext);

  strcpy(protobuf.name, "riftcolony");

  protobuf.n_network_context = 1;
  nwcontexts[0] = &nwcontext;
  strcpy(nwcontext.name,"contextrift");
  protobuf.network_context = nwcontexts;

  ProtobufCMessage *pbcmsg = (ProtobufCMessage *)&protobuf;

  char *out_string = rw_pb_to_cli((rw_yang_model_t*)model, pbcmsg);
  ASSERT_TRUE(out_string);
  const char *expected = "config\n"
                         "  rw-base:colony riftcolony\n"
                         "    network-context contextrift\n"
                         "    exit\n"
                         "  exit\n"
                         "commit\n"
                         "end\n";
  EXPECT_STREQ(out_string, expected);
  free(out_string);
}

TEST (PBtoCLI, PbToCliLogging)
{
  TEST_DESCRIPTION("Test Pb to CLI");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);
  YangModule* test_ncx = model->load_module("rw-base-d");
  ASSERT_TRUE(test_ncx);

  RwBaseD__YangData__RwBaseD__Logging protobuf;
  rw_base_d__yang_data__rw_base_d__logging__init(&protobuf);
  RwBaseD__YangData__RwBaseD__Logging__Filter filter;
  rw_base_d__yang_data__rw_base_d__logging__filter__init(&filter);
  RwBaseD__YangData__RwBaseD__Logging__Filter__Category category;
  rw_base_d__yang_data__rw_base_d__logging__filter__category__init(&category);
  RwBaseD__YangData__RwBaseD__Logging__Filter__Category__Event event;
  rw_base_d__yang_data__rw_base_d__logging__filter__category__event__init(&event);
  RwBaseD__YangData__RwBaseD__Logging__Filter__Category__Attribute attribute;
  rw_base_d__yang_data__rw_base_d__logging__filter__category__attribute__init(&attribute);

  protobuf.filter = &filter;


  category.name = RW_BASE_D_CATEGORY_TYPE_CLI;
  category.n_event = 1;
  event.id = 2;
  category.event[0] = event;
  category.n_attribute = 1;
  strcpy(attribute.key, "mykey");
  attribute.has_value = 1;
  strcpy(attribute.value, "mykeyvalue");
  category.attribute[0] = attribute;
  category.has_severity_level = 1;
  category.severity_level = RW_BASE_D_LOG_LEVEL_NOTICE;
  filter.n_category = 1;
  filter.category[0] = category;

  ProtobufCMessage *pbcmsg = (ProtobufCMessage *)&protobuf;

  char *out_string = rw_pb_to_cli((rw_yang_model_t*)model, pbcmsg);
  ASSERT_TRUE(out_string);
  const char *expected = "config\n"
                         "  logging filter category cli\n"
                         "    event 2\n"
                         "    exit\n"
                         "    attribute mykey\n" 
                         "      value mykeyvalue\n"
                         "    exit\n"
                         "    severity-level notice\n"
                         "  exit\n"
                         "commit\nend\n";

  EXPECT_STREQ(out_string, expected);
  free(out_string);
}

//TODO: Add more testcases
