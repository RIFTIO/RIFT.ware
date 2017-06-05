/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file pb_diff_test.cpp 
 * @author Arun Muralidharan
 * @date 2015/07/02
 * @brief Google test cases for protobuf message diff APIs.
 */
#include <limits.h>
#include <stdlib.h>
#include <iostream>
#include <queue>
#include <sstream>

#include "rwut.h"
#include "rwlib.h"
#include "yangmodel.h"
#include "yangpbc.hpp"

#include "company.pb-c.h"
#include "document.pb-c.h"
#include "flat-conversion.pb-c.h"
#include "test-field-merge-b.pb-c.h"
#include "testy2p-top2.pb-c.h"

using namespace rw_yang;


void init_multi_key_msg(RWPB_T_LISTONLY(Testy2pTop2_data_Topl)& msg)
{
    msg.n_topl = 2;
    msg.topl = (RWPB_T_MSG(Testy2pTop2_data_Topl)**)calloc(2, sizeof(void*));
    msg.topl[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl))); 
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg.topl[0]); 
    msg.topl[0]->k0 = strdup("Key1"); 
    msg.topl[0]->n_la = 2; 
    msg.topl[0]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*)); 
    msg.topl[0]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La))); 
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg.topl[0]->la[0]); 
    msg.topl[0]->la[0]->k1 = strdup("SubKey1"); 
    msg.topl[0]->la[0]->k2 = 100;
    msg.topl[0]->la[0]->has_k2 = 1;
    msg.topl[0]->la[0]->k3 = strdup("SubKey2");
    msg.topl[0]->la[0]->n_lb = 2;
    msg.topl[0]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*)); 
    msg.topl[0]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[0]->la[0]->lb[0]); 
    msg.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1");
    msg.topl[0]->la[0]->lb[0]->k5 = 500;
    msg.topl[0]->la[0]->lb[0]->has_k5 = 1;
    msg.topl[0]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[0]->la[0]->lb[1]); 
    msg.topl[0]->la[0]->lb[1]->k4 = strdup("InnerKey2");
    msg.topl[0]->la[0]->lb[1]->k5 = 600;
    msg.topl[0]->la[0]->lb[1]->has_k5 = 1;
    msg.topl[0]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg.topl[0]->la[1]); 
    msg.topl[0]->la[1]->k1 = strdup("SubKey3");
    msg.topl[0]->la[1]->k2 = 200;
    msg.topl[0]->la[1]->has_k2 = 1;
    msg.topl[0]->la[1]->k3 = strdup("SubKey4");
    msg.topl[0]->la[1]->n_lb = 2;
    msg.topl[0]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)calloc(2, sizeof(void*));
    msg.topl[0]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[0]->la[1]->lb[0]); 
    msg.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3");
    msg.topl[0]->la[1]->lb[0]->k5 = 700;
    msg.topl[0]->la[1]->lb[0]->has_k5 = 1;
    msg.topl[0]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[0]->la[1]->lb[1]); 
    msg.topl[0]->la[1]->lb[1]->k4 = strdup("InnerKey4");
    msg.topl[0]->la[1]->lb[1]->k5 = 800;
    msg.topl[0]->la[1]->lb[1]->has_k5 = 1;
    msg.topl[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl,msg.topl[1]); 
    msg.topl[1]->k0 = strdup("Key2");
    msg.topl[1]->n_la = 3;
    msg.topl[1]->la = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) **)calloc(2, sizeof(void*));
    msg.topl[1]->la[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg.topl[1]->la[0]); 
    msg.topl[1]->la[0]->k1 = strdup("SubKey5");
    msg.topl[1]->la[0]->k2 = 300;
    msg.topl[1]->la[0]->has_k2 = 1;
    msg.topl[1]->la[0]->k3 = strdup("SubKey6");
    msg.topl[1]->la[0]->n_lb = 2;
    msg.topl[1]->la[0]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
    msg.topl[1]->la[0]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[0]->lb[0]); 
    msg.topl[1]->la[0]->lb[0]->k4 = strdup("InnerKey5");
    msg.topl[1]->la[0]->lb[0]->k5 = 900;
    msg.topl[1]->la[0]->lb[0]->has_k5 = 1;
    msg.topl[1]->la[0]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[0]->lb[1]); 
    msg.topl[1]->la[0]->lb[1]->k4 = strdup("InnerKey6");
    msg.topl[1]->la[0]->lb[1]->k5 = 1000;
    msg.topl[1]->la[0]->lb[1]->has_k5 = 1;
    msg.topl[1]->la[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg.topl[1]->la[1]);
    msg.topl[1]->la[1]->k1 = strdup("SubKey7");
    msg.topl[1]->la[1]->k2 = 400;
    msg.topl[1]->la[1]->has_k2 = 1;
    msg.topl[1]->la[1]->k3 = strdup("SubKey8");
    msg.topl[1]->la[1]->n_lb = 2;
    msg.topl[1]->la[1]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(2, sizeof(void*));
    msg.topl[1]->la[1]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[1]->lb[0]); 
    msg.topl[1]->la[1]->lb[0]->k4 = strdup("InnerKey7");
    msg.topl[1]->la[1]->lb[0]->k5 = 1100;
    msg.topl[1]->la[1]->lb[0]->has_k5 = 1;
    msg.topl[1]->la[1]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[1]->lb[1]); 
    msg.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8");
    msg.topl[1]->la[1]->lb[1]->k5 = 1200;
    msg.topl[1]->la[1]->lb[1]->has_k5 = 1;
    
    msg.topl[1]->la[2] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La) *)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La,msg.topl[1]->la[2]);
    msg.topl[1]->la[2]->n_lb = 3;
    msg.topl[1]->la[2]->lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) **)calloc(3, sizeof(void*));
    msg.topl[1]->la[2]->lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[2]->lb[0]);
    msg.topl[1]->la[2]->lb[0]->k4 = strdup("InnerKey9");
    msg.topl[1]->la[2]->lb[0]->k5 = 2200;
    msg.topl[1]->la[2]->lb[0]->has_k5 = 1;

    msg.topl[1]->la[2]->lb[1] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[2]->lb[1]);
    msg.topl[1]->la[2]->lb[1]->k4 = strdup("InnerKey10");
    msg.topl[1]->la[2]->lb[1]->k5 = 3300;
    msg.topl[1]->la[2]->lb[1]->has_k5 = 1;

    msg.topl[1]->la[2]->lb[2] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)*)calloc(1, sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.topl[1]->la[2]->lb[2]);
    msg.topl[1]->la[2]->lb[2]->k4 = strdup("InnerKey10");
    msg.topl[1]->la[2]->lb[2]->k5 = 4400;
    msg.topl[1]->la[2]->lb[2]->has_k5 = 1;

    return;
}


// Helper Macros
#define GET_INT32(fld_info) ((*(int*)fld_info.element))
#define GET_STR(fld_info) (((char*)fld_info.element))
#define GET_FLD_NAME(fld_info) ((fld_info.fdesc->name))

TEST(PbDiff, DiffTest1)
{
    TEST_DESCRIPTION("Test the invalid state");

    protobuf_c_diff_state_t state;
    // Invalid first state
    EXPECT_DEATH(protobuf_c_diff_next(&state), "Assertion");

    bool ok = protobuf_c_diff_done(&state);
    EXPECT_TRUE(ok);

    //Invalid next state
    EXPECT_DEATH(protobuf_c_diff_next(&state), "Assertion");

    protobuf_c_diff_state_t state_1;
    // Directly calling done should be fine
    ok = protobuf_c_diff_done(&state_1);
    EXPECT_TRUE(ok);

}

TEST(PbDiff, DiffTest2)
{
    TEST_DESCRIPTION("Field merge behavior test for lists.");

    /* Protobuf Object 1 */
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont1 = 
                    (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont1);

    tscont1->n_inventory = 3;
    tscont1->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont1->n_inventory, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*));

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        tscont1->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont1->inventory[i]);

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont1->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory[i]->item_id = static_cast<uint32_t>(i);
    }

    tscont1->n_inventory_2 = 3;

    tscont1->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont1->n_inventory_2, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*));

    for (size_t i = 0; i < tscont1->n_inventory_2; i++) {
        tscont1->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont1->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMano-" << i;
        tscont1->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory_2[i]->item_id = static_cast<uint32_t>(i);
        tscont1->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module);
    module.n_all_shops = 5;
    module.all_shops = (char**)calloc(module.n_all_shops, sizeof(char*));

    module.all_shops[0] = strdup("ComicHouse-1");
    module.all_shops[1] = strdup("ComicHouse-2");
    module.all_shops[2] = strdup("ComicHouse-3");
    module.all_shops[3] = strdup("ComicHouse-4");
    module.all_shops[4] = strdup("ComicHouse-5");
    

    // set the shop
    module.shop = tscont1; 

    module.binary_test = (RWPB_T_MSG(TestFieldMergeB_data_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_data_BinaryTest)));
    RWPB_F_MSG_INIT(TestFieldMergeB_data_BinaryTest, module.binary_test);
    module.binary_test->n_binary_list = 2;
    module.binary_test->binary_list = (ProtobufCBinaryData *)malloc(sizeof(ProtobufCBinaryData)*2);
    module.binary_test->binary_list[0].data = (uint8_t *)strdup("Aim "); // Different
    module.binary_test->binary_list[0].len = 5;
    module.binary_test->binary_list[1].data = (uint8_t *)strdup("Bug free code");
    module.binary_test->binary_list[1].len = 14;

    module.binary_test->has_binary1 = true;
    module.binary_test->binary1.len = 3;
    module.binary_test->binary1.data = (uint8_t *)malloc(module.binary_test->binary1.len);
    module.binary_test->binary1.data[0] = 0;
    module.binary_test->binary1.data[1] = 1;
    module.binary_test->binary1.data[2] = 2;


    // Protobuf Object 2 
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont2 =
        (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont2);

    tscont2->n_inventory = 4;

    // TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory
    tscont2->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont2->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory; i++) {
        tscont2->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont2->inventory[i]);
        tscont2->inventory[i]->has_extra_info = true;

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont2->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory[i]->item_id = static_cast<uint32_t>(i);
        tscont2->inventory[i]->extra_info = static_cast<uint32_t>(i);
    }

    tscont2->n_inventory_2 = 3;

    tscont2->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont2->n_inventory_2, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory_2; i++) {
        tscont2->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory2));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont2->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMan-" << i;
        tscont2->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory_2[i]->item_id = static_cast<uint32_t>(i); 
        tscont2->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module2);
    module2.n_all_shops = 5;
    module2.all_shops = (char**)calloc(module2.n_all_shops, sizeof(char*));

    module2.all_shops[0] = strdup("ComicHouse-1");
    module2.all_shops[1] = strdup("ComicHouse-2");
    module2.all_shops[2] = strdup("ComicHouse-7");
    module2.all_shops[3] = strdup("ComicHouse-4");
    module2.all_shops[4] = strdup("ComicHouse-6");

    // set the shop
    module2.shop = tscont2;

    // set binary data
    module2.binary_test = (RWPB_T_MSG(TestFieldMergeB_data_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_data_BinaryTest)));
    RWPB_F_MSG_INIT(TestFieldMergeB_data_BinaryTest, module2.binary_test);
    module2.binary_test->n_binary_list = 2;
    module2.binary_test->binary_list = (ProtobufCBinaryData *)malloc(sizeof(ProtobufCBinaryData)*2);
    module2.binary_test->binary_list[0].data = (uint8_t *)strdup("Aim for");
    module2.binary_test->binary_list[0].len = 8;
    module2.binary_test->binary_list[1].data = (uint8_t *)strdup("Bug free code");
    module2.binary_test->binary_list[1].len = 14;
    
    module2.binary_test->has_binary1 = true;
    module2.binary_test->binary1.len = 3;
    module2.binary_test->binary1.data = (uint8_t *)malloc(module2.binary_test->binary1.len);
    module2.binary_test->binary1.data[0] = 0;
    module2.binary_test->binary1.data[1] = 1;
    module2.binary_test->binary1.data[2] = 3;  // Different data

    module2.binary_test->has_binary2 = true;
    module2.binary_test->binary2.len = 3;
    module2.binary_test->binary2.data = (uint8_t *)malloc(module2.binary_test->binary2.len);
    module2.binary_test->binary2.data[0] = 0;
    module2.binary_test->binary2.data[1] = 1;
    module2.binary_test->binary2.data[2] = 3;

    int changes = 0;
    const ProtobufCDiffInfo *res = NULL;

    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&module.base);
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&module2.base);

    protobuf_c_diff_state_t state;
    ProtobufCDiffOptions opts = {PROTOBUF_C_DIFF_FIND_ALL};

    protobuf_c_diff_start(NULL, &state, &module.base, &module2.base, opts);

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        res = protobuf_c_diff_next(&state);
        ASSERT_TRUE(res);
        EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_EXIST_MISMATCH);
    }

    for (size_t i = 0; i < tscont2->n_inventory_2; i++)
    {
        std::ostringstream oss1, oss2;
        oss1 << "IronMano-" << i;
        oss2 << "IronMan-" << i;
        res = protobuf_c_diff_next(&state);
        ASSERT_TRUE(res);

        EXPECT_STREQ(GET_STR(res->a->fld_info), oss1.str().c_str());
        EXPECT_STREQ(GET_STR(res->b->fld_info), oss2.str().c_str());
        EXPECT_EQ(res->a->list_index, 0);
        EXPECT_EQ(res->b->list_index, 0);
        EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "item_name");
        changes++;
    }

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "ComicHouse-3");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "ComicHouse-7");

    EXPECT_EQ(res->a->list_index, 2);
    EXPECT_EQ(res->b->list_index, 2);
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "ComicHouse-5");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "ComicHouse-6");

    EXPECT_EQ(res->a->list_index, 4);
    EXPECT_EQ(res->b->list_index, 4);
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_VALUE_MISMATCH);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "binary1");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_EXIST_MISMATCH);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "binary2");
    changes++;

    while (res != NULL) {
        res = protobuf_c_diff_next(&state);
    }

    EXPECT_EQ(changes, 7);

    protobuf_c_diff_done(&state);
}


TEST(PbDiff, DiffTest3)
{
    TEST_DESCRIPTION("Bit more complicated case than DiffTest2");

    // Message -1
    RWPB_T_LISTONLY(Testy2pTop2_data_Topl) msg1;
    RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg1);
    init_multi_key_msg(msg1);

    // Message -2
    RWPB_T_LISTONLY(Testy2pTop2_data_Topl) msg2;
    RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg2);
    init_multi_key_msg(msg2);

    // Change few numeric fields in message -2
    msg2.topl[0]->la[0]->k2 = 101;
    msg2.topl[0]->la[0]->lb[1]->k5 = 601;
    msg2.topl[0]->la[1]->lb[0]->k5 = 701;
    msg2.topl[1]->la[1]->lb[1]->k5 = 1201;

    // Change few string fields in message -2
    free(msg2.topl[0]->la[0]->k3);
    msg2.topl[0]->la[0]->k3 = strdup("SubKey2_2");

    free(msg2.topl[0]->la[1]->lb[0]->k4);
    msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3_2");

    free(msg2.topl[1]->la[0]->k3);
    msg2.topl[1]->la[0]->k3 = strdup("SubKey6_2");

    // Change few string fields in message -1
    free(msg1.topl[0]->la[0]->lb[0]->k4);
    msg1.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1_1");

    free(msg1.topl[1]->la[1]->lb[1]->k4);
    msg1.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8_1");

    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

    protobuf_c_diff_state_t state;
    ProtobufCDiffOptions opts = {PROTOBUF_C_DIFF_FIND_ALL};

    protobuf_c_diff_start(NULL, &state, uptr1.get(), uptr2.get(), opts);

    const ProtobufCDiffInfo *res = NULL;
    int changes = 0;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 100);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 101);
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k2");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "SubKey2");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "SubKey2_2");
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k3");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "InnerKey1_1");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "InnerKey1");
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k4");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 600);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 601);
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k5");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "InnerKey3");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "InnerKey3_2");
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k4");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 700);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 701);
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k5");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "SubKey6");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "SubKey6_2");
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k3");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "InnerKey8_1");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "InnerKey8");
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k4");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 1200);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 1201);
    EXPECT_EQ(res->a->list_index, 0);
    EXPECT_EQ(res->b->list_index, 0);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k5");
    changes++;

    EXPECT_EQ(changes, 9);

    while (1) {
        res = protobuf_c_diff_next(&state);
        if (res == NULL) break;
    }

    protobuf_c_diff_done(&state);

    // There should not be any more changes
    EXPECT_EQ(changes, 9);

}



TEST(PbDiff, DiffTest4)
{
    TEST_DESCRIPTION("Test Protobuf Diff after packing unpacking");

    /* Protobuf Object 1 */
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont1 =
            (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont1);

    tscont1->n_inventory = 3;

    // TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory
    tscont1->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont1->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        tscont1->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont1->inventory[i]);
        tscont1->inventory[i]->has_extra_info = false;

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont1->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory[i]->item_id = static_cast<uint32_t>(i);
    }

    tscont1->n_inventory_2 = 3;

    tscont1->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont1->n_inventory_2, sizeof(void*));

    for (size_t i = 0; i < tscont1->n_inventory_2; i++) {
        tscont1->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory2));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont1->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMano-" << i;
        tscont1->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory_2[i]->item_id = static_cast<uint32_t>(i); // Different keyed elements
        tscont1->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module);
    module.n_all_shops = 5;
    module.all_shops = (char**)calloc(module.n_all_shops, sizeof(char*));

    module.all_shops[0] = strdup("ComicHouse-1");
    module.all_shops[1] = strdup("ComicHouse-2");
    module.all_shops[2] = strdup("ComicHouse-3");
    module.all_shops[3] = strdup("ComicHouse-4");
    module.all_shops[4] = strdup("ComicHouse-5");

    // set the shop
    module.shop = tscont1;

    /* Protobuf Object 2 */
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont2 =
           (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont2);

    tscont2->n_inventory = 5;

    // TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory
    tscont2->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont2->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory; i++) {
        tscont2->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont2->inventory[i]);
        tscont2->inventory[i]->has_extra_info = true;

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont2->inventory[i]->item_name = strdup(oss.str().c_str()); // All same keyed elements, this should not be merged
        tscont2->inventory[i]->item_id = static_cast<uint32_t>(i);
        tscont2->inventory[i]->extra_info = static_cast<uint32_t>(i);
    }

    tscont2->n_inventory_2 = 3;

    tscont2->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont2->n_inventory_2, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory_2; i++) {
        tscont2->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory2));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont2->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMan-" << i;
        tscont2->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory_2[i]->item_id = static_cast<uint32_t>(i); // Different keyed elements
        tscont2->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module2);
    module2.n_all_shops = 5;
    module2.all_shops = (char**)calloc(module2.n_all_shops, sizeof(char*));

    module2.all_shops[0] = strdup("ComicHouse-1");
    module2.all_shops[1] = strdup("ComicHouse-2");
    module2.all_shops[2] = strdup("ComicHouse-7"); 
    module2.all_shops[3] = strdup("ComicHouse-4");
    module2.all_shops[4] = strdup("ComicHouse-6"); 

    // set the shop
    module2.shop = tscont2;

    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&module.base);
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&module2.base);

    // Pack - unpack stuff
    uint8_t buff1[4096] = {0,};
    uint8_t buff2[4096] = {0,};

    auto len1 = protobuf_c_message_get_packed_size(nullptr, &module.base);
    auto len2 = protobuf_c_message_get_packed_size(nullptr, &module2.base);

    ASSERT_TRUE(len1 < 4096);
    ASSERT_TRUE(len2 < 4096);

    protobuf_c_message_pack(NULL, &module.base, buff1);
    protobuf_c_message_pack(NULL, &module2.base, buff2);

    auto *out1 = RWPB_F_MSG_UNPACK(TestFieldMergeB_data, NULL, len1, buff1);
    auto *out2 = RWPB_F_MSG_UNPACK(TestFieldMergeB_data, NULL, len2, buff2);

    int changes = 0;
    const ProtobufCDiffInfo *res = NULL;

    UniquePtrProtobufCMessage<>::uptr_t uptr3(&out1->base);
    UniquePtrProtobufCMessage<>::uptr_t uptr4(&out2->base);

    protobuf_c_diff_state_t state;
    ProtobufCDiffOptions opts = {PROTOBUF_C_DIFF_FIND_ALL};

    protobuf_c_diff_start(NULL, &state, &out1->base, &out2->base, opts);

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        res = protobuf_c_diff_next(&state);
        ASSERT_TRUE(res);

        EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_EXIST_MISMATCH);
    }

    for (size_t i = 0; i < tscont2->n_inventory_2; i++)
    {
        res = protobuf_c_diff_next(&state);
        ASSERT_TRUE(res);

        std::ostringstream oss1, oss2;
        oss1 << "IronMano-" << i;
        oss2 << "IronMan-" << i;

        EXPECT_STREQ(GET_STR(res->a->fld_info), oss1.str().c_str());
        EXPECT_STREQ(GET_STR(res->b->fld_info), oss2.str().c_str());
        //EXPECT_EQ(res->a->fld_index, 0);
        //EXPECT_EQ(res->b->fld_index, 0);
        EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "item_name");
        changes++;
    }

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "ComicHouse-3");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "ComicHouse-7");

    //EXPECT_EQ(res->a->fld_index, 1);
    //EXPECT_EQ(res->b->fld_index, 1);
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "ComicHouse-5");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "ComicHouse-6");

    //EXPECT_EQ(res->a->fld_index, 1);
    //EXPECT_EQ(res->a->fld_index, 1);
    changes++;

    EXPECT_EQ(changes, 5);

    res = protobuf_c_diff_next(&state);
    ASSERT_FALSE(res);

    protobuf_c_diff_done(&state);
}




TEST(PbDiff, DiffTest5)
{
    TEST_DESCRIPTION("Testing of stack iterator");

    // Message -1
    RWPB_T_LISTONLY(Testy2pTop2_data_Topl) msg1;
    RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg1);
    init_multi_key_msg(msg1);

    // Message -2
    RWPB_T_LISTONLY(Testy2pTop2_data_Topl) msg2;
    RWPB_F_LISTONLY_INIT(Testy2pTop2_data_Topl, &msg2);
    init_multi_key_msg(msg2);

    // Change few numeric fields in message -2
    msg2.topl[0]->la[0]->k2 = 101;
    msg2.topl[0]->la[0]->lb[1]->k5 = 601;
    msg2.topl[0]->la[1]->lb[0]->k5 = 701;
    msg2.topl[1]->la[1]->lb[1]->k5 = 1201;

    // Change few string fields in message -2
    free(msg2.topl[0]->la[0]->k3);
    msg2.topl[0]->la[0]->k3 = strdup("SubKey2_2");

    free(msg2.topl[0]->la[1]->lb[0]->k4);
    msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3_2");

    free(msg2.topl[1]->la[0]->k3);
    msg2.topl[1]->la[0]->k3 = strdup("SubKey6_2");

    // Change few string fields in message -1
    free(msg1.topl[0]->la[0]->lb[0]->k4);
    msg1.topl[0]->la[0]->lb[0]->k4 = strdup("InnerKey1_1");

    free(msg1.topl[1]->la[1]->lb[1]->k4);
    msg1.topl[1]->la[1]->lb[1]->k4 = strdup("InnerKey8_1");

    msg1.topl[1]->la[2]->lb[2]->k5 = 4402;

    protobuf_c_diff_state_t state;
    ProtobufCDiffOptions opts = {PROTOBUF_C_DIFF_FIND_ALL};
    ProtobufCDiffIterRes iter;

    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

    protobuf_c_diff_start(NULL, &state, uptr1.get(), uptr2.get(), opts);

    const ProtobufCDiffInfo *res = NULL;
    int changes = 0;
    bool st = true;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 100);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 101);

    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k2");
    changes++;

    bool status = protobuf_c_diff_iter_init(&msg2.base, &iter, &state);
    int depth = 0;
    EXPECT_EQ(status, true);

    std::queue<std::tuple<const ProtobufCMessageDescriptor*,
                          const ProtobufCFieldDescriptor*,
                          uint32_t
                         >> field_lvl;

    while (protobuf_c_diff_iter_change_path_up(&iter, &state)) {
      ASSERT_TRUE(iter.result.message);
      field_lvl.push(std::make_tuple(iter.result.message->descriptor,
                                     iter.result.fld_info.fdesc,
                                     iter.result.list_index));
      depth++;
    }

    EXPECT_EQ(depth, 3);
    depth = 0;

    // Verify the entries which were there in the stack
    auto tuple_ent = field_lvl.front();
    auto pair_ent = std::make_pair(std::get<0>(tuple_ent),
                                   std::get<1>(tuple_ent));
    uint32_t list_index = std::get<2>(tuple_ent);

    EXPECT_STREQ(pair_ent.first->name, "testy2p_top2.YangListOnly.Testy2pTop2.Data.Topl");
    EXPECT_STREQ(pair_ent.second->name, "topl");
    EXPECT_EQ(pair_ent.second->label, PROTOBUF_C_LABEL_REPEATED);
    EXPECT_EQ(pair_ent.second->type, PROTOBUF_C_TYPE_MESSAGE);
    field_lvl.pop();

    tuple_ent = field_lvl.front();
    pair_ent = std::make_pair(std::get<0>(tuple_ent),
                              std::get<1>(tuple_ent));
    list_index = std::get<2>(tuple_ent);

    EXPECT_STREQ(pair_ent.first->name, "testy2p_top2.YangData.Testy2pTop2.Topl");
    EXPECT_STREQ(pair_ent.second->name, "la");
    EXPECT_EQ(pair_ent.second->label, PROTOBUF_C_LABEL_REPEATED);
    EXPECT_EQ(pair_ent.second->type, PROTOBUF_C_TYPE_MESSAGE);
    field_lvl.pop();

    tuple_ent = field_lvl.front();
    list_index = std::get<2>(tuple_ent);
    pair_ent = std::make_pair(std::get<0>(tuple_ent),
                              std::get<1>(tuple_ent));
    EXPECT_STREQ(pair_ent.first->name, "testy2p_top2.YangData.Testy2pTop2.Topl.La");
    EXPECT_STREQ(pair_ent.second->name, "k2");
    EXPECT_EQ(pair_ent.second->label, PROTOBUF_C_LABEL_OPTIONAL);
    EXPECT_EQ(pair_ent.second->type, PROTOBUF_C_TYPE_SINT32);
    field_lvl.pop();

    // Iterate past the last element in the stack
    st = protobuf_c_diff_iter_change_path_up(&iter, &state);
    EXPECT_EQ(st, false);
    ASSERT_TRUE(iter.result.message == nullptr);

    // Now, iterate the stack down
    while (protobuf_c_diff_iter_change_path_down(&iter, &state)) {
      ASSERT_TRUE(iter.result.message);
      field_lvl.push(std::make_tuple(iter.result.message->descriptor,
                                     iter.result.fld_info.fdesc,
                                     iter.result.list_index));
      depth++;
    }

    EXPECT_EQ(depth, 2);
    depth = 0;

    // Verify the entries
    tuple_ent = field_lvl.front();
    pair_ent = std::make_pair(std::get<0>(tuple_ent),
                              std::get<1>(tuple_ent));
    list_index = std::get<2>(tuple_ent);

    EXPECT_STREQ(pair_ent.first->name, "testy2p_top2.YangData.Testy2pTop2.Topl");
    EXPECT_STREQ(pair_ent.second->name, "la");
    EXPECT_EQ(pair_ent.second->label, PROTOBUF_C_LABEL_REPEATED);
    EXPECT_EQ(pair_ent.second->type, PROTOBUF_C_TYPE_MESSAGE);
    field_lvl.pop();

    tuple_ent = field_lvl.front();
    pair_ent = std::make_pair(std::get<0>(tuple_ent),
                              std::get<1>(tuple_ent));
    list_index = std::get<2>(tuple_ent);

    EXPECT_STREQ(pair_ent.first->name, "testy2p_top2.YangListOnly.Testy2pTop2.Data.Topl");
    EXPECT_STREQ(pair_ent.second->name, "topl");
    EXPECT_EQ(pair_ent.second->label, PROTOBUF_C_LABEL_REPEATED);
    EXPECT_EQ(pair_ent.second->type, PROTOBUF_C_TYPE_MESSAGE);
    field_lvl.pop();


    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "SubKey2");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "SubKey2_2");
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k3");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_STREQ(GET_STR(res->a->fld_info), "InnerKey1_1");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "InnerKey1");
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k4");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 600);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 601);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k5");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    status = protobuf_c_diff_iter_init(&msg2.base, &iter, &state);
    depth = 0;
    EXPECT_EQ(status, true);
    
    while(protobuf_c_diff_iter_change_path_up(&iter, &state)) {
      ASSERT_TRUE(iter.result.message);
      field_lvl.push(std::make_tuple(iter.result.message->descriptor,
                                     iter.result.fld_info.fdesc,
                                     iter.result.list_index));
      depth++;
    }

    field_lvl.pop();
    tuple_ent = field_lvl.front();
    list_index = std::get<2>(tuple_ent);
    EXPECT_EQ(list_index, 1);

    while (!field_lvl.empty()) {
      field_lvl.pop();
    }

    EXPECT_STREQ(GET_STR(res->a->fld_info), "InnerKey3");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "InnerKey3_2");
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k4");
    changes++;

    // Do up-down-up test on change iteration
    status = protobuf_c_diff_iter_init(&msg2.base, &iter, &state);
    EXPECT_EQ(status, true);

    //msg2.topl[0]->la[1]->lb[0]->k4 = strdup("InnerKey3_2");
    auto i = 0;
    while (i++ < 3) {
      st = protobuf_c_diff_iter_change_path_up(&iter, &state);
      EXPECT_EQ(st, true);
    }

    EXPECT_STREQ(iter.result.message->descriptor->name, "testy2p_top2.YangData.Testy2pTop2.Topl.La");
    EXPECT_STREQ(iter.result.fld_info.fdesc->name, "lb");
    EXPECT_EQ(iter.result.fld_info.fdesc->label, PROTOBUF_C_LABEL_REPEATED);
    EXPECT_EQ(iter.result.fld_info.fdesc->type, PROTOBUF_C_TYPE_MESSAGE);

    st = protobuf_c_diff_iter_change_path_down(&iter, &state);
    EXPECT_EQ(st, true);
    EXPECT_STREQ(iter.result.message->descriptor->name, "testy2p_top2.YangData.Testy2pTop2.Topl");
    EXPECT_STREQ(iter.result.fld_info.fdesc->name, "la");
    EXPECT_EQ(iter.result.fld_info.fdesc->label, PROTOBUF_C_LABEL_REPEATED);
    EXPECT_EQ(iter.result.fld_info.fdesc->label, PROTOBUF_C_LABEL_REPEATED);


    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    EXPECT_EQ(GET_INT32(res->a->fld_info), 700);
    EXPECT_EQ(GET_INT32(res->b->fld_info), 701);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "k5");
    changes++;

    EXPECT_EQ(changes, 6);

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);

    status = protobuf_c_diff_iter_init(&msg2.base, &iter, &state);
    depth = 0;
    EXPECT_EQ(status, true);
    
    while(protobuf_c_diff_iter_change_path_up(&iter, &state)) {
      ASSERT_TRUE(iter.result.message);
      field_lvl.push(std::make_tuple(iter.result.message->descriptor,
                                     iter.result.fld_info.fdesc,
                                     iter.result.list_index));
      depth++;
    }

    tuple_ent = field_lvl.front();
    EXPECT_EQ(std::get<2>(tuple_ent), 1);
    field_lvl.pop();

    tuple_ent = field_lvl.front();
    EXPECT_EQ(std::get<2>(tuple_ent), 2);
    field_lvl.pop();

    tuple_ent = field_lvl.front();
    EXPECT_EQ(std::get<2>(tuple_ent), 2);
    field_lvl.pop();

    while (!field_lvl.empty()) {
      field_lvl.pop();
    }

    while (1) {
        res = protobuf_c_diff_next(&state);
        if (res == NULL) break;
    }

    protobuf_c_diff_done(&state);

    // There should not be any more changes
    EXPECT_EQ(changes, 6);
}


TEST(PbDiff, DiffTest6)
{
    TEST_DESCRIPTION("Simple load test with large number of elements");

    /* Protobuf Object 1 */
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont1 =
            (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont1);

    tscont1->n_inventory = 1000;

    tscont1->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont1->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        tscont1->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont1->inventory[i]);
        tscont1->inventory[i]->has_extra_info = false;

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont1->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory[i]->item_id = static_cast<uint32_t>(i);
    }

    tscont1->n_inventory_2 = 2000;

    tscont1->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont1->n_inventory_2, sizeof(void*));

    for (size_t i = 0; i < tscont1->n_inventory_2; i++) {
        tscont1->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory2));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont1->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMan-" << i;
        tscont1->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory_2[i]->item_id = static_cast<uint32_t>(i); // Different keyed elements
        tscont1->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module);
    module.n_all_shops = 5000;
    module.all_shops = (char**)calloc(module.n_all_shops, sizeof(char*));

    for (size_t i = 0; i < module.n_all_shops; i++) {
      std::ostringstream oss;
      oss << "ComicHouse-" << i;
      module.all_shops[i] = strdup(oss.str().c_str());
    }

    // set the shop
    module.shop = tscont1;

    /* Protobuf Object 2 */
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont2 =
           (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont2);

    tscont2->n_inventory = 1500;

    tscont2->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont2->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory; i++) {
        tscont2->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont2->inventory[i]);

        if (i%2 == 0) {
          tscont2->inventory[i]->has_extra_info = true;
        }

        std::ostringstream oss;
        if (i > 500 && i < 601) {
          oss << "Batman-" << i*2;
        } else {
          oss << "Batman-" << i;
        }
        tscont2->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory[i]->item_id = static_cast<uint32_t>(i);
        tscont2->inventory[i]->extra_info = static_cast<uint32_t>(i);
    }

    tscont2->n_inventory_2 = 2500;

    tscont2->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont2->n_inventory_2, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory_2; i++) {
        tscont2->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory2));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont2->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMan-" << i;
        tscont2->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory_2[i]->item_id = static_cast<uint32_t>(i);
        tscont2->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module2);

    module2.n_all_shops = 4500;
    module2.all_shops = (char**)calloc(module2.n_all_shops, sizeof(char*));

    for (size_t i = 0; i < module2.n_all_shops; i++) {
      std::ostringstream oss;
      oss << "ComicHouse-" << i;
      module2.all_shops[i] = strdup(oss.str().c_str());
    }

    // set the shop
    module2.shop = tscont2;

    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&module.base);
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&module2.base);

    const ProtobufCDiffInfo *res = NULL;
    protobuf_c_diff_state_t state;
    ProtobufCDiffOptions opts = {PROTOBUF_C_DIFF_FIND_ALL};

    protobuf_c_diff_start(NULL, &state, &module.base, &module2.base, opts);

    size_t fld_val_mismatch = 0;
    size_t opt_field_mismatch = 0;

    while (1) {
      res = protobuf_c_diff_next(&state);
      if (!res) break;

      switch (res->mismatch_reason) {
      case PROTOBUF_C_FIELD_VALUE_MISMATCH:
        fld_val_mismatch++;
        break;
      case PROTOBUF_C_FIELD_EXIST_MISMATCH:
        opt_field_mismatch++;
        break;
      default:
        break;
      };
    }

    EXPECT_EQ(fld_val_mismatch, 100);
    EXPECT_EQ(opt_field_mismatch, 500);
}




TEST(PbDiff, DiffTest7)
{
    TEST_DESCRIPTION("Test Diff printing APIs");

    /* Protobuf Object 1 */
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont1 = 
                    (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont1);

    tscont1->n_inventory = 3;
    tscont1->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont1->n_inventory, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*));

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        tscont1->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont1->inventory[i]);

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont1->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory[i]->item_id = static_cast<uint32_t>(i);
    }

    tscont1->n_inventory_2 = 3;

    tscont1->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont1->n_inventory_2, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*));

    for (size_t i = 0; i < tscont1->n_inventory_2; i++) {
        tscont1->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont1->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMano-" << i;
        tscont1->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont1->inventory_2[i]->item_id = static_cast<uint32_t>(i);
        tscont1->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module);
    module.n_all_shops = 5;
    module.all_shops = (char**)calloc(module.n_all_shops, sizeof(char*));

    module.all_shops[0] = strdup("ComicHouse-1");
    module.all_shops[1] = strdup("ComicHouse-2");
    module.all_shops[2] = strdup("ComicHouse-3");
    module.all_shops[3] = strdup("ComicHouse-4");
    module.all_shops[4] = strdup("ComicHouse-5");
    

    // set the shop
    module.shop = tscont1; 

    module.binary_test = (RWPB_T_MSG(TestFieldMergeB_data_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_data_BinaryTest)));
    RWPB_F_MSG_INIT(TestFieldMergeB_data_BinaryTest, module.binary_test);
    module.binary_test->n_binary_list = 2;
    module.binary_test->binary_list = (ProtobufCBinaryData *)malloc(sizeof(ProtobufCBinaryData)*2);
    module.binary_test->binary_list[0].data = (uint8_t *)strdup("Aim "); // Different
    module.binary_test->binary_list[0].len = 5;
    module.binary_test->binary_list[1].data = (uint8_t *)strdup("Bug free code");
    module.binary_test->binary_list[1].len = 14;

    module.binary_test->has_binary1 = true;
    module.binary_test->binary1.len = 3;
    module.binary_test->binary1.data = (uint8_t *)malloc(module.binary_test->binary1.len);
    module.binary_test->binary1.data[0] = 0;
    module.binary_test->binary1.data[1] = 1;
    module.binary_test->binary1.data[2] = 2;


    // Protobuf Object 2 
    //---------------------
    RWPB_T_MSG(TestFieldMergeB_ToyShop)* tscont2 =
        (RWPB_T_MSG(TestFieldMergeB_ToyShop)*) malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_ToyShop)));
    RWPB_F_MSG_INIT(TestFieldMergeB_ToyShop, tscont2);

    tscont2->n_inventory = 4;

    // TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory
    tscont2->inventory = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)**)
                        calloc(tscont2->n_inventory, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory; i++) {
        tscont2->inventory[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory)*)
                           calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory, tscont2->inventory[i]);
        tscont2->inventory[i]->has_extra_info = true;

        std::ostringstream oss;
        oss << "Batman-" << i;
        tscont2->inventory[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory[i]->item_id = static_cast<uint32_t>(i);
        tscont2->inventory[i]->extra_info = static_cast<uint32_t>(i);
    }

    tscont2->n_inventory_2 = 3;

    tscont2->inventory_2 = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)**)
                         calloc(tscont2->n_inventory_2, sizeof(void*));

    for (size_t i = 0; i < tscont2->n_inventory_2; i++) {
        tscont2->inventory_2[i] = (RWPB_T_MSG(TestFieldMergeB_data_Shop_Inventory2)*)
                            calloc(1, sizeof(TestFieldMergeB__YangData__TestFieldMergeB__Shop__Inventory2));

        RWPB_F_MSG_INIT(TestFieldMergeB_data_Shop_Inventory2, tscont2->inventory_2[i]);

        std::ostringstream oss;
        oss << "IronMan-" << i;
        tscont2->inventory_2[i]->item_name = strdup(oss.str().c_str());
        tscont2->inventory_2[i]->item_id = static_cast<uint32_t>(i); 
        tscont2->inventory_2[i]->has_item_id = 1;
    }

    RWPB_M_MSG_DECL_INIT(TestFieldMergeB_data, module2);
    module2.n_all_shops = 5;
    module2.all_shops = (char**)calloc(module2.n_all_shops, sizeof(char*));

    module2.all_shops[0] = strdup("ComicHouse-1");
    module2.all_shops[1] = strdup("ComicHouse-2");
    module2.all_shops[2] = strdup("ComicHouse-7");
    module2.all_shops[3] = strdup("ComicHouse-4");
    module2.all_shops[4] = strdup("ComicHouse-6");

    // set the shop
    module2.shop = tscont2;

    // set binary data
    module2.binary_test = (RWPB_T_MSG(TestFieldMergeB_data_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestFieldMergeB_data_BinaryTest)));
    RWPB_F_MSG_INIT(TestFieldMergeB_data_BinaryTest, module2.binary_test);
    module2.binary_test->n_binary_list = 2;
    module2.binary_test->binary_list = (ProtobufCBinaryData *)malloc(sizeof(ProtobufCBinaryData)*2);
    module2.binary_test->binary_list[0].data = (uint8_t *)strdup("Aim for");
    module2.binary_test->binary_list[0].len = 8;
    module2.binary_test->binary_list[1].data = (uint8_t *)strdup("Bug free code");
    module2.binary_test->binary_list[1].len = 14;
    
    module2.binary_test->has_binary1 = true;
    module2.binary_test->binary1.len = 3;
    module2.binary_test->binary1.data = (uint8_t *)malloc(module2.binary_test->binary1.len);
    module2.binary_test->binary1.data[0] = 0;
    module2.binary_test->binary1.data[1] = 1;
    module2.binary_test->binary1.data[2] = 3;  // Different data

    module2.binary_test->has_binary2 = true;
    module2.binary_test->binary2.len = 3;
    module2.binary_test->binary2.data = (uint8_t *)malloc(module2.binary_test->binary2.len);
    module2.binary_test->binary2.data[0] = 0;
    module2.binary_test->binary2.data[1] = 1;
    module2.binary_test->binary2.data[2] = 3;

    int changes = 0;
    const ProtobufCDiffInfo *res = NULL;

    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&module.base);
    UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&module2.base);

    protobuf_c_diff_state_t state;
    ProtobufCDiffOptions opts = {PROTOBUF_C_DIFF_FIND_ALL};

    protobuf_c_diff_start(NULL, &state, &module.base, &module2.base, opts);

    protobuf_c_diff_print_buffer_t pbuff;

    for (size_t i = 0; i < tscont1->n_inventory; i++) {
        res = protobuf_c_diff_next(&state);
        ASSERT_TRUE(res);
        EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_EXIST_MISMATCH);
        protobuf_c_diff_print_str(&state, &pbuff);
        std::cout << pbuff << std::endl;
    }

    for (size_t i = 0; i < tscont2->n_inventory_2; i++)
    {
        std::ostringstream oss1, oss2;
        oss1 << "IronMano-" << i;
        oss2 << "IronMan-" << i;
        res = protobuf_c_diff_next(&state);
        ASSERT_TRUE(res);

        EXPECT_STREQ(GET_STR(res->a->fld_info), oss1.str().c_str());
        EXPECT_STREQ(GET_STR(res->b->fld_info), oss2.str().c_str());
        //EXPECT_EQ(res->a->fld_index, 0);
        //EXPECT_EQ(res->b->fld_index, 0);
        EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "item_name");
        changes++;

        protobuf_c_diff_print_str(&state, &pbuff);
        std::cout << pbuff << std::endl;
    }

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);
    protobuf_c_diff_print_str(&state, &pbuff);
    std::cout << pbuff << std::endl;

    EXPECT_STREQ(GET_STR(res->a->fld_info), "ComicHouse-3");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "ComicHouse-7");

    //EXPECT_EQ(res->a->fld_index, 1);
    //EXPECT_EQ(res->b->fld_index, 1);
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);
    protobuf_c_diff_print_str(&state, &pbuff);
    std::cout << pbuff << std::endl;

    EXPECT_STREQ(GET_STR(res->a->fld_info), "ComicHouse-5");
    EXPECT_STREQ(GET_STR(res->b->fld_info), "ComicHouse-6");

    //EXPECT_EQ(res->a->fld_index, 1);
    //EXPECT_EQ(res->b->fld_index, 1);
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);
    protobuf_c_diff_print_str(&state, &pbuff);
    std::cout << pbuff << std::endl;

    EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_VALUE_MISMATCH);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "binary1");
    changes++;

    res = protobuf_c_diff_next(&state);
    ASSERT_TRUE(res);
    protobuf_c_diff_print_str(&state, &pbuff);
    std::cout << pbuff << std::endl;

    EXPECT_EQ(res->mismatch_reason, PROTOBUF_C_FIELD_EXIST_MISMATCH);
    EXPECT_STREQ(GET_FLD_NAME(res->a->fld_info), "binary2");
    changes++;

    while (res != NULL) {
        res = protobuf_c_diff_next(&state);
    }

    EXPECT_EQ(changes, 7);

    protobuf_c_diff_done(&state);
}
