# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 1/11/2015
# 

install(
  PROGRAMS
    openmano
    openmano_cleanup.sh
  DESTINATION usr/bin
  COMPONENT ${PKG_LONG_NAME}
)
