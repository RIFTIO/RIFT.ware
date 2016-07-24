#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import setuptools

setuptools.setup(
        name='vcs',
        author='Joshua Downer',
        author_email='joshua.downer@riftio.com',
        packages=[
            'rift',
            'rift/vcs',
            'rift/vcs/compiler',
            ],
        version='0.1.0',
        test_suite="tests",
        )
