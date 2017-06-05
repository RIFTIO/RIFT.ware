/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmemlog_test.cpp
 * @date 2015/09/11
 * @brief Google test cases for testing rwmemlog.
 */

#include <rwmemlog.h>
#include <rwut.h>
#include <inttypes.h>
#include <unistd.h>

#include <rwmemlog_mgmt.h>
#include "rwmemlog_c_test.h"
#include "../src/rwmemlog_impl.h"


TEST(MemlogInstance, CreateDestroy)
{
  const char* instance_name = "Testing";
  RwMemlogInstance mli(instance_name, 12, 3 );
  auto instance = mli.get();
  ASSERT_NE( instance, nullptr );

  EXPECT_STREQ( instance->instance_name, instance_name );
  ASSERT_NE( instance->buffer_pool, nullptr );
  EXPECT_NE( instance->buffer_pool[0], nullptr );
  EXPECT_EQ( instance->buffer_count, 12 );
  EXPECT_NE( nullptr, instance->retired_oldest );
  EXPECT_NE( nullptr, instance->retired_newest );
  EXPECT_EQ( instance->current_retired_count, 12 );
  EXPECT_EQ( instance->minimum_retired_count, 12 );
  EXPECT_EQ( nullptr, instance->keep_oldest );
  EXPECT_EQ( nullptr, instance->keep_newest );
  EXPECT_EQ( instance->current_keep_count, 0 );
  EXPECT_EQ( instance->maximum_keep_count, 3 );
  EXPECT_NE( instance->mutex, nullptr );
  EXPECT_EQ( instance->rwlog_callback, nullptr );
  EXPECT_EQ( instance->rwtrace_callback, nullptr );
  EXPECT_EQ( instance->ks_instance, nullptr );

  EXPECT_TRUE( instance->filter_mask );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM5 );

  instance = mli.release();
  EXPECT_EQ( mli.release(), nullptr );
  EXPECT_EQ( mli.get(), nullptr );

  rwmemlog_instance_destroy( instance );
}

TEST(MemlogInstance, CreateDefaults)
{
  RwMemlogInstance mli("Test");
  auto instance = mli.get();
  ASSERT_NE( instance, nullptr );
  EXPECT_EQ( instance->current_retired_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );
  EXPECT_EQ( instance->minimum_retired_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );
  EXPECT_EQ( instance->buffer_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );
  EXPECT_EQ( instance->current_keep_count, 0 );
  EXPECT_EQ( instance->maximum_keep_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MAX_KEEP_BUFS );
}

TEST(MemlogInstance, Filters)
{
  RwMemlogInstance instance("Test");
  ASSERT_NE( instance.get(), nullptr );

  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM5 );

  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_H );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_I );

  rwmemlog_instance_locked_filters_enable(
    instance,
    RWMEMLOG_MEM6 | RWMEMLOG_CATEGORY_G | RWMEMLOG_CATEGORY_M | RWMEMLOG_CATEGORY_O );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM1 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM2 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM3 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM7 );

  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_F );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_G );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_H );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_I );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_L );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_M );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_N );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_O );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_P );

  rwmemlog_instance_locked_filters_disable( instance, RWMEMLOG_MEM1 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM1 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM2 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM3 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM7 );

  rwmemlog_instance_locked_filters_enable( instance, RWMEMLOG_MEM7 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM1 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM2 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM3 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM7 );

  rwmemlog_instance_locked_filters_disable(
    instance,
    RWMEMLOG_MEM3 | RWMEMLOG_CATEGORY_G | RWMEMLOG_CATEGORY_M | RWMEMLOG_CATEGORY_P );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM1 );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM2 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM3 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM7 );

  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_F );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_G );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_H );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_I );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_L );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_M );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_N );
  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_O );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_P );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_R );

  rwmemlog_filter_mask_t all = 0xFFFFFFFFFFFFFFFFull;

  rwmemlog_instance_locked_filters_enable( instance, all );
  EXPECT_EQ(
    instance->filter_mask & RWMEMLOG_MASK_INSTANCE_CONFIGURABLE,
    RWMEMLOG_MASK_INSTANCE_CONFIGURABLE );
  EXPECT_EQ(
    instance->filter_mask & RWMEMLOG_MASK_INSTANCE_ALWAYS_SET,
    RWMEMLOG_MASK_INSTANCE_ALWAYS_SET );

  rwmemlog_instance_locked_filters_disable( instance, all );
  EXPECT_EQ(
    instance->filter_mask & RWMEMLOG_MASK_INSTANCE_CONFIGURABLE,
    0 );
  EXPECT_EQ(
    instance->filter_mask & RWMEMLOG_MASK_INSTANCE_ALWAYS_SET,
    RWMEMLOG_MASK_INSTANCE_ALWAYS_SET );
}

static int dummy_counter = 0;
static void dummy_log_callback(
  rwmemlog_instance_t* instance,
  ProtobufCMessage* pbcm )
{
  (void)instance;
  (void)pbcm;
  dummy_counter++;
}

static void dummy_trace_callback(
  rwmemlog_instance_t* instance,
  ProtobufCMessage* pbcm )
{
  (void)instance;
  (void)pbcm;
  dummy_counter++;
}

TEST(MemlogInstance, Register)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  EXPECT_EQ( instance->app_context, 0 );
  EXPECT_EQ( instance->rwlog_callback, nullptr );
  EXPECT_EQ( instance->rwtrace_callback, nullptr );

  rwmemlog_instance_register_app(
    instance, 27, dummy_log_callback, dummy_trace_callback, nullptr );

  EXPECT_EQ( instance->app_context, 27 );
  EXPECT_EQ( instance->rwlog_callback, &dummy_log_callback );
  EXPECT_EQ( instance->rwtrace_callback, &dummy_trace_callback );
  EXPECT_EQ( instance->ks_instance, nullptr );

  EXPECT_EQ( 0, dummy_counter );
}

static int locked_test_value = 0;
static rw_status_t locked_test(
  rwmemlog_instance_t* instance,
  intptr_t value)
{
  (void)instance;
  locked_test_value = (int)value;
  if (247 == locked_test_value) {
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}

TEST(MemlogInstance, InvokeLocked)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  auto rs = rwmemlog_instance_invoke_locked( instance, &locked_test, 347 );
  EXPECT_EQ( locked_test_value, 347 );
  EXPECT_EQ( RW_STATUS_SUCCESS, rs );

  rs = rwmemlog_instance_invoke_locked( instance, &locked_test, 247 );
  EXPECT_EQ( locked_test_value, 247 );
  EXPECT_EQ( RW_STATUS_FAILURE, rs );
}

TEST(MemlogBuffer, Get)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  {
    EXPECT_EQ( instance->buffer_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );
    auto oldest = instance->retired_oldest;

    RwMemlogBuffer b1( instance, "test obj", 1 );
    EXPECT_NE(oldest, instance->retired_oldest);
    oldest = instance->retired_oldest;

    RwMemlogBuffer b2( instance, "test obj", 2 );
    EXPECT_NE(oldest, instance->retired_oldest);
    oldest = instance->retired_oldest;

    RwMemlogBuffer b3( instance, "test obj", 3 );
    EXPECT_NE(oldest, instance->retired_oldest);
    oldest = instance->retired_oldest;

    EXPECT_GE( instance->buffer_count, 4 );
  }
}

static const char* dummy_enum_func( int enumval )
{
  return "dummy";
}

TEST(MemlogBuffer, SkipMacros)
{
  rwmemlog_buffer_t* buf = nullptr;
  (void)buf;

  RWMEMLOG( &buf, RWMEMLOG_MEM2, "simple string", );

  RWMEMLOG( &buf, RWMEMLOG_MEM2, "intptr test",
            RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );

  RWMEMLOG( &buf, RWMEMLOG_MEM2, "strncpy test",
            RWMEMLOG_ARG_STRNCPY(12,"yellow dog") );

  enum { junk = 1 } k = junk;
  RWMEMLOG( &buf, RWMEMLOG_MEM2, "enum test",
            RWMEMLOG_ARG_ENUM_FUNC(dummy_enum_func,"junk",k) );

  RWMEMLOG( &buf, RWMEMLOG_MEM2, "arg count test",
             RWMEMLOG_ARG_CONST_STRING("first two are implied"),
             RWMEMLOG_ARG_CONST_STRING("4th"),
             RWMEMLOG_ARG_CONST_STRING("5th"),
             RWMEMLOG_ARG_CONST_STRING("6th"),
             RWMEMLOG_ARG_CONST_STRING("7th"),
             RWMEMLOG_ARG_CONST_STRING("8th"),
             RWMEMLOG_ARG_CONST_STRING("9th"),
             RWMEMLOG_ARG_CONST_STRING("tenth") );

  {
    RWMEMLOG_TIME_SCOPE( &buf, RWMEMLOG_LOPRI, "foo" );
  }

  RWMEMLOG_TIME_CODE(
    (int i = 3;),
    &buf, RWMEMLOG_LOPRI, "foo" );

  EXPECT_EQ( i, 3 );
}

TEST(MemlogBuffer, FillMacros)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  auto header = &buf.get()->header;
  ASSERT_NE( header, nullptr );

  RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
            RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
  EXPECT_EQ(header->used_units, 4);

  RWMEMLOG( buf, RWMEMLOG_MEM2, "strncpy test",
            RWMEMLOG_ARG_STRNCPY(12,"yellow dog") );
  EXPECT_EQ(header->used_units, 9);

  enum { junk = 1 } k = junk;
  RWMEMLOG( buf, RWMEMLOG_MEM2, "enum test",
            RWMEMLOG_ARG_ENUM_FUNC(dummy_enum_func,"junk",k) );
  EXPECT_EQ(header->used_units, 13);

  RWMEMLOG( buf, RWMEMLOG_MEM2, "arg count test",
             RWMEMLOG_ARG_CONST_STRING("first two are implied"),
             RWMEMLOG_ARG_CONST_STRING("4th"),
             RWMEMLOG_ARG_CONST_STRING("5th"),
             RWMEMLOG_ARG_CONST_STRING("6th"),
             RWMEMLOG_ARG_CONST_STRING("7th"),
             RWMEMLOG_ARG_CONST_STRING("8th"),
             RWMEMLOG_ARG_CONST_STRING("9th"),
             RWMEMLOG_ARG_CONST_STRING("tenth") );
  EXPECT_EQ(header->used_units, 16);

  RWMEMLOG( &buf, RWMEMLOG_MEM2, "extra comma", );
  EXPECT_EQ(header->used_units, 19);

  RWMEMLOG( &buf, RWMEMLOG_MEM2, "simple no comma" );
  EXPECT_EQ(header->used_units, 22);

  RWMEMLOG( buf, RWMEMLOG_MEM2, "arg count test",
            RWMEMLOG_ARG_STRNCPY( 6,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY( 7,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY( 8,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY( 9,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(14,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(15,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(16,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(17,"123456789_1234567890") );
  EXPECT_EQ(header->used_units, 39);

  {
    RWMEMLOG_TIME_SCOPE( buf, RWMEMLOG_MEM2, "first scope" );
    usleep(1234);
  }

  {
    RWMEMLOG_TIME_SCOPE( buf, RWMEMLOG_MEM2, "second scope" );
    usleep(3214);
  }

  RWMEMLOG_TIME_CODE(
    (int i = 3; usleep(123);),
    buf, RWMEMLOG_MEM2, "time code" );
  EXPECT_EQ( i, 3 );

  RWMEMLOG_TIME_CODE_RTT(
    buf, RWMEMLOG_MEM2, "time code rtt",
    (usleep(34);) );

  rwmemlog_instance_to_stdout(instance);
}

TEST(MemlogBuffer, AllocBoundary)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );
  EXPECT_EQ( instance->buffer_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  auto header = &buf.get()->header;
  ASSERT_NE( header, nullptr );

  for (unsigned i = 0;
       i < (RWMEMLOG_BUFFER_DATA_SIZE_UNITS-RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS) / 4;
       ++i ) {
    RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
              RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
    EXPECT_EQ(header->used_units, (i+1)*4);
  }

  EXPECT_EQ( header, &header->next->header );
  RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
            RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
  EXPECT_NE( header, &header->next->header );
}

TEST(MemlogBuffer, WrapBoundary)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );
  EXPECT_EQ( instance->buffer_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  auto header = &buf.get()->header;
  ASSERT_NE( header, nullptr );

  for (unsigned i = 0;
       i < RWMEMLOG_BUFFER_DATA_SIZE_UNITS / 4;
       ++i ) {
    RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
              RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
    EXPECT_EQ(header->used_units, (i+1)*4);
  }

  RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
            RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
  auto header2 = &buf.get()->header;
  EXPECT_NE(header, header2);
  EXPECT_EQ(header2->used_units, 4);
}

TEST(MemlogBuffer, WrapManyTimes)
{
  RwMemlogInstance instance( "Test", 2 );
  ASSERT_NE( instance.get(), nullptr );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  auto header = &buf.get()->header;
  ASSERT_NE( header, nullptr );

  for (unsigned i = 0;
       i < RWMEMLOG_BUFFER_DATA_SIZE_UNITS * 3 * RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS;
       ++i ) {
    RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
              RWMEMLOG_ARG_PRINTF_INTPTR("%" PRIdPTR, i) );
  }

  EXPECT_EQ( instance->buffer_count, 4 );
}

TEST(MemlogBuffer, WrapAndReleaseBoundaries)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  RWPB_M_MSG_DECL_INIT(RwMemlog_Command, clear_cmd );
  RWPB_M_MSG_DECL_INIT(RwMemlog_Command_Clear, clear );
  clear_cmd.clear = &clear;
  size_t last_count = 0;

  for (unsigned iter = 0; iter <= 3; ++iter) {
    RwMemlogBuffer bufs[] = {
      { instance, "test obj",  0 },
      { instance, "test obj",  1 },
      { instance, "test obj",  2 },
      { instance, "test obj",  3 },
      { instance, "test obj",  4 },
      { instance, "test obj",  5 },
      { instance, "test obj",  6 },
      { instance, "test obj",  7 },
      { instance, "test obj",  8 },
      { instance, "test obj",  9 },
      { instance, "test obj", 10 },
      { instance, "test obj", 11 },
      { instance, "test obj", 12 },
      { instance, "test obj", 13 },
      { instance, "test obj", 14 },
      { instance, "test obj", 15 },
      { instance, "test obj", 16 },
      { instance, "test obj", 17 },
      { instance, "test obj", 18 },
      { instance, "test obj", 19 },
      { instance, "test obj", 20 },
      { instance, "test obj", 21 },
      { instance, "test obj", 22 },
      { instance, "test obj", 23 },
      { instance, "test obj", 24 },
    };
    constexpr unsigned BUF_CNT = sizeof(bufs)/sizeof(bufs[0]);

    static const unsigned limits[] = {
      0,
      RWMEMLOG_ENTRY_HEADER_SIZE_UNITS,
      RWMEMLOG_BUFFER_USED_SIZE_ALLOC_THRESHOLD_UNITS - RWMEMLOG_ENTRY_HEADER_SIZE_UNITS,
      RWMEMLOG_BUFFER_USED_SIZE_ALLOC_THRESHOLD_UNITS + RWMEMLOG_ENTRY_HEADER_SIZE_UNITS,
      RWMEMLOG_BUFFER_DATA_SIZE_UNITS - RWMEMLOG_ENTRY_HEADER_SIZE_UNITS,
      RWMEMLOG_BUFFER_DATA_SIZE_UNITS,
      RWMEMLOG_BUFFER_DATA_SIZE_UNITS + RWMEMLOG_ENTRY_HEADER_SIZE_UNITS,
    };
    constexpr unsigned LIM_CNT = sizeof(limits)/sizeof(limits[0]);

    // Iterate until all the buffers have been filled to the desired levels.
    unsigned buf_index = 0;
    while (buf_index < BUF_CNT) {
      // Iterate until all the limits have been exercised.
      unsigned previous = 0;
      unsigned lim_index = 0;
      while (lim_index < LIM_CNT && buf_index < BUF_CNT) {
        auto header = &bufs[buf_index].get()->header;
        if (header->used_units < previous) {
          break;
        }

        // Next limit?
        if (header->used_units >= limits[lim_index]) {
          previous = limits[lim_index];
          ++buf_index;
          ++lim_index;
          EXPECT_TRUE(rwmemlog_instance_is_good( instance, false ));
          continue;
        }

        for (unsigned b = buf_index; b < BUF_CNT; ++b) {
          RWMEMLOG_BARE( bufs[b], RWMEMLOG_MEM2 );
        }
      }

      if (iter > 2) {
        rwmemlog_instance_command( instance, &clear_cmd );
        EXPECT_TRUE(rwmemlog_instance_is_good( instance, false ));
      }
    }

    for (unsigned b = 0; b < BUF_CNT; ++b) {
      bufs[b].return_all();
      EXPECT_TRUE(rwmemlog_instance_is_good( instance, false ));
    }

    if (iter > 1) {
      rwmemlog_instance_command( instance, &clear_cmd );
      EXPECT_TRUE(rwmemlog_instance_is_good( instance, false ));
    }

    if (last_count == 0) {
      last_count = instance->buffer_count;
    } else {
      EXPECT_EQ(last_count, instance->buffer_count);
    }
  }
}

TEST(MemlogBuffer, Keep)
{
  RwMemlogInstance instance( "Test", 2 );
  ASSERT_NE( instance.get(), nullptr );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  auto header = &buf.get()->header;
  ASSERT_NE( header, nullptr );

  for (unsigned i = 0;
       i < RWMEMLOG_BUFFER_DATA_SIZE_UNITS * 10 * RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS;
       ++i ) {
    if (2735 == i || 12433 == i) {
      RWMEMLOG( buf, RWMEMLOG_MEM2 | RWMEMLOG_KEEP, "intptr test, keep",
                RWMEMLOG_ARG_PRINTF_INTPTR("%" PRIdPTR, i) );
    } else {
      RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
                RWMEMLOG_ARG_PRINTF_INTPTR("%" PRIdPTR, i) );
    }
  }

  EXPECT_EQ( instance->buffer_count, 7 );
  EXPECT_EQ( instance->current_keep_count, 3 );
}

TEST(MemlogBuffer, CMacros)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  rwmemlog_c_test_macros(buf);

  rwmemlog_instance_to_stdout(instance);
}


TEST(MemlogBuffer, ConfigValidate)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  RWPB_M_MSG_DECL_INIT(RwMemlog_InstanceConfig, config);
  auto rs = rwmemlog_instance_config_validate( instance, &config, nullptr, 0 );
  EXPECT_EQ( RW_STATUS_SUCCESS, rs );

  rwmemlog_filter_mask_t filter_mask = 1;
  config.has_filter_mask = true;
  while (1) {
    config.filter_mask = filter_mask;
    rs = rwmemlog_instance_config_validate( instance, &config, nullptr, 0 );
    if (filter_mask & RWMEMLOG_MASK_INSTANCE_CONFIGURABLE) {
      EXPECT_EQ( RW_STATUS_SUCCESS, rs ) << "filter=" << filter_mask;
    } else {
      EXPECT_NE( RW_STATUS_SUCCESS, rs ) << "filter=" << filter_mask;
    }
    if (filter_mask & (1ull<<63)) {
      break;
    }
    filter_mask <<= 1;
  }
}

TEST(MemlogBuffer, ConfigSet)
{
  RwMemlogInstance instance( "Test" );
  ASSERT_NE( instance.get(), nullptr );

  RWPB_M_MSG_DECL_INIT(RwMemlog_InstanceConfig, config);
  config.instance_name = (char*)"Test";
  config.has_minimum_retired_count = true;
  config.minimum_retired_count = 20;
  config.has_maximum_keep_count = true;
  config.maximum_keep_count = 6;

  // Make sure the changes are actually changes.
  EXPECT_NE( instance->minimum_retired_count, config.minimum_retired_count );
  EXPECT_NE( instance->maximum_keep_count, config.maximum_keep_count );

  auto rs = rwmemlog_instance_config_apply( instance, &config );
  EXPECT_EQ( RW_STATUS_SUCCESS, rs );

  EXPECT_EQ( instance->current_retired_count, RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS );
  EXPECT_EQ( instance->minimum_retired_count, config.minimum_retired_count );
  EXPECT_EQ( instance->current_keep_count, 0 );
  EXPECT_EQ( instance->maximum_keep_count, config.maximum_keep_count );

  EXPECT_EQ( instance->filter_mask, RWMEMLOG_MASK_INSTANCE_DEFAULT );

  config.has_filter_mask = true;
  config.filter_mask = RWMEMLOG_MASK_INSTANCE_CONFIGURABLE;
  rs = rwmemlog_instance_config_apply( instance, &config );
  EXPECT_EQ( RW_STATUS_SUCCESS, rs );

  EXPECT_EQ(
    instance->filter_mask & RWMEMLOG_MASK_INSTANCE_CONFIGURABLE,
    RWMEMLOG_MASK_INSTANCE_CONFIGURABLE );
}

TEST(MemlogBuffer, Filters)
{
  RwMemlogInstance instance("Test");
  ASSERT_NE( instance.get(), nullptr );

  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_MEM5 );

  EXPECT_TRUE( instance->filter_mask & RWMEMLOG_CATEGORY_H );
  EXPECT_FALSE( instance->filter_mask & RWMEMLOG_CATEGORY_I );

  RwMemlogBuffer buf1( instance, "test obj", 1 );
  auto buf1hdr = &buf1.get()->header;
  ASSERT_NE( buf1hdr, nullptr );
  EXPECT_TRUE( buf1hdr->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_TRUE( buf1hdr->filter_mask & RWMEMLOG_CATEGORY_H );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_CATEGORY_I );


  RwMemlogBuffer buf2( instance, "test obj", 1 );
  auto buf2hdr = &buf2.get()->header;
  ASSERT_NE( buf2hdr, nullptr );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( buf2hdr->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_CATEGORY_H );
  EXPECT_FALSE( buf2hdr->filter_mask & RWMEMLOG_CATEGORY_I );

  rwmemlog_buffer_filters_enable( buf2, RWMEMLOG_MEM6 );
  rwmemlog_instance_locked_filters_disable( instance, RWMEMLOG_MEM1 );

  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM1 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM2 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM3 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( buf2hdr->filter_mask & RWMEMLOG_MEM7 );

  EXPECT_TRUE( buf1hdr->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM1 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM2 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM3 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM4 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM5 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM7 );

  // Wrap buf2, make sure it keeps the special filters
  for (unsigned i = 0;
       i < (RWMEMLOG_BUFFER_DATA_SIZE_UNITS / 2) + 2;
       ++i ) {
    RWMEMLOG( buf2, RWMEMLOG_MEM0, "dummy" );
  }

  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( buf2hdr->filter_mask & RWMEMLOG_MEM7 );

  // Wrap buf2 again, make sure it still keeps the special filters
  for (unsigned i = 0;
       i < (RWMEMLOG_BUFFER_DATA_SIZE_UNITS / 2) + 2;
       ++i ) {
    RWMEMLOG( buf2, RWMEMLOG_MEM0, "dummy" );
  }

  EXPECT_TRUE( buf2hdr->filter_mask & RWMEMLOG_MEM6 );
  EXPECT_FALSE( buf2hdr->filter_mask & RWMEMLOG_MEM7 );


  // Wrap buf1, make sure it gets the standard filters.
  for (unsigned i = 0;
       i < (RWMEMLOG_BUFFER_DATA_SIZE_UNITS / 2) + 2;
       ++i ) {
    RWMEMLOG( buf1, RWMEMLOG_MEM0, "dummy" );
  }

  EXPECT_TRUE( buf1hdr->filter_mask & RWMEMLOG_MEM0 );
  EXPECT_FALSE( buf1hdr->filter_mask & RWMEMLOG_MEM1 );
}

struct TestTimedObj {
  TestTimedObj() = delete;
  TestTimedObj(const TestTimedObj&) = delete;
  TestTimedObj& operator=(const TestTimedObj&) = delete;

  RWMEMLOG_OBJECT_LIFETIME_DEF_MEMBERS( rwml_timer_, rwml_loc_ );

  explicit TestTimedObj(
    rwmemlog_buffer_t** bufp )
  : RWMEMLOG_OBJECT_LIFETIME_CONSTRUCT_MEMBER(
      bufp, RWMEMLOG_MEM0, rwml_timer_, rwml_loc_ )
  {}
};

RWMEMLOG_OBJECT_LIFETIME_INIT_LOC( TestTimedObj, rwml_loc_, "TestTimedObj lifetime" );


TEST(MemlogBuffer, TimedObject)
{
  RwMemlogInstance instance( "Test", 2 );
  ASSERT_NE( instance.get(), nullptr );

  RwMemlogBuffer buf( instance, "test obj", 1 );
  auto header = &buf.get()->header;
  ASSERT_NE( header, nullptr );

  {
    TestTimedObj x(buf.ptr());
    usleep(200);
  }

  rwmemlog_instance_to_stdout(instance);
}

