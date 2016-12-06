#!/bin/bash
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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


