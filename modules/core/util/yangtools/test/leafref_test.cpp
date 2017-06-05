/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file leafref_test.cpp
 *
 * Test yang leafref constructs.
 */

#include "yangncx.hpp"

#include "rwut.h"

#include <limits.h>
#include <cstdlib>
#include <iostream>

using namespace rw_yang;


TEST(Leafref, Local)
{
  TEST_DESCRIPTION("Local leafref test - leafrefs to same module");

  YangModelNcx* model = YangModelNcx::create_model();
  ASSERT_NE( nullptr, model );
  YangModel::ptr_t p(model);

  YangModule* rwvnfd = model->load_module("rwvnfd-d");
  ASSERT_NE( nullptr, rwvnfd );

  YangNode* root = model->get_root_node();
  ASSERT_NE( nullptr, root );

  /*
   * Check that a non-leafref node has no target ref.
   */
  YangNode* leafref_ref = root->get_leafref_ref();
  EXPECT_EQ( nullptr, leafref_ref );

  /*
   * Lookup a leafref and find its ref.  Ensure the ref matches the
   * target, by looking up the target directly.
   */
  YangNode* vnfd = rwvnfd->search_node("vnfd", nullptr);
  ASSERT_NE( nullptr, vnfd );
  YangNode* ivld = vnfd->search_child("internal-vlds");
  ASSERT_NE( nullptr, ivld );
  YangNode* icp1 = ivld ->search_child("internal-connection-points");
  ASSERT_NE( nullptr, icp1 );
  YangNode* id1 = icp1->search_child("id");
  ASSERT_NE( nullptr, id1 );
  YangType* id1_type = id1->get_type();
  ASSERT_NE( nullptr, id1_type );
  EXPECT_EQ( id1_type->get_leaf_type(), RW_YANG_LEAF_TYPE_LEAFREF );

  YangNode* id1_ref = id1->get_leafref_ref();
  EXPECT_NE( nullptr, id1_ref );

  YangNode* icp2 = vnfd ->search_child("internal-connection-points");
  ASSERT_NE( nullptr, icp2 );
  YangNode* id2 = icp2->search_child("id");
  ASSERT_NE( nullptr, id2 );
  EXPECT_EQ( id2, id1_ref);
  EXPECT_EQ( nullptr, id2->get_leafref_ref() );

  YangType* id2_type = id2->get_type();
  ASSERT_NE( nullptr, id2_type );
  EXPECT_NE( id2_type->get_leaf_type(), RW_YANG_LEAF_TYPE_LEAFREF );

  /*
   * Ensure that multiple targets to the same ref are equivalent.  Also
   * inverts the order of lookups - does the target lookup first, then
   * the leafref lookups.
   */
  YangNode* vdus = vnfd->search_child("vdus");
  ASSERT_NE( nullptr, vdus );
  YangNode* id3 = vdus->search_child("id");
  ASSERT_NE( nullptr, id3 );
  YangType* id3_type = id3->get_type();
  ASSERT_NE( nullptr, id3_type );
  EXPECT_NE( id3_type->get_leaf_type(), RW_YANG_LEAF_TYPE_LEAFREF );
  EXPECT_EQ( nullptr, id3->get_leafref_ref() );

  YangNode* vdudl = vnfd->search_child("vdu-dependency-list");
  ASSERT_NE( nullptr, vdudl );

  YangNode* vduid = vdudl->search_child("vdu-id");
  ASSERT_NE( nullptr, vduid );
  YangType* vduid_type = vduid->get_type();
  ASSERT_NE( nullptr, vduid_type );
  EXPECT_EQ( vduid_type->get_leaf_type(), RW_YANG_LEAF_TYPE_LEAFREF );
  YangNode* vduid_ref = vduid->get_leafref_ref();
  EXPECT_NE( nullptr, vduid_ref );
  EXPECT_EQ( id3, vduid_ref );

  YangNode* depson = vdudl->search_child("depends-on");
  ASSERT_NE( nullptr, depson );
  YangType* depson_type = depson->get_type();
  ASSERT_NE( nullptr, depson_type );
  EXPECT_EQ( depson_type->get_leaf_type(), RW_YANG_LEAF_TYPE_LEAFREF );
  YangNode* depson_ref = depson->get_leafref_ref();
  EXPECT_NE( nullptr, depson_ref );
  EXPECT_EQ( id3, depson_ref );
}

TEST(Leafref, OtherModule)
{
  TEST_DESCRIPTION("Other Module leafref test - leafrefs to another module");

  YangModelNcx* model = YangModelNcx::create_model();
  ASSERT_NE( nullptr, model );
  YangModel::ptr_t p(model);

  YangModule* rwvnffgd = model->load_module("rwvnffgd-d");
  ASSERT_NE( nullptr, rwvnffgd );

  YangNode* vnffgd = rwvnffgd->search_node("vnffgd", nullptr);
  ASSERT_NE( nullptr, vnffgd );

  /*
   * Lookup some leafrefs that target nodes outside of the leafref
   * module.  Don't load the targetted modules yet - want to force
   * other module deep node populate walk as a test.
   */
  YangNode* dvl = vnffgd->search_child("dependent-virtual-link");
  ASSERT_NE( nullptr, dvl );
  YangNode* vldid1 = dvl->search_child("vld-id");
  ASSERT_NE( nullptr, vldid1 );
  YangNode* vldid1_ref = vldid1->get_leafref_ref();
  EXPECT_NE( nullptr, vldid1_ref );

  YangNode* vcps = vnffgd->search_child("vnf-connection-points");
  ASSERT_NE( nullptr, vcps );
  YangNode* id1 = vcps->search_child("id");
  ASSERT_NE( nullptr, id1 );
  YangNode* id1_ref = id1->get_leafref_ref();
  EXPECT_NE( nullptr, id1_ref );

  YangNode* cvs = vnffgd->search_child("constituent-vnfs");
  ASSERT_NE( nullptr, cvs );
  YangNode* vnfid1 = cvs->search_child("vnf-id");
  ASSERT_NE( nullptr, vnfid1 );
  YangNode* vnfid1_ref = vnfid1->get_leafref_ref();
  EXPECT_NE( nullptr, vnfid1_ref );

  /*
   * Now lookup the targets within their respective modules.
   */
  YangModule* rwvld = model->load_module("rwvld-d");
  ASSERT_NE( nullptr, rwvld );
  YangNode* vld = rwvld->search_node("vld", nullptr);
  ASSERT_NE( nullptr, vld );
  YangNode* vldid2 = vld->search_child("vld-id", nullptr);
  ASSERT_NE( nullptr, vldid2 );
  EXPECT_EQ( vldid1_ref, vldid2 );

  YangModule* rwvnfd = model->load_module("rwvnfd-d");
  ASSERT_NE( nullptr, rwvnfd );
  YangNode* vnfd = rwvnfd->search_node("vnfd", nullptr);
  ASSERT_NE( nullptr, vnfd );
  YangNode* cps = vnfd->search_child("connection-points", nullptr);
  ASSERT_NE( nullptr, cps );
  YangNode* id2 = cps->search_child("id", nullptr);
  ASSERT_NE( nullptr, id2 );
  EXPECT_EQ( id1_ref, id2 );

  YangNode* vnfid2 = vnfd->search_child("vnfd-id", nullptr);
  ASSERT_NE( nullptr, vnfid2 );
  EXPECT_EQ( vnfid1_ref, vnfid2 );
}

