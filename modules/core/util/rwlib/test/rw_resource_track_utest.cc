
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



/**
 * @file rwlib_gtest.cc
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @author Matt Harper (matt.harper@riftio.com)
 * @date 11/18/2013
 * @brief Google test cases for testing rwlib
 * 
 * @details Google test cases for testing rwlib. Specifically
 * This file includes test cases for RIFT_ASSERT framework
 */

/** 
 * Step 1: Include the necessary header files 
 */
#include <limits.h>
#include <cstdlib>
#include "rwlib.h"
#include "rw_resource_track.h"

#include "rwut.h"

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

/*
 * Step 2: Use the TEST macro to define your tests. The following
 * is the notes from Google sample test
 *
 * TEST has two parameters: the test case name and the test name.
 * After using the macro, you should define your test logic between a
 * pair of braces.  You can use a bunch of macros to indicate the
 * success or failure of a test.  EXPECT_TRUE and EXPECT_EQ are
 * examples of such macros.  For a complete list, see gtest.h.
 *
 * In Google Test, tests are grouped into test cases.  This is how we
 * keep test code organized.  You should put logically related tests
 * into the same test case.
 *
 * The test case name and the test name should both be valid C++
 * identifiers.  And you should not use underscore (_) in the names.
 *
 * Google Test guarantees that each test you define is run exactly
 * once, but it makes no guarantee on the order the tests are
 * executed.  Therefore, you should write your tests in such a way
 * that their results don't depend on their order.
 */

TEST(RWResourceTrack, CreateContext) {
  RW_RESOURCE_TRACK_HANDLE rh;

  RWUT_BENCH_ITER(CreateContextBench, 2, 5);
      // TEST create context and free it
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("Hello");
      RW_RESOURCE_TRACK_FREE(rh);
  RWUT_BENCH_END(CreateContextBench);
}

TEST(RWResourceTrack, MallocFree) {
  int *i1;

  RWUT_BENCH_ITER(MallocFreeBench, 2, 5);
      //TEST - MALLOC and FREE
      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      RW_FREE_TYPE(i1, int);
  RWUT_BENCH_END(MallocFreeBench);
}

TEST(RWResourceTrack, MallocTrackFree) {
  RW_RESOURCE_TRACK_HANDLE rh;
  int *i1;

  rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

  RWUT_BENCH_ITER(CreateContextBench, 2, 5);
      //TEST - MALLOC, TRACK and then FREE
      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, i1, int);
      RW_FREE_TYPE(i1, int);
  RWUT_BENCH_END(CreateContextBench);

  //CLEANUP ALL THE MEORY FOOTPRINT
  RW_RESOURCE_TRACK_FREE(rh);
}

TEST(RWResourceTrack, HierarchicalAttach) {
  RW_RESOURCE_TRACK_HANDLE rh, rh1;
  int *i1;
  char *c1, *c2;
  float *f1;

  RWUT_BENCH_ITER(HierarchicalAttachBench, 2, 5);
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("inside HierarchicalAttach");
      //TEST - MALLOC0, hierarchically TRACK
      i1 = RW_MALLOC0_TYPE(sizeof(int), int); *i1 = 100;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, i1, int);
      c2 = RW_MALLOC0_TYPE(sizeof(char), char); *c2 = 202;
      RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, c2, char);
      c1 = RW_MALLOC0_TYPE(sizeof(char), char); *c1 = 200;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, c1, char);
      f1 = RW_MALLOC0_TYPE(sizeof(float), float); *f1 = 300;
      RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, f1, float);

      printf("before frees\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      RW_FREE_TYPE(f1, float);
      RW_FREE_TYPE(c1, char);

      printf("after a couple of frees\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      //CLEANUP ALL THE MEORY FOOTPRINT
      RW_RESOURCE_TRACK_FREE(rh);
  RWUT_BENCH_END(HierarchicalAttachBench);
}

TEST(RWResourceTrack, HierarchicalAttachWNoType) {
  RW_RESOURCE_TRACK_HANDLE rh, rh1;
  int *i1;
  char *c1, *c2;
  float *f1;

  RWUT_BENCH_ITER(HierarchicalAttachWNoTypeBench, 2, 5);
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("inside HierarchicalAttachWNoType");

      //TEST - MALLOC, hirarchically TRACK
      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      rh1 = RW_RESOURCE_TRACK_ATTACH_CHILD(rh, i1, sizeof(int), "i1 of type int");
      c2 = RW_MALLOC_TYPE(sizeof(char), char); *c2 = 202;
      RW_RESOURCE_TRACK_ATTACH_CHILD(rh1, c2, sizeof(char), "c1 of type char");
      c1 = RW_MALLOC_TYPE(sizeof(char), char); *c1 = 200;
      rh1 = RW_RESOURCE_TRACK_ATTACH_CHILD(rh1, c1, sizeof(char), "c2 of type char");
      f1 = RW_MALLOC_TYPE(sizeof(float), float); *f1 = 300;
      RW_RESOURCE_TRACK_ATTACH_CHILD(rh1, f1, sizeof(float), "f1 of type float");

      printf("before frees\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      RW_FREE_TYPE(f1, float);
      RW_FREE_TYPE(c1, char);

      printf("after a couple of frees\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      //CLEANUP ALL THE MEORY FOOTPRINT
      RW_RESOURCE_TRACK_FREE(rh);
  RWUT_BENCH_END(HierarchicalAttachWNoTypeBench);
}

TEST(RWResourceTrack, SimpleTracking) {
  RW_RESOURCE_TRACK_HANDLE rh, rh1;
  int *i1;
  char *c1, *c2;

  RWUT_BENCH_ITER(RemoveTrackingBench, 2, 5);
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("inside SimpleTracking");
      //TEST - MALLOC, TRACK, FREE
      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;

      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, i1, int);
      c1 = RW_MALLOC_TYPE(sizeof(char), char); *c1 = 200;
      RW_RESOURCE_TRACK_ATTACH_CHILD(rh, c1, sizeof(char), "c1 of type char");
      c2 = RW_MALLOC_TYPE(sizeof(char), char); *c2 = 202;
      RW_RESOURCE_TRACK_ATTACH_CHILD(rh, c2, sizeof(char), "c2 of type char");

      printf("before removing tracking\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      RW_RESOURCE_TRACK_REMOVE_TRACKING_AND_FREE(i1);
      RW_RESOURCE_TRACK_REMOVE_TRACKING_AND_FREE(c1);

      printf("tracking removed\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      RW_RESOURCE_TRACK_REMOVE_TRACKING_AND_FREE(c2);

      RW_FREE_TYPE(i1, int);
      RW_FREE_TYPE(c1, char);
      RW_FREE_TYPE(c2, char);

      //CLEANUP ALL THE MEORY FOOTPRINT
      RW_RESOURCE_TRACK_FREE(rh);
  RWUT_BENCH_END(RemoveTrackingBench);
  rh1 = rh1;
}

TEST(RWResourceTrack, RemoveTracking) {
  RW_RESOURCE_TRACK_HANDLE rh, rh1;
  int *i1;
  char *c1, *c2;
  float *f1;

  RWUT_BENCH_ITER(RemoveTrackingBench, 2, 5);
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("inside RemoveTracking");
      //TEST - MALLOC, hirarchically TRACK
      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, i1, int);
      c2 = RW_MALLOC_TYPE(sizeof(char), char); *c2 = 202;
      RW_RESOURCE_TRACK_ATTACH_CHILD(rh1, c2, sizeof(char), "c1 of type char");
      c1 = RW_MALLOC_TYPE(sizeof(char), char); *c1 = 200;
      rh1 = RW_RESOURCE_TRACK_ATTACH_CHILD(rh1, c1, sizeof(char), "c2 of type char");
      f1 = RW_MALLOC_TYPE(sizeof(float), float); *f1 = 300;
      RW_RESOURCE_TRACK_ATTACH_CHILD(rh1, f1, sizeof(float), "f1 of type float");

      printf("before removing tracking\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      RW_RESOURCE_TRACK_TYPE_REMOVE_TRACKING(i1, int);
      RW_RESOURCE_TRACK_REMOVE_TRACKING(c2);

      printf("after a couple of tracking removed\n");
      RW_RESOURCE_TRACK_DUMP(rh);
      printf("\n");

      //CLEANUP ALL THE MEORY FOOTPRINT
      RW_RESOURCE_TRACK_FREE(rh);
      RW_FREE_TYPE(i1, int);
      RW_FREE_TYPE(c2, char);
  RWUT_BENCH_END(RemoveTrackingBench);
}

typedef struct {
  int i1;
  char *cp1;
  float f1;
} my_struct_t;

TEST(RWResourceTrack, MyStruct) {
  RW_RESOURCE_TRACK_HANDLE rh, rh1;
  int *i1;
  char *c1;
  float *f1;
  my_struct_t *ms1;

  RWUT_BENCH_ITER(MyStructBench, 2, 5);
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("inside MyStruct");

      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, i1, int);
      ms1 = RW_MALLOC_TYPE(sizeof(*ms1), my_struct_t);
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, ms1, my_struct_t);
      c1 = RW_MALLOC_TYPE(sizeof(char), char); *c1 = 200;
      RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, c1, char);
      RW_RESOURCE_TRACK_REPARENT(ms1, rh);

      f1 = RW_MALLOC_TYPE(sizeof(float), float); *f1 = 300;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, f1, float);

      //CLEANUP ALL THE MEORY FOOTPRINT
      RW_RESOURCE_TRACK_FREE(rh);

  RWUT_BENCH_END(MyStructBench);
}


TEST(RWResourceTrack, Reparent) {
  RW_RESOURCE_TRACK_HANDLE rh, rh1;
  int *i1;
  char *c1;
  float *f1;
  my_struct_t *ms1;

  RWUT_BENCH_ITER(ReparentBench, 2, 5);
      rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("inside Reparent");

      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, i1, int);
      ms1 = RW_MALLOC_TYPE(sizeof(*ms1), my_struct_t);
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, ms1, my_struct_t);
      c1 = RW_MALLOC_TYPE(sizeof(char), char); *c1 = 200;
      RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh1, c1, char);
      RW_RESOURCE_TRACK_REPARENT(ms1, rh);

      RW_FREE_TYPE(i1, int);

      RW_RESOURCE_TRACK_REPARENT(c1, rh);
      RW_FREE_TYPE(c1, char);
      RW_FREE_TYPE(ms1, my_struct_t);

      f1 = RW_MALLOC_TYPE(sizeof(float), float); *f1 = 300;
      rh1 = RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, f1, float);

      //CLEANUP ALL THE MEORY FOOTPRINT
      RW_RESOURCE_TRACK_FREE(rh);

  RWUT_BENCH_END(ReparentBench);
}

TEST(RWResourceTrack, RepeatAlloc) {
  int *i1;

  g_rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

  RWUT_BENCH_ITER(RepeatAllocBench, 2, 2000);
    i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
    RW_FREE_TYPE(i1, int);
  RWUT_BENCH_END(RepeatAllocBench);

  fprintf(stderr, "DUMP\n");
  RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);
  fprintf(stderr, "DUMP Done\n");
 
  //CLEANUP ALL THE MEORY FOOTPRINT
  RW_RESOURCE_TRACK_FREE(g_rwresource_track_handle);

}

#include <pthread.h>
#define NUM_THREADS     5

typedef struct my_thread_param_s {
  unsigned int thread_id;
  RW_RESOURCE_TRACK_HANDLE rh;
} my_thread_param_t;

void* thread_work(void* ud)
{
  my_thread_param_t *this_thread = (my_thread_param_t*) ud;
  int *i1;
  char *tmp_str = RW_MALLOC_TYPE(20, char);

  sprintf(tmp_str, "Thread %u RT-Handle", this_thread->thread_id);
  this_thread->rh = RW_RESOURCE_TRACK_CREATE_CONTEXT(tmp_str);

  for (int i=0; i<2; i++) {
    i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
    //RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(this_thread->rh, i1, int);
  }
  return NULL;
}


TEST(RWResourceTrack, MultiThread) {
  pthread_t thread[NUM_THREADS];
  my_thread_param_t thread_data[NUM_THREADS];
  int t, rc;

  RWUT_BENCH_ITER(MultiThreadBench, 2, 5);
      g_rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

      for(t=0; t<NUM_THREADS; t++){
        //fprintf(stderr, "In main: creating thread %d\n", t);
        thread_data[t].thread_id = t;
        rc = pthread_create(&thread[t], NULL, thread_work, (void *)&thread_data[t]);
        RW_ASSERT(rc == 0);
      }

      for (t=0; t<NUM_THREADS; t++) {
        void *status;
        rc = pthread_join(thread[t], &status);
        RW_ASSERT(rc == 0);
        //RW_RESOURCE_TRACK_DUMP(thread_data[t].rh);
        RW_RESOURCE_TRACK_FREE(thread_data[t].rh);
      }

      //fprintf(stderr, "DUMP\n");
      //RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);
      RW_RESOURCE_TRACK_FREE(g_rwresource_track_handle);

  RWUT_BENCH_END(MultiThreadBench);
}

TEST(RWResourceTrack, RepeatAllocWithDumpSTRING) {
  int *i1;

  RWUT_BENCH_ITER(RepeatAllocWithDumpSTRINGBench, 2, 5);
    g_rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

    for(int i=0; i<200; i++) {
      i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
      //RW_FREE_TYPE(i1, int);
    }

    char str[1000000];

    fprintf(stderr, "DUMP\n");
    RW_RESOURCE_TRACK_DUMP_STRING(g_rwresource_track_handle, str, sizeof(str));
    fprintf(stderr, "%s", str);
    fprintf(stderr, "DUMP Done\n");

    RW_RESOURCE_TRACK_FREE(g_rwresource_track_handle);
  RWUT_BENCH_END(RepeatAllocWithDumpSTRINGBench);
}

TEST(RWResourceTrack, RepeatAllocWithDumpSTRUCT) {
  int *i1;

  RWUT_BENCH_ITER(RepeatAllocWithDumpSTRUCTBench, 2, 5);
      g_rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

      for(int i=0; i<20; i++) {
        i1 = RW_MALLOC_TYPE(sizeof(int), int); *i1 = 100;
        //RW_FREE_TYPE(i1, int);
      }

      RW_TA_RESOURCE_TRACK_DUMP_SINK  s;

      s = (RW_TA_RESOURCE_TRACK_DUMP_SINK)RW_MALLOC0(sizeof(*s)*10);

      fprintf(stderr, "DUMP\n");
      RW_RESOURCE_TRACK_DUMP_STRUCT(g_rwresource_track_handle, s, 10);
      for (unsigned int i=0; i<s[0].sink_used; i++)
        fprintf(stderr, "%s %d %p %s\n", s[i].name, s[i].size, s[i].sink, s[i].loc);
      fprintf(stderr, "DUMP Done\n");

      RW_FREE(s);
      RW_RESOURCE_TRACK_FREE(g_rwresource_track_handle);
  RWUT_BENCH_END(RepeatAllocWithDumpSTRUCTBench);
}

