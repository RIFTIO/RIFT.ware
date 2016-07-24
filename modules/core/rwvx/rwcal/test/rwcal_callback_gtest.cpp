
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#include <rwut.h>

#include "rwcal-api.h"

struct test_struct {
  int accessed;
};

struct test_struct g_test_struct;

#define RWCAL_RET_UD_IDX(ud, type, idx) ((type *)rwcal_get_userdata_idx(ud, idx))
rw_status_t update_accessed(rwcal_module_ptr_t rwcal, void * ud, int len)
{
  struct test_struct * ts = RWCAL_RET_UD_IDX(ud, struct test_struct, 0);
  ts->accessed++;
  return RW_STATUS_SUCCESS;
}

class RWCalCallbackTest : public ::testing::Test {
  /*
   * This is a tough one to test as we're really relying on the
   * gobject introspection to do all the data marshalling for us
   * correctly.  At this point, all I can think of to do is to
   * just create a closure and then call it the same way it would
   * typically be called in C and make sure that everything
   * executed as expected.
   */
 protected:
  rwcal_module_ptr_t rwcal;

  virtual void SetUp() {
    rwcal = rwcal_module_alloc();
    ASSERT_TRUE(rwcal);

    g_test_struct.accessed = 0;
  }

  virtual void TearDown() {
    rwcal_module_free(&rwcal);
  }

  virtual void TestSuccess() {
    rwcal_closure_ptr_t closure;

    closure = rwcal_closure_alloc(
        rwcal,
        &update_accessed,
        (void *)&g_test_struct);
    ASSERT_TRUE(closure);

    ASSERT_EQ(g_test_struct.accessed, 0);
    rw_cal_closure_callback(closure);
    ASSERT_EQ(g_test_struct.accessed, 1);

    rwcal_closure_free(&closure);
    ASSERT_FALSE(closure);
  }
};


TEST_F(RWCalCallbackTest, TestSuccess) {
  TestSuccess();
}
