#!/usr/bin/env python
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
# @file test_yang_ext_gi.py
# @author Shaji Radhakrishnan(shaji.radhakrishnan@riftio.com)
# @date 28/05/2015
#
"""
import argparse
import logging
import os
import sys
import unittest
import xmlrunner
import gi
gi.require_version('RwFpathDYang', '1.0')
gi.require_version('CompanyYang', '1.0')
gi.require_version('YangModelPlugin', '1.0')

from gi.repository import RwFpathDYang, GLib
from gi.repository import CompanyYang, RwYang
import rw_peas

logger = logging.getLogger(__name__)

class TestRwFpathDStartTraffic(unittest.TestCase):
    def setUp(self):
        self.start = RwFpathDYang.StartTraffic()

    def test_start_traffic_protobuf_conversion(self):
        self.start.name = "test-name"
        self.start.traffic = RwFpathDYang.StartTraffic_Traffic()
        self.start.traffic.port_name = "riftport"

        # recreate a start using to_pbuf and from_pbuf methods
        pbuf = self.start.to_pbuf()
        recreated_start = RwFpathDYang.StartTraffic()
        recreated_start.from_pbuf(pbuf)
        print("Start           : ", self.start)
        print("Recreated Start : ", recreated_start)

        # check the newly created start is identical
        self.assertEqual(self.start.name, recreated_start.name)
        self.assertEqual(len(self.start.name), len(recreated_start.name))
        self.assertEqual(self.start.traffic.port_name, recreated_start.traffic.port_name)
        self.assertEqual(len(self.start.traffic.port_name), len(recreated_start.traffic.port_name))

        #ptr = recreated_start.to_ptr();

    def test_start_traffic_xml_conversion(self):
        # Open rw-fpath-d yang model      
        yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
        yang_model = yang.get_interface('Model')

        #yang_module = yang.get_interface('Module')
        #yang_node = yang.get_interface('Node')
        #yang_key = yang.get_interface('Key')

        model = yang_model.alloc()
        module = yang_model.load_module(model, "rw-fpath-d")

        #populate the start
        self.start.name = "test-name"
        self.start.traffic = RwFpathDYang.StartTraffic_Traffic()
        self.start.traffic.port_name = "riftport"

        # recreate a start using to_xml and from_xml methods
        xml_str = self.start.to_xml(model)
        recreated_start = RwFpathDYang.StartTraffic()
        recreated_start.from_xml(model, xml_str)
        print("Start           : ", self.start)
        print("Recreated Start : ", recreated_start)

        # check the newly created start is identical
        self.assertEqual(self.start.name, recreated_start.name)
        self.assertEqual(len(self.start.name), len(recreated_start.name))
        self.assertEqual(self.start.traffic.port_name, recreated_start.traffic.port_name)
        self.assertEqual(len(self.start.traffic.port_name), len(recreated_start.traffic.port_name))


    def test_fastpath_packet_trace_protobuf_conversion(self):
        self.fastpath_packet_trace = RwFpathDYang.YangNotif_RwFpathD_FastpathPacketTrace()
        self.fastpath_packet_trace.packet_info = str(bin(10203)).encode()
        pbuf = self.fastpath_packet_trace.to_pbuf()
        recreated_fastpath_packet_trace = RwFpathDYang.YangNotif_RwFpathD_FastpathPacketTrace()
        recreated_fastpath_packet_trace.from_pbuf(pbuf)
        print("FpathPacketTrace           : ", self.fastpath_packet_trace)
        print("Recreated FpathPacketTrace : ", recreated_fastpath_packet_trace)
        self.assertEqual(self.fastpath_packet_trace.packet_info, recreated_fastpath_packet_trace.packet_info)

    def test_fastpath_packet_trace_xml_conversion(self):
        yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
        yang_model = yang.get_interface('Model')
        model = yang_model.alloc()
        module = yang_model.load_module(model, "rw-fpath-d")

        self.fastpath_packet_trace = RwFpathDYang.YangNotif_RwFpathD_FastpathPacketTrace()
        self.fastpath_packet_trace.packet_info = str(bin(10203)).encode()

        # recreate a start using to_xml and from_xml methods
        xml_str = self.fastpath_packet_trace.to_xml(model)
        recreated_fastpath_packet_trace = RwFpathDYang.YangNotif_RwFpathD_FastpathPacketTrace()
        recreated_fastpath_packet_trace.from_xml(model, xml_str)
        print("FpathPacketTrace           : ", self.fastpath_packet_trace)
        print("Recreated FpathPacketTrace : ", recreated_fastpath_packet_trace)
        self.assertEqual(self.fastpath_packet_trace.packet_info, recreated_fastpath_packet_trace.packet_info)

    def test_str_max_inline_max_protobuf_conversion(self):
        self.start.name = "123456789 123456789 123456789 123456789 123456789 123456789 123456789"
        # recreate a start using to_pbuf and from_pbuf methods
        pbuf = self.start.to_pbuf()
        recreated_start = RwFpathDYang.StartTraffic()
        recreated_start.from_pbuf(pbuf)
        print("Start           : ", self.start)
        print("Recreated Start : ", recreated_start)

        # check the newly created start is identical
        self.assertEqual(self.start.name, recreated_start.name)
        self.assertEqual(len(self.start.name), len(recreated_start.name))

        start = None
        recreated_start = None
        fastpath_packet_trace = None
        recreated_fastpath_packet_trace = None

class TestRwFpathDColonyBundleEther(unittest.TestCase):
    def setUp(self):
        self.colony = RwFpathDYang.ConfigColony()

    def test_colony_interface_max_protobuf_conversion(self):
        self.colony.name = "riftcolony"
        nc = self.colony.network_context.add()
        nc.name = "trafgen"
        inf = []
        for i in range(33):
          inf.append(nc.interface.add())
          inf[i].name = "TenG"+str(i)
        pbuf = self.colony.to_pbuf()
        recreated_colony = RwFpathDYang.ConfigColony()
        recreated_colony.from_pbuf(pbuf)
        print("Colony           : ", self.colony)
        print("Recreated Colony : ", recreated_colony)
        # check the newly created start is identical
        self.assertEqual(self.colony.name, recreated_colony.name)
        self.assertEqual(nc.name, recreated_colony.network_context[0].name)
        print(len(recreated_colony.network_context))
        self.assertEqual(len(recreated_colony.network_context), 1)
        print(len(recreated_colony.network_context[0].interface))
        self.assertEqual(len(recreated_colony.network_context[0].interface), 33)
        for i in range(len(recreated_colony.network_context[0].interface)):
          self.assertEqual(inf[i].name, recreated_colony.network_context[0].interface[i].name)

    def test_colony_lbprofile_max_protobuf_conversion(self):
        self.colony.name = "riftcolony"
        nc = self.colony.network_context.add()
        nc.name = "trafgen"
        lbprof = []
        for i in range(33):
          lbprof.append(nc.lb_profile.add())
          lbprof[i].name = "DiameterLB"+str(i)
        pbuf = self.colony.to_pbuf()
        recreated_colony = RwFpathDYang.ConfigColony()
        recreated_colony.from_pbuf(pbuf)
        print("Colony           : ", self.colony)
        print("Recreated Colony : ", recreated_colony)
        # check the newly created start is identical
        self.assertEqual(self.colony.name, recreated_colony.name)
        self.assertEqual(nc.name, recreated_colony.network_context[0].name)
        print(len(recreated_colony.network_context))
        self.assertEqual(len(recreated_colony.network_context), 1)
        print(len(recreated_colony.network_context[0].lb_profile))
        self.assertEqual(len(recreated_colony.network_context[0].lb_profile), 32)
        for i in range(len(recreated_colony.network_context[0].lb_profile)):
          self.assertEqual(lbprof[i].name, recreated_colony.network_context[0].lb_profile[i].name)


class TestToAndFromXMLConversion(unittest.TestCase):
  """
  RIFT-8651 Removed a conditional check from xml to pb and pb to xml
  code which prohibited users to use the conversion if the path to be converted
  extended beyond list.
  """
  def setUp(self):
    self.model = RwYang.Model.create_libncx()
    self.model.load_schema_ypbc(CompanyYang.get_schema())
    self.comp_emp_info_object = CompanyYang.Company_Employee()
    self.xml = '''
    <data xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
    <company xmlns="http://riftio.com/ns/core/util/yangtools/tests/company">
      <employee>
        <extra-curriculum>
          <interest>sleeping</interest>
        </extra-curriculum>
      </employee>
    </company>
    </data>
    '''

  def test_xml_api_for_listy_path(self):
    self.comp_emp_info_object.from_xml_v2(self.model, self.xml)
    emp_cur = self.comp_emp_info_object.get_extra_curriculum()
    self.assertEqual(emp_cur.interest, "sleeping")

    expected_resp_xml = """<company:company xmlns:company="http://riftio.com/ns/core/util/yangtools/tests/company"><company:employee><company:extra-curriculum><company:interest>sleeping</company:interest></company:extra-curriculum></company:employee></company:company>"""

    resp = self.comp_emp_info_object.to_xml_v2(self.model)
    self.assertEqual(resp, expected_resp_xml)

  def test_xml_api_for_non_listy_path(self):
    self.comp_emp_info_object = CompanyYang.Company_Employee_ExtraCurriculum()
    self.comp_emp_info_object.from_xml_v2(self.model, self.xml)
    self.assertEqual(self.comp_emp_info_object.interest, "sleeping")
    expected_resp_xml = """<company:company xmlns:company="http://riftio.com/ns/core/util/yangtools/tests/company"><company:employee><company:extra-curriculum><company:interest>sleeping</company:interest></company:extra-curriculum></company:employee></company:company>"""
    resp = self.comp_emp_info_object.to_xml_v2(self.model)

    self.assertEqual(resp, expected_resp_xml)

  def test_xml_api_for_pretty_print(self):
    self.comp_emp_info_object.from_xml_v2(self.model, self.xml)
    emp_cur = self.comp_emp_info_object.get_extra_curriculum()
    self.assertEqual(emp_cur.interest, "sleeping")

    expected_resp_xml = """
<company:company xmlns:company="http://riftio.com/ns/core/util/yangtools/tests/company">

  <company:employee>
    <company:extra-curriculum>
      <company:interest>sleeping</company:interest>
    </company:extra-curriculum>
  </company:employee>

</company:company>"""

    resp = self.comp_emp_info_object.to_xml_v2(self.model, pretty_print=True)
    self.assertEqual(resp, expected_resp_xml)


class TestListKeyOrder(unittest.TestCase):
  def setUp(self):
    self.model = RwYang.Model.create_libncx()
    self.model.load_schema_ypbc(CompanyYang.get_schema())

  def test_list_key_order(self):
    comp = CompanyYang.Company()
    dep = comp.department.add()
    dep.name = 'engineering'
    dep.id = 1
    dep.level = 5
    dep.director = 'Wolverine'

    expected_resp_xml = """
<company:company xmlns:company="http://riftio.com/ns/core/util/yangtools/tests/company">

  <company:department>
    <company:id>1</company:id>
    <company:name>engineering</company:name>
    <company:level>5</company:level>
    <company:director>Wolverine</company:director>
  </company:department>

</company:company>"""

    resp = comp.to_xml_v2(self.model, pretty_print=True)
    self.assertEqual(resp, expected_resp_xml)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    if False:
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
