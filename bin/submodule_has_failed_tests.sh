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
# Author(s): Austin Cormier
# Creation Date: 2014/06/05
#
# Script to be invoked by CMake to determine whether a submodule has failed tests.
# Currently, test_wrapper.sh will create a <target>_FAILED file for each unit test
# that lives within a submodule tree.  We can seach this tree for any *_FAILED file
# to determine if any test failed.
#
#    submodule_has_failed_test.sh <submodule_prefix>
#
# Arguments:
#    <submodule_prefix> - Submodule path relative to the root of the repository.
#
# Returns 0 if failed tests were found and 1 otherwise.

rift_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
rift_root="${rift_root%/bin}"

unittest_dir="$RIFT_UNIT_TEST"

if [ ! $# -eq 1 ]; then
  echo "ERROR: A submodule prefix argument is expected."
  exit 1
fi

submodule_prefix="$1"

if [ ! -d "$rift_root/$submodule_prefix" ]; then
  echo "ERROR: Submodule doesn't exist?: $rift_root/$submodule_prefix"
  exit 1
fi

submodule_unittest_dir="$unittest_dir/$submodule_prefix"
if [ ! -d "$submodule_unittest_dir" ]; then
  echo "WARNING: Submodule unittest output directory doesn't exist: $submodule_unittest_dir"
  exit 1
fi

found_files=$(find "$submodule_unittest_dir" -name "*_FAILED" | wc -l)
if [ $found_files == "0" ]; then
  echo "INFO: Did not find any failed unittests in: $submodule_unittest_dir"
  exit 1
fi

# There a certain cases (build_ladder) where we still want to cache the submodule even
# if there are failures.  This was the easiest place to inject that logic.
if [ $BCACHE_IGNORE_FAILED_SUBMODULE_TESTS -eq 1 ]; then
  echo 'WARNING: $BCACHE_IGNORE_FAILED_SUBMODULE_TESTS env var set, caching submodule regardless of failed unit tests.'
  exit 1
fi

echo "INFO: Found $found_files failed unit tests in: $submodule_unittest_dir"
exit 0
