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


ppid=$PPID

echo "Parent process $ppid monitor wrapping: $@"
"$@"&
pid=$!

# Kill the child process when this monitor process exits
trap "echo Terminating child process: $pid; kill -s SIGTERM $pid" EXIT

# Exit when we detect the parent process has exited
while true; do
  kill -0 $ppid 2>/dev/null
  if [ $? -ne 0 ]; then
    echo "Parent process exited. Exiting."
    exit 0
  fi

  kill -0 $pid 2>/dev/null
  if [ $? -ne 0 ]; then
    echo "Child process died. Exiting."
    exit 1
  fi

  sleep .5
done
