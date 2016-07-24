
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwcal_gtest.cc
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 06/27/2014
 * @brief Google test cases for testing rwcal
 * 
 * @details Google test cases for testing rwcal
 */

/* 
 * Step 1: Include the necessary header files 
 */
#include <limits.h>
#include <cstdlib>
#include "rwut.h"
#include "rwlib.h"
#include "rw_vx_plugin.h"
#include "rwtrace.h"
#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"

#include "rwcal-api.h"

using ::testing::HasSubstr;

struct int_status {
  int x;
  rw_status_t status;
};

/*
 * Simple function for testing node data watchers.  Expects ud to
 * be a pointer to an int which will be monotonically increased
 * each time this callback is executed.
 */
#define RWCAL_RET_UD_IDX(ud, type, idx) ((type *)rwcal_get_userdata_idx(ud, idx))
rw_status_t rwcal_data_watcher(rwcal_module_ptr_t rwcal, void *ud, int len)
{
  int * x = RWCAL_RET_UD_IDX(ud, int, 0);
  (*x)++;
  return RW_STATUS_SUCCESS;
}

/*
 * Simple function for testing node data watchers.  Expects ud to
 * be a pointer to a rw_status_t.  When executed, exits with the
 * value pointed to by ud.
 */
rw_status_t rwcal_data_watcher_retval(rwcal_module_ptr_t rwcal, void *ud, int len)
{
  struct int_status * ctx = (struct int_status *)ud;
  ctx->x++;
  return ctx->status;
}

rw_status_t rwcal_rwzk_create_cb(rwcal_module_ptr_t rwcal, void *ud, int len)
{
  int idx = 0;
  int * x = RWCAL_RET_UD_IDX(ud, int, idx);
  (*x)++;
  idx++;
  while (idx < len) {
    char *y =  RWCAL_RET_UD_IDX(ud, char, idx);
    fprintf (stderr, "Got data %s\n", y);
    idx ++;
  }
  return RW_STATUS_SUCCESS;
}

/*
 * Create a test fixture for testing the plugin framework
 *
 * This fixture is reponsible for:
 *   1) allocating the Virtual Executive (VX)
 *   2) cleanup upon test completion
 */
class RwCalZk : public ::testing::Test {
 public:

 protected:
  rwcal_module_ptr_t m_mod;

  virtual void TestGetNonExistentNodeData() {
    char * data;
    rw_status_t status;

    status = rwcal_rwzk_get(m_mod, "/test_get_non_existent_node_data", &data, NULL);
    ASSERT_EQ(RW_STATUS_NOTFOUND, status);
    ASSERT_FALSE(data);
  }

  virtual void TestSetNonExistentNodeData() {
    rw_status_t status;

    status = rwcal_rwzk_set(m_mod, "/test_set_non_existent_node_data", "blah", NULL);
    ASSERT_EQ(RW_STATUS_NOTFOUND, status);
  }

  virtual void TestNodeData() {
    rw_status_t status;
    const char * data = "aa;pofihea;coiha;cmeioeaher";
    char * out_str;
  
    status = rwcal_rwzk_create(m_mod, "/test_node_data", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    /* test setting the node data */
    status = rwcal_rwzk_set(m_mod, "/test_node_data", data, NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
 
    /* test get */
    status = rwcal_rwzk_get(m_mod, "/test_node_data", &out_str, NULL);

    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_TRUE(out_str);
    ASSERT_STREQ(out_str, data);

    free(out_str);
    out_str = NULL;

    /* test delete */
    status = rwcal_rwzk_delete(m_mod, "/test_node_data", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    int inc_int = 0;
    rwcal_closure_ptr_t closure;

    closure = rwcal_closure_alloc(m_mod, &rwcal_rwzk_create_cb, (void *)&inc_int);
    ASSERT_TRUE(closure);

    status = rwcal_rwzk_create(m_mod, "/async_test_node/data", closure);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    int i;
    for (i=0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);


    inc_int = 0;
    /* test setting the node data */
    status = rwcal_rwzk_set(m_mod, "/async_test_node/data", data, closure);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    for (i=0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);

    inc_int = 0;
    status = rwcal_rwzk_get(m_mod, "/async_test_node/data", &out_str, closure);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    for (i=0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);
    ASSERT_FALSE(out_str);


    /* test get */
    status = rwcal_rwzk_get(m_mod, "/async_test_node/data", &out_str, NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_TRUE(out_str);
    ASSERT_STREQ(out_str, data);
    free(out_str);
    out_str = NULL;

    char ** children;
    status = rwcal_rwzk_get_children(m_mod, "/", &children, NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_TRUE(children);
    ASSERT_STREQ(children[0], "async_test_node");
    ASSERT_FALSE(children[1]);
    free(children[0]);
    free(children);
    children = NULL;

    inc_int = 0;
    status = rwcal_rwzk_get_children(m_mod, "/", &children, closure);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    for (i=0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);
    ASSERT_FALSE(children);

    status = rwcal_rwzk_create(m_mod, "/test_node_data1", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_create(m_mod, "/test_node_data2", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_create(m_mod, "/test_node_data3", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_create(m_mod, "/test_node_data4", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_create(m_mod, "/test_node_data5", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    inc_int = 0;
    status = rwcal_rwzk_get_children(m_mod, "/", &children, closure);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    for (i=0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);
    ASSERT_FALSE(children);

    status = rwcal_rwzk_delete(m_mod, "/test_node_data1", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_delete(m_mod, "/test_node_data2", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_delete(m_mod, "/test_node_data3", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_delete(m_mod, "/test_node_data4", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    status = rwcal_rwzk_delete(m_mod, "/test_node_data5", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);





    inc_int = 0;
    /* test setting the node data */
    status = rwcal_rwzk_delete(m_mod, "/async_test_node/data", closure);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    for (i=0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);

    rwcal_closure_free(&closure);
    ASSERT_FALSE(closure);
  }

  virtual void TestExists() {
    rw_status_t status;
    bool exists;

    exists = rwcal_rwzk_exists(m_mod, "/test_exists");
    ASSERT_FALSE(exists);

    status = rwcal_rwzk_create(m_mod, "/test_exists", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    exists = rwcal_rwzk_exists(m_mod, "/test_exists");
    ASSERT_TRUE(exists);

    status = rwcal_rwzk_delete(m_mod, "/test_exists", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    exists = rwcal_rwzk_exists(m_mod, "/test_exists");
    ASSERT_FALSE(exists);
  }

  virtual void TestCreateExistingNode() {
    rw_status_t status;

    /* create a node */
    status = rwcal_rwzk_create(m_mod, "/test_create_existing_node", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    /* try to create the node again */
    status = rwcal_rwzk_create(m_mod, "/test_create_existing_node", NULL);
    ASSERT_EQ(RW_STATUS_EXISTS, status);

    /*Delete all nodes*/
    status = rwcal_rwzk_delete( m_mod, "/test_create_existing_node", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
  }

  virtual void TestCreateDeleteNode() {
    rw_status_t status;
    char ** children;

    status = rwcal_rwzk_create(m_mod, "/test_create_delete_node", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    status = rwcal_rwzk_get_children(m_mod, "/", &children, NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_TRUE(children);
    ASSERT_STREQ(children[0], "test_create_delete_node");
    ASSERT_FALSE(children[1]);

    free(children[0]);
    free(children);

 
    status = rwcal_rwzk_delete(m_mod, "/test_create_delete_node", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    status = rwcal_rwzk_get_children(m_mod, "/", &children, NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_TRUE(children);
    ASSERT_FALSE(children[0]);

    free(children);
  }

  virtual void TestDeleteNonExistentNode() {
    rw_status_t status;

    /* try deleting a node that doesn't exist */
    status = rwcal_rwzk_delete(m_mod, "/test_delete_non_existent_node", NULL);
    ASSERT_EQ(RW_STATUS_NOTFOUND, status);
  }

  virtual void TestLock() {
    rw_status_t status;
    bool locked;
    struct timeval tv = { .tv_sec = 0, .tv_usec = 1000 };
   
    // Test locking nonexistant node
    status = rwcal_rwzk_lock(m_mod, "/test_lock", NULL);
    ASSERT_EQ(RW_STATUS_NOTFOUND, status);

    status = rwcal_rwzk_unlock(m_mod, "/test_lock");
    ASSERT_EQ(RW_STATUS_NOTFOUND, status);

    locked = rwcal_rwzk_locked(m_mod, "/test_lock");
    ASSERT_FALSE(locked);


    status = rwcal_rwzk_create(m_mod, "/test_lock", NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    // Test unlocking node that has not previously been locked
    status = rwcal_rwzk_unlock(m_mod, "/test_lock");
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    locked = rwcal_rwzk_locked(m_mod, "/test_lock");
    ASSERT_FALSE(locked);

    // Lock the node
    status = rwcal_rwzk_lock(m_mod, "/test_lock", &tv);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    locked = rwcal_rwzk_locked(m_mod, "/test_lock");
    ASSERT_TRUE(locked);

    // Test relocking same node
    status = rwcal_rwzk_lock(m_mod, "/test_lock", &tv);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    // Test unlocking locked node
    status = rwcal_rwzk_unlock(m_mod, "/test_lock");
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    locked = rwcal_rwzk_locked(m_mod, "/test_lock");
    ASSERT_TRUE(locked);

    // Test unlocking previously locked node
    status = rwcal_rwzk_unlock(m_mod, "/test_lock");
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    locked = rwcal_rwzk_locked(m_mod, "/test_lock");
    ASSERT_FALSE(locked);

  }

  virtual void TestWatcher() {
    rw_status_t status;
    int inc_int = 0;
    rwcal_closure_ptr_t closure;

    closure = rwcal_closure_alloc(m_mod, &rwcal_data_watcher, (void *)&inc_int);
    ASSERT_TRUE(closure);

    status = rwcal_rwzk_register_watcher(m_mod, "/test_watcher", closure);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    ASSERT_EQ(inc_int, 0);

    status = rwcal_rwzk_create(m_mod, "/test_watcher", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);

    // Watcher is executed in a separate thread, give it time to
    // update.
    for (int i = 0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);

    inc_int = 0;
    status = rwcal_rwzk_set(m_mod, "/test_watcher", "blah", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    for (int i = 0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);

    inc_int = 0;
    status = rwcal_rwzk_create(m_mod, "/test_watcher2", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    ASSERT_EQ(inc_int, 0);

    inc_int = 0;
    bool exists;
    exists = rwcal_rwzk_exists(m_mod, "/test_watcher");
    ASSERT_TRUE(exists);
    ASSERT_EQ(inc_int, 0);

    inc_int = 0;
    status = rwcal_rwzk_unlock(m_mod, "/test_watcher");
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_EQ(inc_int, 0);

    status = rwcal_rwzk_delete(m_mod, "/test_watcher", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    for (int i = 0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 1);

    status = rwcal_rwzk_unregister_watcher(m_mod, "/test_watcher", closure);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);

    inc_int = 0;
    status = rwcal_rwzk_create(m_mod, "/test_watcher", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    for (int i = 0; i < 100; ++i) {
      if (inc_int)
        break;
      usleep(1000);
    }
    ASSERT_EQ(inc_int, 0);

    rwcal_closure_free(&closure);
    ASSERT_FALSE(closure);

    status = rwcal_rwzk_delete(m_mod, "/test_watcher", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);

    status = rwcal_rwzk_delete(m_mod, "/test_watcher2", NULL);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
  }

#if 0
  /*
   * It would be nice to add this test, but, because the kazoo
   * watcher executes in a separate thread there is no way to
   * catch any exceptions raised there.
   *
   * Hopefully this can be addressed as part of RIFT-2812
   */
  virtual void TestFailingWatcher() {
    rw_status_t status;
    rwcal_closure_ptr_t closure;
    struct int_status ctx = { .x = 0, .status = RW_STATUS_FAILURE };

    closure = rwcal_closure_alloc(m_mod, &rwcal_data_watcher_retval, (void *)&ctx);
    ASSERT_TRUE(closure);

    // Note that each event is currently causing two calls.  See
    // RIFT-2812
    status = rwcal_rwzk_register_watcher(m_mod, "/test_failing_watcher", closure);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);

    status = rwcal_rwzk_create(m_mod, "/test_failing_watcher");
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    for (int i = 0; i < 1000; ++i) {
      if (ctx.x > 0)
        break;
      usleep(1000);
    }
    ASSERT_GT(ctx.x, 0);

    PyGILState_STATE state = PyGILState_Ensure ();

    PyObject * exc = PyErr_Occurred();
    PyGILState_Release (state);

    
  }
#endif

  virtual void InitPluginFramework() {
    rw_status_t status;

    m_mod = rwcal_module_alloc();
    ASSERT_TRUE(m_mod);

    status = rwcal_rwzk_zake_init(m_mod);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
  }

  virtual void TearDown() {
    rwcal_module_free(&m_mod);
  }
};

class RwCalZakePythonPlugin : public RwCalZk {
  virtual void SetUp() {
    InitPluginFramework();
  }
};

TEST_F(RwCalZakePythonPlugin, CreateDeleteNode) {
  TestCreateDeleteNode();
}

TEST_F(RwCalZakePythonPlugin, CreateExistingNode) {
  TestCreateExistingNode();
}

TEST_F(RwCalZakePythonPlugin, NodeData) {
  TestNodeData();
}

TEST_F(RwCalZakePythonPlugin, Exists) {
  TestExists();
}

TEST_F(RwCalZakePythonPlugin, GetNonExistentNodeData) {
  TestGetNonExistentNodeData();
}

TEST_F(RwCalZakePythonPlugin, SetNonExistentNodeData) {
  TestSetNonExistentNodeData();
}

TEST_F(RwCalZakePythonPlugin, DeleteNonExistentNode) {
  TestDeleteNonExistentNode();
}

TEST_F(RwCalZakePythonPlugin, TestLock) {
  TestLock();
}

TEST_F(RwCalZakePythonPlugin, TestWatcher) {
  TestWatcher();
}

TEST_F(RwCalZakePythonPlugin, TestWatcher2) {
  TestWatcher();
}

#if 0
// See comments at TestFailingWatcher's implementation
TEST_F(RwCalZakePythonPlugin, TestFailingWatcher) {
  TestFailingWatcher();
}
#endif

