#!/usr/bin/env python3

import logging
import unittest
import xmlrunner
import sys
import os
import argparse
from ncclient.operations import util

from ncclient.operations.errors import OperationError

import time
import datetime
from datetime import timezone
from datetime import timedelta

logger = logging.getLogger(__name__)

class TestParseDateTime(unittest.TestCase):
    def test_basic_utc(self):
        dt = util.parse_datetime("2016-02-08T09:12:34Z")
        expected = datetime.datetime(2016, 2, 8, 9, 12, 34, 0, timezone.utc)
        self.assertEqual(dt, expected)

        dt = util.parse_datetime("2016-02-08T09:12:34+00:00")
        self.assertEqual(dt, expected)

        dt = util.parse_datetime("2016-02-08t23:12:34z")
        expected = datetime.datetime(2016, 2, 8, 23, 12, 34, 0, timezone.utc)
        self.assertEqual(dt, expected)

        dt = util.parse_datetime("2016-02-08T09:12:34.123Z")
        expected = datetime.datetime(2016, 2, 8, 9, 12, 34, 123000, timezone.utc)
        self.assertEqual(dt, expected)

    def test_time_offset(self):
        dt = util.parse_datetime("2016-02-08T09:12:34-05:00")
        expected = datetime.datetime(2016, 2, 8, 9, 12, 34, 0,
                        timezone(timedelta(hours=-5)))
        self.assertEqual(dt, expected)

        dt = util.parse_datetime("2016-02-08T09:12:34.314156+05:30")
        expected = datetime.datetime(2016, 2, 8, 9, 12, 34, 314156,
                        timezone(timedelta(hours=5, minutes=30)))
        self.assertEqual(dt, expected)

        dt = util.parse_datetime("2011-01-04T12:30:46")
        expected = datetime.datetime(2011, 1, 4, 12, 30, 46, 0, timezone.utc)
        self.assertEqual(dt, expected)

    def test_invalid(self):
        
        with self.assertRaises(OperationError) as context:
            dt = util.parse_datetime("2016-02-08T09:12")

        with self.assertRaises(OperationError) as context:
            dt = util.parse_datetime("2016/02/08 09:12:34")

        with self.assertRaises(OperationError) as context:
            dt = util.parse_datetime("2016-02-08T09:12:34 EST")

        with self.assertRaises(OperationError) as context:
            dt = util.parse_datetime("2016-02-08T09:12:34+25:12")

class TestDatetimeToString(unittest.TestCase):
    def test_basic_utc(self):
        dt_str = util.datetime_to_string(datetime.datetime(2016, 2, 8, 9, 12, 34, 0))
        self.assertEqual(dt_str, "2016-02-08T09:12:34Z")

        dt_str = util.datetime_to_string(
                    datetime.datetime(2016, 2, 8, 9, 12, 34, 0, timezone.utc))
        self.assertEqual(dt_str, "2016-02-08T09:12:34+00:00")

    def test_time_offsets(self):
        dt = datetime.datetime(2016, 2, 8, 9, 12, 34, 314156,
                        timezone(timedelta(hours=5, minutes=30)))
        dt_str = util.datetime_to_string(dt)
        self.assertEqual(dt_str, "2016-02-08T09:12:34.314156+05:30")

        dt = datetime.datetime(2016, 2, 8, 9, 12, 34, 314156,
                        timezone(timedelta(hours=-5)))
        dt_str = util.datetime_to_string(dt)
        self.assertEqual(dt_str, "2016-02-08T09:12:34.314156-05:00")

class TestDateTimeNow(unittest.TestCase):
    def test_basic(self):
        dt_now = util.datetime_now()

        is_dst = time.daylight and time.localtime().tm_isdst
        utc_offset = -time.altzone if is_dst else -time.timezone
        self.assertEqual(dt_now.utcoffset(), timedelta(seconds=utc_offset))


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('unittest_args', nargs='*')
    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + args.unittest_args,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == "__main__":
    main()

# vim: ts=4 sw=4 et
