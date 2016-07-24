#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


FAIL=false

while [ $# -gt 0 ]; do
  case $1 in
    *FAIL)
      FAIL=true
      ;;
  esac
  shift
done

if ${FAIL}; then
  echo "Failing rwmain exiting with status 1"
  exit 1
else
  echo "Succeeding rwmain looping forever"
  while true; do
    sleep 1
  done
fi

