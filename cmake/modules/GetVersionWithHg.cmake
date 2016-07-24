
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
