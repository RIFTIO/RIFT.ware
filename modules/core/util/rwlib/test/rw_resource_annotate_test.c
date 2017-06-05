/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_resource_track.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/13/2014
 * @brief Test routine for testing Memory-Resource Annotation macros
 * @details Test Memory-Resource Annotation macros
 */
// compile using
// gcc rw_resource_track_test2.c ../rw_resource_track.c -lpthread

#include <stdio.h>
#include "rwlib.h"
#include "rw_resource_annotate.h"

#define NUM_ELEMS 10

#ifdef MULTI_THREADED
#define MULTI_THREADED 1
void * test(void *c)
#else
#define MULTI_THREADED 0
int main()
#endif
{
  int i = 100;
  int j = 100;
  int *p = &i;
  int *p1 = &j;
  int *a[NUM_ELEMS];

#if MULTI_THREADED != 0
  UNUSED(c);
#endif

  printf("sizes int %lu int* %lu void* %lu addr_nd_annot_t %lu\n",
         sizeof(int), sizeof(int*), sizeof(void*), sizeof(addr_nd_annot_t));

  for (i=0; i< NUM_ELEMS; i++) {
    a[i]=malloc(sizeof(int));
    RW_RESOURCE_ANNOTATION_ADD_TYPE(a[i], int, MULTI_THREADED);
    RW_RESOURCE_ANNOTATION_DEL(a[i], MULTI_THREADED);
    RW_RESOURCE_ANNOTATION_ADD_TYPE(a[i], int, MULTI_THREADED);
    RW_RESOURCE_ANNOTATION_DEL(a[i], MULTI_THREADED);
    RW_RESOURCE_ANNOTATION_ADD_TYPE(a[i], int, MULTI_THREADED);
  }

  /*
  for (i=0; i< NUM_ELEMS; i++)
    printf("%u 0x%-32x\n", i, a[i]);
  */
  putchar('\n');

  RW_RESOURCE_ANNOTATION_DEL(a[0], MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_DEL(a[3], MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_DEL(a[8], MULTI_THREADED);
  //RW_RESOURCE_ANNOTATION_DEL(a[45]);

  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_ADD_TYPE(p, char *, MULTI_THREADED);

  for (i=0; i<NUM_ELEMS; i++) {
    RW_RESOURCE_ANNOTATION_DEL(a[i], MULTI_THREADED);
  }

  putchar('\n');
  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_DEL(p, MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_ADD_TYPE(p, char *, MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD_TYPE(p, long *, MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD_TYPE(p, int *, MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD(p, "Called from main()", MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD(p, "Called main()", MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD(p, "Called main()", MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD(p, "abcd", MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_ADD(p1, "Testing Del1", MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_ADD(p1, "Testing Del2", MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_DEL(p1,MULTI_THREADED);

  for (i=0; i< NUM_ELEMS; i++)
    RW_RESOURCE_ANNOTATION_DEL(a[i], MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);

  RW_RESOURCE_ANNOTATION_DEL(p, MULTI_THREADED);
  RW_RESOURCE_ANNOTATION_DUMP(MULTI_THREADED);

  return 0;
}
