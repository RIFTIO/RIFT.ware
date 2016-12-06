#!/usr/bin/env python

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


import os
import unittest

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
        # 110 is not used
        # 111 is used
        # 52 was not used on ganga
        self.assertFalse(rwlib.port_in_avoid_list(110, 1))
        self.assertTrue(rwlib.port_in_avoid_list(110, 2))

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
