#!/bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Karun Ganesharatnam
# Creation Date: 02/26/2016
# 
source $RIFT_INSTALL/usr/rift/systemtest/util/mano/mano_common.sh

# Helper script for invoking the mission control system test using the systest_wrapper

SCRIPT_TEST="py.test -x -vvv \
            ${PYTEST_DIR}/mission_control/test_mission_control.py \
            ${PYTEST_DIR}/multi_vm_vnf/test_multi_vm_vnf_trafgen.py \
            ${PYTEST_DIR}/multi_vm_vnf/test_trafgen_data.py"

test_prefix="multi_vm_vnf_trafgen"
test_cmd=""

# Parse command-line argument and set test variables
parse_args "${@}"

# Test uses standalone launchpad
lp_standalone=true

# Construct the test command based on the test variables and create the mvv image
mvv=true
create_mvv_image_file
construct_test_comand

# Execute from pytest root directory to pick up conftest.py
cd "${PYTEST_DIR}"

eval ${test_cmd}
