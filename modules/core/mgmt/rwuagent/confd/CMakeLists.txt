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
# Creation Date: 11/26/2015
# 

cmake_minimum_required(VERSION 2.8)

##
# Install the oper user aaa restrictions
##
install(FILES
    oper_user_restrictions.xml
  DESTINATION
    usr/data/security
  COMPONENT
    ${INSTALL_COMPONENT}
  )

##
# Install the confd northbound schema listing.
##
install(FILES
    confd_nb_schema_list.txt 
  DESTINATION
    usr/data/manifest
  COMPONENT
    ${INSTALL_COMPONENT}
  )

install(FILES rw_confd_prototype.conf
  DESTINATION etc
  COMPONENT ${INSTALL_COMPONENT}
  )
