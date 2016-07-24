# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil 
# Creation Date: 03/03/2016
# 

##
# Install the XML file for CLI
##
install(FILES
    cli_multivm_vnf.xml
    cli_multivm_vnf_schema_listing.txt  
  DESTINATION
    usr/data/manifest
  COMPONENT
    ${PKG_LONG_NAME}
  )
