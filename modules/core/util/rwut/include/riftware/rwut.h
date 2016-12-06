
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
 * @file rwut.h
 * @author Andrew Golovanov (andrew.golovanov@riftio.com)
 * @author Austin Cormier (austin.cormier@riftio.com)
 * @date 04/10/2014
 * @brief Top level include for RIFT unit test framework and benchmarking
 */

#ifndef __RWUT_H__
#define __RWUT_H__

#include <iostream>
#include <time.h>  //timespec
#include <algorithm>
#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"
#include "timespec.h"

/**
 * The environment variable that when set, only takes a single pass through benchmarks.
*/
#define PERF_MODE_ENV_VAR  "ENABLE_PERF_TEST"

/*! \mainpage RWUT (RIFTio Unit Test Framework)

This is the description of the RWUT (RIFT.Ware Unit Testing Framework) including a brief API reference.

# 1. Writing a Google Unit Test

## Google Test Framework Documentation
The Rift Unit Test Framework is build on top of the Google Unit Test Framework.  It would behoove developers then to first take a look at <a href="http://code.google.com/p/googletest/wiki/Primer">Google Test Framework Primer</a> before getting started.

Google Test Framework is a popular C/C++ unit testing framework with a large community.  It has many features to simplify boilerplate and ensure developers focus on what matters most...writing unit tests!
Some notable features:
* - Automatic test case enumeration.
*   - Not neccessary to define a main() and list all unit test within a module.
* - A large assortment of out of the box <a href="https://code.google.com/p/googletest/wiki/V1_7_Primer#Assertions">assertions</a>.
* - <a href="https://code.google.com/p/googletest/wiki/Primer#Test_Fixtures:_Using_the_Same_Data_Configuration_for_Multiple_Te">Test Fixtures</a> - Using the same configuration for multiple tests.

## Test Suite Initialization

SetUp() and TearDown() methods in fixture class are executed before/after every individual test. Sometimes you will need to perform initialization steps for the entire test suite. For example, you may need to create a database or configure an environment that will be shared by all tests.

There are three steps to invoke such custom initialization before running a test suite:
- Add your **initialization function** to the testing code with the following signature (you can use any name instead of "myInit"):
@code
   void myInit(int argc, char** argv);
@endcode
- Call the **RWUT_INIT** macro with the actual name of your initialization function as its argument:
@code
   RWUT_INIT(myInit);
@endcode
- Use **ADD_SETUP** in a call to rift_unittest_build() in cmake.

@note   - Call to RWUT_INIT macro may remain in your code forever: it will be just ignored if you do not use ADD_SETUP. In other words, you activate custom initialization in cmake only. But if you have activated it in cmake, you also have to make sure that RWUT_INIT is called in your source code (of course, with a valid function name), otherwise your testing code will fail to build.

# 2. Adding the Unit Test to CMake

## Building the Unit Test
In cmake, all you need to do is to include a call to the rift_unittest_build() function, which will build an executable unit test target.

@code
  rift_unittest_build(tname
    #ADD_SETUP             # uncomment to add a setup procedure into main()
    TEST_SRCS tests/toysched_utest.cpp
    TEST_LIBS rwsched dispatch BlocksRuntime glib-2.0 rwlib talloc
    DEPENDS rwsched libdispatch-1.0 CF-744.12 glib-2.0 rwlib talloc
  )
@endcode

@param  test_name              Name unit testing target that will be built by this function. 
@param  ADD_SETUP              Use this option if you need to use a custom initialization step for the test suite in performance testing mode
@param  TEST_SRCS              List of source files for the build target(s)
@param  TEST_LIBS              Built libraries to link with
@param  DEPENDS              Dependencies

## Running the Unit Test
### Manually
When developing a unittest, it makes sense to run the generated test manually before worrying about incorporating it into the 'make unittest' target.  This will give you a much faster compile-test cycle and give you an easier opportunity to debug with GDB.

As an example, for a unit test defined in core/rwvx/rwsched using the rift_unittest_build(), the test executable can
be found in this location: <root>/.build/modules/core/rwvx/src/core_rwvx-build/rwsched.  On top of being able to execute it here, you can also rebuild quickly using 'make'.

### Automatically as part of `make unittest`
Build the unittest is not sufficient to add the unittest to the list of tests that get invoked
during a build.  This step is neccessary to add your unittest and defined the various execution
parameters.

@note  -  This will be combined with the rift_unittest_build soon. So defining a unittest will schedule it
to be run as part of the build as well.

@code
    rift_unittest(test_name
      TEST_EXE ${CMAKE_CURRENT_BINARY_DIR}/rwsched_unittest
      TEST_ARGS --gtest_output=xml:${RIFT_UNITTEST_DIR}/rwsched/unittest.xml
      TEST_OUTDIR ${RIFT_UNITTEST_DIR}/rwsched
      TEST_OUTFILE unittest.txt
      DEPENDS rwsched_unittest
    )
@endcode

@param  test_name              Name unit testing target that will be built by this function. 
@param  TEST_EXE               The directory where the unit test is located after build.
@param  TEST_ARGS              Arguments that should be passed into the unit test invocation.
@param  TEST_OUTDIR            Directory where the unit test results are placed (ensures directory is created)
@param  TEST_OUTFILE           Output file where the unit test stdout/stderr is captured.
@param  DEPENDS                Unit test build target which we depend to be created before executing.

# 3. Benchmark Facilities
## Enabling Performance Tests
By default, benchmarks within a unit test will only be run for a single iteration and the benchmark results will not be recorded or written to stdout.  This is to ensure the default behavior for running unit tests is functional testing.

To enable performance tests, define the ENABLE_PERF_TEST environment variable before running the unit test.
@code
(bash)# export ENABLE_PERF_TEST=1
(csh)# setenv ENABLE_PERF_TEST 1
@endcode 

## Benchmark Example

@code
// Let's insert 100 elements at the back of a vector
RWUT_BENCH_ITER(insertBackVector, 5, 100);
    vector<int> v;
    v.insert(v.end(), i);
RWUT_BENCH_END(insertBackVector);
@endcode
This example runs the insert operations for 100 iterations, and does this for 5 runs.  The iterations help improve the signal to noise ratio between what we are benchmarking and the runs average out the time.


## Benchmarking Macros
@code
//Benchmark code section for a number of iterations.
RWUT_BENCH_ITER(name, num_runs, iters_per_run);

// Benchmark code section for <iter_ms> milliseconds.
RWUT_BENCH_MS(name, iter_ms);

// Benchmark a section while <bool_statement> is True.
RWUT_BENCH_WHILE_TRUE(name, bool_statement);

// Finish the benchmark code section.
RWUT_BENCH_END(name);

// Record a benchmark specific property and value
RWUT_BENCH_RECORD_PROPERTY(name, var, value);

// Record a benchmark specific property and value
RWUT_BENCH_RECORD_VARIABLE(name, var);

// Get a benchmark specific variable
RWUT_BENCH_VAR(name, var);
@endcode
 */

// Do not document these private macros.
/// @cond
//
//
//
static inline void to_human_readable(double value, double *coeff, const char **units)
{
    // Static lookup table of byte-based SI units
    static const char *suffix[] = {"", "K", "M", "G", "T", "E", "Z", "Y", "ERR!!"};
    if (value < 10000)
    {
      *coeff = value;
      *units = "";
      return;
    }

    int exp = log(value) / log(1000);
    *coeff = round(value / pow(1000, exp));
    *units  = suffix[exp];
}

#define _RWUT_IS_PERF_TEST_MODE()                                           \
    (getenv(PERF_MODE_ENV_VAR) != NULL)

#define _RWUT_BENCH_COMMON_VARS(name)                                       \
    timespec name##start_time;                                              \
    timespec name##end_time;                                                \
    timespec name##total_run_time = {0};                                    \
    timespec name##time_per_run = {0};                                      \
    unsigned int name##total_iterations = 0;                                \
    unsigned int name##total_runs = 0;                                      \
    bool name##is_perf_test = _RWUT_IS_PERF_TEST_MODE();

#define _RWUT_BENCH_RUN_VARS(name)                                          \
    timespec name##run_start_time;                                          \
    timespec name##run_stop_time;                                           \
    clock_gettime(CLOCK_REALTIME, &name##run_start_time);

#define _RWUT_PRINT_MODE_NOTE()                                             \
    if (! _RWUT_IS_PERF_TEST_MODE())                                        \
        std::cout << "Note: " PERF_MODE_ENV_VAR " Environment Variable is NOT Defined. Only running a single benchmark iteration." << std::endl;

#define _RWUT_BENCH_COMMON_START(name)                                      \
    _RWUT_PRINT_MODE_NOTE();                                                \
    auto name##line_drawer = RWUTDrawLineOnDestruction();                   \
    clock_gettime(CLOCK_REALTIME, &name##start_time);                       \
/**
 *  Macro to draw a horizontal line for better looking benchmark results.  Only works during perf mode.
 */
#define RWUT_BENCH_DRAW_LINE()                                                          \
      if (_RWUT_IS_PERF_TEST_MODE())                                                    \
        std::cout << "            -------------------------------------" << std::endl;

/** Draws line on destruction to help format benchmark results.  
 *  Does not workwell with multiple benchmarks but beautifies the common case.
 */
class RWUTDrawLineOnDestruction {
  public:
    RWUTDrawLineOnDestruction() {}
    ~RWUTDrawLineOnDestruction() { 
      RWUT_BENCH_DRAW_LINE(); 
      std::cout << std::endl; 
    }
};

/// @endcond

/**
 * Macro to set a custom test suite initialization function.
 * @param[in] myInit - custom initialization function which takes int, char** as parameters.
 *
 * Example Usage:
 * @code
 * void myInit(int argc, char** argv){<init routine>};
 * RWUT_INIT(myInit);
 * @endcode
 * Note: RWUT_INIT is only functional if the ADD_SETUP argument is used in
 * the rift_unittest_build() CMake function.
 */
#ifdef TEST_SETUP
#define RWUT_INIT(myInit)                    \
  int main(int argc, char** argv) {          \
     _RWUT_PRINT_MODE_NOTE();                \
    ::testing::InitGoogleMock(&argc, argv);  \
    myInit(argc, argv);                      \
    return RUN_ALL_TESTS();                  \
  }
#else
    #define RWUT_INIT(myInit)                    
#endif


/**
 * Macro to benchmark a code section within a Google Test block 
 * for a number of milliseconds.
 * @param[in] name - benchmark name
 * @param[in] milliseconds - number of milliseconds to iterate
 *
 * Example Usage:
 * @code
 * TEST(testcase_name, test_name)
 * {
 *   RIFT_BENCH_MS(bench_name, 100);
 *      <code to profile for 100ms>
 *   RIFT_BENCH_END(bench_name);
 * }
 * @endcode
 */
#define RWUT_BENCH_MS(name, milliseconds)                                   \
    _RWUT_BENCH_COMMON_VARS(name);                                          \
    _RWUT_BENCH_COMMON_START(name);                                         \
                                                                            \
    while (true) {                                                          \
        timespec name##now_time;                                            \
        clock_gettime(CLOCK_REALTIME, &name##now_time);                     \
        timespec name##diff_time = timespec_diff(name##start_time, name##now_time); \
        long int name##diff_ns = timespec_to_ns(name##diff_time);           \
        /* If we've been running for over <second> seconds, then break.*/   \
        if ((name##diff_ns / NS_PER_MS) >= milliseconds)                    \
            break;                                                          \
                                                                            \
        _RWUT_BENCH_RUN_VARS(name);                                         \
        /* Need to open brace for iter loop */                              \
       {                                                                    \
            /* Force total_runs to stay at 1 for a final result */          \
            name##total_runs = 0;


/**
 * Macro to benchmark a code section for a number of runs and iterations
 * per run.
 * @param[in] name - benchmark name
 * @param[in] num_runs - number of runs to run the iterations
 * @param[in] iters_per_run - number of iterations to run the code section
 *  per run.
 *
 * Example Usage:
 * @code
 * TEST(testcase_name, test_name)
 * {
 *   RIFT_BENCH_ITER(bench_name, 2, 5);
 *      <code to profile for 2 runs of 5 iterations each>
 *   RIFT_BENCH_END(bench_name);
 * }
 * @endcode
 */
#define RWUT_BENCH_ITER(name, num_runs, iters_per_run)                      \
                                                                            \
    _RWUT_BENCH_COMMON_VARS(name);                                          \
    _RWUT_BENCH_COMMON_START(name);                                         \
                                                                            \
    for (int runs = num_runs; runs > 0; runs -= 1)                          \
    {                                                                       \
       _RWUT_BENCH_RUN_VARS(name);                                          \
       for (int name##iters = iters_per_run; name##iters > 0; name##iters--)\
       {


/**
 * Macro to benchmark a code section while bool_statement is True.
 * @param[in] name - benchmark name
 * @param[in] bool_statement - C/C++ expression that is evaluated on each
 * iteration to check whether or not to continue. 
 *
 * Example Usage:
 * @code
 * TEST(testcase_name, test_name)
 * {
 *   int x = 0;
 *   RIFT_BENCH_WHILE_TRUE(bench_name, (x < 5));
 *      <code to profile for 100ms>
 *       x++;
 *   RIFT_BENCH_END(bench_name);
 * }
 * @endcode
 */
#define RWUT_BENCH_WHILE_TRUE(name, bool_statement)                         \
                                                                            \
    _RWUT_BENCH_COMMON_VARS(name);                                          \
    _RWUT_BENCH_COMMON_START(name);                                         \
                                                                            \
    while (bool_statement)                                                  \
    {                                                                       \
        _RWUT_BENCH_RUN_VARS(name);                                         \
        /* Need to open brace for iter loop */                              \
       {                                                                    \
            /* Force total_runs to stay at 1 for a final result */          \
            name##total_runs = 0;


/**
 * Macro to end benchmark <name>'s code section
 * @param[in] name - Same benchmark name that was used to start the benchmark block.
 *
 * Example Usage:
 * @code
 * RWUT_BENCH_(ITER|MSECS|WHILE_TRUE)(bench_name, ...);
 *    <Benchmark code region>
 * RWUT_BENCH_END(bench_name);
 * @endcode
 *
 * When using RWUT_BENCH_[ITER|MSECS|WHILE_TRUE] macros to start a benchmark code section, 
 * you must end the code section with 
 */
#define RWUT_BENCH_END(name)                                                                       \
        name##total_iterations += 1;                                                               \
        if (! name##is_perf_test)                                                                  \
            break;                                                                                 \
       }                                                                                           \
        clock_gettime(CLOCK_REALTIME, &name##run_stop_time);                                       \
        timespec name##run_time = timespec_diff(name##run_start_time, name##run_stop_time);        \
        name##total_run_time = timespec_add(name##total_run_time, name##run_time);                 \
        name##total_runs += 1;                                                                     \
        name##time_per_run = timespec_divide(name##total_run_time, name##total_runs);              \
        if (! name##is_perf_test)                                                                  \
            break;                                                                                 \
    }                                                                                              \
                                                                                                   \
    clock_gettime(CLOCK_REALTIME, &name##end_time);                                                \
                                                                                                   \
    /*If we're running in performance test mode, then record benchmark results in test. */         \
    if (name##is_perf_test) {                                                                      \
        std::cout << std::endl;                                                                    \
        std::cout << "            `"#name << "` Benchmark" << " Results" << std::endl;             \
        RWUT_BENCH_DRAW_LINE();                                                                    \
        RWUT_BENCH_RECORD_PROPERTY(name, runs, name##total_runs);                                  \
        RWUT_BENCH_RECORD_PROPERTY(name, iterations, name##total_iterations);                      \
        RWUT_BENCH_RECORD_PROPERTY(name, iterations_per_run,                                       \
                                   (name##total_iterations / name##total_runs));                   \
        RWUT_BENCH_RECORD_PROPERTY(name, average_run_time_ms, timespec_to_ns(name##time_per_run)/NS_PER_MS);    \
        RWUT_BENCH_RECORD_PROPERTY(name, average_iter_per_sec, (name##total_iterations / name##total_runs) * ((double)1000000000.0/(timespec_to_ns(name##time_per_run))));    \
        RWUT_BENCH_RECORD_PROPERTY(name, total_run_time_ms, timespec_to_ns(name##total_run_time)/NS_PER_MS); \
    }

/**
 * Macro to record a custom JUnit XML property for a particular benchmark.
 * @param[in] name - benchmark name.
 * @param[in] var - key of the property to add.
 * @param[in] value - property value to store.
 *
 * Example Usage:
 * @code
 *   RWUT_BENCH_RECORD_PROPERTY(fooBench, numTimers, 500);
 * @endcode
 *
 * In this example, the property benchmark_fooBench_numTimers will be set to 500 under the particular
 * test case that the benchmark was defined under.  
 */

#define RWUT_BENCH_RECORD_PROPERTY(name, var, value)                     \
    if (_RWUT_IS_PERF_TEST_MODE()) {                                     \
        double coeff;                                                    \
        const char *unit;                                                \
        to_human_readable(value, &coeff, &unit);                         \
        std::cout << std::fixed  << "              - " #var": " << (long)coeff << unit << std::endl; \
        RecordProperty("benchmark_"#name"_"#var, value);                 \
    }

/**
 * Macro to record a custom JUnit XML property using variable available in the benchmark scope.
 * @param[in] name - benchmark name.
 * @param[in] var - property variable name to add.
 *
 * Example Usage:
 * @code
 *   RWUT_BENCH_RECORD_VARIABLE(fooBench, num_timers);
 * @endcode
 *
 * In this example, the property benchmark_fooBench_num_timers will be set to the value of
 * the variable num_timers in the current scope.
 */
#define RWUT_BENCH_RECORD_VARIABLE(name, var)                           \
    if (_RWUT_IS_PERF_TEST_MODE()) {                                    \
        std::cout <<  std::fixed << "              - " #var ": " << var << std::endl; \
        RecordProperty("benchmark_"#name"_"#var, var);                  \
    }

/**
 * Macro to return a variable defined by a benchmark region.
 * @param[in] name - benchmark name.
 * @param[in] var - benchmark variable name.
 *
 * Variables defined by a benchmark:
 * - total_runs(int) -  Total number of runs taken though benchmark region (1 for all but RW_BENCH_ITER macro)
 * - total_iterations(int)  -  Total number of iterations taken through benchmark region (1 for all but RW_BENCH_ITER macro)
 * - average_run_time(timespec) - Timespec representing average time spent in the run loop
 * - start_time(timespec) - Timespec representing time when the benchmark started.
 * - stop_time(timespec) - Timespec reprsenting the time when the benchmark stopped.
 */
#define RWUT_BENCH_VAR(name, var)                        \
    (name##var)

#endif /* __RWUT_H__ */
