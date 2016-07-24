// gcc this_file.c bin/default/libtalloc.so

//#include "replace.h"
//#include "system/time.h"
#include "talloc.h"

#define CHECK_BLOCKS(test, ptr, tblocks) do { \
	if (talloc_total_blocks(ptr) != (tblocks)) { \
		printf("failed: %s [\n%s: wrong '%s' tree blocks: got %u  expected %u\n]\n", \
		       test, __location__, #ptr, \
		       (unsigned)talloc_total_blocks(ptr), \
		       (unsigned)tblocks); \
		talloc_report_full(ptr, stdout); \
		return 0; \
	} \
} while (0)

/*
  test references 
*/
static int test_ref1(void)
{
	void *root, *p1, *p2, *ref, *r1;

  ((void)ref);
	printf("test: ref1\n# SINGLE REFERENCE FREE\n");

	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	p2 = talloc_named_const(p1, 1, "p2");
	talloc_named_const(p1, 1, "abcd");
	talloc_named_const(p1, 2, "xyz");
	talloc_named_const(p1, 3, "x3");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref1", p1, 5);
	CHECK_BLOCKS("ref1", p2, 1);
	CHECK_BLOCKS("ref1", r1, 2);

	fprintf(stderr, "Freeing p2\n");
	talloc_unlink(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref1", p1, 5);
	CHECK_BLOCKS("ref1", p2, 1);
	CHECK_BLOCKS("ref1", r1, 1);

	fprintf(stderr, "Freeing p1\n");
	talloc_free(p1);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref1", r1, 1);

	fprintf(stderr, "Freeing r1\n");
	talloc_free(r1);
	talloc_report_full(NULL, stderr);

	fprintf(stderr, "Testing NULL\n");
	if (talloc_reference(root, NULL)) {
		return 0;
	}

	CHECK_BLOCKS("ref1", root, 1);

	//CHECK_SIZE("ref1", root, 0);

	talloc_free(root);
	printf("success: ref1\n");
	return 1;
}

int main(int argc, char **argv)
{
  ((void)argc);
  ((void)argv);
  test_ref1();
  return 0;
}
