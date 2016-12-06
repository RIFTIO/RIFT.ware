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
# @file test_pb_cli_gi.py
# @author Shaji Radhakrishnan(Shaji.Radhakrishnan@riftio.com)
# @date 2015/03/16
#
"""

import argparse
import logging
import os
import io
import sys
import unittest
import gi
gi.require_version('RwFpathDYang', '1.0')
gi.require_version('RwAppmgrDYang', '1.0')
gi.require_version('YangModelPlugin', '1.0')

from gi.repository import RwFpathDYang, RwAppmgrDYang
import xmlrunner
import rw_peas

yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
yang_model_api = yang.get_interface('Model')
yang_model = yang_model_api.alloc()
base_module = yang_model_api.load_module(yang_model, 'rw-base-d')
fpath_module = yang_model_api.load_module(yang_model, 'rw-fpath-d')
app_module = yang_model_api.load_module(yang_model, 'rw-appmgr-d')

class TestPbToCli(unittest.TestCase):
    def setUp(self):
        self.colony = RwFpathDYang.ConfigColony()
        self.colony1 = RwAppmgrDYang.ConfigColony()

    def fill_colony(self):
        self.colony.name = "trafgen"
        nc = self.colony.network_context.add()
        nc.name = "trafgen"
        inf = nc.interface.add()
        inf.name = "N1TenGi-1"
        inf.bind.port = "trafgen/2/1"
        inf1 = nc.interface.add()
        inf1.name = "N1TenGi-13"
        inf1.bind.port = "trafgen/2"
        lb = nc.lb_profile.add()
        lb.name = "lb1"
        dn = lb.destination_nat.add()
        dn.real_ip = "12.0.1.4"
        dn.tcp_port = 5001
        dn.source_ip = "12.0.1.3"
        lb1 = nc.lb_profile.add()
        lb1.name = "x1"
        loadb = nc.load_balancer.add()
        loadb.name = "x1"
        loadb.virtual_ip = "11.0.1.3"
        loadb.tcp_port = 5678
        loadb.lb_profile = "lb1"

    def fill_app_config(self):
        port = self.colony.port.add()
        port.name = "trafgen/2/1"
        port.open.application.trafgen = True
        port.receive_q_length = 2
        port.trafgen.range_template.destination_mac.dynamic.gateway = "11.0.1.3"
        port.trafgen.range_template.source_ip.start = "11.0.1.4"
        port.trafgen.range_template.source_ip.minimum = "11.0.1.4"
        port.trafgen.range_template.source_ip.maximum = "11.0.1.4"
        port.trafgen.range_template.source_ip.increment = 1
        port.trafgen.range_template.destination_ip.start = "11.0.1.3"
        port.trafgen.range_template.destination_ip.minimum = "11.0.1.3"
        port.trafgen.range_template.destination_ip.maximum = "11.0.1.3"
        port.trafgen.range_template.destination_ip.increment = 1
        port.trafgen.range_template.source_port.start = 10000
        port.trafgen.range_template.source_port.minimum = 10000
        port.trafgen.range_template.source_port.maximum = 10128
        port.trafgen.range_template.source_port.increment = 1
        port.trafgen.range_template.destination_port.start = 5678
        port.trafgen.range_template.destination_port.minimum = 5678
        port.trafgen.range_template.destination_port.maximum = 5678
        port.trafgen.range_template.destination_port.increment = 1
        port.trafgen.range_template.packet_size.start = 512
        port.trafgen.range_template.packet_size.minimum = 512
        port.trafgen.range_template.packet_size.maximum = 512
        port.trafgen.range_template.packet_size.increment = 1
        port.trafgen.receive_param.receive_echo.on = True

    def fill_slb_config(self):
        #SLB config
        nc = self.colony.network_context[0]
        slb = nc.scriptable_lb.add()
        slb.name = "abc"
        slb.plugin_script.script_name = "slb_interface_radius-lua"
        slb.receive_function.function_type = "builtin"
        slb.receive_function.builtin_basic_lb.address = "11.0.1.3"
        slb.receive_function.builtin_basic_lb.ip_proto = "proto_udp"
        slb.receive_function.builtin_basic_lb.port = 1645
        slb.classify_function.function_type = "plugin_script"
        sg = slb.server_selection_function.server_group.add()
        sg.name = "grp1"
        s1 = sg.server.add()
        s1.address = "12.0.1.4"
        s1.port = 5000
        sg.nat_address.src_address = "12.0.1.3"
        sg2 = slb.server_selection_function.server_group.add()
        sg2.name = "grp2"
        s2 = sg2.server.add()
        s2.address = "12.0.1.4"
        s2.port = 5001
        sg2.nat_address.src_address = "12.0.1.3"
        mr = slb.server_selection_function.selection_criteria.radius_lb.match_rule.add()
        mr.priority = 1
        mr.domain = "cnn.com"
        mr.server_group = "grp1"
        mr1 = slb.server_selection_function.selection_criteria.radius_lb.match_rule.add()
        mr1.priority = 2
        mr1.domain = "."
        mr1.server_group = "grp2"
        slb.transform_function.function_type = "builtin"
        slb.transform_function.builtin_transform.transform_type = "double_nat"

    def fill_seagul_config(self):
        self.colony1.name = "trafgen"
        nc = self.colony1.network_context.add()
        nc.name = "trafgen"
        ts = self.colony1.trafsim_service.add()
        ts.name = "seagull_client"
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.diameter_interface.gx = True
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.network_context.name = "trafgen"
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.ip_address = "11.0.1.4"
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.port = 52000
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.instances_per_vm = 2
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.number_of_vm = 2
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.server_address = "11.0.1.3"
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.server_port = 3868
        ts.protocol_mode.diameter_sim.operational_mode.diameter_client.call_rate = 10

    def test_pb_to_cli_1(self):
        self.fill_colony()
        self.fill_app_config()
        self.fill_slb_config()
        clistr = self.colony.to_pb_cli(yang_model)
        dirp = os.path.dirname(os.path.abspath(__file__))
        filename = dirp + "/pb-to-cli-1.cfg"
        print("Comparing the generated CLI commands with " + filename)
        with open (filename, 'r') as f:
           contents = f.read()
        self.assertEqual(clistr, contents)
        #print clistr

    def test_pb_to_cli_2(self):
        self.fill_seagul_config()
        clistr = self.colony1.to_pb_cli(yang_model)
        dirp = os.path.dirname(os.path.abspath(__file__))
        filename = dirp + "/pb-to-cli-2.cfg"
        print("Comparing the generated CLI commands with " + filename)
        with open (filename, 'r') as f:
          contents = f.read()
        self.assertEqual(clistr, contents)
        #print clistr

def main(argv=sys.argv[1:]):
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
