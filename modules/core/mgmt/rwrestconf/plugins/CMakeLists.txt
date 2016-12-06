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
# Author(s): Max Beckett
# Creation Date: 8/25/2015
# 

include(rift_yang)
include(rift_python)
include(rift_plugin)


rift_install_python_plugin(
  restconf
    restconf.py
  COMPONENT
    ${INSTALL_COMPONENT}
  )

rift_generate_python_log_yang(
    LOG_CATEGORY_NAME rw-restconf-log
    START_EVENT_ID 68000
    OUT_YANG_FILE_VAR rw_restconf_log_file
    )

rift_add_yang_target(
  TARGET
    ietf_restconf_monitoring
  YANG_FILES
    ietf-restconf-monitoring.yang
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  LIBRARIES
    rwmanifest_yang_gen
    rw_vcs_yang_gen
  )

rift_add_yang_target(
  TARGET
    rwrestconf_yang
  YANG_FILES
    rw-restconf.yang
    ${rw_restconf_log_file}
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  LIBRARIES
    rwuagent_yang_gen
    rwmanifest_yang_gen
    rw_vcs_yang_gen
    ietf_restconf_monitoring_gen
  )

rift_install_python_plugin(
  reststream
    reststream.py
  COMPONENT
    ${INSTALL_COMPONENT}
  )

rift_add_yang_target(
  TARGET
    rwreststream_yang
  YANG_FILES
    rw-reststream.yang
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  LIBRARIES
    rwmanifest_yang_gen
    rw_vcs_yang_gen
  )

rift_install_python_plugin(
  restportforward restportforward.py
  COMPONENT
    ${INSTALL_COMPONENT}
  )

rift_add_yang_target(
  TARGET
    restportforward_yang
  YANG_FILES
    rw-restportforward.yang
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  LIBRARIES
    rwmanifest_yang_gen
    rw_vcs_yang_gen
  )

# for testing dynamic schema
rift_add_yang_target(
  TARGET
    rwdynschematesta_yang
  YANG_FILES
    rw-dyn-schema-test-a.yang
  LIBRARIES
    rw_vcs_yang_gen
  )

rift_add_yang_target(
  TARGET
    rwdynschematestb_yang
  YANG_FILES
    rw-dyn-schema-test-b.yang
  LIBRARIES
    rw_vcs_yang_gen
  )
