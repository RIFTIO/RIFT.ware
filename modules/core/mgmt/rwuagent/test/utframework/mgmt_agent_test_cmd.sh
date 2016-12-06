#!/bin/bash

result_file="${RIFT_MODULE_TEST}/mgmtagt_test_result.txt"
rpc_cmd="agent-tests all continue-on-failure false"

test_prefix="mgmt_agent_test"

# RIFT_VAR_ROOT env would have been set by sytest_wrapper, no need to pass it
test_cmd="$RIFT_INSTALL/usr/bin/rwcli --rwmsg --username admin --passwd admin "

echo "Executing test command - ${test_cmd} -c '${rpc_cmd}' > ${result_file}"
${test_cmd} -c '${rpc_cmd}' > ${result_file}

rc=$?
exit $rc
