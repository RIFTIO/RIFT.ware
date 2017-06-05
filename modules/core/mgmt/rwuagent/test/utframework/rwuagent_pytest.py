#!/usr/bin/env python3
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file rwuagent_pytest.py
# @author Sujithra Periasamy
# @date 11/03/2015
#
"""
import logging
import os
import sys
import unittest
import xmlrunner
from subprocess import Popen, DEVNULL
import signal
import argparse
import time
import gi
import rift.auto.proxy
import random
from rift.auto.session import NetconfSession
from random import choice
from string import ascii_lowercase
import xml.etree.ElementTree as ET

from gi.repository import GLib

gi.require_version('RwYang', '1.0')
from gi.repository import RwYang

gi.require_version('InterfacesYang', '1.0')
gi.require_version('DnsYang', '1.0')
gi.require_version('RoutesYang', '1.0')
gi.require_version('UtCompositeYang', '1.0')
gi.require_version('RwMgmtagtYang', '1.0')
from gi.repository import InterfacesYang, DnsYang, RoutesYang, UtCompositeYang, RwMgmtagtYang

logger = logging.getLogger(__name__)

WAIT_TIME = 45 #seconds

class TestMgmtAgent(unittest.TestCase):

    sys_proc=None
    model=None
    mgmt_session=None
    mgmt_proxy=None
    confd_host="127.0.0.1"

    @classmethod
    def setUpClass(cls):
       # Start the mgmt-agent system test in background

       install_dir = os.environ.get("RIFT_INSTALL")
       demo_dir = os.path.join(install_dir, 'demos')
       testbed = os.path.join(demo_dir, 'mgmt_tbed.py')

       tmp_dir = install_dir + "/tmp"
       if not os.path.exists(tmp_dir):
          os.makedirs(tmp_dir)

       logfile = tmp_dir + "/logfile"
       cls.fd = open(logfile, "w")

       try:
         cls.sys_proc=Popen([testbed], stdin=DEVNULL, stdout=cls.fd, stderr=cls.fd,
                             preexec_fn=os.setsid)
       except Exception as e:
         print("Failed to start system test.. error: {0}".format(e))
         raise

       print("Started the Mgmt Agent Mini System Test..sleeping for {} secs".format(WAIT_TIME))
       time.sleep(WAIT_TIME)
       # TODO: Validate whether everything has started.

       cls.mgmt_session=NetconfSession(cls.confd_host)
       cls.mgmt_session.connect()

       cls.model = RwYang.Model.create_libncx()
       cls.model.load_schema_ypbc(UtCompositeYang.get_schema())

    def test_edit_config(self):
       interface=InterfacesYang.Interface()
       interface.name="eth0"
       interface.enabled="true"
       interface.speed="hundred"
       interface.duplex="full"
       interface.mtu=1500
       mgmt_proxy=self.mgmt_session.proxy(InterfacesYang)
       mgmt_proxy.merge_config("/interfaces", interface)
 
       interface.name="eth1"
       interface.enabled="true"
       interface.speed="ten"
       interface.duplex="half"
       interface.mtu=1500
       mgmt_proxy.merge_config("/interfaces", interface)

       ret_obj=mgmt_proxy.get_config("/interfaces")
       self.assertTrue(ret_obj)
       self.assertEqual(len(ret_obj.get_interface()), 2)
       print(ret_obj)

    def test_get(self):
       mgmt_proxy=self.mgmt_session.proxy(InterfacesYang)
       ret_obj=mgmt_proxy.get("/interfaces")
       self.assertTrue(ret_obj)
       print(ret_obj)

    def test_rpc(self):
        mgmt_proxy=self.mgmt_session.proxy(InterfacesYang)
        rpc=InterfacesYang.ClearInterface()
        rpc.interface.name="eth0"
        rpc.interface.receive=True
        ret_obj=mgmt_proxy.rpc(rpc)
        self.assertTrue(ret_obj)
        self.assertEqual(ret_obj.status, "Success")
        print(ret_obj)

    def test_notification(self):
        mgmt_proxy=self.mgmt_session.proxy(InterfacesYang)

        print("Raise new route notification")
        rpc = InterfacesYang.RaiseNewRoute()
        rpc.notif = True
        ret_obj = mgmt_proxy.rpc(rpc)
        self.assertTrue(ret_obj)
        self.assertEqual(ret_obj.status, "Success")
        print(ret_obj)

        print("Raise temperature threshold alarm")
        rpc = InterfacesYang.RaiseTempAlarm()
        rpc.notif = True
        ret_obj = mgmt_proxy.rpc(rpc)
        self.assertTrue(ret_obj)
        self.assertEqual(ret_obj.status, "Success")
        print(ret_obj)

    def test_dts_rpcs_edit_cfg(self):
        # Send an edit-config via agent rpc.
        ec_rpc=RwMgmtagtYang.YangInput_RwMgmtagt_MgmtAgent()
        ec_rpc.pb_request.xpath = "C,/dns:dns"
        dnspb=DnsYang.Dns()
        s1=dnspb.search.add()
        s1.name=100
        s1.domain="abc.com"
        serv1=dnspb.server.add()
        serv1.address="10.0.1.9"
        dnspb.options.ndots=2
        dnspb.options.timeout=30
        dnspb.options.attempts=5
        serv2=dnspb.server.add()
        serv2.address="10.0.1.10"
        dnspb.options.ndots=3
        dnspb.options.timeout=40
        dnspb.options.attempts=5
        ec_rpc.pb_request.data=dnspb.to_pbuf()
        ec_rpc.pb_request.request_type="edit_config"
        ec_rpc.pb_request.edit_type="merge"
        mgmt_proxy=self.mgmt_session.proxy(RwMgmtagtYang)
        ret_obj=mgmt_proxy.rpc(ec_rpc)
        self.assertTrue(ret_obj)
        print(ret_obj)

    def test_dts_rpcs_del_cfg(self): 
        self.test_dts_rpcs_edit_cfg();
        # Send a delete cfg rpc
        dc_rpc=RwMgmtagtYang.YangInput_RwMgmtagt_MgmtAgent()
        dc_rpc.pb_request.xpath = "C,/dns:dns/dns:server[dns:address='10.0.1.10']"
        dnspb=DnsYang.Dns()
        serv1=dnspb.server.add()
        serv1.address="10.0.1.10"
        dc_rpc.pb_request.data=dnspb.to_pbuf()
        dc_rpc.pb_request.request_type="edit_config"
        dc_rpc.pb_request.edit_type="delete"
        mgmt_proxy=self.mgmt_session.proxy(RwMgmtagtYang)
        ret_obj=mgmt_proxy.rpc(dc_rpc)
        self.assertTrue(ret_obj)
        print(ret_obj)

    def test_large_binary_blob(self):
        routes=RoutesYang.Routes()
        routes.binaryblob=bytearray(random.getrandbits(8) for _ in range(5000))
        routes.bigstring=''.join([choice(ascii_lowercase) for _ in range(10000)])
        mgmt_proxy=self.mgmt_session.proxy(RoutesYang)
        mgmt_proxy.merge_config("/routes", routes)
        ret_obj=mgmt_proxy.get_config("/routes")
        self.assertTrue(ret_obj)
        self.assertEqual(len(ret_obj.binaryblob), 5000)
        self.assertEqual(len(ret_obj.bigstring), 10000)
        
    @classmethod
    def tearDownClass(cls):
        print("Stopping the Mgmt Agent Mini System Test..")
        cls.sys_proc.terminate()
        cls.fd.close()

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
