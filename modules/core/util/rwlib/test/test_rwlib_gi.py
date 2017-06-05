#!/usr/bin/env python

# STANDARD_RIFT_IO_COPYRIGHT


import os
import unittest
import socket

import xmlrunner
import gi
gi.require_version('rwlib', '1.0')
import gi.repository.rwlib as rwlib
import gi.repository.RwTypes as rwtypes

class RwSysTest(unittest.TestCase):
    """
    This just tests the basic functionality going through GI.  More extensive
    testing is done in rwlib_utest.cc
    """
    def test_instance_uid(self):
        old_uid = os.environ.get('RIFT_INSTANCE_UID')
        if old_uid is not None:
            os.unsetenv('RIFT_INSTANCE_UID')

        status, uid = rwlib.instance_uid()
        self.assertEqual(status, rwtypes.RwStatus.SUCCESS)
        self.assertLess(uid, rwlib.MAXIMUM_INSTANCES)
        #self.assertGreater(uid, rwlib.RESERVED_INSTANCES)

        if old_uid is not None:
            os.environ['RIFT_INSTANCE_UID'] = old_uid

    def test_unique_port(self):
        old_uid = os.environ.get('RIFT_INSTANCE_UID')
        os.environ['RIFT_INSTANCE_UID'] = '10'

        status, port= rwlib.unique_port(100)
        self.assertEqual(status, rwtypes.RwStatus.SUCCESS)
        self.assertEqual(port, 110)

        if old_uid is not None:
            os.environ['RIFT_INSTANCE_UID'] = old_uid

    def test_port_in_avoid_list(self):
        #Open a socket, bind it to IPADDR_ANY(use ephemeral port, listen, get sock info,
        #and then test the port in avoid list using the port allocated for the socket.
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        except socket.error as msg:
            s = None
        if (s is not None):
            s.listen(1)
            (host, port) = s.getsockname()
            self.assertTrue(rwlib.port_in_avoid_list(port-1, 2))
            s.close()

    def test_setenv(self):
        os.environ['RW_VAR_RIFT'] = '/tmp'
        self.assertEqual(rwlib.setenv("ab/c", "abc"), rwtypes.RwStatus.FAILURE)
        self.assertEqual(rwlib.setenv("abc", "abc"), rwtypes.RwStatus.SUCCESS)
        self.assertEqual(rwlib.setenv("abc", "123"), rwtypes.RwStatus.SUCCESS)
        self.assertEqual(rwlib.getenv("abc"), "123")
        rwlib.unsetenv("abc")

def main():
    unittest.main(testRunner=xmlrunner.XMLTestRunner(output=os.environ['RIFT_MODULE_TEST']))

if __name__ == '__main__':
    main()

# vim: sw=4
