# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Varun Prasad
# Creation Date: 10/30/2015
# 

cmake_minimum_required(VERSION 2.8)

rift_python_install_tree(
  FILES
    rift/test/__init__.py
    rift/test/dts.py
  PYTHON3_ONLY
  COMPONENT ${PKG_LONG_NAME})