
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
 * @file rw_resource_track.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/13/2014
 * @brief Test routine for testing Memory-Resource Annotation macros
 * @details Test Memory-Resource Annotation macros in a multi-threaded use-case
 */
// compile using
// gcc rw_resource_track_test2.c rw_resource_track_test.c ../rw_resource_track.c -DMULTI_THREADED -lpthread

#include <stdio.h>
#include "rw_resource_annotate.h"

struct context {
  unsigned int tid;
  unsigned int previous;
  unsigned int next;
  struct entry *buffer;
};

extern void * test(void *c);

int                                                                                                                                                 
main(int argc, char *argv[])
{
  int i, r;
  pthread_t *thread;
  static int nthr;
  static struct context *_context;


  if (argc <= 1) {
    printf("Usage: %s <threads>\n", argv[0]);
    exit(-1);
  }

  nthr = atoi(argv[1]);
  assert(nthr >= 1);

  _context = malloc(sizeof(*_context) * nthr);
  assert(_context);

  thread = malloc(sizeof(pthread_t) * nthr);
  assert(thread);

  printf(" test:");
  for (i = 0; i < nthr; i++) {
    _context[i].tid = i;
    if (i == 0) {
      _context[i].previous = nthr - 1;
      _context[i].next = i + 1;
    } else if (i == nthr - 1) {
      _context[i].next = 0;
      _context[i].previous = i - 1;
    } else {
      _context[i].next = i + 1;
      _context[i].previous = i - 1;
    }

    r = pthread_create(thread + i, NULL, test, _context + i);
    assert(r == 0);
  }

  for (i = 0; i < nthr; i++)
    pthread_join(thread[i], NULL);

  printf(" done\n");

  return 0;
}
