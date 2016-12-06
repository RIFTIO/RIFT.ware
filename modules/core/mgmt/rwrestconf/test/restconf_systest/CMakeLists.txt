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
# Author(s): Balaji Rajappa 
# Creation Date: 20/07/2016
# 

cmake_minimum_required(VERSION 2.8)

##
# Install the system test launch script
##
install(
  PROGRAMS
    restconf_systest.sh
  DESTINATION usr/rift/systemtest/restconf
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Install the test script that tests the system
##
install(
  PROGRAMS
    pytest/restconf_systest.py
  DESTINATION usr/rift/systemtest/restconf/pytest
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Install the automation config
##
install(
  FILES
    racfg/restconf_systest.racfg
    racfg/restconf_systest_xml.racfg
  DESTINATION
    usr/rift/systemtest/restconf
    COMPONENT ${INSTALL_COMPONENT}
)

