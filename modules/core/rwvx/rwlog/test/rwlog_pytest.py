#!/usr/bin/env python
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file rwlog_pytest.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 11/02/2014
#
"""

import argparse
import logging
import os
import sys
import unittest
import gi
gi.require_version('RwLogYang', '1.0')
gi.require_version('RwGenericLogYang', '1.0')

from gi.repository import RwLogYang
from gi.repository import RwGenericLogYang
import xmlrunner
import rwlogger

class TestRwLog(unittest.TestCase):
    def setUp(self):
        self.rwlogger = rwlogger.RwLogger(subcategory="rw-appmgr-log")   # default cat
        self.logger = logging.getLogger(__name__)
        self.logger.addHandler(self.rwlogger)

    def test_rwlog_basic(self):
        self.assertNotEqual(rwlogger, None)

        ev = RwGenericLogYang.Warn()
        ev.log = "Basic log event"
        self.assertNotEqual(ev, None)

        self.rwlogger.log_event(ev);

    def test_log_warning(self):
        self.logger.warning("TEST...THIS IS A WARNING!")

    def test_log_error(self):
        self.logger.error("TEST...THIS IS AN ERROR!")


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')

    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
