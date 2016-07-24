#!/bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sujithra Periasamy
# Creation Date: 25/09/2015
#
if [ -z "$RIFT_ROOT" ]; then
  echo "Error: You must be in a RIFT_SHELL to run this script."
  exit 1
fi

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
  $RIFT_ROOT/scripts/env/envset.sh -e pyang --path=$yang_path $yang_file -f recursive --recurse-include-path $woff_op -o $out_file &
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
