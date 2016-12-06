#!/usr/bin/env python3
"""
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

@file test_wrapper.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 08/05/2015
@summary: A wrapper which imports and invokes a system test module parsed from the command line
"""

if __name__ == "__main__":
    import argparse
    import importlib
    import logging
    import sys

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)

    # Consume just the '--test-module' argument from argv
    parser = argparse.ArgumentParser()
    parser.add_argument('--test-module', help="system test module to import and invoke.")
    parsed, unparsed = parser.parse_known_args(sys.argv[1:])
    sys.argv[1:] = unparsed

    test_module = importlib.import_module('rasystem.{}'.format(parsed.test_module))
    test_module.main()
