
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file rwmemlog.h
 * @date 2015/09/10
 * @brief RW.Memlog: In-memory tracing.
 */

#ifndef RWMEMLOG_H_
#define RWMEMLOG_H_

#ifndef __GI_SCANNER__

// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif


/*!
 * @mainpage
 *
 * The RW.Memlog library defines an in-memory tracing facility based on
 * chains of buffers.
 *
 * RW.Memlog has the following primary features:
 *  - In-memory tracing that is "always on".
 *  - Printf-capable.
 *  - Printf is delayed until the tracing buffers are read through DTS.
 *    If a buffer is never accessed, the printf never occurs.
 *  - Tracing occurs on a per-object basis.  The application defined
 *    what an object is.  For example, DTS traces each transaction
 *    separately.
 *  - There are 8 primary level-based filters (0-7, 0=always traced,
 *    7=rarely traced).  5 levels are enabled by default, 3 are
 *    disabled.  The enabled level can be changed at runtime.
 *  - 24 "category" control levels (A-X).  A category is an arbitrary
 *    grouping of related traces, as defined by the application.  As
 *    many or as few categories can be specified in a trace invocation
 *    as you would like.  8 of the categories are on by default, and 16
 *    are off.
 *  - Configurable at runtime via CLI.
 *  - Each buffer can be independently fitlered, allowing per-object
 *    tracing levels.
 *  - Library-based DTS integration: To register your RW.Memlog
 *    instance with DTS, you just need to call
 *    rwmemlogdts_instance_register() once.  The library will take care
 *    of the finer points of the DTS interactions.
 *
 * Trace buffers are maintained in chains.  There are three kinds of
 * chains: a per-object chain of "active" buffers, a global chain of
 * "retired" buffers, and a global chain of "keep" buffers.
 *  - Active: Each actively traced object has a chain of 1 or 2 buffers
 *    (rather than a ring of buffers).  Objects always have a current
 *    buffer where tracing actively goes.  The active chain may also
 *    have either the previously-filled or the next-to-be-filled
 *    (empty) buffer for the object.  When the current active buffer is
 *    almost full, the previous buffer is retired (if there was one)
 *    and a new buffer is allocated to be the next current buffer.
 *  - Retired: The retired buffers are those buffers not currently
 *    associated with an actively traced object.  They form the history
 *    of what has been traced in the past.  When an object needs to
 *    allocate another active buffer, the oldest retired buffer gets
 *    erased and recycled.  The retired chain has a minimum length.
 *  _ Keep: The keep buffers are special retired buffers that will
 *    never be recycled.  At trace time, the application can mark the
 *    current buffer as a keep buffer - when the buffer eventually goes
 *    inactive, instead of being placed on the retired chain, it will
 *    instead be placed on the keep chain.  The application defines
 *    what is important - indeed, the application does not have to use
 *    keep at all.  A good use for keep is when the code passes are
 *    rare or exceptional error condition whose cause is not readily
 *    apparent from the data available, but whose cause might possibly
 *    be determined from the trace history.  Keep buffers are not kept
 *    indefinitely.  If the keep chain is full, the oldest keep buffer
 *    will be retired to make room for a new keep buffer.  The keep
 *    chain has a maximum length.
 *
 * Examples:
 *  - @ref RwMemlogExInstance
 *  - @ref RwMemlogExBuffer
 *  - @ref RwMemlogExEntry
 *  - @ref RwMemlogExDts
 *  - @ref RwMemlogExDebug
 *
 * Topics:
 *  - @ref RwMemlogInstance
 *  - @ref RwMemlogBuffer
 *  - @ref RwMemlogInvoke
 *  - @ref RwMemlogEntry
 *  - @ref RwMemlogFilters
 *  - @ref RwMemlogArguments
 *
 * Details:
 *  - @ref RwMemlogArgDecode
 *  - ATTN: How to define a new ARG type
 */


/*****************************************************************************/
/*!
 * @page RwMemlogExInstance Instance Management Examples
 *
 * RW.Memlog instances must be created by the RW.Memlog library.  The
 * library will allocate and initialize the instance, as well as
 * allocate an initial pool.  The pool size parameters get defaults if
 * you pass 0, as in the following example.
 *
 * @code
 *   #include <rwmemlog.h>
 *   ...
 *
 *   struct my_instance_t {
 *     rwmemlog_instance_t* memlog_inst;
 *   } my_inst;
 *   ...
 *
 *   my_inst->memlog_inst = rwmemlog_instance_alloc( "MyApp", 0, 0 );
 * @endcode
 *
 * When your tasklet/library gets destroyed, you should clean-up your
 * RW.Memlog instance.  To do this, just call destroy.  When you call
 * destroy, there must be no active buffers (the existence of active
 * buffers implies an active object, which is an error in your code).
 *
 * @code
 *   rwmemlog_instance_destroy( my_inst->memlog_inst );
 *   my_inst->memlog_inst = NULL;
 * @endcode
 *
 * In C++ code, you can create an object that uses RAII to create and
 * destroy the RW.Memlog instance in conjunction with the object's
 * lifetime:
 *
 * @code
 *   #include <rwmemlog.h>
 *   ...
 *
 *   class MyClass {
 *     public:
 *       MyClass() {}
 *     private:
 *       RwMemlogInstance memlog_inst_;
 *   } my_inst;
 * @endcode
 */


/*****************************************************************************/
/*!
 * @page RwMemlogExBuffer Buffer Management Examples
 *
 * To use RW.Memlog to trace in your application, you must associate
 * buffer chains with objects to be traced.  This does not necessarily
 * mean that you are required to have a unique buffer chain for every
 * single object in your application - it just means that you are
 * responsible for figuring out how to map your objects to buffers.
 * However, if you share a buffer chain between multiple objects in
 * your application, you must ensure that all such sharing objects
 * share exactly one buffer chain object - the buffer pointer will be
 * updated by the RWMEMLOG macros!
 *
 * To start a new buffer chain, allocate the first buffer from the
 * RW.Memlog instance.  When you first start a chain, you assign the
 * chain an object name and ID number.  These values are copied to each
 * new buffer that gets added to the chain, allowing you to connect
 * buffers together when they are later output.
 *
 * @code
 *   struct my_obj_t {
 *     int id;
 *     rwmemlog_buffer_t* memlog_buf;
 *   } *my_obj;
 *   ...
 *
 *   my_obj->memlog_buf = rwmemlog_instance_get_buffer(
 *     my_inst->memlog_inst,
 *     "My Obj",
 *     my_obj->id );
 * @endcode
 *
 * When you are done tracing your object, you must return the active
 * buffer chain to the RW.Memlog instance.  If you do not, then the
 * buffers will leak.
 *
 * @code
 *   rwmemlog_buffer_return_all( my_obj->memlog_buf );
 *   free(my_obj);
 * @endcode
 *
 * In C++ code, you can create an object that uses RAII to allocate and
 * return the RW.Memlog buffer chain automatically, in conjunction with
 * the object's lifetime:
 *
 * @code
 *   #include <rwmemlog.h>
 *   ...
 *
 *   class MyOjbect {
 *     public:
 *       MyOjbect(
 *         rwmemlog_instance_t* memlog_inst)
 *       : memlog_buf_(memlog_inst, "My Obj", (intptr_t)this)
 *       {}
 *     private:
 *       RwMemlogBuffer memlog_buf_;
 *   };
 * @endcode
 */


/*****************************************************************************/
/*!
 * @page RwMemlogExEntry Trace Definition Examples
 *
 * To trace using RW.Memlog, you use one of the RWMEMLOG* macros.
 * Typically, you'll use RWMEMLOG(), which contains two default
 * arguments: the timestamp as the first argument, and then a static
 * const string as the second.  You then have up to 8 additional
 * arguments you can provide that are invocation-specific.  Some
 * examples (taken from the test file):
 *
 * @code
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "simple string", );
 *
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "intptr test",
 *             RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
 *
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "strncpy test",
 *             RWMEMLOG_ARG_STRNCPY(12,"yellow dog") );
 *
 *   enum { junk = 1 } k = junk;
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "enum test",
 *             RWMEMLOG_ARG_ENUM_FUNC(dummy_enum_func,"junk",k) );
 *
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "arg count test",
 *             RWMEMLOG_ARG_CONST_STRING("first two are implied"),
 *             RWMEMLOG_ARG_CONST_STRING("4th"),
 *             RWMEMLOG_ARG_CONST_STRING("5th"),
 *             RWMEMLOG_ARG_CONST_STRING("6th"),
 *             RWMEMLOG_ARG_CONST_STRING("7th"),
 *             RWMEMLOG_ARG_CONST_STRING("8th"),
 *             RWMEMLOG_ARG_CONST_STRING("9th"),
 *             RWMEMLOG_ARG_CONST_STRING("tenth") );
 * @endcode
 *
 * You can also do various timing measurements using more complex
 * RWMEMLOG macros.  In C++, you can time an entire block scope.  And
 * in both C and C++, you can time any arbitrary code sequence.
 *
 * To time a code sequence, use the RWMEMLOG_TIME_CODE() macro.  In the
 * following example, taken from RW.MgmtAgent, the second line is the
 * arbitrary code being timed.  The code is wrapped in a single pair of
 * parens, and it must be a complete C/C++ statement, with trailing
 * semi-colon.  Any additional RW.Memlog arguments provided to the
 * macro are traced only at the start.  At the end of the timed code, a
 * fixed set of arguments gets used - the message, and a
 * round-trip-time.
 *
 * @code
 *  RWMEMLOG_TIME_CODE( (
 *      auto rv = confd_data_reply_next_key (ctxt_, NULL, -1, -1);
 *    ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key no dts yet" );
 * @endcode
 *
 * In C++, to time a scope, you use the RWMEMLOG_TIME_SCOPE() macro.
 * The macro times from the point of the macro in the source until the
 * end of the enclosing scope.  In RW.MgmtAgent, this is used to time
 * numerous entire functions.  Here's an example:
 *
 * @code
 *   void Instance::setup_dom(const char *module_name)
 *   {
 *     RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup dom");
 *     ... lots of work ...
 *   }
 * @endcode
 *
 * You are not forced to trace all of your objects.  If you would like
 * to suppress tracing for whatever reason, you can just set the buffer
 * pointer to NULL.  The RWMEMLOG macros contain a check for NULL and
 * will simply skip tracing in that case.
 *
 * @code
 *   if (no_trace) {
 *     my_obj->memlog_buf = NULL;
 *   } else {
 *     ...
 *   }
 *   ...
 *
 *   // Safe whether memlog_buf is NULL or valid.
 *   RWMEMLOG( &my_obj->memlog_buf, RWMEMLOG_MEM2, "message" );
 * @endcode
 */


/*****************************************************************************/
/*!
 * @page RwMemlogExDts RW.DTS Integration Examples
 *
 * RW.Memlog is designed to be displayed via a yang model, like most
 * things in RIFT.ware.  A small library has been written that
 * integrates RW.Memlog with RW.DTS, so that you don't have to write
 * any code.  All you need to do is call a registration function, and
 * the rest is handled automatically.
 *
 * The RW.DTS integration is defined by a separate library than the
 * RW.Memlog base library.  There are two libraries because core/util
 * needs to make use of RW.Memlog, but RW.DTS integration is not
 * available to core/util.  The RW.DTS integration is defined in
 * core/rwvx.
 *
 * @code
 *   #include <rwmemlogdts.h>
 *   ...
 *
 *   rw_status_t rs = rwmemlogdts_instance_register(
 *     my_inst->rwdts_api,
 *     my_inst->memlog_inst,
 *     rwtasklet_info_get_instance_name(my_inst->tasklet) );
 *   RW_ASSERT(RW_STATUS_SUCCESS == rs);
 * @endcode
 *
 * When your tasklet is destroyed, you must deregister your RW.DTS
 * registration prior to destroying the RW.Memlog instance or tasklet.
 * This API is idempotent.
 *
 * @code
 *   rwmemlogdts_instance_deregister( my_inst->memlog_inst );
 * @endcode
 */


/*****************************************************************************/
/*!
 * @page RwMemlogExDebug GDB Debug Examples
 *
 * Access to RW.Memlog tracing may be very useful from a running gdb
 * session.  In order to support this, two functions have been defined
 * that allow direct access to RW.Memlog data from within gdb.  Both
 * are designed to simply print to stdout.
 *
 * @code
 *   (gdb) p rwmemlog_instance_to_stdout(my_inst->memlog_inst)
 *   ...
 *   (gdb) p rwmemlog_buffer_to_stdout(my_obj->memlog_buf)
 *   ...
 * @endcode
 *
 * ATTN: Currently, there is no support for decoding RW.Memlog buffers
 * from a core file.
 */


/*
 * This ifndef is used to test the preprocessor macros during
 * development.  Please leave this in the code as a convenience to the
 * developers.
 *
 * For reference:
 *   gcc -E -DRWMEMLOG_PP_TESTING - < rwmemlog.h | clang-format
 *   g++ -std=c++11 -E -DRWMEMLOG_PP_TESTING -D__cplusplus - < rwmemlog.h | clang-format | less
 */
#ifndef RWMEMLOG_PP_TESTING

#include <rwlib.h>
#include <protobuf-c/rift-protobuf-c.h>

#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#endif /* ndef RWMEMLOG_PP_TESTING */


#include <sys/cdefs.h>

__BEGIN_DECLS

typedef struct rwmemlog_instance_t rwmemlog_instance_t;
typedef struct rwmemlog_buffer_t rwmemlog_buffer_t;
typedef struct rwmemlog_buffer_header_t rwmemlog_buffer_header_t;
typedef struct rwmemlog_entry_t rwmemlog_entry_t;
typedef struct rwmemlog_location_t rwmemlog_location_t;
typedef struct rwmemlog_output_t rwmemlog_output_t;

typedef intptr_t rwmemlog_unit_t;
typedef uint64_t rwmemlog_ns_t;


/*****************************************************************************/
/*!
 * @defgroup RwMemlogFilters RW.Memlog Filters
 * @{
 *
 * RW.Memlog has 3 primary code paths: test, fast, and slow.  RW.Memlog
 * uses a 64-bit filter mask to determine whether to run the fast-path
 * or slow-path.
 *
 * Every RWMEMLOG macro runs the test-path.  The test-path boils down
 * to two pointer checks, two indirect reads, a pair of bitwise-ands,
 * and a test.  The constant mask specified in the macro arguments gets
 * anded to the buffer's filter_mask to produce the matching-mask:
 * these are all the filters that both apply to the invocation and are
 * also currently enabled in the buffer.  The matching-mask is further
 * compared to a constant fast-path-mask.
 *
 * The fast-path gets invoked when the matching-mask matches anything
 * in the constant fast-path-mask.  If the fast-path gets invoked, an
 * entry will be traced to the buffer.  The fast-path allocates space
 * in the buffer and copies the trace arguments to the buffer.  The
 * fast-path will also check if the slow-path should be triggered.
 *
 * The slow-path gets triggered when the matching-mask matches anything
 * in the constant slow-path-mask.  The slow-path is a function call
 * into the RW.Memlog library, which performs certain expensive
 * operations.  The slow-path gets invoked to extend the buffer chain
 * and/or keep the buffer.
 */


/*!
 * RW.Memlog filter bits and masks.
 *
 * As noted below, some filters are disallowed in the RWMEMLOG mask
 * arguments - they are reserved for internal use.  Some additional
 * filters don't match the fast-path filter, but will match the
 * slow-path filter.  These filters trigger the slow-path only if the
 * fast-path gets triggered for other reasons.
 *
 * This enum also defined several constant masks that make programming
 * RW.Memlog easier.
 */
enum rwmemlog_filter_t
{
  /*!
   * The highest trace priority.  This is always set in the RW.Memlog
   * instance, so any invocation with with this filter will be saved,
   * always.
   */
  RWMEMLOG_MEM0 = 1ul << 0,

  //! Semantic for RWMEMLOG_MEM0.
  RWMEMLOG_ALWAYS = RWMEMLOG_MEM0,

  /*!
   * The second highest trace priority.  On by default in the instance,
   * but may be disabled by configuration.
   */
  RWMEMLOG_MEM1 = 1ul << 1,

  //! Semantic alias for RWMEMLOG_MEM1.
  RWMEMLOG_HIPRI = RWMEMLOG_MEM1,

  /*!
   * The third highest trace priority.  On by default in the instance,
   * but may be disabled by configuration.
   */
  RWMEMLOG_MEM2 = 1ul << 2,

  /*!
   * The fourth highest trace priority.  On by default in the instance,
   * but may be disabled by configuration.
   */
  RWMEMLOG_MEM3 = 1ul << 3,

  //! Semantic alias for RWMEMLOG_MEM3.
  RWMEMLOG_MDPRI = RWMEMLOG_MEM3,

  /*!
   * The fourth lowest trace priority.  This is the lowest priority
   * that is on by default in the instance.  May be disabled by
   * configuration.
   */
  RWMEMLOG_MEM4 = 1ul << 4,

  /*!
   * The third lowest trace priority.  Off by default in the instance,
   * but may be enabled by configuration.
   */
  RWMEMLOG_MEM5 = 1ul << 5,

  //! Semantic alias for RWMEMLOG_MEM5.
  RWMEMLOG_LOPRI = RWMEMLOG_MEM5,

  /*!
   * The second lowest trace priority.  Off by default in the instance,
   * but may be enabled by configuration.
   */
  RWMEMLOG_MEM6 = 1ul << 6,

  /*!
   * The lowest trace priority.  Off by default in the instance, but
   * may be enabled by configuration.
   */
  RWMEMLOG_MEM7 = 1ul << 7,

  //! Semantic alias for RWMEMLOG_MEM7.
  RWMEMLOG_OFF = RWMEMLOG_MEM7,

  /*
   * Reserving one-bit here for each of RW.Log and RW.Trace, for
   * possible future integration.
   *
   * ATTN: Not implemented.  May change to use RUNTIME filters.
   */
  RWMEMLOG_RESERVED_8  = 1ul <<  8,
  RWMEMLOG_RESERVED_9  = 1ul <<  9,
  RWMEMLOG_RESERVED_10 = 1ul << 10,
  RWMEMLOG_RESERVED_11 = 1ul << 11,
  RWMEMLOG_RESERVED_12 = 1ul << 12,
  RWMEMLOG_RESERVED_13 = 1ul << 13,
  RWMEMLOG_RESERVED_14 = 1ul << 14,
  RWMEMLOG_RESERVED_15 = 1ul << 15,
  RWMEMLOG_RESERVED_16 = 1ul << 16,
  RWMEMLOG_RESERVED_17 = 1ul << 17,
  RWMEMLOG_RESERVED_18 = 1ul << 18,
  RWMEMLOG_RESERVED_19 = 1ul << 19,
  RWMEMLOG_RESERVED_20 = 1ul << 20,
  RWMEMLOG_RESERVED_21 = 1ul << 21,
  RWMEMLOG_RESERVED_22 = 1ul << 22,
  RWMEMLOG_RESERVED_23 = 1ul << 23,

  /*!
     Invoke RW.Trace on the entry if it matches the RW.Trace filters,
     fast-path filter.  This triggers RW.Memlog fast-path only if
     RW.Trace fast-path filters match.  This cannot be specified in the
     RWMEMLOG() mask argument - it will be set by the
     RWMEMLOG_ARG_TRACE() implementation.
   *
   * ATTN: Not yet implemented.
   */
  RWMEMLOG_RUNTIME_TRACE = 1ul << 24,

  /*!
     Invoke RW.Trace on the entry if it matches the RW.Trace filters,
     slow-path filter.  This will never match the RW.Memlog fast-path
     filter directly.  In order for this filter to have any effect, the
     invocation must otherwise match the RW.Memlog fast-path filter.
     This cannot be specified in the RWMEMLOG() mask argument - it will
     be set by the RWMEMLOG_ARG_TRACE() implementation.
   *
   * ATTN: Not yet implemented.
   */
  RWMEMLOG_ALSO_TRACE = 1ul << 25,

  /*!
   * Invoke RW.Log on the entry if it matches the RW.Log filters,
   * fast-path filter.  This triggers RW.Memlog fast-path only if
   * RW.Log fast-path filters match.  This cannot be specified in the
   * RWMEMLOG() mask argument - it will be set by the
   * RWMEMLOG_ARG_LOG() implementation.
   *
   * ATTN: Not yet implemented.
   */
  RWMEMLOG_RUNTIME_LOG = 1ul << 26,

  /*!
   * Invoke RW.Log on the entry if it matches the RW.Log filters,
   * slow-path filter.  This will never match the RW.Memlog fast-path
   * filter directly.  In order for this filter to have any effect, the
   * invocation must otherwise match the RW.Memlog fast-path filter.
   * This cannot be specified in the RWMEMLOG() mask argument - it will
   * be set by the RWMEMLOG_ARG_LOG() implementation.
   *
   * ATTN: Not yet implemented.
   */
  RWMEMLOG_ALSO_LOG = 1ul << 27,

  /*!
   * This forces all fast-path matches to take the slow-path as well.
   * This filter is always implicitly set in every invocation.  This
   * filter is off by default in the instance and buffer.  The filter
   * exists as a way to force every fast-path match to also take the
   * slow-path, if there is some kind of check that the slow path needs
   * to do (such as advanced filtering).
   */
  RWMEMLOG_ALSO_SLOW_ALL = 1ul << 28,

  /*!
   * Extend the buffer chain.  This filter gets set when the current
   * invocation leaves less room in the buffer than the maximum-sized
   * invocation requires.  The slow-path will extend the buffer chain.
   */
  RWMEMLOG_RUNTIME_ALLOC = 1ul << 29,

  /*!
   * Take the fast-path.  This filter is never set explicitly in the
   * invocation.  Argument implementation macros may set this filter at
   * runtime in order to match the fast-path filter without otherwise
   * affecting filter semantics.
   */
  RWMEMLOG_RUNTIME_FAST = 1ul << 30,

  /*!
   * Take the slow-path.  This filter is never set explicitly in the
   * invocation.  This filter never matches the fast-path filter.  Argument
   * implementation macros may set this filter at runtime in order to
   * force the slow-path to execute, without otherwise affecting filter
   * semantics.
   */
  RWMEMLOG_RUNTIME_SLOW = 1ul << 31,

  /*!
   * Keep the buffer.  This filter triggers the fast-path and marks the
   * buffer for long-term keeping.  This filter should be specified in
   * invocations that occur only in particularly interesting diagnostic
   * situations - where something bad has happened in the code and you
   * would like to know what led up to it.
   *
   * A kept buffer gets a special marking in the buffer that prevents
   * it from being reactivated from the retirement list.  When a kept
   * buffer would otherwise be reactivated, it is instead placed on a
   * special kept list that has a fixed size.  The kept buffers may
   * eventually be reactivated if the keep list exceeds its fixed size,
   * but it otherwise remains retired forever.  The point of the keep
   * list is lost if it is used frequently - the KEEP filter should
   * only be used in invocations that match in highly exceptional code
   * paths.
   */
  RWMEMLOG_KEEP = 1ul << 32,

  /*!
   * Keep the buffer, runtime filter.  This filter triggers the
   * fast-path and slow-path and keeps the buffer.  This filter cannot
   * be specified directly in in RWMEMLOG() filter argument - it will
   * be set by the RWMEMLOG_ARG implementation.
   */
  RWMEMLOG_RUNTIME_KEEP = 1ul << 33,

  /*!
   * Keep the buffer, slow-path runtime filter.  This filter does not
   * trigger the fast-path.  If the invocation matches the fast-path
   * filter, the buffer will be kept.
   */
  RWMEMLOG_ALSO_KEEP = 1ul << 34,


  RWMEMLOG_RESERVED_35 = 1ul << 35,
  RWMEMLOG_RESERVED_36 = 1ul << 36,
  RWMEMLOG_RESERVED_37 = 1ul << 37,
  RWMEMLOG_RESERVED_38 = 1ul << 38,
  RWMEMLOG_RESERVED_39 = 1ul << 39,

  /*
   * As many or as few of these filters may be set in either
   * invocation, buffer, or instance.  If any are set in both
   * invocation and buffer, then will trace to memory.  If any are both
   * set and the additional ALSO_TRACE or ALSO_LOG are set for the
   * invocation, then take the slow path.
   */
  RWMEMLOG_CATEGORY_A = 1ul << 40,
  RWMEMLOG_CATEGORY_B = 1ul << 41,
  RWMEMLOG_CATEGORY_C = 1ul << 42,
  RWMEMLOG_CATEGORY_D = 1ul << 43,
  RWMEMLOG_CATEGORY_E = 1ul << 44,
  RWMEMLOG_CATEGORY_F = 1ul << 45,
  RWMEMLOG_CATEGORY_G = 1ul << 46,
  RWMEMLOG_CATEGORY_H = 1ul << 47,
  RWMEMLOG_CATEGORY_I = 1ul << 48,
  RWMEMLOG_CATEGORY_J = 1ul << 49,
  RWMEMLOG_CATEGORY_K = 1ul << 50,
  RWMEMLOG_CATEGORY_L = 1ul << 51,
  RWMEMLOG_CATEGORY_M = 1ul << 52,
  RWMEMLOG_CATEGORY_N = 1ul << 53,
  RWMEMLOG_CATEGORY_O = 1ul << 54,
  RWMEMLOG_CATEGORY_P = 1ul << 55,
  RWMEMLOG_CATEGORY_Q = 1ul << 56,
  RWMEMLOG_CATEGORY_R = 1ul << 57,
  RWMEMLOG_CATEGORY_S = 1ul << 58,
  RWMEMLOG_CATEGORY_T = 1ul << 59,
  RWMEMLOG_CATEGORY_U = 1ul << 60,
  RWMEMLOG_CATEGORY_V = 1ul << 61,
  RWMEMLOG_CATEGORY_W = 1ul << 62,
  RWMEMLOG_CATEGORY_X = 1ul << 63,

  /*!
   * Mask of all level-based filters.
   */
  RWMEMLOG_MASK_ALL_LEVELS =
  (   RWMEMLOG_MEM0
    | RWMEMLOG_MEM1
    | RWMEMLOG_MEM2
    | RWMEMLOG_MEM3
    | RWMEMLOG_MEM4
    | RWMEMLOG_MEM5
    | RWMEMLOG_MEM6
    | RWMEMLOG_MEM7 ),

  /*!
   * Mask of filters that invoke the slow-path if any match.
   */
  RWMEMLOG_MASK_SLOW =
  (   RWMEMLOG_RUNTIME_TRACE
    | RWMEMLOG_ALSO_TRACE
    | RWMEMLOG_RUNTIME_LOG
    | RWMEMLOG_ALSO_LOG
    | RWMEMLOG_ALSO_SLOW_ALL
    | RWMEMLOG_RUNTIME_ALLOC
    | RWMEMLOG_RUNTIME_SLOW
    | RWMEMLOG_KEEP
    | RWMEMLOG_RUNTIME_KEEP
    | RWMEMLOG_ALSO_KEEP ),

  /*!
   * Mask of filters that invoke the fast-path if any match.  Fast-path
   * means trace to memory.  This mask generally includes all of the
   * slow-path filters as well (because the slow-path requires the
   * fast-path in order to copy the arguments).  However there are a
   * few special cases of slow-path that are not included in fast-path.
   * These special cases trigger the slow-path only if the fast-path
   * was triggered for some other filters.
   */
  RWMEMLOG_MASK_FAST =
  (   RWMEMLOG_MEM0
    | RWMEMLOG_MEM1
    | RWMEMLOG_MEM2
    | RWMEMLOG_MEM3
    | RWMEMLOG_MEM4
    | RWMEMLOG_MEM5
    | RWMEMLOG_MEM6
    | RWMEMLOG_MEM7
    | RWMEMLOG_RUNTIME_TRACE
    | RWMEMLOG_RUNTIME_LOG
    | RWMEMLOG_RUNTIME_ALLOC
    | RWMEMLOG_RUNTIME_FAST
    | RWMEMLOG_KEEP
    | RWMEMLOG_RUNTIME_KEEP
    | RWMEMLOG_CATEGORY_A
    | RWMEMLOG_CATEGORY_B
    | RWMEMLOG_CATEGORY_C
    | RWMEMLOG_CATEGORY_D
    | RWMEMLOG_CATEGORY_E
    | RWMEMLOG_CATEGORY_F
    | RWMEMLOG_CATEGORY_G
    | RWMEMLOG_CATEGORY_H
    | RWMEMLOG_CATEGORY_I
    | RWMEMLOG_CATEGORY_J
    | RWMEMLOG_CATEGORY_K
    | RWMEMLOG_CATEGORY_L
    | RWMEMLOG_CATEGORY_M
    | RWMEMLOG_CATEGORY_N
    | RWMEMLOG_CATEGORY_O
    | RWMEMLOG_CATEGORY_P
    | RWMEMLOG_CATEGORY_Q
    | RWMEMLOG_CATEGORY_R
    | RWMEMLOG_CATEGORY_S
    | RWMEMLOG_CATEGORY_T
    | RWMEMLOG_CATEGORY_U
    | RWMEMLOG_CATEGORY_V
    | RWMEMLOG_CATEGORY_W
    | RWMEMLOG_CATEGORY_X ),

  /*!
   * Mask of filters that are disallowed at compile time in the filter
   * argument to the RWMEMLOG macros.  These filters are all either
   * undefined, or they are reserved for the argument implementation
   * macros to perform filtering at runtime.
   */
  RWMEMLOG_MASK_ILLEGAL_ARG =
  (   RWMEMLOG_RESERVED_8
    | RWMEMLOG_RESERVED_9
    | RWMEMLOG_RESERVED_10
    | RWMEMLOG_RESERVED_11
    | RWMEMLOG_RESERVED_12
    | RWMEMLOG_RESERVED_13
    | RWMEMLOG_RESERVED_14
    | RWMEMLOG_RESERVED_15
    | RWMEMLOG_RESERVED_16
    | RWMEMLOG_RESERVED_17
    | RWMEMLOG_RESERVED_18
    | RWMEMLOG_RESERVED_19
    | RWMEMLOG_RESERVED_20
    | RWMEMLOG_RESERVED_21
    | RWMEMLOG_RESERVED_22
    | RWMEMLOG_RESERVED_23
    | RWMEMLOG_RUNTIME_TRACE
    | RWMEMLOG_ALSO_TRACE
    | RWMEMLOG_RUNTIME_LOG
    | RWMEMLOG_ALSO_LOG
    | RWMEMLOG_RUNTIME_ALLOC
    | RWMEMLOG_RUNTIME_FAST
    | RWMEMLOG_RUNTIME_SLOW
    | RWMEMLOG_RUNTIME_KEEP
    | RWMEMLOG_RESERVED_35
    | RWMEMLOG_RESERVED_36
    | RWMEMLOG_RESERVED_37
    | RWMEMLOG_RESERVED_38
    | RWMEMLOG_RESERVED_39 ),

  /*!
   * Mask of filters that are ALWAYS set in the buffer and instance.
   * These are always set because they are used by the implementation
   * at runtime to optionally select the fast-path or slow-path, or
   * otherwise enable runtime decisions.  Configuring these filters as
   * a debug user makes no sense.
   */
  RWMEMLOG_MASK_INSTANCE_ALWAYS_SET =
  (   RWMEMLOG_MEM0
    | RWMEMLOG_RUNTIME_TRACE
    | RWMEMLOG_ALSO_TRACE
    | RWMEMLOG_RUNTIME_LOG
    | RWMEMLOG_ALSO_LOG
    | RWMEMLOG_RUNTIME_ALLOC
    | RWMEMLOG_RUNTIME_FAST
    | RWMEMLOG_RUNTIME_SLOW
    | RWMEMLOG_KEEP
    | RWMEMLOG_RUNTIME_KEEP
    | RWMEMLOG_ALSO_KEEP ),

  /*!
   * Mask of the default filter settings for an instance, at
   * initialization time.  Includes the filters that must always be set
   * in the instance.
   */
  RWMEMLOG_MASK_INSTANCE_DEFAULT =
  (   RWMEMLOG_MASK_INSTANCE_ALWAYS_SET
    | RWMEMLOG_MEM1
    | RWMEMLOG_MEM2
    | RWMEMLOG_MEM3
    | RWMEMLOG_MEM4
    | RWMEMLOG_CATEGORY_A
    | RWMEMLOG_CATEGORY_B
    | RWMEMLOG_CATEGORY_C
    | RWMEMLOG_CATEGORY_D
    | RWMEMLOG_CATEGORY_E
    | RWMEMLOG_CATEGORY_F
    | RWMEMLOG_CATEGORY_G
    | RWMEMLOG_CATEGORY_H ),

  /*!
   * Mask of the buffer and instance filter settings that can be
   * changed with configuration.  All of these can be enabled/disabled
   * at will.
   */
  RWMEMLOG_MASK_INSTANCE_CONFIGURABLE =
  (   RWMEMLOG_MEM1
    | RWMEMLOG_MEM2
    | RWMEMLOG_MEM3
    | RWMEMLOG_MEM4
    | RWMEMLOG_MEM5
    | RWMEMLOG_MEM6
    | RWMEMLOG_MEM7
    | RWMEMLOG_CATEGORY_A
    | RWMEMLOG_CATEGORY_B
    | RWMEMLOG_CATEGORY_C
    | RWMEMLOG_CATEGORY_D
    | RWMEMLOG_CATEGORY_E
    | RWMEMLOG_CATEGORY_F
    | RWMEMLOG_CATEGORY_G
    | RWMEMLOG_CATEGORY_H
    | RWMEMLOG_CATEGORY_I
    | RWMEMLOG_CATEGORY_J
    | RWMEMLOG_CATEGORY_K
    | RWMEMLOG_CATEGORY_L
    | RWMEMLOG_CATEGORY_M
    | RWMEMLOG_CATEGORY_N
    | RWMEMLOG_CATEGORY_O
    | RWMEMLOG_CATEGORY_P
    | RWMEMLOG_CATEGORY_Q
    | RWMEMLOG_CATEGORY_R
    | RWMEMLOG_CATEGORY_S
    | RWMEMLOG_CATEGORY_T
    | RWMEMLOG_CATEGORY_U
    | RWMEMLOG_CATEGORY_V
    | RWMEMLOG_CATEGORY_W
    | RWMEMLOG_CATEGORY_X ),

  /*
   * Mask of the filters that require runtime message output.
   */
  RWMEMLOG_MASK_ANY_OUTPUT =
  (   RWMEMLOG_RUNTIME_TRACE
    | RWMEMLOG_RUNTIME_LOG
    | RWMEMLOG_ALSO_TRACE
    | RWMEMLOG_ALSO_LOG ),

  /*
   * Mask of the filters that require runtime tracing.
   */
  RWMEMLOG_MASK_ANY_TRACE =
  (   RWMEMLOG_RUNTIME_TRACE
    | RWMEMLOG_ALSO_TRACE ),

  /*
   * Mask of the filters that require runtime tracing.
   */
  RWMEMLOG_MASK_ANY_LOG =
  (   RWMEMLOG_RUNTIME_LOG
    | RWMEMLOG_ALSO_LOG ),

  /*
   * Mask of the filters that require keeping the buffer.
   */
  RWMEMLOG_MASK_ANY_KEEP =
  (   RWMEMLOG_KEEP
    | RWMEMLOG_RUNTIME_KEEP
    | RWMEMLOG_ALSO_KEEP ),
};

typedef enum rwmemlog_filter_t rwmemlog_filter_t;
typedef uint64_t rwmemlog_filter_mask_t;

/*! @} */


/*!
 * @addtogroup RwMemlogBuffer
 * @{
 */

/*!
 * Buffer flags.  Buffer flags control special behaviors of individual
 * buffers.
 */
enum rwmemlog_bufflag_t
{
  /*!
   * Place the buffer on the keep list instead of reactivating.
   */
  RWMEMLOG_BUFFLAG_KEEP = 1 << 0,

  /*!
   * The buffer is currently on the keep list.
   */
  RWMEMLOG_BUFFLAG_ON_KEEP_LIST = 1 << 1,

  /*!
   * The buffer filters have been explicitly changed, relative to the
   * instance filters.
   */
  RWMEMLOG_BUFFLAG_EXPLICIT_FILTER = 1 << 2,

  /*!
   * The buffer is active.
   */
  RWMEMLOG_BUFFLAG_ACTIVE = 1 << 3,

  /*!
   * This buffer chain is a closed ring.  Do not add more buffers
   * unless the keep flag gets set.
   *
   * ATTN: Unimplemented.
   */
  RWMEMLOG_BUFFLAG_RING = 1 << 4,

  /*!
   * This buffer has entries that actually own resources (such as a
   * PBCM reference or a string pointer).  Prior to reactivating the
   * buffer, it must be reclaimed.
   *
   * ATTN: Unimplemented.
   */
  RWMEMLOG_BUFFLAG_CLEANUP = 1 << 5,

  /*!
   * Used to reap excess buffers in a mark-and-sweep algorithm.  This
   * flag is set during the mark phase, and the buffer is actually
   * reclaimed during the sweep phase.
   *
   * ATTN: Unimplemented.
   */
  RWMEMLOG_BUFFLAG_REAP = 1 << 6,

  /*!
   * Used to mark a buffer that is part of a larger memory allocation.
   * This buffer cannot be reaped by itself.  The buffers in a chunk
   * will be sequential in the buffer pool.
   *
   * ATTN: Unimplemented.
   */
  RWMEMLOG_BUFFLAG_CHUNK = 1 << 7,

  RWMEMLOG_BUFFLAG_RESERVED_8 = 1 << 8,
  RWMEMLOG_BUFFLAG_RESERVED_9 = 1 << 9,
  RWMEMLOG_BUFFLAG_RESERVED_10 = 1 << 10,
  RWMEMLOG_BUFFLAG_RESERVED_11 = 1 << 11,
  RWMEMLOG_BUFFLAG_RESERVED_12 = 1 << 12,
  RWMEMLOG_BUFFLAG_RESERVED_13 = 1 << 13,
  RWMEMLOG_BUFFLAG_RESERVED_14 = 1 << 14,
  RWMEMLOG_BUFFLAG_RESERVED_15 = 1 << 15,
};

typedef enum rwmemlog_bufflag_t rwmemlog_bufflag_t;
typedef uint16_t rwmemlog_buffer_flag_mask_t;

/*! @} */


/*****************************************************************************/
/*!
 * @defgroup RwMemlogArgDecode RW.Memlog Argument Decoding
 * @{
 *
 * RW.Memlog defers trace entry string formatting until something
 * actually requires the entry to be output.  Ordinarily, a traced
 * entry never gets output - it just sits in memory for a little while,
 * then gets overwritten.
 *
 * Because entry formatting is separated from entry creation, the
 * output routines need the know following information:
 *  - How much data was traced.
 *  - What kind of data was traced.
 *  - How to transform the data into a human-readable form.
 *
 * This information is provided in two ways: the trace entry meta-data,
 * and the trace entry header.
 *
 * The trace entry meta-data is defined by a static constant object,
 * known as the "location".  The meta-data includes the file and line
 * number, but also ny function pointers and constant strings needed to
 * make sense of each argument in the trace.  For example, if the trace
 * contains a printf, there will be a function pointer to the printf
 * converter function, and a pointer to the format string.  These are
 * constants, and stored the meta-data.
 *
 * The trace entry header defines what was actually stored into a
 * RW.Memlog buffer.  The trace entry header contains two things: the
 * location and the decode flags.  The location is just a pointer to
 * the meta-data for the trace entry.  The decode flags record how many
 * bytes of data was recorded for each argument.
 *
 * Together, the meta-data and the entry header allow conversion to a
 * human-readable string, long after the trace entry was recorded.
 * However, it is more expensive that a simple printf at trace time,
 * because of the additional decode step.
 */

/*!
 * Argument meta-data decoding.  These values define how to decode the
 * argument meta-data.  The argument meta-data defines how to convert
 * the raw trace buffer bytes into human-readable strings.
 *
 * These identifiers don't directly define how the conversion occurs -
 * the conversion is left to argument-specific functions.  Rather,
 * these define the arguments the conversion functions take.
 */
enum rwmemlog_argtype_t
{
  /*
   * No argument.
   */
  RWMEMLOG_ARGTYPE_NONE = 0,

  /*
   * A string.  No actual argument.  The meta-data is:
   *  - STRING: An arbitrary description string.
   */
  RWMEMLOG_ARGTYPE_CONST_STRING = 1,

  /*
   * A fixed conversion function, with no options.  The meta-data is:
   *  - FUNC: String conversion function.  Must be of the type
   *    rwmemlog_argtype_func_fp_t prototype.
   *  - SIZE: The number of units in the data.
   */
  RWMEMLOG_ARGTYPE_FUNC = 2,

  /*
   * A simple string conversion function.  The meta-data is:
   *  - FUNC: String conversion function.  Must be of the type
   *    rwmemlog_argtype_func_string_fp_t prototype.
   *  - STRING: An arbitrary description string.
   */
  RWMEMLOG_ARGTYPE_FUNC_STRING = 3,

  /*
   * A straight enum-to-string conversion function.  The meta-data is:
   *  - EFUNC: Enum to const string conversion function.  Must be of
   *    the type rwmemlog_argtype_enum_fp_t prototype.
   *  - STRING: An arbitrary description string.
   */
  RWMEMLOG_ARGTYPE_ENUM_STRING = 4,

  /* Reserved types */
  RWMEMLOG_ARGTYPE_RESERVED_5,
  RWMEMLOG_ARGTYPE_RESERVED_6,
  RWMEMLOG_ARGTYPE_RESERVED_7,
};
typedef enum rwmemlog_argtype_t rwmemlog_argtype_t;



/*!
 * Argument data decoding.
 *
 * These values define how to decode the argument data.  The argument
 * data is the bytes actually stored in a RW.Memlog buffer.  Largely,
 * this enumeration defines shifts and masks used to decode
 * rwmemlog_entry_t.entry_decode.
 */
enum
{
  // Indices into rwmemlog_location_t.meta_decode[]
  RWMEMLOG_LOCATION_INDEX_TOTAL_ARGS = 0,
  RWMEMLOG_LOCATION_INDEX_RESERVED_1 = 1,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG0 = 2,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG1 = 3,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG2 = 4,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG3 = 5,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG4 = 6,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG5 = 7,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG6 = 8,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG7 = 9,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG8 = 10,
  RWMEMLOG_LOCATION_INDEX_DECODE_ARG9 = 11,


  // Masks and shift for decoding the values for RWMEMLOG_LOCATION_INDEX_DECODE_ARG*
  RWMEMLOG_META_DECODE_ARGTYPE_MASK = 0x7,
  RWMEMLOG_META_DECODE_ARGTYPE_SHIFT = 0,

  RWMEMLOG_META_DECODE_RESERVED_MASK = 0x7,
  RWMEMLOG_META_DECODE_RESERVED_SHIFT = 3,

  RWMEMLOG_META_DECODE_META_COUNT_MASK = 0x3,
  RWMEMLOG_META_DECODE_META_COUNT_SHIFT = 6,


  // Masks and shift for decoding rwmemlog_entry_t.entry_decode
  RWMEMLOG_ENTRY_DECODE_PER_ARG_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_PER_ARG_SHIFT = 5,

  RWMEMLOG_ENTRY_DECODE_ARG0_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG0_SHIFT = 0,

  RWMEMLOG_ENTRY_DECODE_ARG1_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG1_SHIFT = 5,

  RWMEMLOG_ENTRY_DECODE_ARG2_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG2_SHIFT = 10,

  RWMEMLOG_ENTRY_DECODE_ARG3_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG3_SHIFT = 15,

  RWMEMLOG_ENTRY_DECODE_ARG4_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG4_SHIFT = 20,

  RWMEMLOG_ENTRY_DECODE_ARG5_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG5_SHIFT = 25,

  RWMEMLOG_ENTRY_DECODE_ARG6_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG6_SHIFT = 30,

  RWMEMLOG_ENTRY_DECODE_ARG7_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG7_SHIFT = 35,

  RWMEMLOG_ENTRY_DECODE_ARG8_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG8_SHIFT = 40,

  RWMEMLOG_ENTRY_DECODE_ARG9_MASK = 0x1F,
  RWMEMLOG_ENTRY_DECODE_ARG9_SHIFT = 45,

  RWMEMLOG_ENTRY_DECODE_RESERVED_MASK = 0x3FFF,
  RWMEMLOG_ENTRY_DECODE_RESERVED_SHIFT = 50,
};


/*!
 * RWMEMLOG_ARGTYPE_CONST_STRING meta-data definition.
 */
typedef struct {
  union {
    const char* string;
    intptr_t i1_;
  };
} rwmemlog_argtype_const_string_meta_t;



/*!
 * RWMEMLOG_ARGTYPE_FUNC conversion function type.
 */
typedef rw_status_t (*rwmemlog_argtype_func_fp_t)(
  rwmemlog_output_t* output,
  size_t ndata,
  const rwmemlog_unit_t* data
);

/*!
 * RWMEMLOG_ARGTYPE_FUNC meta-data definition.
 */
typedef struct {
  union {
    rwmemlog_argtype_func_fp_t fp;
    intptr_t i1_;
  };
} rwmemlog_argtype_func_meta_t;



/*!
 * RWMEMLOG_ARGTYPE_FUNC_STRING conversion function type.
 */
typedef rw_status_t (*rwmemlog_argtype_func_string_fp_t)(
  rwmemlog_output_t* output,
  const char* format,
  size_t ndata,
  const rwmemlog_unit_t* data
);

/*!
 * RWMEMLOG_ARGTYPE_FUNC_STRING meta-data definition.
 */
typedef struct {
  union {
    rwmemlog_argtype_func_string_fp_t fp;
    intptr_t i1_;
  };
  union {
    const char* string;
    intptr_t i2_;
  };
} rwmemlog_argtype_func_string_meta_t;



/*!
 * RWMEMLOG_ARGTYPE_ENUM_STRING conversion function type.
 */
typedef const char* (*rwmemlog_argtype_enum_string_fp_t)(
  int enum_value
);

/*!
 * RWMEMLOG_ARGTYPE_ENUM_STRING meta-data definition.
 */
typedef struct {
  union {
    rwmemlog_argtype_enum_string_fp_t fp;
    intptr_t i1_;
  };
  union {
    const char* string;
    intptr_t i2_;
  };
} rwmemlog_argtype_enum_string_meta_t;


enum {
  /*!
   * The maximum number of arguments to an entry.
   */
  RWMEMLOG_ARG_MAX_COUNT = 10,
};

/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RwMemlogEntry RW.Memlog Trace Entry
 * @{
 *
 * A RW.Memlog trace entry is a traced invocation of a RWMEMLOG macro -
 * as distinct from the line of code that does the trace (that is
 * referred to as an "invocation").
 */

/*!
 * A single RW.Memlog trace invocation source code location.  This
 * struct is always followed by a zero or more structures that define
 * the meta-data for each argument.  Each meta-data structure is an
 * integral number of intptr_t's big.
 */
struct rwmemlog_location_t
{
  /*! Source file */
  const char* file;

  /*! Source line */
  uint32_t line;

  /*!
   * Meta-data decoding.  Records the type of each argument and the
   * size of the arguments' meta-data structures.
   */
  uint8_t meta_decode[2+RWMEMLOG_ARG_MAX_COUNT];
};


/*!
 * A single trace entry, as stored in a buffer.  This is just a header;
 * the argument data (if any) follows this header.  Each argument's
 * data is stored as an integral number of rwmemlog_unit_t's.
 */
struct rwmemlog_entry_t
{
  /*!
   * The location and meta-data for the entry.  Needed to decode the
   * data upon output.
   *
   * ATTN: We will need something that works on a core file!
   */
  const rwmemlog_location_t* location;

  /*!
   * Data decoding flags.  These help decode the entry data from the
   * buffer.  Minimally, it identifies the length of each argument, in
   * units of rwmemlog_unit_t.
   */
  uint64_t entry_decode;
};

/*! @} */


/*****************************************************************************/
/*!
 * @defgroup RwMemlogBuffer RW.Memlog Buffer
 * @{
 *
 * A buffer is the fundamental unit of memory allocation in RW.Memlog.
 * Buffers are allocated/retired as whole units.  Buffers are never
 * shared between traced objects.
 *
 * Buffers have no mutual exclusion protection!  It is not legal to
 * trace to the same buffer simultaneously from multiple threads.
 * However, it is legal to simultaneously trace to different buffers
 * from different threads.
 *
 * Each buffer has an independent filter setting.  Initially, the
 * filters are set from the instance's filters, so all buffers use the
 * same filters.  By default, any changes to the global instance's
 * filter settings will be propagated to the active buffers.  However,
 * filters in a specific buffer can be set to a different value than
 * the instance.  Once a buffer has had filters set explicitly, changes
 * to the instance global settings will no longer propagate to that
 * buffer chain.
 */

/*!
 * RW.Memlog trace buffer header.  Every buffer begins with this
 * header.  Following the header is a buffer of rwmemlog_unit_t data,
 * which can be decoded into individual entries by casting data[0] to a
 * rwmemlog_entry_t and then decoding each entry in turn.  Because of
 * the variable-length encoding, it is not possible to index entries
 * directly.
 *
 * The header maintains the linkages between buffers in a chain, and
 * records information about the object that traced to the buffer.
 */
struct rwmemlog_buffer_header_t
{
  /*!
   * The owning instance.  The buffer will go back to this instance
   * when retired.  This instance is also how management clients will
   * find the buffer when outputting entry data.
   */
  rwmemlog_instance_t* instance;

  /*! A description of the object using the buffer. */
  const char* object_name;

  /*!
   * An additional identifier for the object - typically an instance
   * pointer or an instance ID number.
   */
  intptr_t object_id;

  /*!
   * The time the buffer was activated or retired, depending on the
   * buffer RWMEMLOG_BUFFLAG_ACTIVE flag.
   *
   * If RWMEMLOG_BUFFLAG_ACTIVE is set, the buffer is active.  The time
   * is when the buffer was (re)activated.
   *
   * If RWMEMLOG_BUFFLAG_ACTIVE is clear, the buffer is retired.  The
   * time is when the buffer was retired.  Newly allocated empty
   * buffers will have retire a time of 0.
   */
  rwmemlog_ns_t time_ns;

  /*!
   * The filter being used for the buffer.  Initially populated from
   * the instance, this filter can be changed for each individual
   * active buffer (for example, to trace specific objects with more or
   * less verbosity).
   */
  rwmemlog_filter_mask_t filter_mask;

  /*! Incremented every time the buffer gets reused.  */
  uint32_t version;

  /*! The used portion of data[], in units. */
  uint16_t used_units;

  /*! Buffer control flags. */
  rwmemlog_buffer_flag_mask_t flags_mask;

  /*!
   * Singly linked list of buffers.
   *
   * If the buffer is active: this points to the previous buffer
   * associated with this object, if any.  Active buffers have a chain
   * length of at most 2, rooted in the object owning the buffer.
   *
   * If the buffer is inactive: this points to the next oldest retired
   * or keep buffer.  Buffers are reused in order of oldest retirement
   * time.  If an empty buffer gets retired, it is placed on the head
   * of the list to be recycled immediately, thus preserving retired
   * buffers that still hold useful trace data.
   */
  rwmemlog_buffer_t* next;

  /*! The data. */
  rwmemlog_unit_t data[1];
};


/*!
 * Buffer constants.
 */
enum {
  /*!
   * The size of a single buffer, in bytes.
   */
  RWMEMLOG_BUFFER_ALLOC_SIZE_BYTES = 8192,

  /*!
   * The size of the more-data portion of a single buffer, in data
   * units.
   */
  RWMEMLOG_BUFFER_MORE_DATA_SIZE_UNITS
    = (1 +  (  RWMEMLOG_BUFFER_ALLOC_SIZE_BYTES
             - sizeof(rwmemlog_buffer_header_t))
          / sizeof(rwmemlog_unit_t)),

  /*!
   * The size of the entire data portion of a single buffer, in data
   * units.  This accounts for the unit stored in the header.
   */
  RWMEMLOG_BUFFER_DATA_SIZE_UNITS
    = RWMEMLOG_BUFFER_MORE_DATA_SIZE_UNITS + 1,

  /*!
   * The size of the entire data portion of a single buffer, in bytes.
   */
  RWMEMLOG_BUFFER_DATA_SIZE_BYTES
    = RWMEMLOG_BUFFER_DATA_SIZE_UNITS * sizeof(rwmemlog_unit_t),


  /*!
   * The maximum size of a single entry, in bytes.
   */
  RWMEMLOG_ENTRY_SIZE_MAX_BYTES = 512,

  /*!
   * The maximum size of a single entry, in units.
   */
  RWMEMLOG_ENTRY_SIZE_ARGS_MAX_UNITS
    = RWMEMLOG_ENTRY_SIZE_MAX_BYTES / sizeof(rwmemlog_unit_t),

  /*!
   * The size of an entry header, in units.
   */
  RWMEMLOG_ENTRY_HEADER_SIZE_UNITS
    = sizeof(rwmemlog_entry_t) / sizeof(rwmemlog_unit_t),

  /*!
   * Total maximum number of units in an entry.
   *
   * This is also the next buffer allocation threshold, in units.  The
   * first time a buffer's remaining units count goes below this
   * threshold, a new buffer will be activated and chained, while the
   * previous buffer (if any) will be retired.
   *
   * The threshold must be large enough that the largest possible entry
   * can be added to the buffer on the NEXT trace.  It works this way
   * because entry allocation only happens in the fast-path, while
   * buffer allocation only happens in the slow-path.  The slow-path
   * always runs after the fast-path, so more complex logic would be
   * required in the fast-path in order to allocate buffers on demand.
   * So, instead, they are pre-allocated.
   */
  RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS
    = RWMEMLOG_ENTRY_SIZE_ARGS_MAX_UNITS + RWMEMLOG_ENTRY_HEADER_SIZE_UNITS,

  /*!
   * maximum number of used units that leaves room for another maximum
   * sized entry.
   */
  RWMEMLOG_BUFFER_USED_SIZE_ALLOC_THRESHOLD_UNITS
    =   RWMEMLOG_BUFFER_DATA_SIZE_UNITS
      - RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS,

  /*!
   * The maximum size of a single argument copied into the buffer
   * entry, in units.  This has a relationship to the maximum number of
   * arguments and the en/decoding of the entry.
   */
  RWMEMLOG_ARG_SIZE_MAX_UNITS
    = RWMEMLOG_ENTRY_DECODE_PER_ARG_MASK,

  /*!
   * The maximum size of a single entry, in bytes.
   */
  RWMEMLOG_ARG_SIZE_MAX_BYTES
    = RWMEMLOG_ARG_SIZE_MAX_UNITS * sizeof(rwmemlog_unit_t),
};


/*!
 * A whole buffer, including trace entry data.
 */
struct rwmemlog_buffer_t
{
  /*!
   * Buffer description and general information.  Also contains the
   * first word of data.
   */
  rwmemlog_buffer_header_t header;

  /*!
   * Most of the traced data lives here.  One unit of data is kept in
   * the header, for programming convenience.
   */
  rwmemlog_unit_t more_data[RWMEMLOG_BUFFER_MORE_DATA_SIZE_UNITS];
};


/*!
 * Change the filter settings for a buffer.  As buffers are added to
 * the chain, they will get the same filter settings.
 */
void rwmemlog_buffer_filters_set(
  /*!
   * [in] The buffer to change.
   */
  rwmemlog_buffer_t* buffer,
  /*!
   * [in] The filter settings.  Illegal filters are ignored.  The
   * always-on filters will remain set, regardless of whether they are
   * set in this argument or not.
   */
  rwmemlog_filter_mask_t filter_mask
);


/*!
 * Enable additional filters for a buffer.  As buffers are added to the
 * chain, they will get the same filter settings.
 */
void rwmemlog_buffer_filters_enable(
  /*!
   * [in] The buffer to change.
   */
  rwmemlog_buffer_t* buffer,
  /*!
   * [in] The filters to enable.  No changes are made filters not
   * specified in filter_mask.  Illegal filters are ignored.
   */
  rwmemlog_filter_mask_t filter_mask
);


/*!
 * Disable filter settings for a buffer.  As buffers are added to the
 * chain, they will get the same filter settings.
 */
void rwmemlog_buffer_filters_disable(
  /*!
   * [in] The buffer to change.
   */
  rwmemlog_buffer_t* buffer,
  /*!
   * [in] The filters to disable.  No changes are made filters not
   * specified in filter_mask.  The always-on filters will remain set,
   * regardless of whether they are set in this argument or not.
   */
  rwmemlog_filter_mask_t filter_mask
);


/*!
 * Return all buffers associated with an object.  You must call this
 * API is used when a traced object gets destroyed - failing to call
 * this API will result in leaked buffers.
 *
 * The buffers will be retired, and reused after an indeterminate
 * period of time.
 */
void rwmemlog_buffer_return_all(
  /*!
   * [in] The buffer (chain) to retire.
   */
  rwmemlog_buffer_t* buffer
);


/*! @} */


/*****************************************************************************/
/*!
 * @defgroup RwMemlogInstance RW.Memlog Instance
 * @{
 *
 * Every library or application that uses RW.Memlog must allocate a
 * RW.Memlog instance to maintain the buffer pool and serve as the
 * RW.DTS management endpoint (although RW.DTS integration is not
 * strictly required).
 */

/*!
 * RW.Memlog slow-path callback for RW.Log integration.
 *
 * ATTN: Integration is not implemented.
 */
typedef void (*rwmemlog_callback_rwlog_fp_t)(
  rwmemlog_instance_t* instance,
  ProtobufCMessage* pbcm /* ATTN: This needs to be changed to the proper yang-based type. */
);

/*!
 * RW.Memlog slow-path callback for RW.Trace integration.
 *
 * ATTN: Integration is not implemented.
 */
typedef void (*rwmemlog_callback_rwtrace_fp_t)(
  rwmemlog_instance_t* instance,
  ProtobufCMessage* pbcm /* ATTN: This needs to be changed to the proper yang-based type. */
);

/*!
 * RW.Memlog locked-API callback.  This function type is used to make
 * an arbitrary function call while a RW.Memlog instance's mutex is
 * held.  This may be useful for certain application to read data out
 * of their own RW.Memlog instance.
 */
typedef rw_status_t (*rwmemlog_instance_lock_fp_t)(
  rwmemlog_instance_t* instance,
  intptr_t context
);


/*!
 * RW.Memlog Instance.
 *
 * The RW.Memlog instance contains the buffer pool, the retired and
 * keep buffer chains, the instance tracing configuration, the
 * instance's pool configuration, and various objects related to RW.DTS
 * integration.
 */
struct rwmemlog_instance_t
{
  /*!
   * A description for the instance.  Must be unique within a
   * particular tasklet, as it will be a list key in yang.
   */
  char instance_name[256];

  /*!
   * The complete pool of buffers.  This can grow at runtime.  It
   * points to all buffers, and is used to walk them when outputting
   * the data.
   */
  rwmemlog_buffer_t** buffer_pool;

  /*!
   * The number of buffers in buffer_pool.  This is the size of
   * buffer_pool.
   */
  size_t buffer_count;

  /*!
   * The minimum number of retired buffers to keep in the retired
   * list.  When the number of retired buffers exceeds this value by
   * more than 2x, a portion of the retired list will be reclaimed.
   */
  uint32_t minimum_retired_count;

  /*!
   * The current number of retired buffers.
   */
  uint32_t current_retired_count;

  /*!
   * The maximum number of buffers to keep retired.  Once this limit
   * is reached, new keeps will release older keeps back to the retired
   * pool.
   */
  uint32_t maximum_keep_count;

  /*!
   * The current number of kept buffers.
   */
  uint32_t current_keep_count;

  /*!
   * Pointer to the oldest retired buffer.  This is the head of the
   * linked list of buffers.  rwmemlog_buffer_header_t.next points away
   * from this buffer.
   */
  rwmemlog_buffer_t* retired_oldest;

  /*!
   * Pointer to the youngest retired buffer.  This is the tail of the
   * linked list of buffers.  rwmemlog_buffer_header_t.next points
   * towards this buffer.  This buffer should have a null next pointer.
   */
  rwmemlog_buffer_t* retired_newest;

  /*!
   * Pointer to the oldest kept buffer.  This is the head of the linked
   * list of buffers.  rwmemlog_buffer_header_t.next points away from
   * this buffer.
   */
  rwmemlog_buffer_t* keep_oldest;

  /*!
   * Pointer to the youngest kept buffer.  This is the tail of the
   * linked list of buffers.  rwmemlog_buffer_header_t.next points
   * towards this buffer.  This buffer should have a null next pointer.
   */
  rwmemlog_buffer_t* keep_newest;

  /*!
   * The mask of current filters.
   * ATTN: Details about how these are managed.
   */
  rwmemlog_filter_mask_t filter_mask;

  /*!
   * Owning tasklet's context - needed for callbacks.
   */
  intptr_t app_context;

  /*!
   * Callback for entries that require invoking RW.Log.
   */
  rwmemlog_callback_rwlog_fp_t rwlog_callback;

  /*!
   * Callback for entries that require invoking RW.Trace.
   */
  rwmemlog_callback_rwtrace_fp_t rwtrace_callback;

  /*!
   * Keyspec instance.  Also used for protobuf-c instance.
   */
  void* ks_instance;

  /*!
   * The mutual exclusion.  Kept as a void pointer because it may be a
   * C++ type.  Actually std::mutex.
   */
  void* mutex;

  /*!
   * The tasklet info pointer.  Actually rwtasklet_info_ptr_t.
   */
  void* tasklet_info;

  /*!
   * The RW.DTS API handle.  Actually rwdts_api_t.
   */
  void* dts_api;

  /*!
   * The DTS registration for state data.  Actually rwdts_member_reg_handle_t.
   */
  void* dts_reg_state;

  /*!
   * The DTS registration for RPC commands.  Actually rwdts_member_reg_handle_t.
   */
  void* dts_reg_rpc;

  /*!
   * The DTS appconf group.  Actually rwdts_appconf_t.
   */
  void* dts_config_group;

  /*!
   * The DTS config registration.  Actually rwdts_member_reg_handle_t.
   */
  void* dts_config_reg;

  /* ATTN: The categories A-X should have names, which should be visible in yang */
};


/*!
 * RW.Memlog instance constants.
 */
enum {
  /*!
   * The default buffer pool size, in buffers.
   */
  RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS = 8,

  /*!
   * The default maximum buffer pool size, in buffers.
   */
  RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MAX_KEEP_BUFS = 16,
};


/*!
 * Allocate and initialize a new RW.Memlog instance.
 *
 * @return The new instance.  Crashes on alloc failure.
 */
rwmemlog_instance_t* rwmemlog_instance_alloc(
  /*!
   * [in] Description string.  Copied into instance, so does not need
   * to survive this call.
   */
  const char* descr,

  /*!
   * [in] Initial number of buffers to allocate.  If 0, a default value
   * will be used.
   */
  uint32_t minimum_retired_count,

  /*!
   * [in] The maximum number of buffers to keep retired.  Once this
   * limit is reached, new keeps will release older keeps back to the
   * regular retired pool.
   */
  uint32_t maximum_keep_count
);


/*!
 * Destroy a RW.Memlog instance.  Releases all memory owned by the
 * instance.  Will crash is any buffers are active or if the RW.DTS
 * registrations are still active.
 */
void rwmemlog_instance_destroy(
  /*!
   * [in] The instance to destroy.
   */
  rwmemlog_instance_t* instance
);


/*!
 * Register application callbacks.  These are callbacks (instead of
 * being coded directly) because of submodule build ordering.
 */
void rwmemlog_instance_register_app(
  /*!
   * [inout] The instance to configure.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] Opaque context for the app's use.
   */
  intptr_t app_context,
  /*!
   * [in] Callback for runtime message output to RW.Log.  May be NULL.
   */
  rwmemlog_callback_rwlog_fp_t rwlog_callback,
  /*!
   * [in] Callback for runtime message output to RW.Trace.  May be NULL.
   */
  rwmemlog_callback_rwtrace_fp_t rwtrace_callback,
  /*!
   * [in] Keyspec instance, needed for keyspec and message memory
   * management.  May be NULL.
   */
  void* ks_instance
);


/*!
 * Invoke a function from within an instance's lock.
 *
 * @return The status from function.
 */
rw_status_t rwmemlog_instance_invoke_locked(
  /*!
   * [in] The instance to lock.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] The function to call.  Must not be NULL.
   */
  rwmemlog_instance_lock_fp_t function,
  /*!
   * [in] Opaque context for the app's use.
   */
  intptr_t context
);


/*!
 * Allocate a buffer for use by an associated object.
 *
 * @return The newly activated buffer.
 */
rwmemlog_buffer_t* rwmemlog_instance_get_buffer(
  /*!
   * [in] The instance to allocate from.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] A constant string that describes the object doing the tracing.
   * The pointer to this string will be saved in the buffer, and the
   * buffer may live indeterminately long.  Therefore, this string MUST
   * be a constant!  Must not be NULL.
   */
  const char* object_name,
  /*!
   * [in] An arbitrary value used to identify the object instance.
   * Typical values are a pointer or integer identifier.
   */
  intptr_t object_id
);


/*!
 * Change the filter settings for an instance.  As buffers go active,
 * they will get the current filter settings.
 */
void rwmemlog_instance_locked_filters_set(
  /*!
   * [in] The instance to change.  Must be locked.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] The filter settings.  Illegal filters are ignored.  The
   * always-on filters will remain set, regardless of whether they are
   * set in this argument or not.
   */
  rwmemlog_filter_mask_t filter_mask
);


/*!
 * Enable additional filters for an instance.  As buffers go active,
 * they will get the current filter settings.
 */
void rwmemlog_instance_locked_filters_enable(
  /*!
   * [in] The instance to change.  Must be locked.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] The filters to enable.  No changes are made filters not
   * specified in filter_mask.  Illegal filters are ignored.
   */
  rwmemlog_filter_mask_t filter_mask
);


/*!
 * Disable filter settings for an instance.  As buffers go active,
 * they will get the current filter settings.
 */
void rwmemlog_instance_locked_filters_disable(
  /*!
   * [in] The instance to change.  Must be locked.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] The filters to disable.  No changes are made filters not
   * specified in filter_mask.  The always-on filters will remain set,
   * regardless of whether they are set in this argument or not.
   */
  rwmemlog_filter_mask_t filter_mask
);


/*! @} */

__END_DECLS



/*****************************************************************************/
/*!
 * @defgroup RwMemlogInvoke RW.Memlog Trace Invocation Macros
 * @{
 *
 * A RW.Memlog invocation is defined by the RWMEMLOG family of macros.
 * In invocation may or may not produce an actual trace entry,
 * depending on the existence of a buffer or the filter settings.  Each
 * RWMEMLOG macro takes a minimum of two parameters: a pointer to a
 * mutable rwmemlog_buffer_t pointer, and a const filter mask.
 *
 * The macros take a pointer to pointer to buffer, instead of a simple
 * pointer to buffer, because RW.Memlog uses buffer chains instead of
 * just circular buffers.  When a buffer fills up, the head of the
 * chain advances to a new buffer.  So, the macro must update the
 * buffer pointer.
 *
 * The mask defines when the invocation will be traced.  The mask MUST
 * be a compile-time constant - it will be passed to a static assertion
 * inside the macro implementation.
 *
 * In addition to the two standard arguments, each macro may include
 * several other parameters for the macro flavor, followed by up to 10
 * arbitrary argument specifications.  The argument specifications both
 * define the data to trace and how to convert the data to a string at
 * output time.
 */

/*!
 * Trace using RW.Memlog.  This macro traces a timestamp, the
 * description string, and up to 8 additional arguments
 *
 * @param [inout] bufp_ - Pointer to buffer chain pointer.
 * @param [in] mask_ - The default trace settings for this
 *   invocation.  This is a bitmask of RWMEMLOG filters.
 * @param [in] descr_ - A description string.
 * @param [in] __VA_ARGS__ - Up to 8 additional arguments.  Each
 *   argument must take the form of a RWMEMLOG_ARG expression, e.g.
 *   RWMEMLOG_ARG_CONST_STRING("str").
 */
#define RWMEMLOG( bufp_, mask_, descr_, ... ) \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_main_body, \
    (bufp_), (mask_), \
    RWMEMLOG_i_buf_check_both, \
    RWMEMLOG_i_location_inner_use_local, \
    (/*extra_test_code*/), \
    (RWMEMLOG_get_tstamp();), \
    (RW_STATIC_ASSERT(0 == (RWMEMLOG_MASK_ILLEGAL_ARG & (mask_)));), \
    RWMEMLOG_ARG_TSTAMP(), \
    RWMEMLOG_ARG_CONST_STRING(descr_), \
    __VA_ARGS__ )

/*!
 * Trace using RW.Memlog.  This macro has no default trace arguments.
 * Up to 10 trace arguments can be specified.
 *
 * @param [inout] bufp_ - Pointer to buffer chain pointer.
 * @param [in] mask_ - The default trace settings for this
 *   invocation.  This is a bitmask of RWMEMLOG filters.
 * @param [in] __VA_ARGS__ - Up to 10 additional arguments.  Each
 *   argument must take the form of a RWMEMLOG_ARG expression, e.g.
 *   RWMEMLOG_ARG_CONST_STRING("str").
 */
#define RWMEMLOG_BARE( bufp_, mask_, ... ) \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_main_body, \
    (bufp_), (mask_), \
    RWMEMLOG_i_buf_check_both, \
    RWMEMLOG_i_location_inner_use_local, \
    (/*extra_test_code*/), \
    (/*extra_fast_code*/), \
    ( \
      rwmemlog_ns_t rwmemlog_i_tstamp = 0; \
      RW_STATIC_ASSERT(0 == (RWMEMLOG_MASK_ILLEGAL_ARG & (mask_))); \
    ), \
    __VA_ARGS__ )


#ifdef __cplusplus

/*!
 * Trace the lifetime of the current scope using RW.Memlog.  Upon
 * invocation, this macro traces a start message and up to 8 additional
 * arguments.  Upon scope exit, the macro will trace the lifetime of
 * the scope using a fixed set of 3 arguments.
 *
 * This macro exists only in C++, because it relies upon a destructor
 * to trace at scope exit.
 *
 * ATTN: Implement C support using gcc local variable cleanup
 * extension.
 *
 * @param [inout] bufp_ - Pointer to buffer chain pointer.
 * @param [in] mask_ - The default trace settings for this
 *   invocation.  This is a bitmask of RWMEMLOG filters.
 * @param [in] descr_ - A description string.
 * @param [in] __VA_ARGS__ - Up to 8 additional arguments.  Each
 *   argument must take the form of a RWMEMLOG_ARG expression, e.g.
 *   RWMEMLOG_ARG_CONST_STRING("str").  These arguments are traced upon
 *   entry only.  The arguments traced upon exit are fixed.
 */
#define RWMEMLOG_TIME_SCOPE( bufp_, mask_, descr_, ... ) \
  /* Defining a location constant. */ \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_location_def_local_var, \
    RWMEMLOG_TIME_SCOPE_i_leave_args("leave: " descr_) ); \
  /* Local variable that traces on self-destruct */ \
  RwMemlogTimedEntry RWMEMLOG_i_localvar(timed_scope)( \
    (bufp_), \
    (mask_), \
    &RWMEMLOG_i_localvar(loc).loc ); \
  /* Log the start */ \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_main_body, \
    (bufp_), (mask_), \
    RWMEMLOG_i_buf_check_both, \
    RWMEMLOG_i_location_inner_use_local, \
    (/*extra_test_code*/), \
    (rwmemlog_ns_t rwmemlog_i_tstamp = RWMEMLOG_i_localvar(timed_scope).get_start();), \
    (RW_STATIC_ASSERT(0 == (RWMEMLOG_MASK_ILLEGAL_ARG & (mask_)));), \
    RWMEMLOG_ARG_TSTAMP(), \
    RWMEMLOG_ARG_CONST_STRING("enter: " descr_), \
    __VA_ARGS__ )

#define RWMEMLOG_TIME_SCOPE_i_leave_args(descr_) \
  RWMEMLOG_ARG_TSTAMP(), \
  RWMEMLOG_ARG_CONST_STRING(descr_), \
  RWMEMLOG_ARG_PRINTF_INTPTR("rtt=%" PRIu64 "us", (uint64_t)rtt/1000)

#endif /* __cplusplus */


/*!
 * Trace the lifetime of an expression using RW.Memlog.  Upon
 * invocation, this macro traces a start message and up to 8 additional
 * arguments.  Upon completion of the expression, the macro will trace
 * the lifetime of the expression using a fixed set of 3 arguments.
 *
 * @param [in] code_ - The code to time.  Must be a complete C
 *   statement or series of statements, enclosed in parentheses.
 * @param [inout] bufp_ - Pointer to buffer chain pointer.
 * @param [in] mask_ - The default trace settings for this
 *   invocation.  This is a bitmask of RWMEMLOG filters.
 * @param [in] descr_ - A description string.
 * @param [in] __VA_ARGS__ - Up to 8 additional arguments.  Each
 *   argument must take the form of a RWMEMLOG_ARG expression, e.g.
 *   RWMEMLOG_ARG_CONST_STRING("str").  These arguments are traced upon
 *   entry only.  The arguments traced upon exit are fixed.
 */
#define RWMEMLOG_TIME_CODE( code_, bufp_, mask_, descr_, ... ) \
  rwmemlog_ns_t RWMEMLOG_i_localvar(rwmemlog_i_ts_start) = rwmemlog_ns(); \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_main_body, \
    (bufp_), (mask_), \
    RWMEMLOG_i_buf_check_both, \
    RWMEMLOG_i_location_inner_use_local, \
    (/*extra_test_code*/), \
    (rwmemlog_ns_t rwmemlog_i_tstamp = RWMEMLOG_i_localvar(rwmemlog_i_ts_start);), \
    (RW_STATIC_ASSERT(0 == (RWMEMLOG_MASK_ILLEGAL_ARG & (mask_)));), \
    RWMEMLOG_ARG_TSTAMP(), \
    RWMEMLOG_ARG_CONST_STRING("invoke: " descr_), \
    __VA_ARGS__ ); \
  RWMEMLOG_i_expand code_; \
  rwmemlog_ns_t RWMEMLOG_i_localvar(rwmemlog_i_ts_end) = rwmemlog_ns(); \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_main_body, \
    (bufp_), (mask_), \
    RWMEMLOG_i_buf_check_both, \
    RWMEMLOG_i_location_inner_use_local, \
    (/*extra_test_code*/), \
    (rwmemlog_ns_t rwmemlog_i_tstamp = RWMEMLOG_i_localvar(rwmemlog_i_ts_end);), \
    (/*extra_slow_code*/), \
    RWMEMLOG_ARG_TSTAMP(), \
    RWMEMLOG_ARG_CONST_STRING("return: " descr_), \
    RWMEMLOG_ARG_PRINTF_INTPTR("rtt=%" PRIu64 "us", \
      (uint64_t)(  RWMEMLOG_i_localvar(rwmemlog_i_ts_end) \
                 - RWMEMLOG_i_localvar(rwmemlog_i_ts_start))/1000 ) )

/*!
 * Trace the lifetime of an expression using RW.Memlog.  Upon
 * completion of the expression, the macro will trace the lifetime of
 * the expression using a fixed set of 3 arguments.
 *
 * @param [inout] bufp_ - Pointer to buffer chain pointer.
 * @param [in] mask_ - The default trace settings for this
 *   invocation.  This is a bitmask of RWMEMLOG filters.
 * @param [in] descr_ - A description string.
 * @param [in] code_ - The code to time.  Must be a complete C
 *   statement or series of statements, enclosed in parentheses.
 */
#define RWMEMLOG_TIME_CODE_RTT( bufp_, mask_, descr_, code_ ) \
  rwmemlog_ns_t RWMEMLOG_i_localvar(rwmemlog_i_ts_start) = rwmemlog_ns(); \
  RWMEMLOG_i_expand code_; \
  rwmemlog_ns_t RWMEMLOG_i_localvar(rwmemlog_i_ts_end) = rwmemlog_ns(); \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_main_body, \
    (bufp_), (mask_), \
    RWMEMLOG_i_buf_check_both, \
    RWMEMLOG_i_location_inner_use_local, \
    (/*extra_test_code*/), \
    (rwmemlog_ns_t rwmemlog_i_tstamp = RWMEMLOG_i_localvar(rwmemlog_i_ts_end);), \
    (/*extra_slow_code*/), \
    RWMEMLOG_ARG_TSTAMP(), \
    RWMEMLOG_ARG_CONST_STRING("timed: " descr_), \
    RWMEMLOG_ARG_PRINTF_INTPTR("rtt=%" PRIu64 "us", \
      (uint64_t)(  RWMEMLOG_i_localvar(rwmemlog_i_ts_end) \
                 - RWMEMLOG_i_localvar(rwmemlog_i_ts_start))/1000 ) )

/*! @} */


/*****************************************************************************/
/*!
 * @defgroup RwMemlogArguments RW.Memlog Argument Macros
 * @{
 *
 * RW.Memlog trace arguments are defined using a special argument
 * syntax that looks like a macro invocation.  However, the apparent
 * argument macros do not really exist as real macros in the code.
 * Instead, the argument identifiers are used in a series of macro
 * token-pasting operations during preprocessing to extract information
 * about the argument: the meta-data, the data size, the code needed to
 * copy the data to the buffer, et cetera.
 *
 * An argument specifier starts with RWMEMLOG_ARG followed by the
 * argument type.  Each argument macro takes zero or more arguments.
 * For example, an argument that performs a printf-like conversion will
 * take a const string for the format string, and a value that will be
 * stored as data in the buffer.
 *
 * In the following example, there is a single explicit argument (as
 * well as two implicit arguments in the RWMEMLOG() implementation).
 * The RWMEMLOG_ARG_PRINTF_INTPTR argument type uses a printf
 * conversion of the data using an arbitrary format string.  Here,
 * value is a variable.
 */


/*****************************************************************************/
#ifdef RW_DOXYGEN_PARSING
/*!
 * \def RWMEMLOG_ARG_CONST_STRING(str_)
 *
 * Defines a constant string argument.  The entire human-readable
 * output will consist solely of the string str_.
 *
 * Example:
 * @code
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "example",
 *             RWMEMLOG_ARG_CONST_STRING("constant string") );
 * @endcode
 *
 * @param str_ The constant string.  Must be a C quoted string constant.
 */
#define RWMEMLOG_ARG_CONST_STRING(str_)
#endif

/*! @cond RWMEMLOG_INTERNALS */
#define RWMEMLOG_i_expand_RWMEMLOG_ARG_CONST_STRING(str_) \
  RWMEMLOG_ARG_CONST_STRING, str_

#define RWMEMLOG_ARG_CONST_STRING_loc_decl( str_, argn_ ) \
  rwmemlog_argtype_const_string_meta_t meta_ ## argn_;

#define RWMEMLOG_ARG_CONST_STRING_type_init( str_, argn_ ) \
  RWMEMLOG_type_init( \
    RWMEMLOG_ARGTYPE_CONST_STRING, \
    rwmemlog_argtype_const_string_meta_t )

#define RWMEMLOG_ARG_CONST_STRING_loc_init( str_, argn_ ) \
  { \
    { str_ "", }, \
  },

#define RWMEMLOG_ARG_CONST_STRING_const_mask( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_test_code( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_fast_filter( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_fast_code( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_sum_units( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_entry_decode( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_copy( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_slow_code( str_, argn_ )

#define RWMEMLOG_ARG_CONST_STRING_validate( str_, argn_ )
/*! @endcond */


/*****************************************************************************/
#ifdef RW_DOXYGEN_PARSING
/*!
 * \def RWMEMLOG_ARG_TSTAMP()
 *
 * Defines the standard trace timstamp.  Most RWMEMLOG macros use this
 * as the first implicit trace invocation argument.
 *
 * Example:
 * @code
 *   RWMEMLOG_BARE( bufp, RWMEMLOG_MEM2,
 *                  RWMEMLOG_ARG_TSTAMP() );
 * @endcode
 */
#define RWMEMLOG_ARG_TSTAMP()
#endif

/*! @cond RWMEMLOG_INTERNALS */
#define RWMEMLOG_i_expand_RWMEMLOG_ARG_TSTAMP() \
  RWMEMLOG_ARG_TSTAMP

#define RWMEMLOG_ARG_TSTAMP_loc_decl( argn_ ) \
  rwmemlog_argtype_func_meta_t meta_ ## argn_;

#define RWMEMLOG_ARG_TSTAMP_type_init( argn_ ) \
  RWMEMLOG_type_init( \
    RWMEMLOG_ARGTYPE_FUNC, \
    rwmemlog_argtype_func_meta_t )

#define RWMEMLOG_ARG_TSTAMP_loc_init( argn_ ) \
  { \
    { &rwmemlog_output_entry_tsns, }, \
  },

#define RWMEMLOG_ARG_TSTAMP_const_mask( argn_ )

#define RWMEMLOG_ARG_TSTAMP_test_code( argn_ )

#define RWMEMLOG_ARG_TSTAMP_fast_filter( argn_ )

#define RWMEMLOG_ARG_TSTAMP_fast_code( argn_ )

#define RWMEMLOG_ARG_TSTAMP_sum_units( argn_ ) \
  + RWMEMLOG_sizeof_units(rwmemlog_ns_t)

#define RWMEMLOG_ARG_TSTAMP_entry_decode( argn_ ) \
  | RWMEMLOG_entry_decode( RWMEMLOG_sizeof_units(rwmemlog_ns_t), argn_ )

#define RWMEMLOG_ARG_TSTAMP_copy( argn_ ) \
  *(rwmemlog_ns_t*)rwmemlog_i_p = rwmemlog_i_tstamp; \
  rwmemlog_i_p += RWMEMLOG_sizeof_units(rwmemlog_ns_t);

#define RWMEMLOG_ARG_TSTAMP_slow_code( argn_ )

#define RWMEMLOG_ARG_TSTAMP_validate( argn_ )

__BEGIN_DECLS

/*!
 * RW.Memlog output structure.
 *
 * Used to create a complete formatted string from the args 
 * in a trace entry.
 */
struct rwmemlog_output_t
{
  char* arg_eos;
  char* arg_string;
  size_t arg_left;
  size_t arg_size;
  size_t arg_index;
};

rw_status_t rwmemlog_output_entry_tsns(
  rwmemlog_output_t* output,
  size_t ndata,
  const rwmemlog_unit_t* data
);

__END_DECLS
/*! @endcond */


/*****************************************************************************/
#ifdef RW_DOXYGEN_PARSING
/*!
 * \def RWMEMLOG_ARG_STRNCPY(chars_, str_)
 *
 * Defines an argument that copies an arbitrary string into the trace
 * buffer, using a specified string size.  The entirety of the
 * specified string size will be stored in the buffer, whether the str_
 * argument needs the space or not.
 *
 * Example:
 * @code
 *   char* string = get_a_string();
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "example",
 *             RWMEMLOG_ARG_STRNCPY(24, string) );
 * @endcode
 *
 * @param n_ The maximum number of bytes to copy.  The implementation
 *    will actually round this up to the next integral multiple of
 *    rwmemlog_unit_t.
 * @param str_ The C string argument to copy.  May be mutable or const.
 *    Must not be NULL - will be passed as the second arg to strncpy().
 */
#define RWMEMLOG_ARG_STRNCPY(chars_, str_)
#endif

/*! @cond RWMEMLOG_INTERNALS */
#define RWMEMLOG_i_expand_RWMEMLOG_ARG_STRNCPY(chars_, str_) \
  RWMEMLOG_ARG_STRNCPY, chars_, str_

#define RWMEMLOG_ARG_STRNCPY_loc_decl( chars_, str_, argn_ ) \
  rwmemlog_argtype_func_meta_t meta_ ## argn_;

#define RWMEMLOG_ARG_STRNCPY_type_init( chars_, str_, argn_ ) \
  RWMEMLOG_type_init( \
    RWMEMLOG_ARGTYPE_FUNC, \
    rwmemlog_argtype_func_meta_t )

#define RWMEMLOG_ARG_STRNCPY_loc_init( chars_, str_, argn_ ) \
  { \
    { &rwmemlog_output_entry_strncpy, }, \
  },

#define RWMEMLOG_ARG_STRNCPY_const_mask( chars_, str_, argn_ )

#define RWMEMLOG_ARG_STRNCPY_test_code( chars_, str_, argn_ )

#define RWMEMLOG_ARG_STRNCPY_fast_filter( chars_, str_, argn_ )

#define RWMEMLOG_ARG_STRNCPY_fast_code( chars_, str_, argn_ )

#define RWMEMLOG_ARG_STRNCPY_sum_units( chars_, str_, argn_ ) \
  + RWMEMLOG_ARG_STRNCPY_i_units(chars_)

#define RWMEMLOG_ARG_STRNCPY_entry_decode( chars_, str_, argn_ ) \
  | RWMEMLOG_entry_decode( RWMEMLOG_ARG_STRNCPY_i_units(chars_), argn_ )

#define RWMEMLOG_ARG_STRNCPY_copy( chars_, str_, argn_ ) \
  strncpy( (char*)rwmemlog_i_p, (str_), \
           RWMEMLOG_ARG_STRNCPY_i_chars(chars_) ); \
  rwmemlog_i_p += RWMEMLOG_ARG_STRNCPY_i_units(chars_);

#define RWMEMLOG_ARG_STRNCPY_slow_code( chars_, str_, argn_ )

#define RWMEMLOG_ARG_STRNCPY_validate( chars_, str_, argn_ ) \
  const char* p_ ## argn_ = (str_); \
  (void) p_ ## argn_; \
  RW_STATIC_ASSERT_MSG( \
    (RWMEMLOG_ARG_STRNCPY_i_units(chars_) <= RWMEMLOG_ARG_SIZE_MAX_UNITS), \
    "too many units in ARG_STRNCPY arg" #argn_ );

#define RWMEMLOG_ARG_STRNCPY_i_units( chars_ ) \
  ((chars_ + sizeof(rwmemlog_unit_t) - 1) / sizeof(rwmemlog_unit_t))

#define RWMEMLOG_ARG_STRNCPY_i_chars( chars_ ) \
  (RWMEMLOG_ARG_STRNCPY_i_units(chars_) * sizeof(rwmemlog_unit_t))

__BEGIN_DECLS

rw_status_t rwmemlog_output_entry_strncpy(
  rwmemlog_output_t* output,
  size_t ndata,
  const rwmemlog_unit_t* data
);

rw_status_t rwmemlog_output_strncpy(
  rwmemlog_output_t* output,
  const char* string,
  size_t n 
);

__END_DECLS
/*! @endcond */


/*****************************************************************************/
#ifdef RW_DOXYGEN_PARSING
/*!
 * \def RWMEMLOG_ARG_STRCPY_MAX(str_, max_)
 *
 * Defines an argument that copies an arbitrary string into the trace
 * buffer, using a specified MAXIMUM string size.  Only the minimum
 * necessary space to hold the string is used, at the cost of having to
 * walk the bytes twice (once for length, once for copy).
 *
 * Example:
 * @code
 *   char* string = get_a_string();
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "example",
 *             RWMEMLOG_ARG_STRCPY_MAX(string, 24) );
 * @endcode
 *
 * @param str_ The C string argument to copy.  May be mutable or const.
 *    Must not be NULL - will be passed as the second arg to strncpy().
 * @param max_ The maximum number of bytes to copy.  The implementation
 *    will actually copy the minimum number of bytes needed to consume
 *    integral multiple of rwmemlog_unit_t.
 */
#define RWMEMLOG_ARG_STRCPY_MAX(str_, max_)
#endif

/*! @cond RWMEMLOG_INTERNALS */
#define RWMEMLOG_i_expand_RWMEMLOG_ARG_STRCPY_MAX(str_, max_) \
  RWMEMLOG_ARG_STRCPY_MAX, str_, max_

#define RWMEMLOG_ARG_STRCPY_MAX_loc_decl( str_, max_, argn_ ) \
  rwmemlog_argtype_func_meta_t meta_ ## argn_;

#define RWMEMLOG_ARG_STRCPY_MAX_type_init( str_, max_, argn_ ) \
  RWMEMLOG_type_init( \
    RWMEMLOG_ARGTYPE_FUNC, \
    rwmemlog_argtype_func_meta_t )

#define RWMEMLOG_ARG_STRCPY_MAX_loc_init( str_, max_, argn_ ) \
  { \
    { &rwmemlog_output_entry_strncpy, }, \
  },

#define RWMEMLOG_ARG_STRCPY_MAX_const_mask( str_, max_, argn_ )

#define RWMEMLOG_ARG_STRCPY_MAX_test_code( str_, max_, argn_ )

#define RWMEMLOG_ARG_STRCPY_MAX_fast_filter( str_, max_, argn_ )

#define RWMEMLOG_ARG_STRCPY_MAX_fast_code( str_, max_, argn_ ) \
  const char* RWMEMLOG_i_argvar(eos, argn_) = \
    (str_) ? (const char*)memchr( (str_), '\0', max_ ) : (str_); \
  size_t RWMEMLOG_i_argvar(units, argn_) = \
    (   (RWMEMLOG_i_argvar(eos, argn_)) \
      ? (   (   (RWMEMLOG_i_argvar(eos, argn_) - (str_)) \
              + sizeof(rwmemlog_unit_t) - 1) \
          / sizeof(rwmemlog_unit_t)) \
      : 0);

#define RWMEMLOG_ARG_STRCPY_MAX_sum_units( str_, max_, argn_ ) \
  + RWMEMLOG_i_argvar(units, argn_)

#define RWMEMLOG_ARG_STRCPY_MAX_entry_decode( str_, max_, argn_ ) \
  | RWMEMLOG_entry_decode( RWMEMLOG_i_argvar(units, argn_), argn_ )

#define RWMEMLOG_ARG_STRCPY_MAX_copy( str_, max_, argn_ ) \
  strncpy( (char*)rwmemlog_i_p, (str_), \
           RWMEMLOG_i_argvar(units, argn_) * sizeof(rwmemlog_unit_t) ); \
  rwmemlog_i_p += RWMEMLOG_i_argvar(units, argn_);

#define RWMEMLOG_ARG_STRCPY_MAX_slow_code( str_, max_, argn_ )

#define RWMEMLOG_ARG_STRCPY_MAX_validate( str_, max_, argn_ ) \
  const char* p_ ## argn_ = (str_); \
  (void) p_ ## argn_; \
  RW_STATIC_ASSERT_MSG( \
    (RWMEMLOG_ARG_STRCPY_MAX_i_max_units(max_) <= RWMEMLOG_ARG_SIZE_MAX_UNITS), \
    "too many units in ARG_STRCPY_MAX arg" #argn_ );

#define RWMEMLOG_ARG_STRCPY_MAX_i_max_units( max_ ) \
  ((max_ + sizeof(rwmemlog_unit_t) - 1) / sizeof(rwmemlog_unit_t))

#define RWMEMLOG_ARG_STRCPY_MAX_i_chars( max_ ) \
  (RWMEMLOG_ARG_STRCPY_MAX_i_max_units(max_) * sizeof(rwmemlog_unit_t))

#define RWMEMLOG_ARG_STRCPY_MAX_LIMIT \
  RWMEMLOG_ARG_SIZE_MAX_BYTES


/*! @endcond */


/*****************************************************************************/
#ifdef RW_DOXYGEN_PARSING
/*!
 * \def RWMEMLOG_ARG_PRINTF_INTPTR(format_, int_)
 *
 * Defines a printf-converted intptr_t argument.  The argument int_
 * will be stared in the buffer as an intptr_t.  At output time, the
 * argument data will be converted using snprintf with the format
 * string format_.
 *
 * Example:
 * @code
 *   #include <inttypes.h>
 *   ...
 *   intptr_t index = get_the_index();
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "example",
 *             RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRNuPTR, index) );
 * @endcode
 *
 * @param format_ The format string.  Must use a intptr_t-compatible
 *    format specifier.
 * @param int_ The value to trace.  Must be an intptr_t, or implicitly
 *    convertable to intptr_t, otherwise a compilation error or warning
 *    will result.
 */
#define RWMEMLOG_ARG_PRINTF_INTPTR(format_, int_)
#endif

/*! @cond RWMEMLOG_INTERNALS */
#define RWMEMLOG_i_expand_RWMEMLOG_ARG_PRINTF_INTPTR(format_, int_) \
  RWMEMLOG_ARG_PRINTF_INTPTR, format_, int_

#define RWMEMLOG_ARG_PRINTF_INTPTR_loc_decl( format_, int_, argn_ ) \
  rwmemlog_argtype_func_string_meta_t meta_ ## argn_;

#define RWMEMLOG_ARG_PRINTF_INTPTR_type_init( format_, int_, argn_ ) \
  RWMEMLOG_type_init( \
    RWMEMLOG_ARGTYPE_FUNC_STRING, \
    rwmemlog_argtype_func_string_meta_t )

#define RWMEMLOG_ARG_PRINTF_INTPTR_loc_init( format_, int_, argn_ ) \
  { \
    { &rwmemlog_output_entry_printf_intptr, }, \
    { format_ "", }, \
  },

#define RWMEMLOG_ARG_PRINTF_INTPTR_const_mask( format_, int_, argn_ )

#define RWMEMLOG_ARG_PRINTF_INTPTR_test_code( format_, int_, argn_ )

#define RWMEMLOG_ARG_PRINTF_INTPTR_fast_filter( format_, int_, argn_ )

#define RWMEMLOG_ARG_PRINTF_INTPTR_fast_code( format_, int_, argn_ )

#define RWMEMLOG_ARG_PRINTF_INTPTR_sum_units( format_, int_, argn_ ) \
  + RWMEMLOG_sizeof_units(intptr_t)

#define RWMEMLOG_ARG_PRINTF_INTPTR_entry_decode( format_, int_, argn_ ) \
  | RWMEMLOG_entry_decode( RWMEMLOG_sizeof_units(intptr_t), argn_ )

#define RWMEMLOG_ARG_PRINTF_INTPTR_copy( format_, int_, argn_ ) \
  *(intptr_t*)rwmemlog_i_p = (int_); \
  rwmemlog_i_p += RWMEMLOG_sizeof_units(intptr_t);

#define RWMEMLOG_ARG_PRINTF_INTPTR_slow_code( format_, int_, argn_ )

#define RWMEMLOG_ARG_PRINTF_INTPTR_validate( format_, int_, argn_ ) \
  intptr_t v_ ## argn_ = (int_); \
  (void) v_ ## argn_; \
  printf( (format_), v_ ## argn_ );

__BEGIN_DECLS

rw_status_t rwmemlog_output_entry_printf_intptr(
  rwmemlog_output_t* output,
  const char* format,
  size_t ndata,
  const rwmemlog_unit_t* data
);

__END_DECLS
/*! @endcond */


/*****************************************************************************/
#ifdef RW_DOXYGEN_PARSING
/*!
 * \def RWMEMLOG_ARG_ENUM_FUNC( func_, tag_, enumval_ )
 *
 * Defines an int-sized enumeration conversion via an enum-to-string
 * lookup function.
 *
 * Example:
 * @code
 *   extern const char* my_enum_to_string(my_enum_t e);
 *   ...
 *   my_enum_t status = do_something();
 *   RWMEMLOG( bufp, RWMEMLOG_MEM2, "example",
 *             RWMEMLOG_ARG_ENUM_FUNC( &my_enum_to_string, "status", status ) );
 * @endcode
 *
 * @param func_ A function pointer to a function that takes an int
 *    argument and returns a const C string pointer.
 * @param tag_ A constant string to "name" the argument.  May be NULL.
 * @param enumval_ The enum value to save to the buffer.
 */
#define RWMEMLOG_ARG_ENUM_FUNC( func_, tag_, enumval_ )
#endif


/*! @cond RWMEMLOG_INTERNALS */
/*
 * Argument specifier: RWMEMLOG_ARG_ENUM_FUNC(func_, tag_, enumval_)
 */
#define RWMEMLOG_i_expand_RWMEMLOG_ARG_ENUM_FUNC( func_, tag_, enumval_ ) \
  RWMEMLOG_ARG_ENUM_FUNC, func_, tag_, enumval_

#define RWMEMLOG_ARG_ENUM_FUNC_loc_decl( func_, tag_, enumval_, argn_ ) \
  rwmemlog_argtype_enum_string_meta_t meta_ ## argn_;

#define RWMEMLOG_ARG_ENUM_FUNC_type_init( func_, tag_, enumval_, argn_ ) \
  RWMEMLOG_type_init( \
    RWMEMLOG_ARGTYPE_ENUM_STRING, \
    rwmemlog_argtype_enum_string_meta_t )

#define RWMEMLOG_ARG_ENUM_FUNC_loc_init( func_, tag_, enumval_, argn_ ) \
  { \
    { (rwmemlog_argtype_enum_string_fp_t)(func_), }, \
    { tag_ "", }, \
  },

#define RWMEMLOG_ARG_ENUM_FUNC_const_mask( func_, tag_, enumval_, argn_ )

#define RWMEMLOG_ARG_ENUM_FUNC_test_code( func_, tag_, enumval_, argn_ )

#define RWMEMLOG_ARG_ENUM_FUNC_fast_filter( func_, tag_, enumval_, argn_ )

#define RWMEMLOG_ARG_ENUM_FUNC_fast_code( func_, tag_, enumval_, argn_ )

#define RWMEMLOG_ARG_ENUM_FUNC_sum_units( func_, tag_, enumval_, argn_ ) \
  + RWMEMLOG_sizeof_units(int)

#define RWMEMLOG_ARG_ENUM_FUNC_entry_decode( func_, tag_, enumval_, argn_ ) \
  | RWMEMLOG_entry_decode( RWMEMLOG_sizeof_units(int), argn_ )

#define RWMEMLOG_ARG_ENUM_FUNC_copy( func_, tag_, enumval_, argn_ ) \
  *(int*)rwmemlog_i_p = (enumval_); \
  rwmemlog_i_p += RWMEMLOG_sizeof_units(int);

#define RWMEMLOG_ARG_ENUM_FUNC_slow_code( func_, tag_, enumval_, argn_ )

#define RWMEMLOG_ARG_ENUM_FUNC_validate( func_, tag_, enumval_, argn_ ) \
  int e_ ## argn_ = (enumval_); \
  const char* s_ ## argn_ = (func_)(e_ ## argn_); \
  (void)s_ ## argn_;

__BEGIN_DECLS

rw_status_t rwmemlog_output_entry_strncpy(
  rwmemlog_output_t* output,
  size_t ndata,
  const rwmemlog_unit_t* data
);

rw_status_t rwmemlog_output_strcpy(
  rwmemlog_output_t* output,
  const char* string 
);

rw_status_t rwmemlog_output_snprintf(
  rwmemlog_output_t* output,
  const char* format,
  ... );

__END_DECLS
/*! @endcond */

/*! @} */

/*****************************************************************************/
/* IMPLEMENTATION DETAILS */
/* BE VERY CAREFUL DOWN HERE! */

/*! @cond RWMEMLOG_INTERNALS */

__BEGIN_DECLS

static inline rwmemlog_ns_t rwmemlog_ns()
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  rwmemlog_ns_t ns
    =   ((rwmemlog_ns_t)ts.tv_sec)*(rwmemlog_ns_t)1000000000
      + (rwmemlog_ns_t)ts.tv_nsec;
  return ns;
}

void rwmemlog_buffer_slow_path(
  rwmemlog_buffer_t* const* bufp,
  rwmemlog_filter_mask_t filter_mask,
  const rwmemlog_entry_t* entry,
  rwmemlog_ns_t ns
);

__END_DECLS

/*! @endcond */


/*****************************************************************************/
/*
 * General utility macros used by the RWMEMLOG implementation.
 */

/*! @cond RWMEMLOG_INTERNALS */

/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_top series of macros expand the main implementation
 * of a RW.Memlog trace invocation.  The primary purpose of this series
 * of macros is to eat the extra commas out of the variable arguments
 * list.
 */
#define RWMEMLOG_i_top( ... ) \
  RWMEMLOG_i_top_1( \
    __VA_ARGS__, \
    /* and a bunch of gratuitous extra empty args */ \
    ,,,,, ,,,,, ,,,,, ,,,,, ,,,,, ,,,,, )

#define RWMEMLOG_i_top_1( ... ) \
  RWMEMLOG_i_top_2( __VA_ARGS__ )

#define RWMEMLOG_i_top_2( \
    a00_, a01_, a02_, a03_, a04_, \
    a05_, a06_, a07_, a08_, a09_, \
    a10_, a11_, a12_, a13_, a14_, \
    a15_, a16_, a17_, a18_, a19_, \
    a20_, a21_, a22_, a23_, a24_, \
    a25_, a26_, a27_, a28_, a29_, \
    ... ) \
  RWMEMLOG_i_top_3( \
    RWMEMLOG_i_junk \
    RWMEMLOG_i_emptyize( a00_ ) \
    RWMEMLOG_i_emptyize( a01_ ) \
    RWMEMLOG_i_emptyize( a02_ ) \
    RWMEMLOG_i_emptyize( a03_ ) \
    RWMEMLOG_i_emptyize( a04_ ) \
    RWMEMLOG_i_emptyize( a05_ ) \
    RWMEMLOG_i_emptyize( a06_ ) \
    RWMEMLOG_i_emptyize( a07_ ) \
    RWMEMLOG_i_emptyize( a08_ ) \
    RWMEMLOG_i_emptyize( a09_ ) \
    RWMEMLOG_i_emptyize( a10_ ) \
    RWMEMLOG_i_emptyize( a11_ ) \
    RWMEMLOG_i_emptyize( a12_ ) \
    RWMEMLOG_i_emptyize( a13_ ) \
    RWMEMLOG_i_emptyize( a14_ ) \
    RWMEMLOG_i_emptyize( a15_ ) \
    RWMEMLOG_i_emptyize( a16_ ) \
    RWMEMLOG_i_emptyize( a17_ ) \
    RWMEMLOG_i_emptyize( a18_ ) \
    RWMEMLOG_i_emptyize( a19_ ) \
    RWMEMLOG_i_emptyize( a20_ ) \
    RWMEMLOG_i_emptyize( a21_ ) \
    RWMEMLOG_i_emptyize( a22_ ) \
    RWMEMLOG_i_emptyize( a23_ ) \
    RWMEMLOG_i_emptyize( a24_ ) \
    RWMEMLOG_i_emptyize( a25_ ) \
    RWMEMLOG_i_emptyize( a26_ ) \
    RWMEMLOG_i_emptyize( a27_ ) \
    RWMEMLOG_i_emptyize( a28_ ) \
    RWMEMLOG_i_emptyize( a29_ ) \
  )

#define RWMEMLOG_i_top_3( ... ) \
  RWMEMLOG_i_top_4( __VA_ARGS__ )

#define RWMEMLOG_i_top_4( junk_, ... ) \
  RWMEMLOG_i_top_5( __VA_ARGS__ )

#define RWMEMLOG_i_top_5( top_macro_, ... ) \
  top_macro_( __VA_ARGS__ )


/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_main_body series of macros expand the main code path
 * of an invocation into a buffer check wrapper.  The macro may also
 * produce a local variable within the invoking scope for invocations
 * that time something.  The wrapper allows the application to assume
 * (or not) the validity of the chain and/or buffer pointers, or obtain
 * the buffer pointers in an application-specific way.
 */
#define RWMEMLOG_i_main_body( ... ) \
  RWMEMLOG_i_main_body_1( __VA_ARGS__ )

#define RWMEMLOG_i_main_body_1( ... ) \
  RWMEMLOG_i_main_body_2( __VA_ARGS__ )

#define RWMEMLOG_i_main_body_2( \
    bufp_, mask_, \
    buf_check_macro_, inner_loc_macro_, \
    extra_test_code_, extra_fast_code_, extra_slow_code_, ... ) \
  RWMEMLOG_i_main_body_3( \
    buf_check_macro_, bufp_, \
    RWMEMLOG_i_inner_code( \
      mask_, inner_loc_macro_, \
      extra_test_code_, extra_fast_code_, extra_slow_code_, __VA_ARGS__ ) )

#define RWMEMLOG_i_main_body_3( buf_check_macro_, bufp_, test_code_ ) \
  buf_check_macro_( bufp_, test_code_ )


/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_inner_code series of macros expand the main code path
 * of an invocation - test path, fast path, and slow path.
 */
#define RWMEMLOG_i_inner_code( \
    mask_, inner_loc_macro_, \
    extra_test_code_, extra_fast_code_, extra_slow_code_, ... ) \
  RWMEMLOG_i_inner_code_1( \
    mask_, inner_loc_macro_, extra_test_code_, extra_fast_code_, \
    ( RWMEMLOG_i_slow_path( extra_slow_code_, __VA_ARGS__ ) ), \
    __VA_ARGS__ )

#define RWMEMLOG_i_inner_code_1( \
    mask_, inner_loc_macro_, \
    extra_test_code_, extra_fast_code_, slow_path_code_, ... ) \
  RWMEMLOG_i_inner_code_2( \
    mask_, \
    extra_test_code_, \
    ( RWMEMLOG_i_fast_path( \
        inner_loc_macro_, \
        extra_fast_code_, \
        slow_path_code_, \
        __VA_ARGS__ ) ), \
    __VA_ARGS__ )

#define RWMEMLOG_i_inner_code_2( \
    mask_, extra_test_code_, fast_path_code_, ... ) \
  ( RWMEMLOG_i_test_path( mask_, extra_test_code_, fast_path_code_, __VA_ARGS__ ) )


/*---------------------------------------------------------------------------*/
/* Buffer check wrappers */

/*
 * This macro wraps the main tracing code after checking for a buffer
 * pointer pointer and buffer pointer.  If either doesn't exist, then
 * no tracing is attempted.
 */
#define RWMEMLOG_i_buf_check_both( bufp_, test_path_code_ ) \
  do { \
    rwmemlog_buffer_t** rwmemlog_i_bufp = (bufp_); \
    if (RW_LIKELY(NULL != rwmemlog_i_bufp)) { \
      rwmemlog_buffer_t* rwmemlog_i_buf = *rwmemlog_i_bufp; \
      if (RW_LIKELY(NULL != rwmemlog_i_buf)) { \
        RWMEMLOG_i_expand test_path_code_ \
      } \
    } \
  } while(0)

/*
 * This macro wraps the main tracing code after checking for a buffer
 * pointer only; it assumes the buffer pointer pointer is valid.  If
 * the buffer does not exist, then no tracing is attempted.
 */
#define RWMEMLOG_i_buf_check_buf_only( bufp_, test_path_code_ ) \
  do { \
    rwmemlog_buffer_t** rwmemlog_i_bufp = (bufp_); \
    rwmemlog_buffer_t* rwmemlog_i_buf = *rwmemlog_i_bufp; \
    if (RW_LIKELY(NULL != rwmemlog_i_buf)) { \
      RWMEMLOG_i_expand test_path_code_ \
    } \
  } while(0)

/*
 * This macro wraps the main tracing code assuming that both the
 * buffer pointer pointer and buffer pointer are valid.
 */
#define RWMEMLOG_i_buf_check_none( bufp_, test_path_code_ ) \
  do { \
    RW_STATIC_ASSERT(0 == (RWMEMLOG_MASK_ILLEGAL_ARG & (mask_))); \
    rwmemlog_buffer_t** rwmemlog_i_bufp = (bufp_); \
    rwmemlog_buffer_t* rwmemlog_i_buf = *rwmemlog_i_bufp; \
    RWMEMLOG_i_expand test_path_code_ \
  } while(0)


/*---------------------------------------------------------------------------*/
/*
 * This macro defines the main body of the "test path".  The test path
 * code always runs if there is a valid buffer.  The test path decides
 * if the invocation will be traced.
 *
 * It is critically important that this path be as efficient as
 * possible, and that it compiles to as small of a code sequence as
 * possible.  It may look like alot, but most of the expressions in
 * this macro are constants or aliases that the compiler will remove.
 * In optimized code, this macro will typically reduce to a load, a
 * 64-bit AND and a compare.  In the future, this might also include
 * some portion of the RW.Log or RW.Trace filtering, depending on the
 * trace arguments.
 */
#define RWMEMLOG_i_test_path( mask_, extra_test_code_, fast_path_code_, ... ) \
  /* Calculate the filters and check if the fast-path should be */ \
  /* taken.  The fast path is to-memory. */ \
  const rwmemlog_filter_mask_t rwmemlog_i_const_mask \
    =   RWMEMLOG_ALSO_SLOW_ALL \
      | (mask_) \
        RWMEMLOG_i_n((arg,const_mask), __VA_ARGS__); \
  rwmemlog_filter_mask_t rwmemlog_i_skip_mask = rwmemlog_i_const_mask; \
  RWMEMLOG_i_n((arg,test_code), __VA_ARGS__) \
  RWMEMLOG_i_expand extra_test_code_ \
  rwmemlog_buffer_header_t* rwmemlog_i_hdr = &rwmemlog_i_buf->header; \
  rwmemlog_filter_mask_t rwmemlog_i_match_mask \
    =    rwmemlog_i_hdr->filter_mask \
       & (  rwmemlog_i_skip_mask \
            RWMEMLOG_i_n((arg,fast_filter), __VA_ARGS__) ); \
  if (RW_LIKELY(rwmemlog_i_match_mask & RWMEMLOG_MASK_FAST)) { \
    RWMEMLOG_i_expand fast_path_code_ \
  }


/*---------------------------------------------------------------------------*/
/*
 * This macro defines the main body of the "fast path".  The fast path
 * code runs if the invocation will be traced.  The fast path allocates
 * space in the buffer, and copies the arguments to the buffer.
 */
#define RWMEMLOG_i_fast_path( inner_loc_macro_, extra_fast_code_, slow_path_code_, ... ) \
  inner_loc_macro_( __VA_ARGS__ ) \
  \
  /* fast_code is used to do work in the fast path.  Typically */ \
  /* used to obtain the timestamp.  Might also strlen() for */ \
  /* variable-length arguments. */ \
  RWMEMLOG_i_n((arg,fast_code), __VA_ARGS__) \
  RWMEMLOG_i_expand extra_fast_code_ \
  \
  /* Allocate space in the buffer.  The size is generally a */ \
  /* compile-time constant, but some arguments might have a */ \
  /* variable length (such as keypath strings).  Even */ \
  /* variable-length items have a strict maximum in order to */ \
  /* ensure good behavior overall. */ \
  const size_t rwmemlog_i_units_needed \
    = RWMEMLOG_ENTRY_HEADER_SIZE_UNITS \
      RWMEMLOG_i_n((arg,sum_units), __VA_ARGS__); \
  uint16_t rwmemlog_i_units_left \
    = RWMEMLOG_BUFFER_DATA_SIZE_UNITS - rwmemlog_i_hdr->used_units; \
  if (RW_UNLIKELY(rwmemlog_i_units_left < rwmemlog_i_units_needed)) { \
    RW_ASSERT(rwmemlog_i_hdr->next); \
    rwmemlog_i_buf = rwmemlog_i_hdr->next; \
    *rwmemlog_i_bufp = rwmemlog_i_buf; \
    rwmemlog_i_hdr = &rwmemlog_i_buf->header; \
    rwmemlog_i_units_left = RWMEMLOG_BUFFER_DATA_SIZE_UNITS; \
  } \
  /* Need to alloc a new buffer? */ \
  if (RW_UNLIKELY( \
           (   rwmemlog_i_units_left \
            >= RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS) \
        && (   (rwmemlog_i_units_left-rwmemlog_i_units_needed) \
             < RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS) )) { \
    rwmemlog_i_match_mask |= RWMEMLOG_RUNTIME_ALLOC; \
  } \
  /* Copy the data and consume the space. */ \
  rwmemlog_entry_t* rwmemlog_i_e \
    = (rwmemlog_entry_t*)(rwmemlog_i_hdr->data + rwmemlog_i_hdr->used_units); \
  rwmemlog_i_hdr->used_units += rwmemlog_i_units_needed; \
  rwmemlog_i_e->entry_decode = 0 RWMEMLOG_i_n((arg,entry_decode), __VA_ARGS__); \
  rwmemlog_unit_t* rwmemlog_i_p = ((rwmemlog_unit_t*)&(rwmemlog_i_e)[1]); \
  (void)rwmemlog_i_p; \
  RWMEMLOG_i_n((arg,copy), __VA_ARGS__) \
  RWMEMLOG_entry_filled( rwmemlog_i_e, rwmemlog_i_loc ); \
  \
  /* Optionally, also take the slow path. */ \
  if (RW_UNLIKELY(rwmemlog_i_match_mask & RWMEMLOG_MASK_SLOW)) { \
    RWMEMLOG_i_expand slow_path_code_ \
  }


/*---------------------------------------------------------------------------*/
/*
 * This macro defines the main body of the "slow path".  The slow path
 * code runs if there are any expensive operations needed on the entry,
 * such as:
 *  - Need to mark the buffer with flags (keep, et cetera).
 *  - Need to alloc another buffer to the chain.
 *  - Need to print, send to RW.Log, or RW.Trace.
 *  - Other magic capabilities yet to be understood.
 *
 * The slow path also includes some debug-build only static assertions
 * used to validate the argument parameters.  Validations are
 * desireable because the argument values get saved in an opaque buffer
 * and are decoded by functions at output time - there is plenty of
 * chance for programming errors to result in crashes or the like.  The
 * intent of the validate code is to check the arguments to the extent
 * possible, at compile time.  None of these validations should result
 * in runtime code execution in a release build, but they are stuffed
 * in the slow path just in case.
 */
#define RWMEMLOG_i_slow_path( extra_slow_code_, ... ) \
  RWMEMLOG_i_n((arg,slow_code), __VA_ARGS__) \
  RWMEMLOG_i_expand extra_slow_code_ \
  rwmemlog_buffer_slow_path( \
    rwmemlog_i_bufp, rwmemlog_i_match_mask, \
    rwmemlog_i_e, rwmemlog_i_tstamp ); \
  \
  /* Because the argument values get saved in an opaque buffer, */ \
  /* there is some chance for bad coding.  This code validates */ \
  /* the arguments to the extent possible, at compile time. */ \
  /* It is placed in the slow path just in case errors actually */ \
  /* make it real CPU time. */ \
  RWMEMLOG_i_debug_only( \
    if (0) { \
      RWMEMLOG_i_n((arg,validate), __VA_ARGS__); \
    } \
  )


/*---------------------------------------------------------------------------*/
/*
 * This macro defines a RW.Memlog location.  A location may have a
 * separate existence outside of the fast-path because the location may
 * need to be stored in an object with dynamic lifetime.  These "outer"
 * locations are used mostly to produce round-trip-time traces.
 */
#define RWMEMLOG_i_location_def( ... ) \
  struct { \
    rwmemlog_location_t loc; \
    RWMEMLOG_i_n((arg,loc_decl), __VA_ARGS__) \
  }

#define RWMEMLOG_i_location_init( file_, line_, ... ) \
  { \
    { \
      file_, \
      line_, \
      { \
        0 RWMEMLOG_i_n((count), __VA_ARGS__), \
        0 /* spare byte, reserved for future */, \
        RWMEMLOG_i_n((arg,type_init), __VA_ARGS__) \
      }, \
    }, \
    RWMEMLOG_i_n((arg,loc_init), __VA_ARGS__) \
  }

#define RWMEMLOG_i_location_inner_use_outer(...) \
  const rwmemlog_location_t* rwmemlog_i_loc = &RWMEMLOG_i_localvar(loc).loc;

#define RWMEMLOG_i_location_inner_use_local(...) \
  static RWMEMLOG_i_constexpr RWMEMLOG_i_location_def( __VA_ARGS__) \
    rwmemlog_i_loc_full = \
      RWMEMLOG_i_location_init( RWMEMLOG_FILE(), __LINE__, __VA_ARGS__); \
  const rwmemlog_location_t* rwmemlog_i_loc = &rwmemlog_i_loc_full.loc;

#define RWMEMLOG_i_location_inner_use_member(...) \
  const rwmemlog_location_t* rwmemlog_i_loc = loc_;

#define RWMEMLOG_i_location_def_local_var( ... ) \
  static RWMEMLOG_i_constexpr RWMEMLOG_i_location_def( __VA_ARGS__) \
    RWMEMLOG_i_localvar(loc) = \
      RWMEMLOG_i_location_init( RWMEMLOG_FILE(), __LINE__, __VA_ARGS__)

#define RWMEMLOG_i_location_def_loc_member_var( loc_name_, ... ) \
  typedef RWMEMLOG_i_location_def( __VA_ARGS__) rwmemlog_t_ ## loc_name_; \
  static const rwmemlog_t_ ## loc_name_ loc_name_

#define RWMEMLOG_i_location_init_loc_member_var( class_name_, loc_name_, ... ) \
  const class_name_::rwmemlog_t_ ## loc_name_ class_name_::loc_name_ = \
    RWMEMLOG_i_location_init( RWMEMLOG_FILE(), __LINE__, __VA_ARGS__ )

#define RWMEMLOG_OBJECT_LIFETIME_DEF_MEMBERS( timer_name_, loc_name_ ) \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_location_def_loc_member_var, \
    loc_name_, \
    RWMEMLOG_TIME_SCOPE_i_leave_args(""/*no-op here*/) ); \
  RwMemlogTimedEntry timer_name_

#define RWMEMLOG_OBJECT_LIFETIME_CONSTRUCT_MEMBER( bufp_, mask_, timer_name_, loc_name_ ) \
  timer_name_( (bufp_), (mask_), &(loc_name_).loc )

#define RWMEMLOG_OBJECT_LIFETIME_INIT_LOC( class_name_, loc_name_, descr_ ) \
  RWMEMLOG_i_top( \
    RWMEMLOG_i_location_init_loc_member_var, \
    class_name_, \
    loc_name_, \
    RWMEMLOG_TIME_SCOPE_i_leave_args(descr_) )

/*
 * File string constant for RW.Memlog.  Expands to a length-limited
 * string pointer, based on the preprocessor __FILE__.
 */
#define RWMEMLOG_FILE() \
  __FILE__+(sizeof(__FILE__)>24?sizeof(__FILE__)-24:0)


/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_emptyize series of macros are designed to convert an
 * empty macro argument into nothing, and non-empty macro arguments
 * into themselves plus a leading comma.  The purpose of these macros
 * is to eat extra arguments out of an argument list, because some
 * implementation macros may produce empty arguments in the middle of
 * the argument list.
 */
#define RWMEMLOG_i_emptyize( arg_, ... ) \
    RWMEMLOG_i_emptyize_1( (arg_), RWMEMLOG_i_emptyize_check_parens arg_ )

#define RWMEMLOG_i_emptyize_1( ... ) \
  RWMEMLOG_i_emptyize_2( __VA_ARGS__ )

#define RWMEMLOG_i_emptyize_2( wrapped_arg_, found_or_not_found_, ... ) \
  RWMEMLOG_i_emptyize_3( wrapped_arg_, RWMEMLOG_i_emptyize_ ## found_or_not_found_ )

#define RWMEMLOG_i_emptyize_3( ... ) \
  RWMEMLOG_i_emptyize_4( __VA_ARGS__ )

#define RWMEMLOG_i_emptyize_4( wrapped_arg_, macro_found_or_not_found_, ... ) \
  macro_found_or_not_found_( wrapped_arg_ )

#define RWMEMLOG_i_emptyize_check_parens( ... ) RWMEMLOG_i_emptyize_check_parens_found
#define RWMEMLOG_i_emptyize_RWMEMLOG_i_emptyize_check_parens RWMEMLOG_i_emptyize_is_not_parens,
#define RWMEMLOG_i_emptyize_RWMEMLOG_i_emptyize_check_parens_found RWMEMLOG_i_emptyize_is_not_empty,

#define RWMEMLOG_i_emptyize_is_not_parens( wrapped_arg_, ... ) \
  RWMEMLOG_i_emptyize_is_not_parens_1( \
    wrapped_arg_, \
    RWMEMLOG_i_emptyize_unwrap wrapped_arg_ RWMEMLOG_i_emptyize_empty_test )

#define RWMEMLOG_i_emptyize_is_not_parens_1( ... ) \
  RWMEMLOG_i_emptyize_is_not_parens_2( __VA_ARGS__ )

#define RWMEMLOG_i_emptyize_is_not_parens_2( wrapped_arg_, empty_or_not_empty_, ... ) \
  RWMEMLOG_i_emptyize_is_not_parens_3( \
    wrapped_arg_,  \
    RWMEMLOG_i_emptyize_ ## empty_or_not_empty_, \
    RWMEMLOG_i_emptyize_is_not_empty )

#define RWMEMLOG_i_emptyize_is_not_parens_3( ... ) \
  RWMEMLOG_i_emptyize_is_not_parens_4( __VA_ARGS__ )

#define RWMEMLOG_i_emptyize_is_not_parens_4( wrapped_arg_, junk_, macro_empty_or_not_empty_, ... ) \
  macro_empty_or_not_empty_( wrapped_arg_ )

#define RWMEMLOG_i_emptyize_RWMEMLOG_i_emptyize_empty_test \
  , RWMEMLOG_i_emptyize_is_empty

#define RWMEMLOG_i_emptyize_is_empty( wrapped_arg_ )

#define RWMEMLOG_i_emptyize_is_not_empty( wrapped_arg_ ) \
  , RWMEMLOG_i_emptyize_unwrap wrapped_arg_

#define RWMEMLOG_i_emptyize_unwrap( ... ) \
  __VA_ARGS__


/*---------------------------------------------------------------------------*/
/*
 * General utility macros, otherwise uncategorized..
 */
#define RWMEMLOG_get_tstamp() \
  rwmemlog_ns_t rwmemlog_i_tstamp = rwmemlog_ns()

#define RWMEMLOG_entry_decode( units_, argn_ ) \
  ( ((uint64_t)(units_)) << (argn_ * RWMEMLOG_ENTRY_DECODE_PER_ARG_SHIFT) )

#define RWMEMLOG_sizeof_units( type_ ) \
  ( (sizeof(type_) + sizeof(rwmemlog_unit_t) - 1) / sizeof(rwmemlog_unit_t) )

#define RWMEMLOG_type_init( argtype_, metadata_type_ ) \
  (  (    (   (argtype_) \
            & RWMEMLOG_META_DECODE_ARGTYPE_MASK) \
       << RWMEMLOG_META_DECODE_ARGTYPE_SHIFT) \
   | (    (   (sizeof(metadata_type_)/sizeof(intptr_t)) \
            & RWMEMLOG_META_DECODE_META_COUNT_MASK) \
       << RWMEMLOG_META_DECODE_META_COUNT_SHIFT) ),

#define RWMEMLOG_entry_filled( entry_, loc_ ) \
  /* ATTN: barrier? write-release(e->l, l)? */ \
  ( (entry_)->location = (loc_) )

#define RWMEMLOG_i_expand(...) __VA_ARGS__
#define RWMEMLOG_i_expand_macro(a, ...) a(__VA_ARGS__)

// ATTN: Needs debug check
#define RWMEMLOG_i_debug_only(...) __VA_ARGS__

#define RWMEMLOG_i_junk(...)

#ifdef __cplusplus
#define RWMEMLOG_i_constexpr constexpr
#else
#define RWMEMLOG_i_constexpr const
#endif


/*---------------------------------------------------------------------------*/
/*
 * Create a local variable from a tag and the line number.  Needed by
 * some RW.Memlog macros that create local variables in the macro's
 * invoking scope.
 */
#define RWMEMLOG_i_localvar(v_) RWMEMLOG_i_localvar_1(v_,__LINE__)
#define RWMEMLOG_i_localvar_1(v_,l_) RWMEMLOG_i_localvar_2(v_,l_)
#define RWMEMLOG_i_localvar_2(v_,l_) rwmemlog_i_ ## v_ ## _ ## l_

#define RWMEMLOG_i_argvar(v_, n_) RWMEMLOG_i_argvar_1(v_, n_ ,__LINE__)
#define RWMEMLOG_i_argvar_1(v_, n_, l_) RWMEMLOG_i_argvar_2(v_, n_, l_)
#define RWMEMLOG_i_argvar_2(v_, n_, l_) rwmemlog_i_ ## v_ ## _ ## n_ ## _ ## l_


/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_n series of macros expands the list of RWMEMLOG trace
 * arguments into various code fragments.  The expansion happens in
 * several stages.  These top-level stages have several goals:
 *  - Determine which arguments are missing and produce nothing for them.
 *  - Verify that the number of arguments is not too big.
 *  - Pass the argument-specific data along to the operation expansion
 *    macros (which expand to the operation's code fragment).
 */
#define RWMEMLOG_i_n( args_, ... ) \
  RWMEMLOG_i_n_1( \
    RWMEMLOG_i_n_is_zero(__VA_ARGS__), \
    args_, \
    __VA_ARGS__, \
    RWMEMLOG_i_junk, RWMEMLOG_i_junk, RWMEMLOG_i_junk, RWMEMLOG_i_junk, RWMEMLOG_i_junk, \
    RWMEMLOG_i_junk, RWMEMLOG_i_junk, RWMEMLOG_i_junk, RWMEMLOG_i_junk, RWMEMLOG_i_junk, \
    RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, \
    RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, RWMEMLOG_i_n_do, \
    RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, \
    RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip, RWMEMLOG_i_n_skip )

#define RWMEMLOG_i_n_1( empty_or_not_, args_, \
    a0_, a1_, a2_, a3_, a4_, \
    a5_, a6_, a7_, a8_, a9_, \
    junk_0_, junk_1_, junk_2_, junk_3_, junk_4_, \
    junk_5_, junk_6_, junk_7_, junk_8_, junk_9_, \
    do_0_, do_1_, do_2_, do_3_, do_4_, \
    do_5_, do_6_, do_7_, do_8_, do_9_, \
    verify_count_, ... ) \
  empty_or_not_( \
    RWMEMLOG_i_n_2( \
      a0_, a1_, a2_, a3_, a4_, \
      a5_, a6_, a7_, a8_, a9_, \
      do_0_, do_1_, do_2_, do_3_, do_4_, \
      do_5_, do_6_, do_7_, do_8_, do_9_, \
      RWMEMLOG_i_expand args_ ) ) \
  RWMEMLOG_i_expand_macro( RWMEMLOG_i_n_ARGUMENT_COUNT_ERROR_ ## verify_count_ )

#define RWMEMLOG_i_n_2( \
    a0_, a1_, a2_, a3_, a4_, \
    a5_, a6_, a7_, a8_, a9_, \
    do_0_, do_1_, do_2_, do_3_, do_4_, \
    do_5_, do_6_, do_7_, do_8_, do_9_, \
    ... ) \
  do_0_( RWMEMLOG_i_n_3( __VA_ARGS__, a0_, 0 ) ) \
  do_1_( RWMEMLOG_i_n_3( __VA_ARGS__, a1_, 1 ) ) \
  do_2_( RWMEMLOG_i_n_3( __VA_ARGS__, a2_, 2 ) ) \
  do_3_( RWMEMLOG_i_n_3( __VA_ARGS__, a3_, 3 ) ) \
  do_4_( RWMEMLOG_i_n_3( __VA_ARGS__, a4_, 4 ) ) \
  do_5_( RWMEMLOG_i_n_3( __VA_ARGS__, a5_, 5 ) ) \
  do_6_( RWMEMLOG_i_n_3( __VA_ARGS__, a6_, 6 ) ) \
  do_7_( RWMEMLOG_i_n_3( __VA_ARGS__, a7_, 7 ) ) \
  do_8_( RWMEMLOG_i_n_3( __VA_ARGS__, a8_, 8 ) ) \
  do_9_( RWMEMLOG_i_n_3( __VA_ARGS__, a9_, 9 ) )

#define RWMEMLOG_i_n_3( arg_, ... ) \
  RWMEMLOG_i_n_4( RWMEMLOG_i_n_op_ ## arg_, __VA_ARGS__ )

#define RWMEMLOG_i_n_4( macro_, ... ) \
  macro_( __VA_ARGS__ )


#define RWMEMLOG_i_n_do(...) __VA_ARGS__
#define RWMEMLOG_i_n_skip(...)

#define RWMEMLOG_i_n_ARGUMENT_COUNT_ERROR_RWMEMLOG_i_n_skip(...)
#define RWMEMLOG_i_n_ARGUMENT_COUNT_ERROR_RWMEMLOG_i_n_do(...) RWMEMLOG_i_n_ARGUMENT_COUNT_ERROR
#define RWMEMLOG_i_n_ARGUMENT_COUNT_ERROR_RWMEMLOG_i_n_junk(...) RWMEMLOG_i_n_ARGUMENT_COUNT_ERROR


/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_n_is_zero series of macros tries to determine if the
 * number of trace arguments is zero.  Depending on whether the number
 * of arguments is zero or not, this series of macros expands to the
 * NAME of another macro.  The output macro either produces code for
 * the arguments, or completely suppresses the code expansion.  No
 * arguments - no code.
 */
#define RWMEMLOG_i_n_is_zero( ... ) \
    RWMEMLOG_i_n_is_zero_1( RWMEMLOG_i_n_is_zero_map_parens __VA_ARGS__, __VA_ARGS__ )

#define RWMEMLOG_i_n_is_zero_map_parens( ... ) \
  RWMEMLOG_i_n_is_zero_map_parens_found

#define RWMEMLOG_i_n_is_zero_1( ... ) \
  RWMEMLOG_i_n_is_zero_2( __VA_ARGS__ )

#define RWMEMLOG_i_n_is_zero_2( x_, ... ) \
  RWMEMLOG_i_n_is_zero_3( RWMEMLOG_i_n_ ## x_, __VA_ARGS__ )

#define RWMEMLOG_i_n_RWMEMLOG_i_n_is_zero_map_parens RWMEMLOG_i_n_is_zero_5,
#define RWMEMLOG_i_n_RWMEMLOG_i_n_is_zero_map_parens_found RWMEMLOG_i_n_is_zero_not_empty,

#define RWMEMLOG_i_n_is_zero_3( ... ) \
  RWMEMLOG_i_n_is_zero_4( __VA_ARGS__ )

#define RWMEMLOG_i_n_is_zero_4( macro_, junk_, ... ) \
  macro_( RWMEMLOG_i_n_zero_test __VA_ARGS__ () )

#define RWMEMLOG_i_n_is_zero_not_empty(...) RWMEMLOG_i_n_varargs_is_not_empty

#define RWMEMLOG_i_n_zero_test(...) RWMEMLOG_i_n_varargs_is_empty

#define RWMEMLOG_i_n_is_zero_5( ... ) \
  RWMEMLOG_i_n_is_zero_6( __VA_ARGS__ )

#define RWMEMLOG_i_n_is_zero_6( x_, ... ) \
  RWMEMLOG_i_n_is_zero_7( RWMEMLOG_i_n_zero_test_ ## x_, RWMEMLOG_i_junk )


#define RWMEMLOG_i_n_zero_test_RWMEMLOG_i_n_varargs_is_empty RWMEMLOG_i_n_varargs_is_empty
#define RWMEMLOG_i_n_zero_test_RWMEMLOG_i_n_zero_test RWMEMLOG_i_n_varargs_is_not_empty,

#define RWMEMLOG_i_n_is_zero_7( x_, ... ) \
  RWMEMLOG_i_n_is_zero_8( x_, RWMEMLOG_i_junk )

#define RWMEMLOG_i_n_is_zero_8( x_, ... ) \
  x_

#define RWMEMLOG_i_n_varargs_is_empty(x_)

#define RWMEMLOG_i_n_varargs_is_not_empty(...) \
  __VA_ARGS__


/*---------------------------------------------------------------------------*/
/*
 * The RWMEMLOG_i_n_op_arg series of macros expand a single argument
 * applied to a single operation.  The result of this expansion may be
 * nothing, or it may be a code fragment.
 */
#define RWMEMLOG_i_n_op_arg( op_, arg_, n_ ) \
  RWMEMLOG_i_n_op_arg_1( op_, RWMEMLOG_i_expand_ ## arg_, n_ )

#define RWMEMLOG_i_n_op_arg_1( op_, ... ) \
  RWMEMLOG_i_n_op_arg_2( op_, __VA_ARGS__ )

#define RWMEMLOG_i_n_op_arg_2( op_, type_, ... ) \
  RWMEMLOG_i_n_op_arg_3( type_ ## _ ## op_, __VA_ARGS__ )

#define RWMEMLOG_i_n_op_arg_3( ... ) \
  RWMEMLOG_i_n_op_arg_4( __VA_ARGS__ )

#define RWMEMLOG_i_n_op_arg_4( macro_, ... ) \
  macro_( __VA_ARGS__ )


/* This macro is used with RWMEMLOG_i_n to count the number of arguments */
#define RWMEMLOG_i_n_op_count(...) +1

/*! @endcond */


/*****************************************************************************/

#ifdef __cplusplus
#ifndef RWMEMLOG_PP_TESTING

/*!
 * @addtogroup RwMemlogInstance
 * @{
 */

/*!
 * RW.Memlog instance wrapper.  Applies RAII to rwmemlog_instance_t.
 */
class RwMemlogInstance
{
 public:
  /*!
   * Allocate and initialize a new RW.Memlog instance.
   */
  explicit RwMemlogInstance(
    /*!
     * [in] Description string.  Copied into instance, so does not need
     * to survive this call.
     */
    const char* descr,

    /*!
     * [in] Initial number of buffers to allocate.  If 0, a default value
     * will be used.
     */
    uint32_t minimum_retired_count = 0,

    /*!
     * [in] The maximum number of buffers to keep retired.  Once this
     * limit is reached, new keeps will release older keeps back to the
     * regular retired pool.
     */
    uint32_t maximum_keep_count = 0 )
  {
    instance_ = rwmemlog_instance_alloc(
      descr, minimum_retired_count, maximum_keep_count);
  }

  /*!
   * Destroy the RW.Memlog instance.
   */
  ~RwMemlogInstance()
  {
    destroy();
  }

  /*!
   * Convert to raw C instance pointer.
   */
  operator rwmemlog_instance_t* () const
  {
    return instance_;
  }

  /*!
   * Convert to const raw C instance pointer.
   */
  operator const rwmemlog_instance_t* () const
  {
    return instance_;
  }

  /*!
   * Indirection operator.
   */
  rwmemlog_instance_t* operator->()
  {
    return instance_;
  }

  /*!
   * Const indirection operator.
   */
  const rwmemlog_instance_t* operator->() const
  {
    return instance_;
  }

  /*!
   * Get the raw C instance pointer.  Better to use this than the
   * conversion operators in explicit code.
   */
  rwmemlog_instance_t* get() const
  {
    return instance_;
  }

  /*!
   * Release the raw C instance pointer from this object, transferring
   * ownership to the caller.
   */
  rwmemlog_instance_t* release()
  {
    auto retval = instance_;
    instance_ = nullptr;
    return retval;
  }

  /*!
   * Destroy the instance explicitly.
   */
  void destroy()
  {
    if (instance_) {
      rwmemlog_instance_destroy(instance_);
      instance_ = nullptr;
    }
  }

 private:
  RwMemlogInstance() = delete;
  RwMemlogInstance(const RwMemlogInstance&) = delete;
  RwMemlogInstance& operator=(const RwMemlogInstance&) = delete;

  rwmemlog_instance_t* instance_;
};

/*! @} */


/*!
 * @addtogroup RwMemlogBuffer
 * @{
 */

/*!
 * RW.Memlog buffer wrapper.  Applies RAII to rwmemlog_buffer_t.
 */
class RwMemlogBuffer
{
 public:

  /*!
   * Obtain a new buffer chain for the associated object.
   */
  RwMemlogBuffer(
    /*!
     * [in] The instance to allocate from.
     */
    rwmemlog_instance_t* instance,
    /*!
     * [in] A constant string that describes the object doing the tracing.
     * The pointer to this string will be saved in the buffer, and the
     * buffer may live indeterminately long.  Therefore, this string MUST
     * be a constant!  Must not be NULL.
     */
    const char* object_name,
    /*!
     * [in] An arbitrary value used to identify the object instance.
     * Typical values are a pointer or integer identifier.
     */
    intptr_t object_id = 0 )
  {
    if (instance) {
      buffer_ = rwmemlog_instance_get_buffer( instance, object_name, object_id );
    }
  }

  /*!
   * Return all buffers associated with an object.  This API is used when
   * an object gets destroyed.  The buffers will be retired, and reused
   * after an indeterminate period of time.
   */
  ~RwMemlogBuffer()
  {
    return_all();
  }

  /*!
   * Convert to raw C buffer pointer.
   */
  operator rwmemlog_buffer_t* () const
  {
    return buffer_;
  }

  /*!
   * Convert to const raw C buffer pointer.
   */
  operator const rwmemlog_buffer_t* () const
  {
    return buffer_;
  }

  /*!
   * Convert to raw C buffer pointer pointer.  This conversion is quite
   * useful for the main RWMEMLOG() macros.
   */
  operator rwmemlog_buffer_t** ()
  {
    return &buffer_;
  }

  /*!
   * Get address of the raw C buffer pointer pointer.  This conversion
   * is useful for the main RWMEMLOG() macros.
   */
  rwmemlog_buffer_t** operator&()
  {
    return &buffer_;
  }

  /*!
   * Indirection operator.
   */
  rwmemlog_buffer_t* operator->()
  {
    return buffer_;
  }

  /*!
   * Const indirection operator.
   */
  const rwmemlog_buffer_t* operator->() const
  {
    return buffer_;
  }

  /*!
   * Get the raw C buffer pointer.  Better to use this than the
   * conversion operators in explicit code.
   */
  rwmemlog_buffer_t* get() const
  {
    return buffer_;
  }

  /*!
   * Get a pointer to the raw C buffer pointer.  This can be used for
   * explicit parameters to the RWMEMLOG macros.
   */
  rwmemlog_buffer_t** ptr()
  {
    return &buffer_;
  }

  /*!
   * Release the raw C buffer pointer from this object, transferring
   * ownership to the caller.
   */
  rwmemlog_buffer_t* release()
  {
    auto retval = buffer_;
    buffer_ = nullptr;
    return retval;
  }

  /*!
   * Release the raw C buffer pointer from this object, transferring
   * ownership to the caller.
   */
  void return_all()
  {
    if (buffer_) {
      rwmemlog_buffer_return_all(buffer_);
      buffer_ = nullptr;
    }
  }

 private:
  RwMemlogBuffer() = delete;
  RwMemlogBuffer(const RwMemlogBuffer&) = delete;
  RwMemlogBuffer& operator=(const RwMemlogBuffer&) = delete;

  rwmemlog_buffer_t* buffer_ = nullptr;
};

/*! @} */

#endif /* ndef RWMEMLOG_PP_TESTING */


/*! @cond RWMEMLOG_INTERNALS */

/*
 * RW.Memlog scoped timer.  This traces on construction and on
 * destruction.  Optionally can trace "destruction" early.
 */
class RwMemlogTimedEntry
{
 public:

  RwMemlogTimedEntry(
    rwmemlog_buffer_t** bufp,
    rwmemlog_filter_mask_t mask,
    const rwmemlog_location_t* loc )
  : bufp_(bufp),
    mask_(mask),
    loc_(loc),
    start_(rwmemlog_ns())
  {}

  ~RwMemlogTimedEntry()
  {
    done();
  }

  rwmemlog_ns_t get_start() const
  {
    return start_;
  }

  void done()
  {
    if (bufp_) {
      rwmemlog_ns_t rwmemlog_i_tstamp = rwmemlog_ns();
      rwmemlog_ns_t rtt = rwmemlog_i_tstamp - start_;
      RWMEMLOG_i_top(
        RWMEMLOG_i_main_body,
        bufp_,
        mask_,
        RWMEMLOG_i_buf_check_buf_only,
        RWMEMLOG_i_location_inner_use_member,
        (/*extra_test_code*/),
        (/*extra_fast_code*/),
        (/*extra_slow_code*/),
        RWMEMLOG_TIME_SCOPE_i_leave_args(""/*no-op: loc was pre-defined*/) );
      bufp_ = nullptr;
    }
  }

 private:
  RwMemlogTimedEntry() = delete;
  RwMemlogTimedEntry(const RwMemlogTimedEntry&) = delete;
  RwMemlogTimedEntry& operator=(const RwMemlogTimedEntry&) = delete;

  rwmemlog_buffer_t** bufp_;
  const rwmemlog_filter_mask_t mask_;
  const rwmemlog_location_t* loc_;
  const rwmemlog_ns_t start_;
};




/*! @endcond */

#endif /* __cplusplus */


/*****************************************************************************/
/* preprocessor test examples */

/*! @cond RWMEMLOG_INTERNALS */

#ifdef RWMEMLOG_PP_TESTING
//#undef RWMEMLOG_i_main_body_3

RWMEMLOG_BARE( bufp, RWMEMLOG_MEM4 );

RWMEMLOG( bufp, RWMEMLOG_MEM0, "foo",
          RWMEMLOG_ARG_PRINTF_INTPTR("%d",1),
          RWMEMLOG_ARG_STRNCPY(12,"yellow dog") );

RWMEMLOG_TIME_SCOPE( bufp, RWMEMLOG_LOPRI, "foo" );

RWMEMLOG_TIME_CODE(
  (int i = 3;),
  bufp, RWMEMLOG_LOPRI, "foo" );

RWMEMLOG( bufp, RWMEMLOG_MEM0, "no extra args" );

RWMEMLOG( bufp, RWMEMLOG_MEM0, "no extra args",, );



#endif /* def RWMEMLOG_PP_TESTING */

/*! @endcond */


#endif /* ndef __GI_SCANNER__ */
#endif /* RWMEMLOG_H_ */
