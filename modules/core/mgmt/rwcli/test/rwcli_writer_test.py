#!/usr/bin/python3

"""
#
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
#
# @file rwcli_writer_test.py
# @author Balaji Rajappa (balaji.rajappa@riftio.com)
# @date 2015/10/23
#
"""

from gi import require_version
require_version('RwYang', '1.0')

from gi.repository import RwYang as rwyang
import unittest
import logging
import argparse
import sys
import rift.rwcli_writer as cliwriter
import io
import xmlrunner
import os

logger = logging.getLogger('rwcli_writer_test')

class TestConfigWriter(unittest.TestCase):
    modules = [ 'rwcli_writer_test']
    def setUp(self):
        self.model = rwyang.Model.create_libncx() 
        for module in TestConfigWriter.modules:
            self.model.load_module(module)

    def tearDown(self):
        self.model = None

    def test_empty_config(self):
        expected_output = """config
end
"""
        out_stream = io.StringIO()
        xml_stream = io.BytesIO(b"""<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base"/>""")
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_multi_mode_list_with_keys(self):
        # Test multiple list with keys
        # Also list key without mode is also tested
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
  <colony xmlns="http://riftio.com/ns/yangtools/rwclitest">
    <name>trafgen</name>
    <network-context>
        <name>trafgen-lb</name>
        <interface>
            <name>N1TenGi-1</name>
            <ip>
                <address>11.0.1.4/24</address>
            </ip>
        </interface>
    </network-context>
  </colony>
</data>""")
        expected_output = """config
  colony trafgen
    network-context trafgen-lb
      interface N1TenGi-1
        ip 11.0.1.4/24
      exit
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_multi_mode_container(self):
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
  <colony xmlns="http://riftio.com/ns/yangtools/rwclitest">
    <name>trafgen</name>
    <port>
      <name>trafgen/2/1</name>
      <receive-q-length>2</receive-q-length>
      <open>
        <application><trafgen/></application>
      </open>
      <trafgen>
        <transmit-params>
            <transmit-mode><range/></transmit-mode>
        </transmit-params>
        <range-template>
            <packet-size>
            <start>512</start>
            <increment>1</increment>
            <maximum>512</maximum>
            <minimum>512</minimum>
            </packet-size>
            <source-ip>
                <start>11.0.1.4</start>
                <increment>1</increment>
                <minimum>11.0.1.4</minimum>
                <maximum>11.0.1.4</maximum>
            </source-ip>
            <destination-ip>
                <start>11.0.1.3</start>
                <increment>1</increment>
                <minimum>11.0.1.3</minimum>
                <maximum>11.0.1.3</maximum>
            </destination-ip>
        </range-template>
      </trafgen>
    </port>
  </colony>
</data>""")
        expected_output = """config
  colony trafgen
    port trafgen/2/1
      receive-q-length 2
      open  application trafgen
      trafgen
        transmit-params
          transmit-mode range
        exit
        range-template
          packet-size  start 512 increment 1 maximum 512 minimum 512
          source-ip  start 11.0.1.4 increment 1 minimum 11.0.1.4 maximum 11.0.1.4
          destination-ip  start 11.0.1.3 increment 1 minimum 11.0.1.3 maximum 11.0.1.3
        exit
      exit
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_multi_mode_ignore_unknown(self):
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
  <colony xmlns="http://riftio.com/ns/yangtools/rwclitest">
    <name>trafgen</name>
    <port>
      <name>trafgen/2/1</name>
      <receive-q-length>2</receive-q-length>
      <open>
        <application><trafgen/></application>
      </open>
      <trafsink>
        <transmit-params>
            <transmit-mode><range/></transmit-mode>
        </transmit-params>
        <range-template>
            <packet-size>
            <start>512</start>
            <increment>1</increment>
            <maximum>512</maximum>
            <minimum>512</minimum>
            </packet-size>
            <source-ip>
                <start>11.0.1.4</start>
                <increment>1</increment>
                <minimum>11.0.1.4</minimum>
                <maximum>11.0.1.4</maximum>
            </source-ip>
            <destination-ip>
                <start>11.0.1.3</start>
                <increment>1</increment>
                <minimum>11.0.1.3</minimum>
                <maximum>11.0.1.3</maximum>
            </destination-ip>
        </range-template>
      </trafsink>
    </port>
  </colony>
</data>""")
        expected_output = """config
  colony trafgen
    port trafgen/2/1
      receive-q-length 2
      open  application trafgen
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_mode_path_single(self):
        # Test multiple list with keys
        # Also list key without mode is also tested
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
  <vnf-config xmlns="http://riftio.com/ns/yangtools/rwclitest">
  <vnf>
    <name>trafgen</name>
    <instance>0</instance>
    <network-context>
        <name>trafgen-lb</name>
        <interface>
            <name>N1TenGi-1</name>
            <ip>
                <address>11.0.1.4/24</address>
            </ip>
            <ip>
                <address>11.0.1.5/24</address>
            </ip>
        </interface>
    </network-context>
  </vnf>
  </vnf-config>
</data>""")
        expected_output = """config
  vnf-config vnf trafgen 0
    network-context trafgen-lb
      interface N1TenGi-1
        ip 11.0.1.4/24
        ip 11.0.1.5/24
      exit
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_mode_path_multi_list(self):
        # Test multiple list with keys
        # Also list key without mode is also tested
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
  <vnf-config xmlns="http://riftio.com/ns/yangtools/rwclitest">
  <vnf>
    <name>trafgen</name>
    <instance>0</instance>
    <network-context>
        <name>trafgen-lb</name>
        <interface>
            <name>N1TenGi-1</name>
            <ip>
                <address>11.0.1.4/24</address>
            </ip>
        </interface>
    </network-context>
  </vnf>
  <vnf>
    <name>trafsink</name>
    <instance>1</instance>
    <network-context>
        <name>trafsink-lb</name>
        <interface>
            <name>N1TenGi-2</name>
            <ip>
                <address>11.0.1.5/24</address>
            </ip>
        </interface>
    </network-context>
  </vnf>
  </vnf-config>
</data>""")
        expected_output = """config
  vnf-config vnf trafgen 0
    network-context trafgen-lb
      interface N1TenGi-1
        ip 11.0.1.4/24
      exit
    exit
  exit
  vnf-config vnf trafsink 1
    network-context trafsink-lb
      interface N1TenGi-2
        ip 11.0.1.5/24
      exit
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_list_key_path_single(self):
        # Test multiple list with keys
        # Also list key without mode is also tested
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
    <cloud xmlns="http://riftio.com/ns/yangtools/rwclitest">
        <account>
            <name>c1</name>
            <account-type>cloudsim</account-type>
        </account>
    </cloud>
</data>""")
        expected_output = """config
  cloud account c1 account-type cloudsim
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_list_key_path_multi(self):
        # Test multiple list with keys
        # Also list key without mode is also tested
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
    <cloud xmlns="http://riftio.com/ns/yangtools/rwclitest">
        <account>
            <name>c1</name>
            <account-type>cloudsim</account-type>
        </account>
        <account>
            <name>c2</name>
            <account-type>openstack</account-type>
        </account>
        <account>
            <name>c3</name>
            <account-type>cloudsim</account-type>
        </account>
    </cloud>
</data>""")
        expected_output = """config
  cloud account c1 account-type cloudsim
  cloud account c2 account-type openstack
  cloud account c3 account-type cloudsim
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_list_within_list_one(self):
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
    <network xmlns="http://riftio.com/ns/yangtools/rwclitest">
        <network-id>l2net1</network-id>
        <node>
            <node-id>node1</node-id>
            <termination-point>
                <tp-id>tp1</tp-id>
                <attributes>
                    <maximum-frame-size>1500</maximum-frame-size>
                    <eth-encapsulation>vxlan</eth-encapsulation>
                </attributes>
            </termination-point>
        </node>
    </network>
</data>""")
        expected_output = """config
  network l2net1 node node1 termination-point tp1 attributes  maximum-frame-size 1500 eth-encapsulation vxlan
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)
        
    def test_list_within_list_multi(self):
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
    <network xmlns="http://riftio.com/ns/yangtools/rwclitest">
        <network-id>l2net1</network-id>
        <node>
            <node-id>node1</node-id>
            <termination-point>
                <tp-id>tp1</tp-id>
                <attributes>
                    <maximum-frame-size>1500</maximum-frame-size>
                    <eth-encapsulation>vxlan</eth-encapsulation>
                </attributes>
            </termination-point>
        </node>
    </network>
    <network xmlns="http://riftio.com/ns/yangtools/rwclitest">
        <network-id>l2net2</network-id>
        <node>
            <node-id>node2</node-id>
            <termination-point>
                <tp-id>tp2</tp-id>
                <attributes>
                    <maximum-frame-size>1500</maximum-frame-size>
                    <eth-encapsulation>ethernet</eth-encapsulation>
                </attributes>
            </termination-point>
        </node>
    </network>
</data>""")
        expected_output = """config
  network l2net1 node node1 termination-point tp1 attributes  maximum-frame-size 1500 eth-encapsulation vxlan
  network l2net2 node node2 termination-point tp2 attributes  maximum-frame-size 1500 eth-encapsulation ethernet
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_list_empty(self):
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
    <vnf-config xmlns="http://riftio.com/ns/yangtools/rwclitest">
    <vnf>
        <name>trafgen</name>
        <instance>0</instance>
        <network-context>
            <name>trafgen-lb</name>
            <interface>
                <name>N1TenGi-1</name>
            </interface>
        </network-context>
    </vnf>
    </vnf-config>
</data>
""")
        expected_output="""config
  vnf-config vnf trafgen 0
    network-context trafgen-lb
      interface N1TenGi-1
      exit
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

    def test_child_has_same_keyword(self):
        xml_stream = io.BytesIO(b"""
<data xmlns="http://riftio.com/ns/riftware-1.0/rw-base">
  <colony xmlns="http://riftio.com/ns/yangtools/rwclitest">
    <name>trafgen</name>
    <port>
      <name>trafgen/2/1</name>
      <trafgen>
        <diameter-interface>
            <diameter-interface>Gx</diameter-interface>
        </diameter-interface>
      </trafgen>
    </port>
  </colony>
</data>""")
        expected_output = """config
  colony trafgen
    port trafgen/2/1
      trafgen
        diameter-interface Gx
      exit
    exit
  exit
end
"""
        out_stream = io.StringIO()
        writer = cliwriter.ConfigWriter(self.model, xml_stream)
        writer.write(out_stream)
        logger.debug("Outstream=%s", out_stream.getvalue())
        self.assertEqual(out_stream.getvalue(), expected_output)

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='%(asctime)s:%(levelname)s:%(name)s:%(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('unittest_args', nargs='*')
    args = parser.parse_args(argv)
    logger.setLevel(logging.DEBUG if args.verbose else logging.ERROR)
 
    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + args.unittest_args,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))


if __name__ == '__main__':
    main()

# vim: ts=4 sw=4 et 
