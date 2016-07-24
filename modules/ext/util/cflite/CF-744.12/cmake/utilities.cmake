################################################################################
# Generates an object file with an arbitrary binary file embedded inside.
# 
# @param FILEPATH     the path to the file to embed
# @param OBJECT_FILEPATH output path for the object file
# @param SYMBOL_NAME  arbitrary prefix for the symbols that will be defined in
#                     the generated object file. 
#                     The following symbols will be defined:
#                      - [SYMBOL_NAME]_start
#                      - [SYMBOL_NAME]_end
#                      - [SYMBOL_NAME]_size
#
# @see http://www.burtonini.com/blog/computers/ld-blobs-2007-07-13-15-50
#
function (ConvertBinaryFileToObjectFile FILEPATH OBJECT_FILEPATH SYMBOL_NAME)
  get_filename_component(FILEPATH ${FILEPATH} ABSOLUTE)
  get_filename_component(parent_dir ${FILEPATH} PATH)
  get_filename_component(filename ${FILEPATH} NAME)
  get_filename_component(object_dir ${OBJECT_FILEPATH} PATH)

  string(REGEX REPLACE "[^a-zA-Z_]" "_" raw_symbol "${filename}")

  add_custom_command(
    COMMENT "Objectifying ${FILEPATH} => ${OBJECT_FILEPATH}"
     OUTPUT ${OBJECT_FILEPATH}
    DEPENDS ${FILEPATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${object_dir}
    COMMAND ${CMAKE_LINKER} ARGS -r -b binary -o ${OBJECT_FILEPATH} ${filename}
    COMMAND ${CMAKE_OBJCOPY}
       ARGS --rename-section .data=.rodata,alloc,load,readonly,data,contents 
            --redefine-sym=_binary_${raw_symbol}_start=${SYMBOL_NAME}_start
            --redefine-sym=_binary_${raw_symbol}_end=${SYMBOL_NAME}_end
            --redefine-sym=_binary_${raw_symbol}_size=${SYMBOL_NAME}_size
            ${OBJECT_FILEPATH} ${OBJECT_FILEPATH}
    WORKING_DIRECTORY ${parent_dir}
    VERBATIM
  )
endfunction ()
