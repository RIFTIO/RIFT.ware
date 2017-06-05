# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Varun Prasad
# Creation Date: 10/30/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

rift_python_install_tree(
  FILES
    rift/test/__init__.py
    rift/test/dts.py
  PYTHON3_ONLY
  COMPONENT ${INSTALL_COMPONENT})
