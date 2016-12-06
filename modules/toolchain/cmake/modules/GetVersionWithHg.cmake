
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

# This file is called at build time. It regenerates the version.h file based on the hg version.

EXECUTE_PROCESS(
	COMMAND ${HGCOMMAND} id -i
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	RESULT_VARIABLE reshash
	OUTPUT_VARIABLE verhash
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE)
EXECUTE_PROCESS(
	COMMAND ${HGCOMMAND} id -n
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	RESULT_VARIABLE resval
	OUTPUT_VARIABLE verval
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE)
	
if (reshash EQUAL 0) 
	SET(FD_PROJECT_VERSION_HG "${verval}(${verhash})")
 	message(STATUS "Source version: ${FD_PROJECT_VERSION_HG}")
endif (reshash EQUAL 0)

CONFIGURE_FILE(${SRC} ${DST})
