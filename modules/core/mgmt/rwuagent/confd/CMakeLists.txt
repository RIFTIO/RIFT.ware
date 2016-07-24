# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
    ${PKG_LONG_NAME}
  )

##
# Install the confd northbound schema listing.
##
install(FILES
    confd_nb_schema_list.txt 
  DESTINATION
    usr/data/manifest
  COMPONENT
    ${PKG_LONG_NAME}
  )

install(FILES rw_confd_prototype.conf
  DESTINATION etc
  COMPONENT ${PKG_LONG_NAME}
  )
