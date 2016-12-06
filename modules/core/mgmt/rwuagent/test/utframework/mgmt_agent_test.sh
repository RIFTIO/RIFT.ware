#!/bin/bash

DEMO_DIR=$RIFT_INSTALL/demos
CORE_UTIL_DIR=$RIFT_INSTALL/usr/rift/systemtest/util
MGMT_SYSYEST_DIR=$RIFT_INSTALL/usr/rift/systemtest/mgmtagt-testbed

wait_system=1000

test_prefix="mgmt_agent_test"
system_cmd="${DEMO_DIR}/mgmt_tbed.py --test-name ${test_prefix}"

up_cmd="${CORE_UTIL_DIR}/wait_until_system_started.py --max-wait $wait_system"

test_cmd="${MGMT_SYSYEST_DIR}/mgmt_agent_test_cmd.sh"

${CORE_UTIL_DIR}/systest_wrapper.sh --sysinfo --system_cmd "$system_cmd" --up_cmd "$up_cmd" --test_cmd "$test_cmd"

# Save the exit code from systest_wrapper.sh
rc=$?

# Return the saved exit code
exit $rc
