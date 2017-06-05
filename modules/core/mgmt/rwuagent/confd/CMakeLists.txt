# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 11/26/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
