#!/bin/bash
# STANDARD_RIFT_IO_COPYRIGHT
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
