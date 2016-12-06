
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

# - Look for GNU flex, the lexer generator.
# Defines the following:
#  FLEX_EXECUTABLE - path to the flex executable
#  FLEX_FILE - parse a file with flex
#  FLEX_PREFIX_OUTPUTS - Set to true to make FLEX_FILE produce outputs of
#                        lex.${filename}.c, not lex.yy.c . Passes -P to flex. 

IF(NOT DEFINED FLEX_PREFIX_OUTPUTS)
  SET(FLEX_PREFIX_OUTPUTS FALSE)
ENDIF(NOT DEFINED FLEX_PREFIX_OUTPUTS) 

IF(NOT FLEX_EXECUTABLE)
  MESSAGE(STATUS "Looking for flex")
  FIND_PROGRAM(FLEX_EXECUTABLE flex)
  IF(FLEX_EXECUTABLE)
    MESSAGE(STATUS "Looking for flex -- ${FLEX_EXECUTABLE}")
  ENDIF(FLEX_EXECUTABLE)
 MARK_AS_ADVANCED(FLEX_EXECUTABLE)
ENDIF(NOT FLEX_EXECUTABLE) 

IF(FLEX_EXECUTABLE)
  MACRO(FLEX_FILE FILENAME)
    GET_FILENAME_COMPONENT(PATH "${FILENAME}" PATH)
    IF("${PATH}" STREQUAL "")
      SET(PATH_OPT "")
    ELSE("${PATH}" STREQUAL "")
      SET(PATH_OPT "/${PATH}")
    ENDIF("${PATH}" STREQUAL "")
    IF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
      FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
    ENDIF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
    IF(FLEX_PREFIX_OUTPUTS)
      GET_FILENAME_COMPONENT(PREFIX "${FILENAME}" NAME_WE)
    ELSE(FLEX_PREFIX_OUTPUTS)
      SET(PREFIX "yy")
    ENDIF(FLEX_PREFIX_OUTPUTS)
    SET(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}/lex.${PREFIX}.c")
    ADD_CUSTOM_COMMAND(
      OUTPUT "${OUTFILE}"
      COMMAND "${FLEX_EXECUTABLE}"
      ARGS "-P${PREFIX}"
      "-o${OUTFILE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}")
    SET_SOURCE_FILES_PROPERTIES("${OUTFILE}" PROPERTIES GENERATED TRUE)
  ENDMACRO(FLEX_FILE)
ENDIF(FLEX_EXECUTABLE)
