#!/bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Jeremy Mordkoff 
# Creation Date: 2015/01/28
#
# Wrapper script for invoking test targets, recording the output, and returning a failure
# code only if the return code is >128.  This allows cmake to continue on to other unit tests on simple test failures.
#
#    test_wrapper.sh  <command_to_run>
#
# Arguments:
#   command_to_run - The full unit test command line to be executed.
#


# Ensure we got the correct number of arguments
if [ $# -lt 1 ]; then
  echo "test_wrapper2 missing args"
  exit 1
fi

echo "test_wrapper(INFO): Running test command ($@)"
echo "======================  Start of Test Output  ==============================="

{ set -o monitor;  "$@"; }
cmd_rc=$?
echo "======================   End of Test Output   ==============================="
echo ""

# If the return code was greater than 128, then the script caught a fatal signal (SIGSEV, etc)
if [ $cmd_rc -gt 128 ]; then
  # Calculate the actual signal caught
  let signal=$cmd_rc-128
  echo "test_wrapper(ERROR): Test caught fatal signal($signal)"
  exit 3
fi

if [ $cmd_rc -gt 0 ]; then
  echo "test_wrapper(WARN): Test exited with error($cmd_rc)"
  exit 1
fi

exit 0


