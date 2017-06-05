/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwperson_database_gtest.cpp
 * @author Sujithra Periasamy
 * @date 02/19/2014
 * @brief Test cases for testing the protoc/yangpbc generated gi code.
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <random>

#include "rwut.h"
#include "rwlib.h"
#include "rwperson-db.pb-c.h"


uint64_t malloc_reqs = 0;
uint64_t free_reqs = 0;

static gpointer my_malloc(gsize n_bytes)
{
  malloc_reqs++;
  return malloc(n_bytes);
}

static gpointer my_realloc(gpointer mem, gsize n_bytes)
{
  return realloc (mem, n_bytes);
}

static void my_free(gpointer mem)
{
  free_reqs++;
  free(mem);
}

GMemVTable my_vtable = { my_malloc, my_realloc, my_free, NULL, NULL, NULL };

void rwpersondb_test_fill_fav(rwpb_gi_RwpersonDb_FlatPerson_Favourite *boxed_fav)
{
  const gchar *places[] = {"bangalore", "boston"};
  GError *err = NULL;
  rwperson_db__yang_data__rwperson_db__flat_person__favourite__gi_set_places(boxed_fav, const_cast<gchar **>(places), 2, &err);

  const int numbers[] = {31, 14};
  rwperson_db__yang_data__rwperson_db__flat_person__favourite__gi_set_numbers(boxed_fav, const_cast<int *>(numbers), 2, &err);

  const gchar *colors[] = {"green", "purple"};
  rwperson_db__yang_data__rwperson_db__flat_person__favourite__gi_set_colors(boxed_fav, colors, 2, &err);
}

void rwpersondb_test_fill_phone(rwpb_gi_RwpersonDb_FlatPhoneNumber *phone)
{
  static int number=1234;
  GError *err = NULL;
  rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_set_number(phone, const_cast<gchar *>(std::to_string(number).c_str()), &err);
  number++;
}

void rwpersondb_test_fill_emerg_phone(rwpb_gi_RwpersonDb_FlatPhoneNumber1 *emer_phone)
{
  static int number=2222;
  GError *err = NULL;
  rwperson_db__yang_data__rwperson_db__flat_person__emergency_phone__gi_set_number(emer_phone, const_cast<gchar *>(std::to_string(number).c_str()), &err);
  number++;
}

TEST(RwpersonDB, RefCounting)
{
  TEST_DESCRIPTION("Test the reference counting for gi boxed structures.");
  GError *err = NULL;

  rwpb_gi_RwpersonDb_FlatPerson *boxed_person = rwperson_db__yang_data__rwperson_db__flat_person__gi_new();
  EXPECT_TRUE(boxed_person);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 1);

  rwpb_gi_RwpersonDb_FlatPerson_Favourite *boxed_fav = rwperson_db__yang_data__rwperson_db__flat_person__gi_get_favourite(boxed_person, &err);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 2);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 2);

  protobuf_c_message_gi_ref(&boxed_fav->box);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 2);

  protobuf_c_message_gi_ref(&boxed_fav->box);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 4);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 2);

  rwpersondb_test_fill_fav(boxed_fav);

  rwpb_gi_RwpersonDb_FlatPhoneNumber *phone1 = rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_new();
  EXPECT_TRUE(phone1);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 1);
  rwpersondb_test_fill_phone(phone1);

  rwpb_gi_RwpersonDb_FlatPhoneNumber *phone2 = rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_new();
  EXPECT_TRUE(phone2);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 1);
  rwpersondb_test_fill_phone(phone2);

  rwpb_gi_RwpersonDb_FlatPhoneNumber *phone_list[] = {phone1, phone2};
  rwperson_db__yang_data__rwperson_db__flat_person__gi_set_phone(boxed_person, phone_list, 2, &err);

  EXPECT_EQ(phone1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 2);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 4);

  protobuf_c_message_gi_ref(&phone1->box);
  protobuf_c_message_gi_ref(&phone2->box);

  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 4);

  rwpb_gi_RwpersonDb_FlatPhoneNumber1 *boxed_emer = rwperson_db__yang_data__rwperson_db__flat_person__emergency_phone__gi_new();
  EXPECT_TRUE(boxed_emer);
  EXPECT_EQ(boxed_emer->s.gi_base.ref_count, 1);
  rwpersondb_test_fill_emerg_phone(boxed_emer);

  rwperson_db__yang_data__rwperson_db__flat_person__gi_set_emergency_phone(boxed_person, boxed_emer, &err);
  EXPECT_EQ(boxed_emer->s.gi_base.ref_count, 2);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 5);

  rwpb_gi_RwpersonDb_FlatPhoneNumber1 *boxed_emer1 = rwperson_db__yang_data__rwperson_db__flat_person__emergency_phone__gi_new();
  EXPECT_TRUE(boxed_emer1);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 1);
  rwpersondb_test_fill_emerg_phone(boxed_emer1);

  rwperson_db__yang_data__rwperson_db__flat_person__gi_set_emergency_phone(boxed_person, boxed_emer1, &err);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 5);
  EXPECT_EQ(boxed_emer->s.gi_base.ref_count, 1);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 4);

  rwperson_db__yang_data__rwperson_db__flat_person__emergency_phone__gi_unref(boxed_emer);
  boxed_emer = NULL;
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 5);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 4);

  rwperson_db__yang_data__rwperson_db__flat_person__favourite__gi_unref(boxed_fav);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 5);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 3);

  rwperson_db__yang_data__rwperson_db__flat_person__favourite__gi_unref(boxed_fav);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 5);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 2);

  rwperson_db__yang_data__rwperson_db__flat_person__favourite__gi_unref(boxed_fav);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 4);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_fav->s.gi_base.ref_count, 1);

  rwperson_db__yang_data__rwperson_db__flat_person__gi_unref(boxed_person);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 3);
  EXPECT_EQ(boxed_emer1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);
  boxed_fav = NULL;

  rwperson_db__yang_data__rwperson_db__flat_person__emergency_phone__gi_unref(boxed_emer1);
  boxed_emer1 = NULL;
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 3);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 3);

  rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_unref(phone1);
  rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_unref(phone2);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone1->s.gi_base.ref_count, 2);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 2);

  rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_unref(phone1);
  EXPECT_EQ(boxed_person->s.gi_base.ref_count, 1);
  EXPECT_EQ(phone2->s.gi_base.ref_count, 2);

  rwperson_db__yang_data__rwperson_db__flat_person__phone__gi_unref(phone2);
}

void rwpersondb_test_setup(int argc, char** argv)
{
// g_mem_set_vtable (&my_vtable);
}

RWUT_INIT(rwpersondb_test_setup);
