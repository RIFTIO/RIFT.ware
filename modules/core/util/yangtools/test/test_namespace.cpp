/* 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 * */

/**
 * @file test_namespace.cpp
 * @author Tom Seidenberg
 * @date 2014/09/02
 * @brief Tests rw_yang::Namespace
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>

#include "rw_namespace.h"
#include "rwut.h"

using namespace rw_yang;

TEST(Namespace, CreateLookup)
{
  TEST_DESCRIPTION("Creates a NamespaceManager");

  rw_namespace_manager_init();

  NamespaceManager& mgr1 = NamespaceManager::get_global();
  NamespaceManager& mgr2 = NamespaceManager::get_global();
  ASSERT_EQ(&mgr1, &mgr2);
}

TEST(Namespace, LookupRuntime)
{
  TEST_DESCRIPTION("Lookup runtime-registered namespaces");

  NamespaceManager& mgr = NamespaceManager::get_global();

  const char* ns1 = "http://riftio.com/core/util/yangtools/test/test_namespace.cpp";
  rw_namespace_id_t nsid = rw_namespace_string_to_nsid(ns1);
  EXPECT_EQ(nsid, RW_NAMESPACE_ID_NULL);

  nsid = rw_namespace_find_or_register(ns1, nullptr/*prefix*/);
  EXPECT_NE(nsid, RW_NAMESPACE_ID_NULL);
  const char* ns2 = mgr.nsid_to_string(nsid);
  EXPECT_STREQ(ns1, ns2);

  rw_namespace_id_t dup_nsid = mgr.find_or_register(ns1, nullptr/*prefix*/);
  EXPECT_EQ(nsid, dup_nsid);

  mgr.unittest_runtime_clear();
  nsid = rw_namespace_string_to_nsid(ns1);
  EXPECT_EQ(nsid, RW_NAMESPACE_ID_NULL);
}

