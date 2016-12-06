#!/bin/bash

if [ ! -e /etc/issue ]; then
  echo "Error: /etc/issue does not exist" >&2
  exit 1
fi

issue="$(cat /etc/issue)"

grep -q "Fedora release 20" /etc/issue
if [ $? -eq 0 ]; then
  echo -n "fc20"
  exit 0
fi

egrep -q "Ubuntu 16.04.* LTS" /etc/issue
if [ $? -eq 0 ]; then
  echo -n "ub16"
  exit 0
fi

echo "Error: could not detect platform from /etc/issue" >&2
exit 1
