
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file testncx.cpp
 * @author
 * @date 09/04/2014
 * @brief Rift AppData Cache Testcases.
 *
 * Unit tests for rw_app_data.cpp
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <random>

#include "rwtrace.h"
#include "rwut.h"
#include "rw_app_data.hpp"
#include "yangncx.hpp"

using namespace rw_yang;

TEST(AppData, Registration)
{
  TEST_DESCRIPTION("Library registration APIs");
  AppDataManager am;

  typedef AppDataToken<const char*> adt_t;
  adt_t adt0;
  EXPECT_TRUE(am.app_data_get_token("mod", "ext1", &adt0));
  EXPECT_EQ(adt0.index_, 0);
  EXPECT_TRUE(adt0.deleter_);

  adt_t adt1;
  EXPECT_TRUE(am.app_data_get_token("mod", "ext2", &adt1));
  EXPECT_EQ(adt1.index_, 1);
  EXPECT_EQ(adt1.deleter_, adt0.deleter_);

  adt_t adt2;
  EXPECT_TRUE(am.app_data_get_token("mod", "ext1", &adt2));
  EXPECT_EQ(adt2.index_, 0);
}

TEST(AppData, Lookup)
{
  TEST_DESCRIPTION("Library cache and lookup APIs");
  AppDataManager am;
  AppDataCache ac(am);

  typedef AppDataToken<const char*> adt_t;
  adt_t adt0;
  EXPECT_TRUE(am.app_data_get_token("mod", "ext1", &adt0));
  EXPECT_EQ(adt0.index_, 0);
  EXPECT_TRUE(adt0.deleter_);

  adt_t adt1;
  EXPECT_TRUE(am.app_data_get_token("mod", "ext2", &adt1));
  EXPECT_EQ(adt1.index_, 1);
  EXPECT_EQ(adt1.deleter_, adt0.deleter_);

  adt_t adt2;
  EXPECT_TRUE(am.app_data_get_token("mod", "ext1", &adt2));
  EXPECT_EQ(adt2.index_, 0);

  EXPECT_FALSE(ac.is_cached(&adt0));
  EXPECT_FALSE(ac.is_cached(&adt1));
  EXPECT_FALSE(ac.is_cached(&adt0));
  EXPECT_FALSE(ac.is_cached(&adt1));

  const char* my_data = "not nullptr";
  const char* not_nullptr = my_data;

  EXPECT_FALSE(ac.check_and_get(&adt0, &my_data));
  EXPECT_EQ(my_data, not_nullptr);
  EXPECT_FALSE(ac.check_and_get(&adt1, &my_data));
  EXPECT_EQ(my_data, not_nullptr);

  const char* junk = strdup("junk");
  const char* old = nullptr;

  old = ac.set_and_give_ownership(&adt0, junk);
  EXPECT_EQ(old, nullptr);
  old = ac.set_and_keep_ownership(&adt1, not_nullptr);
  EXPECT_EQ(old, nullptr);

  const char* lookup = nullptr;
  EXPECT_TRUE(ac.check_and_get(&adt0, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, junk);
  lookup = nullptr;
  EXPECT_TRUE(ac.check_and_get(&adt1, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, not_nullptr);

  lookup = ac.set_uncached(&adt0);
  EXPECT_EQ(lookup, junk);
  EXPECT_FALSE(ac.is_cached(&adt0));
  lookup = nullptr;
  EXPECT_FALSE(ac.check_and_get(&adt0, &lookup));
  EXPECT_EQ(lookup, nullptr);
  EXPECT_TRUE(ac.is_looked_up(&adt0));
}

TEST(AppData, Limits)
{
  TEST_DESCRIPTION("Library limits tests");
  AppDataManager am;
  AppDataCache ac(am), ac1(am);

  static const unsigned LIMIT = 4096; // ATTN: get from header?

  // Create extensions
  typedef AppDataToken<char*> adt_t;
  std::vector<adt_t> adts;
  adts.resize(LIMIT);

  unsigned i = 0;
  for (i = 0; i < LIMIT; ++i ) {
    char ext[16];
    int bytes = snprintf(ext, sizeof(ext), "ext%04u", i);
    ASSERT_EQ(bytes, 7);
    adt_t adt;
    EXPECT_TRUE(am.app_data_get_token("mod", ext, &adt));
    EXPECT_EQ(adt.index_, i);
    EXPECT_NE(adt.deleter_, nullptr);

    adts[i] = adt;
  }

  // Make sure that too many extensions fails gracefully.
  adt_t adtfail;
  EXPECT_FALSE(am.app_data_get_token("mod", "extfail", &adtfail));
  EXPECT_EQ(adtfail.index_, 0);
  EXPECT_EQ(adtfail.deleter_, nullptr);

  // Insert all possible app data values in order.
  for (i = 0; i < LIMIT; ++i ) {
    // lookup token
    char ext[16];
    int bytes = snprintf(ext, sizeof(ext), "ext%04u", i);
    ASSERT_EQ(bytes, 7);
    adt_t adt;
    ASSERT_TRUE(am.app_data_get_token("mod", ext, &adt));
    EXPECT_EQ(adt.index_, adts[i].index_);

    // set app data
    char val[16];
    bytes = snprintf(val, sizeof(val), "val%04u", i);
    ASSERT_EQ(bytes, 7);
    char* save_val = strdup(val);
    char* old = ac.set_and_give_ownership(&adt, save_val);
    EXPECT_EQ(old, nullptr);

    // verify app data
    char* lookup = nullptr;
    EXPECT_TRUE(ac.check_and_get(&adt, &lookup));
    ASSERT_TRUE(lookup);
    EXPECT_STREQ(lookup, val);
    EXPECT_EQ(lookup, save_val);
  }

  // Verify all app data a second time - now that the tree is filled
  for (i = 0; i < LIMIT; ++i ) {
    // verify app data
    char val[16];
    int bytes = snprintf(val, sizeof(val), "val%04u", i);
    ASSERT_EQ(bytes, 7);
    char* lookup = nullptr;
    EXPECT_TRUE(ac.check_and_get(&adts[i], &lookup));
    ASSERT_TRUE(lookup);
    EXPECT_STREQ(lookup, val);
  }

  // Insert app data randomly
  std::default_random_engine generator;
  generator.seed(getpid());
  std::vector<std::unique_ptr<char>> values;
  values.resize(LIMIT);
  for (unsigned j = 0; j < LIMIT; ++j ) {
    unsigned i = static_cast<unsigned>(generator()) % LIMIT;
    // find next unset entry.
    for (; values[i]; i = (i+1) % LIMIT) {
      ; /** empty loop **/
    }

    // verify app data
    char val[16];
    int bytes = snprintf(val, sizeof(val), "val%04u.%04u", j, i);
    ASSERT_EQ(bytes, 12);
    values[i].reset(strdup(val));
    char* old = ac1.set_and_keep_ownership(&adts[i], values[i].get());
    EXPECT_EQ(old, nullptr);

    // verify app data
    char* lookup = nullptr;
    EXPECT_TRUE(ac1.check_and_get(&adts[i], &lookup));
    ASSERT_TRUE(lookup);
    EXPECT_STREQ(lookup, values[i].get());
  }

  // Verify all app data a second time - now that the tree is filled
  for (i = 0; i < LIMIT; ++i ) {
    // verify app data
    char* lookup = nullptr;
    EXPECT_TRUE(ac1.check_and_get(&adts[i], &lookup));
    ASSERT_TRUE(lookup);
    EXPECT_STREQ(lookup, values[i].get());
  }
}

TEST(AppData, ModelRegistration)
{
  TEST_DESCRIPTION("Registration to YangModel");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx-mod-app-data");
  EXPECT_TRUE(test_ncx);

  typedef AppDataToken<const char*> adt_t;

  adt_t adt0;
  EXPECT_TRUE(model->app_data_get_token("mod", "mext1", &adt0));
  EXPECT_EQ(adt0.index_, 0);
  EXPECT_TRUE(adt0.deleter_);

  adt_t adt1;
  EXPECT_TRUE(model->app_data_get_token("mod", "mext2", &adt1));
  EXPECT_EQ(adt1.index_, 1);
  EXPECT_EQ(adt1.deleter_, adt0.deleter_);

  adt_t adt2;
  EXPECT_TRUE(model->app_data_get_token("mod", "mext1", &adt2));
  EXPECT_EQ(adt2.index_, 0);
}


TEST(AppData, NodeLookup1)
{
  TEST_DESCRIPTION("YangNode cache and lookup test 1");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx-app-data");
  EXPECT_TRUE(test_ncx);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);

  typedef AppDataToken<const char*> adt_t;
  adt_t adt0;
  EXPECT_TRUE(model->app_data_get_token("mod", "ext1", &adt0));
  EXPECT_EQ(adt0.index_, 0);
  EXPECT_TRUE(adt0.deleter_);

  adt_t adt1;
  EXPECT_TRUE(model->app_data_get_token("mod", "ext2", &adt1));
  EXPECT_EQ(adt1.index_, 1);
  EXPECT_EQ(adt1.deleter_, adt0.deleter_);

  EXPECT_FALSE(root->app_data_is_cached(&adt0));
  EXPECT_FALSE(root->app_data_is_cached(&adt1));
  EXPECT_FALSE(top->app_data_is_cached(&adt0));
  EXPECT_FALSE(top->app_data_is_cached(&adt1));

  const char* my_data = "not nullptr";
  const char* not_nullptr = my_data;

  EXPECT_FALSE(root->app_data_check_and_get(&adt0, &my_data));
  EXPECT_EQ(my_data, not_nullptr);
  EXPECT_FALSE(root->app_data_check_and_get(&adt1, &my_data));
  EXPECT_EQ(my_data, not_nullptr);

  EXPECT_FALSE(top->app_data_check_and_get(&adt0, &my_data));
  EXPECT_EQ(my_data, not_nullptr);
  EXPECT_FALSE(top->app_data_check_and_get(&adt1, &my_data));
  EXPECT_EQ(my_data, not_nullptr);

  const char* junk = strdup("junk");
  const char* old = nullptr;

  const char* junk1 = strdup("junk");
  const char* junk2 = strdup("junk");

  old = root->app_data_set_and_keep_ownership(&adt0, junk1);
  EXPECT_EQ(old, nullptr);
  old = root->app_data_set_and_keep_ownership(&adt1, junk2);
  EXPECT_EQ(old, nullptr);

  old = top->app_data_set_and_give_ownership(&adt0, junk);
  EXPECT_EQ(old, nullptr);
  old = top->app_data_set_and_keep_ownership(&adt1, not_nullptr);
  EXPECT_EQ(old, nullptr);

  const char* lookup = nullptr;
  EXPECT_TRUE(root->app_data_check_and_get(&adt0, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, junk1);
  lookup = nullptr;
  EXPECT_TRUE(root->app_data_check_and_get(&adt1, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, junk2);

  lookup = nullptr;
  EXPECT_TRUE(top->app_data_check_and_get(&adt0, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, junk);
  lookup = nullptr;
  EXPECT_TRUE(top->app_data_check_and_get(&adt1, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, not_nullptr);
}

TEST(AppData, NodeLookup2)
{
  TEST_DESCRIPTION("YangNode cache and lookup test 2");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx-app-data");
  EXPECT_TRUE(test_ncx);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);

  const char* ns = "http://riftio.com/ns/core/util/yangtools/tests/testncx-app-data";
  typedef AppDataToken<const char*> adt_t;

  adt_t adt1;
  EXPECT_TRUE(model->app_data_get_token(ns, "ext1", &adt1));
  EXPECT_EQ(adt1.index_, 0);
  EXPECT_STREQ(adt1.ns_.c_str(), ns);
  EXPECT_STREQ(adt1.ext_.c_str(), "ext1");
  EXPECT_TRUE(adt1.deleter_);

  adt_t adt2;
  EXPECT_TRUE(model->app_data_get_token(ns, "ext2", &adt2));
  EXPECT_EQ(adt2.index_, 1);
  EXPECT_STREQ(adt2.ns_.c_str(), ns);
  EXPECT_STREQ(adt2.ext_.c_str(), "ext2");
  EXPECT_TRUE(adt2.deleter_);

  const char* const junk = "junk";
  const char* lookup = junk;
  EXPECT_FALSE(top->app_data_is_cached(&adt1));
  EXPECT_FALSE(top->app_data_is_looked_up(&adt1));
  EXPECT_FALSE(top->app_data_check_and_get(&adt1, &lookup));
  EXPECT_EQ(lookup, junk);
  EXPECT_TRUE(top->app_data_check_lookup_and_get(&adt1, &lookup));
  EXPECT_NE(lookup, junk);
  ASSERT_TRUE(lookup);
  EXPECT_STREQ(lookup, "ext1 in top");
  EXPECT_TRUE(top->app_data_is_cached(&adt1));
  EXPECT_TRUE(top->app_data_is_looked_up(&adt1));

  lookup = junk;
  EXPECT_FALSE(top->app_data_is_cached(&adt2));
  EXPECT_FALSE(top->app_data_is_looked_up(&adt2));
  EXPECT_FALSE(top->app_data_check_and_get(&adt2, &lookup));
  EXPECT_EQ(lookup, junk);
  EXPECT_FALSE(top->app_data_check_lookup_and_get(&adt2, &lookup));
  EXPECT_EQ(lookup, junk);
  EXPECT_FALSE(top->app_data_is_cached(&adt2));
  EXPECT_TRUE(top->app_data_is_looked_up(&adt2));
}

TEST(AppData, ModuleLookup1)
{
  TEST_DESCRIPTION("YangModule cache and lookup test 1");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx-mod-app-data");
  EXPECT_TRUE(test_ncx);

  const char* ns = "http://riftio.com/ns/core/util/yangtools/tests/testncx-app-data";
  typedef AppDataToken<const char*> adt_t;

  adt_t adt1;
  EXPECT_TRUE(model->app_data_get_token(ns, "mext1", &adt1));
  EXPECT_EQ(adt1.index_, 0);
  EXPECT_STREQ(adt1.ns_.c_str(), ns);
  EXPECT_STREQ(adt1.ext_.c_str(), "mext1");
  EXPECT_TRUE(adt1.deleter_);

  adt_t adt2;
  EXPECT_TRUE(model->app_data_get_token(ns, "mext2", &adt2));
  EXPECT_EQ(adt2.index_, 1);
  EXPECT_STREQ(adt2.ns_.c_str(), ns);
  EXPECT_STREQ(adt2.ext_.c_str(), "mext2");
  EXPECT_TRUE(adt2.deleter_);

  EXPECT_FALSE(test_ncx->app_data_is_cached(&adt1));
  EXPECT_FALSE(test_ncx->app_data_is_cached(&adt2));
  EXPECT_FALSE(test_ncx->app_data_is_cached(&adt1));
  EXPECT_FALSE(test_ncx->app_data_is_cached(&adt2));

  const char* const junk = "junk";
  const char* lookup = junk;
  EXPECT_FALSE(test_ncx->app_data_is_cached(&adt1));
  EXPECT_FALSE(test_ncx->app_data_is_looked_up(&adt1));
  EXPECT_FALSE(test_ncx->app_data_check_and_get(&adt1, &lookup));
  EXPECT_EQ(lookup, junk);
  EXPECT_TRUE(test_ncx->app_data_check_lookup_and_get(&adt1, &lookup));
  EXPECT_NE(lookup, junk);
  ASSERT_TRUE(lookup);
  EXPECT_STREQ(lookup, "ext1 in module");
  EXPECT_TRUE(test_ncx->app_data_is_cached(&adt1));
  EXPECT_TRUE(test_ncx->app_data_is_looked_up(&adt1));

  lookup = junk;
  EXPECT_FALSE(test_ncx->app_data_is_cached(&adt2));
  EXPECT_FALSE(test_ncx->app_data_is_looked_up(&adt2));
  EXPECT_FALSE(test_ncx->app_data_check_and_get(&adt2, &lookup));
  EXPECT_EQ(lookup, junk);
  EXPECT_TRUE(test_ncx->app_data_check_lookup_and_get(&adt2, &lookup));
  EXPECT_STREQ(lookup, "ext2 in module");
  EXPECT_TRUE(test_ncx->app_data_is_cached(&adt2));
  EXPECT_TRUE(test_ncx->app_data_is_looked_up(&adt2));
}

TEST(AppData, ModuleLookup2)
{
  TEST_DESCRIPTION("YangModule cache and lookup test 2");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx-mod-app-data");
  EXPECT_TRUE(test_ncx);

  typedef AppDataToken<const char*> adt_t;

  adt_t adt0;
  EXPECT_TRUE(model->app_data_get_token("mod", "mext1", &adt0));
  EXPECT_EQ(adt0.index_, 0);
  EXPECT_TRUE(adt0.deleter_);

  adt_t adt1;
  EXPECT_TRUE(model->app_data_get_token("mod", "mext2", &adt1));
  EXPECT_EQ(adt1.index_, 1);
  EXPECT_EQ(adt1.deleter_, adt0.deleter_);

  const char* my_data = "not nullptr";
  const char* not_nullptr = my_data;

  EXPECT_FALSE(test_ncx->app_data_check_and_get(&adt0, &my_data));
  EXPECT_EQ(my_data, not_nullptr);
  EXPECT_FALSE(test_ncx->app_data_check_and_get(&adt1, &my_data));
  EXPECT_EQ(my_data, not_nullptr);

  const char* junk = strdup("junk");
  const char* old = nullptr;

  old = test_ncx->app_data_set_and_give_ownership(&adt0, junk);
  EXPECT_EQ(old, nullptr);
  old = test_ncx->app_data_set_and_keep_ownership(&adt1, not_nullptr);
  EXPECT_EQ(old, nullptr);

  const char* lookup = nullptr;
  EXPECT_TRUE(test_ncx->app_data_check_and_get(&adt0, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, junk);
  lookup = nullptr;
  EXPECT_TRUE(test_ncx->app_data_check_and_get(&adt1, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_EQ(lookup, not_nullptr);
}

TEST(AppData, CliNewMode)
{
  TEST_DESCRIPTION("CLI new mode extension");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx");
  EXPECT_TRUE(test_ncx);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);

  YangNode* a = top->get_first_child();
  ASSERT_TRUE(a);

  typedef AppDataToken<const char*, AppDataTokenDeleterNull> adt_t;
  adt_t adt0;
  const char* mode_string = NULL;
  EXPECT_TRUE(model->app_data_get_token(RW_YANG_CLI_MODULE, RW_YANG_CLI_EXT_NEW_MODE, &adt0));
  EXPECT_EQ(adt0.index_, 0);
  EXPECT_TRUE(a->app_data_check_lookup_and_get(&adt0, &mode_string));
  EXPECT_FALSE(strcmp(mode_string, "test-new-mode"));

  YangNode* cont_in_a = a->get_first_child();
  ASSERT_TRUE(cont_in_a);
  EXPECT_FALSE(cont_in_a->app_data_check_lookup_and_get(&adt0, &mode_string));
}

TEST(AppData, CliPrintHook)
{
  TEST_DESCRIPTION("CLI print hook extension");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx");
  EXPECT_TRUE(test_ncx);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);

  YangNode* show = top->search_child("show");
  ASSERT_TRUE(show);

  YangNode *counters = show->get_first_child()->get_first_child();
  ASSERT_TRUE(counters);

  typedef AppDataToken<const char*, AppDataTokenDeleterNull> adt_t;
  adt_t adt1;
  const char* cli_print_hook_string = NULL;
  EXPECT_TRUE(model->app_data_get_token(RW_YANG_CLI_MODULE, RW_YANG_CLI_EXT_CLI_PRINT_HOOK, &adt1));
  EXPECT_EQ(adt1.index_, 0);
  EXPECT_TRUE(counters->app_data_check_lookup_and_get(&adt1, &cli_print_hook_string));
  EXPECT_FALSE(strcmp(cli_print_hook_string, "test-cli-print-hook"));
}

