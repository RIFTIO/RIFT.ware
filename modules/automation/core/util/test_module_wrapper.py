#!/usr/bin/env python3
"""
# STANDARD_RIFT_IO_COPYRIGHT #

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
