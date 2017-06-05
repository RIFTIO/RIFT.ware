#!/bin/bash
# STANDARD_RIFT_IO_COPYRIGHT
# Author(s): Austin Cormier
# Creation Date: 2014/05/20
#
# Wrapper script for invoking unit test targets, recording the output, and returning a failure
# code only if the return code is >128.  This allows cmake to continue on to other unit tests on simple test failures.
#
#    test_wrapper.sh <output_file> <command_to_run>
#
# Arguments:
#   output_file - The file to log the stdout and stderr of the test command that follows.
#   command_to_run - The full unit test command line to be executed.
#
# Environment:
#   ABORT_ON_TEST_FAILURE - If this environment variable is defined and set to 1, then
#                           return error code on any test failure.


# Ensure we got the correct number of arguments
if [ $# -lt 2 ]; then
  echo "test_wrapper(ERROR): Expecting at least two arguments.  An output file and the command to run."
  exit 1
fi

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_SCRIPTS_DIR="$THIS_DIR"
SH2JU_SH="${TEST_SCRIPTS_DIR}/sh2ju.sh"

source "$SH2JU_SH"
juSetOutputDir ${RIFT_TEST_RESULTS}

# If the ABORT_ON_TEST_FAILURE is defined as an environment or the first
# argument is fail_fast, then return error code when the first test fails instead of moving on.
# Otherwise, the test only fails on a fatal signal.  This is to prevent a single unit test failure from
# blocking all others.  Sometimes, aborting quickly is desired for developers or system test.
ABORT_ON_TEST_FAILURE="${ABORT_ON_TEST_FAILURE:=0}"
if [ $1 == "--fail_fast" ]; then
  ABORT_ON_TEST_FAILURE="1"
  shift
fi

# If the GDB environment variable is passed in, this is being run manually
# from a developer and we don't want test_wrapper interfering with envset.sh
# GDB invocation.
if [ -n "$GDB" -a "$GDB" == "1" ]; then
  shift 1
  echo "test_wrapper(INFO): GDB environment variable is set, execing $*"
  exec "$@"
fi

# Ensure the output file is writable
output_dir="$1"
test_exe="$(basename $2)"
output_file="$output_dir/${test_exe}_output.txt"
failure_file="$output_dir/${test_exe}_FAILED"

juSetSuiteName ${test_exe}

touch "$output_file"
if [ $? -ne 0 ]; then
  echo "test_wrapper(ERROR): Could not create a empty file at: $output_file."
  exit 1
fi

# Shift out the output dir and target parameter
shift 2

echo ""
echo "test_wrapper(INFO): Running test command ($@)"
echo "======================  Start of Test Output  ==============================="

# Run the command and pipe both stdout and stderr to the output_file.  
# Run command inside braces to capture signal error (this is sent to the shell, not the process).
# Force monitor mode to ensure we are running the processes within
# seperate process groups.  Otherwise the reaper could take down this script and even Jenkins
tail -f "$output_file" &
tail_pid=$!

{ set -o monitor;  juLog -name=${test_exe} $@ >${output_file} 2>&1; }
cmd_rc=$?
kill $tail_pid


echo "======================   End of Test Output   ==============================="
echo ""

# If the return code was greater than 128, then the script caught a fatal signal (SIGSEV, etc)
if [ $cmd_rc -gt 128 ]; then
  # Calculate the actual signal caught
  let signal=$cmd_rc-128
  echo "test_wrapper(ERROR): Test caught fatal signal($signal)"

  # Mark that the test failed.
  touch $failure_file

# Just print a warning if the test exited with an error.  We dont want to return error code because CMAKE will die.
elif [ $cmd_rc -gt 0 ]; then
  echo "test_wrapper(WARN): Test exited with error($cmd_rc)"

  # Mark that the test failed.
  touch $failure_file

else
    rm -f $failure_file
fi

# If the ABORT_ON_TEST_FAILURE is set and set to 1, then exit with an error code here.
if [ $cmd_rc != 0 ]; then
  if [ "$ABORT_ON_TEST_FAILURE" == "1" ]; then
    exit $cmd_rc
  fi
fi
