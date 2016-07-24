# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 04/25/2016
# 

cmake_minimum_required(VERSION 2.8)

rift_py3test(utest_monparams
    TEST_ARGS
    ${CMAKE_CURRENT_SOURCE_DIR}/utest_mon_params.py
    )
