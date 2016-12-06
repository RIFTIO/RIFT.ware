#!/usr/bin/python

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
