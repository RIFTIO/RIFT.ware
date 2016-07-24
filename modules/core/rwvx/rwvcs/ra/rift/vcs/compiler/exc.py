"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file exc.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 03/25/2015
#

The classes in this file define exceptions that are raised by the compiler.

"""


class CompilationError(Exception):
    pass


class ConstraintError(CompilationError):
    pass


class TransformError(CompilationError):
    pass

