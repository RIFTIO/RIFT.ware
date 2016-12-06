
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

# - Find PostgreSQL library
#
# This module defines:
#  POSTGRESQL_FOUND - True if the package is found
#  POSTGRESQL_INCLUDE_DIR - containing libpq-fe.h
#  POSTGRESQL_LIBRARIES - Libraries to link to use PQ functions.

if (POSTGRESQL_INCLUDE_DIR AND POSTGRESQL_LIBRARIES)
  set(POSTGRESQL_FIND_QUIETLY TRUE)
endif (POSTGRESQL_INCLUDE_DIR AND POSTGRESQL_LIBRARIES)

# Include dir
find_path(POSTGRESQL_INCLUDE_DIR 
	NAMES libpq-fe.h
	PATH_SUFFIXES pgsql postgresql
)

# Library
find_library(POSTGRESQL_LIBRARY 
  NAMES pq
)

# handle the QUIETLY and REQUIRED arguments and set POSTGRESQL_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(POSTGRESQL DEFAULT_MSG POSTGRESQL_LIBRARY POSTGRESQL_INCLUDE_DIR)

IF(POSTGRESQL_FOUND)
  SET( POSTGRESQL_LIBRARIES ${POSTGRESQL_LIBRARY} )
ELSE(POSTGRESQL_FOUND)
  SET( POSTGRESQL_LIBRARIES )
ENDIF(POSTGRESQL_FOUND)

# Lastly make it so that the POSTGRESQL_LIBRARY and POSTGRESQL_INCLUDE_DIR variables
# only show up under the advanced options in the gui cmake applications.
MARK_AS_ADVANCED( POSTGRESQL_LIBRARY POSTGRESQL_INCLUDE_DIR )
