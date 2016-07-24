
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

macro(add_target_gir TARGET_NAME GIR_NAME HEADER C_FILES CFLAGS PKG_VERSION)
    set(PACKAGES "")
    foreach(PKG ${ARGN})
        set(PACKAGES ${PACKAGES} --include=${PKG})
    endforeach()

    set(ENV{LD_LIBRARY_PATH} \"${CMAKE_CURRENT_BINARY_DIR}:\$ENV{LD_LIBRARY_PATH}\")

    set(PKG_GIR_NAME ${GIR_NAME}-${PKG_VERSION})

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PKG_GIR_NAME}.gir
            COMMAND ${INTROSPECTION_SCANNER} ${CFLAGS} -n ${GIR_NAME}
            --library ${TARGET_NAME} ${PACKAGES}
            --warn-all
            -o ${CMAKE_CURRENT_BINARY_DIR}/${PKG_GIR_NAME}.gir
            -L${CMAKE_CURRENT_BINARY_DIR}
            --nsversion=${PKG_VERSION} ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER} ${C_FILES})

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PKG_GIR_NAME}.typelib
            COMMAND ${INTROSPECTION_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/${PKG_GIR_NAME}.gir 
            -o ${CMAKE_CURRENT_BINARY_DIR}/${PKG_GIR_NAME}.typelib
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PKG_GIR_NAME}.gir)

endmacro()
