# /bin/bash
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
# Creation Date: 2014/06/03
#
# The build ladder gets the topological sorted list of submodules and builds
# the submodules one at a time using a custom build cache location.
#
# REQUIRES ALL SUBMODULES IN UNINITIALIZED STATE
#
# 1. Create an empty directory to use as the build cache location.
# 3. Generate list of sorted submodules (using dependency_sort.sh).
# 4. For each submodule in the sorted list:
#    1. Build only that submodule (make rw.submodule SUBMODULE= BUILDCACHE_DIR=)
#    2. If the submodule build fails, stop.
#    3. Make submodule package.
#    4. Make submodule build cache.
#    5. Deinitialize submodule.

# These steps should verify that all submodule dependencies are correct and the
# artifact packaging is complete.  If any submodule fails, then the required 
# dependencies are somehow incomplete or incorrect.

set -o nounset

THIS_DIR="$(realpath $(dirname $0))"

# Set some vars if not defined by env variables.  Used for testing.
DEPENDENCY_SORT_BIN=${DEPENDENCY_SORT_BIN:-"$THIS_DIR/dependency_sort.py"}
MAKE_CMD=${MAKE_CMD:-"make VERBOSE=1"}


function verify_cwd_at_root(){
  if [ ! -f ".gitmodules.deps" ]; then
    echo "ERROR: This script should be run at the top-level"
    exit 1
  fi
}

function verify_submodules_uninitialized(){
  while read line; do
    if [[ $line != -* ]]; then
      echo "ERROR: Found a initialized submodule: $line"
      exit 1
    fi
  done < <(git submodule | egrep -v 'modules/(toolchain|tools)')
}

function verify_single_submodule_initialized(){
  submodule=$1
  git submodule | while read line; do
    if [[ $line != -* ]]; then
      if [[ $line != *$submodule* ]]; then
            echo "ERROR: Found a initialized submodule: $line"
            exit 1
      fi
    fi
  done < <(git submodule | egrep -v 'modules/(toolchain|tools)' )
}

# Capture stdout to store cache directory.
function generate_build_cache_dir(){
  local dir=$(mktemp -d -t "build_ladder_XXX") || exit 1

  echo $dir
}

function get_sorted_submodule_list(){
  local sorted_submodules=$($DEPENDENCY_SORT_BIN)
  if [ $? -ne 0 ]; then
    echo "ERROR: Could not get list of sorted submodules."
    exit 1
  fi

  echo "$sorted_submodules"
}

# Log the command and run it.
function log_and_run_cmd(){
  echo "INFO: Running command: $@"
  $@
}

##
# Build the submodule using the top-level rw.submodule target. 

# Arguments: 
#   $1 - submodule
#   $2 - build cache location
##
function submodule_build(){
  local submodule="$1"
  local build_cache="$2"

  # Build only this submodule
  build_cmd="$MAKE_CMD rw.submodule SUBMODULE=$submodule BUILDCACHE_DIR=$build_cache"

  log_and_run_cmd $build_cmd
  if [ $? -ne 0 ]; then
    echo "ERROR: Building submodule '$submodule' failed. (command: $build_cmd)"
    exit 1
  fi

  verify_single_submodule_initialized "$submodule" || exit 1
}


##
# Package the submodule using the top-level rw.package target. 
#
# Arguments: 
#   $1 - submodule
##
function submodule_package(){
  local submodule="$1"

  # SUBMODULE argument is NOT necessary but doesn't hurt to include it.
  local package_cmd="$MAKE_CMD rw.package SUBMODULE=$submodule"

  log_and_run_cmd $package_cmd
  if [ $? -ne 0 ]; then
    echo "ERROR: Packaging submodule '$submodule' failed. (command: $package_cmd)"
    exit 1
  fi
}

##
# Create the build cache using the packaged submodule artifacts
#
# Arguments: 
#   $1 - submodule
#   $2 - build cache location
##
function submodule_bcache(){
  local submodule="$1"
  local build_cache="$2"

  # SUBMODULE argument is NOT necessary.
  local bcache_cmd="$MAKE_CMD rw.bcache SUBMODULE=$submodule BUILDCACHE_DIR=$build_cache"

  BCACHE_IGNORE_FAILED_SUBMODULE_TESTS=1 log_and_run_cmd $bcache_cmd
  if [ $? -ne 0 ]; then
    echo "ERROR: Bcaching submodule '$submodule' failed. (command: $bcache_cmd)"
    exit 1
  fi
}

##
# Deinitialize the submodule so submodule build past this point will not have
# access to the submodule's sources but only the pre-built artifacts.
#
# Arguments:
#   $1 - submodule
##
function submodule_deinit(){
  local submodule="$1"

  local deinit_cmd="git submodule deinit $submodule"

  $deinit_cmd
  if [ $? -ne 0 ]; then
    echo "ERROR: Deinitializing submodule failed. (command: $deinit_cmd)"
    exit 1
  fi
}

##
# Make clean to clear out everything previously generated in the workspace.
# This ensures only artifacts from build cache are retrieved.
##
function make_clean(){
  make clean
}

##
#
##
function make_unittests(){
   make rw.unittest ABORT_ON_TEST_FAILURE=0 VERBOSE=1
}

verify_cwd_at_root
verify_submodules_uninitialized || exit 1

build_cache=$(generate_build_cache_dir)
# Set up a trap to automatically clean up build cache directory on exit or catchable signal
if [ "$USER" == "jenkins" ]; then
    trap "[ -d $build_cache ] && rm -rf $build_cache" EXIT SIGINT SIGTERM
else
    trap "echo you should remove $build_cache" EXIT SIGINT SIGTERM
fi

echo "INFO: Created new build cache ($build_cache)"

sorted_submodules=$(get_sorted_submodule_list)
echo "INFO: Got list of sorted submodules ($sorted_submodules)"

# Convert the string into an array using the default IFS of ' '
declare -a sorted_submodules_array
read -a sorted_submodules_array <<< "$sorted_submodules"

MODULES_TO_SKIP="modules/(docs|ext/(cloud|intel-qat|nwepc|openssl-async|vnfs)|tools|toolchain|app|pristine)($|/)"
make_clean

for submodule in "${sorted_submodules_array[@]}"; do
  if [[ $submodule =~ $MODULES_TO_SKIP ]]; then
    echo "INFO: skipping $submodule"
    continue
  fi

  submodule_build "$submodule" "$build_cache" || (submodule_deinit "$submodule"; exit 1)
  make_unittests || echo "Unit tests in submodule $submodule failed, continuing."
  submodule_package "$submodule" || (submodule_deinit "$submodule"; exit 1)
  submodule_bcache $submodule "$build_cache" || (submodule_deinit "$submodule"; exit 1)
  submodule_deinit "$submodule" || exit 1

  make_clean
done

echo "INFO: Build ladder was successful!"
exit 0

