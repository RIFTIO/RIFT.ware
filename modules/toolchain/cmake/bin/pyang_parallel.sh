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
# Author(s): Sujithra Periasamy
# Creation Date: 25/09/2015
#
if [ -z "$RIFT_ROOT" ]; then
  echo "Error: You must be in a RIFT_SHELL to run this script."
  exit 1
fi

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

pid=()
count=1

while [[ $# > 0 ]]
do
  if [[ " $1" = " -b" ]]; then
     current_bin_dir="$2"
  elif [[ " $1" = " -p" ]]; then
     yang_path="$2"
  elif [[ " $1" = " -w" ]]; then
     all_warning="$2"
  elif [[ " $1" = " --" ]]; then
    start_files=1
  fi

  shift
  if [[ $start_files != 1 ]]; then
    shift
  else
    break
  fi
done

if [[ $# = 0 ]]; then
  echo "Error: Invalid arguments to the scritps"
  exit 1
fi

woff_op=""
if [[ $all_warning != 1 ]]; then
  woff_op="--ignore-warning=UNUSED_IMPORT"
fi

while [[ $# > 0 ]]
do
  yang_file="$1"
  bare_yfile=$(basename $yang_file)
  out_file="$current_bin_dir/$bare_yfile.depends"
  #invoke the pyang commands in the background
  ${THIS_DIR}/envset.sh -e pyang --path=$yang_path $yang_file -f recursive --recurse-include-path $woff_op -o $out_file &
  pid[$count]=$!
  count=$(($count + 1))
  shift
done

exitstatus=0
for i in ${!pid[@]}
do
   wait ${pid[i]}
   estat=$?
   if [[ $estat != 0 ]]; then
     exitstatus=$estat
   fi
done
exit $exitstatus
