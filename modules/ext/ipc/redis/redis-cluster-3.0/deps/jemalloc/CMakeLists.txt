# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

include (ExternalProject)

ExternalProject_add(jemallocmake
    DOWNLOAD_COMMAND ""
    PREFIX
    ${CMAKE_CURRENT_BINARY_DIR}/
    SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/
    --with-jemalloc-prefix=je_ --enable-cc-silence
    BUILD_COMMAND
    make
    )

