# /bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 2014/06/03
#
# This script is meant to assist Jenkins in performing incremental
# builds by deinitializing submodules if the build cache exists for the
# submodule hash.
#
#    uninitialize_cached_submodules.sh <build_type>
#
#  Arguments:
#      <build_type> - Debug, Debug_Coverage, Release
#

set -o nounset
set -u

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPENDENCY_PARSER_CMD="$THIS_DIR/dependency_parser.py"

# Set some vars if not defined by env variables.  Used for testing.
GET_SUBMODULES_CMD="$THIS_DIR/dependency_sort.py"

function verify_cwd_at_root(){
  if [ ! -f ".gitmodules.deps" ]; then
    echo "ERROR: This script should be run at the top-level"
    exit 1
  fi
}

##
#  Calculate and return submodule hash.  Capture stdout to get hash.
#  $1 - submodule to calculate hash for
##
function get_submodule_hash(){
  local submodule="$1"

  set -x
  local parser_cmd="$DEPENDENCY_PARSER_CMD --dependency-file=.gitmodules.deps "--submodule=$submodule" --print-hash"
  local hash=$($parser_cmd)
  set +x
  if [ $? -ne 0 ]; then
    echo "ERROR: Command failed to retrieve submodule hash (command: $parser_cmd)"
    exit 1
  fi

  echo $hash
}

##
#  Gets list of submodules available in workspace.  Capture stdout to get list.
##
function get_submodules(){
  local sorted_submodules=$($GET_SUBMODULES_CMD)
  if [ $? -ne 0 ]; then
    echo "ERROR: Could not get list of submodules."
    exit 1
  fi

  echo "$sorted_submodules"
}

##
# Builds the full build cache path.  Capture stdout to get the path.
#   $1 - Submodule
#   $2 - Submodule Hash
#   $3 - Build Type
##
function get_full_cache_path(){
  local submodule="$1"
  local hash="$2"
  local build_type="$3"

  local cache_path="$RIFT_BUILD_CACHE_DIR/$build_type/$submodule/$hash"

  echo $cache_path
}

if [ $# -ne 1 ]; then
  echo "ERROR: Expecting a single build_type argument"
  exit 1
fi

if [[ "$1" != "Debug_FILES" && "$1" != "Debug" && "$1" != "Debug_Coverage" && $1 != "Release" ]]; then
  echo "ERROR: Build type should be in the set (Debug, Debug_Coverage, Release)."
  exit 1
fi

build_type=$1

submodules="$(get_submodules)"
# Convert the string into an array using the default IFS of ' '
read -a submodules_array <<< "$submodules"

for submodule in "${submodules_array[@]}"; do
  if [ ! -e "$submodule" ]; then
    echo "WARNING: Could not find $submodule path."
    continue
  fi

  hash="$(get_submodule_hash $submodule)"
  echo "INFO: Calculated submodule hash for $submodule: $hash"

  full_cache_path="$(get_full_cache_path $submodule $hash $build_type)"
  echo "INFO: Checking if submodule cache path exists for $submodule: $full_cache_path"
  if [ -e "$full_cache_path" ]; then
    echo "INFO: Build cache exists for submodule $submodule.  Deinitializing."
    git submodule deinit $submodule
    if [ $? -ne 0 ]; then
      echo "ERROR: Could not deinitialize submodule: $submodule"
      continue
    fi
  fi
done

