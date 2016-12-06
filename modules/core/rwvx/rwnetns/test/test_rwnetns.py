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


import argparse
import logging
import os
import sys
import unittest
import xmlrunner
import tempfile
import urllib2
import re

from gi.repository import RwNetns


logger = logging.getLogger(__name__)

class TestRwNetns(unittest.TestCase):
    def setUp(self):
        self.original_fd = RwNetns.get_current_netfd()
        logger.debug("Original netfd: {}" .format(self.original_fd))
 
        if os.geteuid() != 0:
           exit("You need to have root privileges to run this script. Exiting.")

    def tearDown(self):
          logger.debug("Closing netfd: {}" .format(self.original_fd))
          os.close(self.original_fd)

    def test_netns_create(self):
        create_rc = RwNetns.create_context("foo")
        self.assertTrue(create_rc >= 0)
	logger.debug("Create Context Foo Rc:{}".format(create_rc))

	foo_fd = RwNetns.get_netfd("foo")
	self.assertTrue(foo_fd >= 0)
	logger.debug("Get foo fd: {}".format(foo_fd))

        delete_rc = RwNetns.delete_context("foo")
        self.assertEqual(delete_rc,0)
        logger.debug("Delete Context Foo Rc:{}" .format(delete_rc))

    def test_netns_change(self):
        create_rc = RwNetns.create_context("bar")
        self.assertTrue(create_rc >= 0)
	logger.debug("Create Context Bar Rc:{}" .format(create_rc))

	bar_fd = RwNetns.get_netfd("bar")
	self.assertTrue(bar_fd >= 0)
	logger.debug("Get bar fd: {}" .format(bar_fd))

	change_rc = RwNetns.change("bar")
	self.assertEqual(change_rc,0)
	logger.debug("Change Context Bar Rc:{}" .format(change_rc))

        current_fd = RwNetns.get_current_netfd()
	self.assertTrue(current_fd >= 0)
	logger.debug("Current netfd:{}" .format(current_fd))

        change_fd_rc = RwNetns.change_fd(self.original_fd)
        self.assertEqual(change_fd_rc,0)
        logger.debug("Change fd {} Rc: {}" .format(self.original_fd, change_fd_rc))

        delete_rc = RwNetns.delete_context("bar")
        self.assertEqual(delete_rc,0)
        logger.debug("Delete Context Foo Rc: {}" .format(delete_rc))

    def test_netns_check_dns(self):

        # Find nameserver address and use it
        fd = open('/etc/resolv.conf','r')
        self.nameserver_ip = None
	for elem in fd.readlines():
	    match = re.match('nameserver\s+(\S+)',elem)
	    if match:
	       logger.debug("Nameserver IP is {}" .format(match.group(1)))
	       self.nameserver_ip = match.group(1)

        if self.nameserver_ip:
           fake_nameserver = "nameserver "+ self.nameserver_ip + "\n"
        else:
	   fake_nameserver = "nameserver 1.1.1.1\n"
        new_resolvconf_hdl = tempfile.NamedTemporaryFile(delete=False)
	new_resolvconf_hdl_path = new_resolvconf_hdl.name
	new_resolvconf_hdl.write(fake_nameserver)
        new_resolvconf_hdl.close()
        logger.debug("Temp file name is :{}".format(new_resolvconf_hdl_path))

        RwNetns.mount_bind_resolv_conf(new_resolvconf_hdl_path)

	a = urllib2.urlopen("http://www.google.com", timeout=1)
        self.assertEqual(a.getcode(),200)
        logger.debug("URL return code is :{}".format(a.getcode()))

        os.remove(new_resolvconf_hdl_path)

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
