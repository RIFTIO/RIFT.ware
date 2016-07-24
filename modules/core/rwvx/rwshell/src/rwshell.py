#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Rajesh Ramankutty
# Creation Date: 2015/05/18
# 

"""
This is a skeletal python tool that invokes the rwshell plugin
to perform cloud operations.
"""

import argparse
import os
import sys
import logging

from gi.repository import GObject, Peas, GLib, GIRepository
from gi.repository import RwShell, RwTypes

def main(args=sys.argv[1:]):
    logging.basicConfig(format='RWSHELL %(message)s')

    ##
    # Command line argument specification
    ##
    desc="""This tool is used to implement variour shell based tools"""
    parser = argparse.ArgumentParser(description=desc)
    subparsers = parser.add_subparsers()

    assert status == RwTypes.RwStatus.SUCCESS

if __name__ == "__main__":
    main()
