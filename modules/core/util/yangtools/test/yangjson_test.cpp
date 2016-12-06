/* 
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
 * */
/**
 * @file yangjson_test.hpp
 * @date 2016/02/05
 * @brief Tests for  yang node to json conversion.
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"
#include "yangmodel.h"
#include "yangncx.hpp"
#include "rw_pb_schema.h"
#include "rw_yang_json.h"
#include "test-json-schema.pb-c.h"
#include "test-json-schema-aug-base.pb-c.h"

using namespace rw_yang;
namespace pt = boost::property_tree;

TEST (JsonSchema, JsonAugmentTest)
{
  TEST_DESCRIPTION ("Test for simple yang to json conversion");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);
  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  
  YangModule* tnaa1 = model->load_module("test-json-schema");
  ASSERT_TRUE(tnaa1);
  YangAugment* person = tnaa1->get_first_augment();
  ASSERT_TRUE(person);
  YangNode* node = person->get_target_node();

  std::stringstream oss(node->to_json_schema(true));
  std::cout << oss.str() << std::endl;

  pt::ptree tree;
  pt::read_json(oss, tree);
  bool found_comp_list = false;

  BOOST_FOREACH(const pt::ptree::value_type& val, tree.get_child("person.properties"))
  {
    auto name = val.second.get<std::string>("name");

    if (name == "test-json-schema:company-list") {
      found_comp_list = true;
      EXPECT_STREQ (val.second.get<std::string>("type").c_str(), "list");
      EXPECT_STREQ (val.second.get<std::string>("cardinality").c_str(), "0..N");

      BOOST_FOREACH (const pt::ptree::value_type& v, val.second.get_child("properties")) {
        auto iname = v.second.get<std::string>("name");
        std::cout << iname << std::endl;
        if (iname == "iref1") {
          const auto& dtype = v.second.get_child("data-type.idref");
          EXPECT_STREQ (dtype.get<std::string>("base").c_str(), "tjs:riftio");
        }
        if (iname == "iref2") {
          const auto& dtype = v.second.get_child("data-type.idref");
          EXPECT_STREQ (dtype.get<std::string>("base").c_str(), "tjs:cloud-platform");
        }
      }
    }
  }

  EXPECT_TRUE (found_comp_list);

}
