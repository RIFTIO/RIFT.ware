#!/usr/bin/env python3

# STANDARD_RIFT_IO_COPYRIGHT


# Standard libraries
import argparse
import logging
import os
import sys
import unittest

# 3rd party libraries
import mock
import ndl
import xmlrunner

# Internal libraries
import rift.vcs.pool

logger = logging.getLogger(__name__)


class TestAddressPool(unittest.TestCase):
    def test_static_pool(self):
        addresses = [
                '10.0.0.1',
                '10.0.0.2']

        pool = rift.vcs.pool.StaticAddressPool(addresses)

        # Check the starting number of addresses
        self.assertEqual(2, len(pool._addresses))
        self.assertEqual(0, len(pool._released))

        # Acquire 2 addresses and check the lists
        addr1 = pool.acquire()
        addr2 = pool.acquire()
        self.assertEqual(0, len(pool._addresses))
        self.assertEqual(2, len(pool._released))

        # Acquiring more than the available number of addresses will raise an
        # exception
        with self.assertRaises(rift.vcs.pool.NoAvailableAddresses):
            pool.acquire()

        # Trying to release an address that did not come from the pool will
        # raise an exception
        with self.assertRaises(rift.vcs.pool.UnrecognizedAddress):
            pool.release('10.0.0.3')

        # Release the addresses
        pool.release(addr1)
        pool.release(addr2)

        # Check that the pool is back in its initial state
        self.assertEqual(2, len(pool._addresses))
        self.assertEqual(0, len(pool._released))

        # Trying to release an address that has already been released will raise
        # an exception
        with self.assertRaises(rift.vcs.pool.UnrecognizedAddress):
            pool.release(addr1)

    def test_reserved_pool(self):
        addresses = [
                '10.0.0.1',
                '10.0.0.2']

        # Instead of making a call to the reservation system in a unit test,
        # which would be a bad idea, we mock the fuction used by the
        # ReservedAddressPool class to return a defined set of addresses.
        ndl.get_vms = mock.Mock(return_value=addresses)

        pool = rift.vcs.pool.ReservedAddressPool(
                username='nobody',
                num_addresses=len(addresses))

        # Check the starting number of addresses
        self.assertEqual(2, len(pool._addresses))
        self.assertEqual(0, len(pool._released))

        # Acquire 2 addresses and check the lists
        addr1 = pool.acquire()
        addr2 = pool.acquire()
        self.assertEqual(0, len(pool._addresses))
        self.assertEqual(2, len(pool._released))

        # Acquiring more than the available number of addresses will raise an
        # exception
        with self.assertRaises(rift.vcs.pool.NoAvailableAddresses):
            pool.acquire()

        # Trying to release an address that did not come from the pool will
        # raise an exception
        with self.assertRaises(rift.vcs.pool.UnrecognizedAddress):
            pool.release('10.0.0.3')

        # Release the addresses
        pool.release(addr1)
        pool.release(addr2)

        # Check that the pool is back in its initial state
        self.assertEqual(2, len(pool._addresses))
        self.assertEqual(0, len(pool._released))

        # Trying to release an address that has already been released will raise
        # an exception
        with self.assertRaises(rift.vcs.pool.UnrecognizedAddress):
            pool.release(addr1)

    def test_fake_pool(self):
        pool = rift.vcs.pool.FakeAddressPool(prefix='192.168.0.')

        # Check the starting number of addresses
        self.assertEqual(0, len(pool._addresses))
        self.assertEqual(0, len(pool._released))

        # Acquire 2 addresses and check the lists
        addr1 = pool.acquire()
        addr2 = pool.acquire()
        self.assertEqual('192.168.0.1', addr1)
        self.assertEqual('192.168.0.2', addr2)
        self.assertEqual(0, len(pool._addresses))
        self.assertEqual(2, len(pool._released))

        # Trying to release an address that did not come from the pool will
        # raise an exception
        with self.assertRaises(rift.vcs.pool.UnrecognizedAddress):
            pool.release('10.0.0.3')

        # Release the addresses
        pool.release(addr1)
        pool.release(addr2)

        # Check that the pool is back in its initial state
        self.assertEqual(2, len(pool._addresses))
        self.assertEqual(0, len(pool._released))

        # Trying to release an address that has already been released will raise
        # an exception
        with self.assertRaises(rift.vcs.pool.UnrecognizedAddress):
            pool.release(addr1)


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
