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
# Creation Date: 2015/10/22
#
# Helper script for invoking the restconf_systest system test using the systest_wrapper
# Usage:
#     ./restconf_systest.sh
#

SYS_TEST=$RIFT_INSTALL/usr/rift/systemtest/
CORE_UTIL_DIR=$SYS_TEST/util
DEMO_DIR=$RIFT_INSTALL/demos
PYTEST_DIR=$SYS_TEST/restconf/pytest

up_cmd="${CORE_UTIL_DIR}/wait_until_system_started.py --max-wait 1000"
use_xml_mode=false

if ! ARGPARSE=$(getopt -o ',' -l use-xml-mode -- "$@")
then
    exit 1
fi
eval set -- "$ARGPARSE"

while [ $# -gt 0 ]
do
    case "$1" in
    --use-xml-mode)
        use_xml_mode=true
        ;;
    --)
        break
        ;;
    -*)
        echo "$0: error - unrecognized option $1" 1>&2
        exit 1
        ;;
    *)
        echo "$0: error - not an option $1" 1>&2
        exit 1
        ;;
    esac
    shift
done

system_cmd="\
  ${DEMO_DIR}/mgmt_tbed.py "

if [ $use_xml_mode == true ]; then
    system_cmd+=" --use-xml-mode"
fi

test_cmd="${PYTEST_DIR}/restconf_systest.py"

${CORE_UTIL_DIR}/systest_wrapper.sh \
  --up_cmd "${up_cmd}" \
  --system_cmd "${system_cmd}" \
  --test_cmd ${test_cmd} 
