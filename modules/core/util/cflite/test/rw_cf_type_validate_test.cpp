#include <limits.h>
#include <cstdlib>
#include "rwut.h"

#include "rw_cf_type_validate.h"
#include "rw_cf_type_list.h"
#include "rw_cf_type_validate_test.h"
#include "rw_resource_track.h"

/**
 * Location of the plugin files used for this test program
 */
//#ifndef PLUGINDIR
//#error - Makefile must define PLUGINDIR
//#endif // PLUGINDIR
//#define FEATUREPLUGINDIR PLUGINDIR "/../../feature_plugin/plugins"

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

#define GTEST_ON
#undef GTEST_ON
#define GTEST_ON

int TypeValidatePositiveCases();
int TypeValidateNegativeCases();
int TypeValidatePositiveCaseWResTracking();
int TypeValidateNegativeCaseWResTracking();
int TypeAnnotationTet();

void
myInit(int argc, char** argv)
{

  if (argc == 1)
    {
      // Yes this is cheating on argv memory, but oh well...
      argv[1] = (char *) "--gtest_catch_exceptions=0";
      argc++;
    }

  {
    pthread_t self_id;
    self_id=pthread_self();
    printf("\nMain thread %lu\n",self_id);
  }

#ifndef GTEST_ON
  printf("calling TypeValidatePositiveCases\n");
  TypeValidatePositiveCases();

  printf("calling TypeValidateNegativeCases\n");
  TypeValidateNegativeCases();

  printf("calling TypeValidatePositiveCaseWResTracking\n");
  TypeValidatePositiveCaseWResTracking();

  printf("calling TypeValidateNegativeCaseWResTracking\n");
  TypeValidateNegativeCaseWResTracking();

  printf("calling TypeAnnotationTet\n");
  TypeAnnotationTet();
#endif

}
RWUT_INIT(myInit);


RW_CF_TYPE_DEFINE("rwtype_05", rwtype_05_ptr_t); // DEFINE ONCE
//RW_CF_TYPE_DEFINE("rwtype_05", rwtype_05_ptr_t);   // Cannot DEFINE a second time

#ifndef GTEST_ON
int TypeValidatePositiveCases()
{
#else
TEST(RwCF, TypeValidatePositiveCases) {
  TEST_DESCRIPTION("This test performs unit positive tests to validate CF enabled data-structures"); 
#endif
  rwtype_01_ptr_t t1_1;
  rwtype_01_ptr_t t1_2;
  rwtype_02_ptr_t t2_1;
  rwtype_05_ptr_t t5_1;

  // The Call Registers all the extended objects with CoreFoundation
  rw_cf_register_types();

  RW_CF_TYPE_REGISTER(rwtype_05_ptr_t); // Register RF-type w/ CF
  RW_CF_TYPE_REGISTER(rwtype_05_ptr_t); // Register may be called multiple times; not recomended

  // Allocate memory for the new instance
  for (int i=0; i<3; i++) {
  t1_1 = (rwtype_01_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t1_1), rwtype_01_ptr_t);
  t1_2 = (rwtype_01_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t1_1), rwtype_01_ptr_t);
  t2_1 = (rwtype_02_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t2_1), rwtype_02_ptr_t);
  t5_1 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_1), rwtype_05_ptr_t);


  // The below VALIDATION macros validated the data-type using the CF routines
  RW_CF_TYPE_VALIDATE(t1_1, rwtype_01_ptr_t); // Now Validation of type is possible
  RW_CF_TYPE_VALIDATE(t2_1, rwtype_02_ptr_t);
  RW_CF_TYPE_VALIDATE(t5_1, rwtype_05_ptr_t);

  struct rwtype_05 t5_2_data;
  rwtype_05_ptr_t t5_2;
  t5_2 = &t5_2_data;
  RW_CF_TYPE_ASSIGN(t5_2, rwtype_05_ptr_t);
  RW_CF_TYPE_VALIDATE(t5_2, rwtype_05_ptr_t);

  RW_CF_TYPE_RETAIN(t1_1, rwtype_01_ptr_t);
  RW_CF_TYPE_FREE(t1_1, rwtype_01_ptr_t);
  RW_CF_TYPE_FREE(t1_1, rwtype_01_ptr_t);

  RW_CF_TYPE_FREE(t1_2, rwtype_01_ptr_t);
  RW_CF_TYPE_FREE(t2_1, rwtype_02_ptr_t);
  RW_CF_TYPE_FREE(t5_1, rwtype_05_ptr_t);
  }
}

#ifndef GTEST_ON
int TypeValidateNegativeCases()
{
#else
TEST(RwCF, TypeValidateNegativeCases) {
  TEST_DESCRIPTION("This test performs unit negetive tests to validate CF enabled data-structures"); 

#endif
  rwtype_01_ptr_t t1_1;
  rwtype_02_ptr_t t2_1;
  rwtype_05_ptr_t t5_1;

  // The Call Registers all the extended objects with CoreFoundation
  rw_cf_register_types();

  RW_CF_TYPE_REGISTER(rwtype_05_ptr_t);

  // Allocate memory for the new instance
  t1_1 = (rwtype_01_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t1_1), rwtype_01_ptr_t);
  t2_1 = (rwtype_02_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t2_1), rwtype_02_ptr_t);
  t5_1 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_1), rwtype_05_ptr_t);

  // The below VALIDATION macros validated the data-type using the CF routines
  EXPECT_DEATH({ RW_CF_TYPE_VALIDATE(0, rwtype_02_ptr_t); }, " is not a ");
  EXPECT_DEATH({ RW_CF_TYPE_VALIDATE(t1_1, rwtype_02_ptr_t); }, " is not a ");
  EXPECT_DEATH({ RW_CF_TYPE_VALIDATE(t2_1, rwtype_01_ptr_t); }, " is not a ");
  EXPECT_DEATH({ RW_CF_TYPE_VALIDATE(t5_1, rwtype_01_ptr_t); }, " is not a ");
  RW_CF_TYPE_FREE(t1_1, rwtype_01_ptr_t);
  EXPECT_DEATH({ RW_CF_TYPE_VALIDATE(t1_1, rwtype_01_ptr_t); }, "");
  t1_1 = (rwtype_01_ptr_t)0xbad;
  EXPECT_DEATH({ RW_CF_TYPE_VALIDATE(t1_1, rwtype_01_ptr_t); }, "");

  EXPECT_DEATH({ RW_CF_TYPE_FREE(t1_1, rwtype_01_ptr_t); }, "");
  RW_CF_TYPE_FREE(t2_1, rwtype_02_ptr_t);
  RW_CF_TYPE_FREE(t5_1, rwtype_05_ptr_t);
}

#define kNumFamilyMembers 5

#ifndef GTEST_ON
int TypeValidatePositiveCaseWResTracking()
{
#else
TEST(RwCF, TypeValidatePositiveCaseWResTracking) {
  TEST_DESCRIPTION("This test performs unit positive tests to validate & track RW-CF objects");
#endif
  rwtype_01_ptr_t t1_1;
  rwtype_05_ptr_t t5_1, t5_2, t5_3, t5_4, t5_5;
  RW_RESOURCE_TRACK_HANDLE rh;

  CFStringRef names[kNumFamilyMembers];
  CFArrayRef  array;
  CFRange range = {0, kNumFamilyMembers};
  char name[100];
  const void *ret_val = NULL;

  // The Call Registers all the extended objects with CoreFoundation
  rw_cf_register_types();
  rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

  // Define the family members.
  names[0] = CFSTR("Marge");
  names[1] = CFSTR("Homer");
  names[2] = CFSTR("Bart");
  names[3] = CFSTR("Lisa");
  names[4] = CFSTR("Maggie");
  // Create a property list using the string array of names.
  array = CFArrayCreate( rw_cf_Allocator(100),
              (const void **)names,
              kNumFamilyMembers,
              &kCFTypeArrayCallBacks );
  printf("Array size = %d\n", CFArrayGetCount(array));
  printf("Array has %s %d times\n", "Marge", CFArrayGetCountOfValue(array, range, CFSTR("Marge")));


  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, (void*)array, "array of type CFArrayRef");

  RW_CF_TYPE_REGISTER(rwtype_05_ptr_t); // Register RF-type w/ CF

  t1_1 = (rwtype_01_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t1_1), rwtype_01_ptr_t);
  RW_CF_TYPE_FREE(t1_1, rwtype_01_ptr_t);

  // Allocate memory for the new instance
  t5_1 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_1), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, t5_1, rwtype_05_ptr_t);
  RW_CF_TYPE_VALIDATE(t5_1, rwtype_05_ptr_t); // Now Validation of type is possible

  t5_2 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_2), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_2, "t5_2 of type rwtype_05_ptr_t");
  RW_CF_TYPE_VALIDATE(t5_2, rwtype_05_ptr_t); // Now Validation of type is possible

#if 1
  t5_3 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_3), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_3, "t5_3 of type rwtype_05_ptr_t");

  t5_4 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_4), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_4, "t5_4 of type rwtype_05_ptr_t");

  t5_5 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_5), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, t5_5, rwtype_05_ptr_t);
#endif

  RW_RESOURCE_TRACK_DUMP(rh);

  RW_CF_RESOURCE_TRACK_REMOVE_TRACKING(t5_1);
  RW_CF_TYPE_FREE(t5_1, rwtype_05_ptr_t);

  //CLEANUP ALL THE MEORY FOOTPRINT
  RW_RESOURCE_TRACK_FREE(rh);
}

#ifndef GTEST_ON
int TypeValidateNegativeCaseWResTracking()
{
#else
TEST(RwCF, TypeValidateNegativeCaseWResTracking) {
  TEST_DESCRIPTION("This test performs unit negative tests to validate & track RW-CF objects");
#endif
  rwtype_05_ptr_t t5_1, t5_2, t5_3, t5_4, t5_5;
  RW_RESOURCE_TRACK_HANDLE rh;

  // The Call Registers all the extended objects with CoreFoundation
  rw_cf_register_types();
  rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");
  RW_RESOURCE_TRACK_FREE(rh);
  EXPECT_DEATH({ RW_RESOURCE_TRACK_FREE(rh); }, "");

  rh = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

  RW_CF_TYPE_REGISTER(rwtype_05_ptr_t); // Register RF-type w/ CF

  // Allocate memory for the new instance
  t5_1 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_1), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_1, "t5_1 of type rwtype_05_ptr_t");

  t5_2 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_2), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_2, "t5_2 of type rwtype_05_ptr_t");

  t5_3 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_3), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_3, "t5_3 of type rwtype_05_ptr_t");

  t5_4 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_4), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, t5_4, "t5_4 of type rwtype_05_ptr_t");

  t5_5 = (rwtype_05_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t5_5), rwtype_05_ptr_t);
  RW_CF_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, t5_5, rwtype_05_ptr_t);

  // The below VALIDATION macros validated the data-type using the CF routines
  RW_CF_TYPE_VALIDATE(t5_1, rwtype_05_ptr_t); // Now Validation of type is possible

  RW_RESOURCE_TRACK_DUMP(rh);

  RW_CF_TYPE_FREE(t5_1, rwtype_05_ptr_t);

  //CLEANUP ALL THE MEORY FOOTPRINT
  //EXPECT_DEATH({ RW_RESOURCE_TRACK_FREE(rh); }, "");
}

//extern "C" __declspec(dllexport) void __CFSetLastAllocationEventName(void *ptr, const char *classname);
extern "C" void __CFSetLastAllocationEventName(void *ptr, const char *classname);
bool __CFOASafe = true;
void rw_cfRecordAllocationFunction(int i1, void *p1, int64_t i2, uint64_t i3, const char *p2)
{
      printf("allocation called\n");
}

void rw_cfLastAllocEventNameFunction(void *p1, const char *p2)
{ 
      printf("%p : last alloc called: %s\n", p1, p2);
}
                                                                                                                                                     
//extern "C" void (*__CFObjectAllocRecordAllocationFunction)(int, void *, int64_t , uint64_t, const char *) = rw_cfRecordAllocationFunction;
extern "C" void (*__CFObjectAllocSetLastAllocEventNameFunction)(void *, const char *);

#ifndef GTEST_ON
int TypeAnnotationTet()
{
#else
TEST(RwCF, TypeAnnotationTest) {
  TEST_DESCRIPTION("This test performs unit TypeAnnotationTest validate CF annotation works");
#endif
  rwtype_01_ptr_t t1_1;
  rwtype_02_ptr_t t2_1;
  void (*tmp_CFObjectAllocSetLastAllocEventNameFunction)(void *, const char *) = __CFObjectAllocSetLastAllocEventNameFunction;

  // The Call Registers all the extended objects with CoreFoundation
  rw_cf_register_types();

  // Allocate memory for the new instance
  t1_1 = (rwtype_01_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t1_1), rwtype_01_ptr_t);
  t2_1 = (rwtype_02_ptr_t)RW_CF_TYPE_MALLOC0(sizeof(*t2_1), rwtype_02_ptr_t);

  __CFObjectAllocSetLastAllocEventNameFunction = rw_cfLastAllocEventNameFunction;

  __CFSetLastAllocationEventName(t1_1, "THIS");
  __CFSetLastAllocationEventName(t2_1, "THAT");

  __CFObjectAllocSetLastAllocEventNameFunction = tmp_CFObjectAllocSetLastAllocEventNameFunction;

  RW_CF_TYPE_FREE(t1_1, rwtype_01_ptr_t);
  RW_CF_TYPE_FREE(t2_1, rwtype_02_ptr_t);
}
