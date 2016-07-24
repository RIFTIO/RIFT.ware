# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

# This file sets the RIFT common subdirectory variables

if (NOT DEFINED PROJECT_TOP_DIR)
  message(FATAL_ERROR "PROJECT_TOP_DIR is not defined")
endif (NOT DEFINED PROJECT_TOP_DIR)

# PATH for searching cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/granite)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/vala)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/helper)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/patches)

# cmake cache variable CMAKE_INSTALL_PREFIX
set(CMAKE_INSTALL_PREFIX ${PROJECT_TOP_DIR}/.install)
