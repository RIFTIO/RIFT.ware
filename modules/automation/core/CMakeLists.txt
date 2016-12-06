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
# Author(s): Andrew Golovanov
# Creation Date: 8/25/2014
# 

##
# DEPENDENCY ALERT
# The submodule dependencies must be specified in the
# .gitmodules.dep file at the top level (supermodule) directory
# If this submodule depends other submodules remember to update
# the .gitmodules.dep
##

cmake_minimum_required(VERSION 2.8)

##
# DO NOT add any code before this and DO NOT
# include this file anywhere else
##
include(rift_submodule)

##
# Submodule specific includes will go here,
# These are specified here, since these variables are accessed
# from multiple sub directories. If the variable is subdirectory
# specific it must be declared in the subdirectory.
##

##
# Include the subdirs
##
set(subdirs
  lib
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##
rift_add_submodule_targets(
  SUBMODULE_PACKAGE_NAME "rw.automation.core"
  DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package-description.txt"
  DESCRIPTION_SUMMARY "RIFTware Automation Core Package"
  )

##
#
##
install(
  PROGRAMS
    util/systest_wrapper.sh
    util/test_module_wrapper.py
    util/wait_until_system_started.py
    util/test_openstack_wrapper.py
    util/sarplot.py
  DESTINATION usr/rift/systemtest/util
  COMPONENT RWAUTO)

install(
  PROGRAMS
    util/mano/mano_common.sh
  DESTINATION usr/rift/systemtest/util/mano
  COMPONENT RWAUTO)

install(
  PROGRAMS
    util/multi_vm_vnf/add_vnf_vm.py
    util/multi_vm_vnf/multivm_vnf_startvm.py
  DESTINATION usr/rift/systemtest/util/multi_vm_vnf
  COMPONENT RWAUTO)

install(
  PROGRAMS
    util/multi_vm_vnf/json/vm_data_schema.json
    util/multi_vm_vnf/json/sample_vnf_data.json
  DESTINATION usr/rift/systemtest/util/multi_vm_vnf/json
  COMPONENT RWAUTO)
##
# Install the harness
##
install(
  DIRECTORY harness
  DESTINATION usr/rift/systemtest
  USE_SOURCE_PERMISSIONS
  COMPONENT RWAUTO)
