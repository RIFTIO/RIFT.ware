
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



#include <rwut.h>

#include "rwvcs_zk_api.h"

struct test_struct {
  int accessed;
};

struct test_struct g_test_struct;

#define RWVCS_ZK_RET_UD_IDX(ud, type, idx) ((type *)rwvcs_zk_get_userdata_idx(ud, idx))
rw_status_t update_accessed(rwvcs_zk_module_ptr_t rwvcs_zk, void * ud, int len)
{
  struct test_struct * ts = RWVCS_ZK_RET_UD_IDX(ud, struct test_struct, 0);
  ts->accessed++;
  return RW_STATUS_SUCCESS;
}



class RWVcsZkCallbackTest : public ::testing::Test {
  /*
   * This is a tough one to test as we're really relying on the
   * gobject introspection to do all the data marshalling for us
   * correctly.  At this point, all I can think of to do is to
   * just create a closure and then call it the same way it would
   * typically be called in C and make sure that everything
   * executed as expected.
   */
 protected:
  rwvcs_zk_module_ptr_t rwvcs_zk;

  virtual void SetUp() {
    rwvcs_zk = rwvcs_zk_module_alloc();
    ASSERT_TRUE(rwvcs_zk);

    g_test_struct.accessed = 0;
  }

  virtual void TearDown() {
    rwvcs_zk_module_free(&rwvcs_zk);
  }

  virtual void TestSuccess() {
    ASSERT_TRUE(rwvcs_zk);

    rwvcs_zk_closure_ptr_t closure;

    closure = rwvcs_zk_closure_alloc(
        rwvcs_zk,
        &update_accessed,
        (void *)&g_test_struct);
    ASSERT_TRUE(closure);

    ASSERT_EQ(g_test_struct.accessed, 0);
    rw_vcs_zk_closure_callback(closure);
    ASSERT_EQ(g_test_struct.accessed, 1);

    rwvcs_zk_closure_free(&closure);
    ASSERT_FALSE(closure);

  }
};


TEST_F(RWVcsZkCallbackTest, TestSuccess) {
  TestSuccess();
}
