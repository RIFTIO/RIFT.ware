
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

include(rift_python)

rift_python_install_tree(
  FILES
    dict2xml.py
    ndl.py
    rw_checks.py
    rw_common.py
    rw_plotting.py
    rw_testcase.py
    rw_testconfig.py
    rw_vm.py
  COMPONENT RWAUTO)

rift_python_install_tree(
  FILES
    rift/auto/log.py
    rift/auto/xml_utils.py
    rift/auto/descriptor.py
    rift/auto/proxy.py
    rift/auto/session.py
    rift/auto/test_summary.py
    rift/auto/parse_utils.py
    rift/auto/trafgen_utils.py
    rift/auto/vm_utils.py
    rift/auto/os_utils.py
    rift/auto/ip_utils.py
    rift/auto/json_utils.py
    rift/auto/mano.py
    rift/auto/ssh.py
    rift/auto/__init__.py
  COMPONENT RWAUTO)

install(
  FILES
    pytest/conftest.py
  DESTINATION usr/rift/systemtest/pytest
  COMPONENT RWAUTO)

# RA testcase schema
install(
  FILES
    racfg_schema.json
    raconfg.json
  DESTINATION usr/rift/systemtest/etc
  COMPONENT RWAUTO)

# Documentation target
set(PKG_NAME automation_framework)
set(doxy_files
  rw_common.py
  rw_plotting.py
  rw_checks.py
  ndl.py
  )
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "API Reference for the Automation Framework"
  DOXY_FILES ${doxy_files}
  DEST_DIR ${PKG_NAME}
)
