#!/usr/bin/env python3
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
# @file utest_fastpath.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 09/18/2014
#
"""

import argparse
import collections
import logging
import os
import sys
import unittest

import xmlrunner

import rift.vcs
import rift.vcs.core
import rift.vcs.fastpath

logger = logging.getLogger(__name__)


class TestLeadVM(rift.vcs.core.VirtualMachine):
    def __init__(self, *args, **kwargs):
        super(TestLeadVM, self).__init__(*args, **kwargs)
        self.add_proc(rift.vcs.Controller())
        self.add_proc(rift.vcs.NetworkContextManager())
        self.add_proc(rift.vcs.Fastpath(name='RW.Fpath'))
        self.add_proc(rift.vcs.InterfaceManager())
        self.add_proc(rift.vcs.ApplicationManager())


class TestFastpath(unittest.TestCase):
    def test_assign_port_map(self):
        """
        The assign_port_map function sets the ports on fastpath tasklets using a
        port map, which is a dict associating an IP address to a list of ports.
        """

        # Define a simple system that consists of two cluster: the first cluster
        # has a lead VM, which contains a fastpath tasklet, and the second
        # cluster contains a generic virtual machine, which does not have a
        # fastpath tasklet.
        sysinfo = rift.vcs.core.SystemInfo(
                mode='ethsim',
                collapsed=True,
                zookeeper=None,
                colonies=[
                    rift.vcs.core.Colony(
                        clusters=[
                            rift.vcs.core.Cluster(
                                virtual_machines=[
                                    TestLeadVM(ip='10.0.0.1')
                                    ]),
                            rift.vcs.core.Cluster(
                                virtual_machines=[
                                    rift.vcs.core.VirtualMachine(ip='10.0.0.2')
                                    ])
                            ])
                    ])

        # Define a basic port map
        port_map = {}
        port_map['10.0.0.1'] = ['port1','port2']

        # Assign the port map to the fastpath tasklets
        rift.vcs.fastpath.assign_port_map(sysinfo, port_map)

        # Retrieve the list of fastpath tasklets in the system -- there can be
        # only one -- and check that it is assigned the port map
        tasklets = sysinfo.list_by_class(rift.vcs.fastpath.Fastpath)

        self.assertEqual(1, len(tasklets))
        self.assertEqual(tasklets[0].port_map, port_map['10.0.0.1'])

    def test_assign_port_names(self):
        """
        The assign_port_names function is used to populate the real names of the
        port maps in the fastpath tasklets.

        """
        # Define a simple system that consists of two cluster -- each with a
        # lead VM.
        sysinfo = rift.vcs.core.SystemInfo(
                mode='ethsim',
                collapsed=True,
                zookeeper=None,
                colonies=[
                    rift.vcs.core.Colony(
                        clusters=[
                            rift.vcs.core.Cluster(
                                virtual_machines=[
                                    TestLeadVM(ip='10.0.0.1')
                                    ]),
                            rift.vcs.core.Cluster(
                                virtual_machines=[
                                    TestLeadVM(ip='10.0.0.2')
                                    ])
                            ])
                    ])

        # Define a generic port map
        port_map = collections.defaultdict(list)
        port_map['10.0.0.1'].append(rift.vcs.Fabric(logical_name='vfap/1/1'))
        port_map['10.0.0.2'].append(rift.vcs.Fabric(logical_name='vfap/2/1'))
        port_map['10.0.0.2'].append(rift.vcs.External(logical_name='trafgen/2/1'))

        # Define a mapping from logical port names to 'real' port names
        port_names = {
                'vfap/1/1': 'a',
                'vfap/2/1': 'b',
                'trafgen/2/1': 'c'}

        # Assign the port map to the fastpath tasklets
        rift.vcs.fastpath.assign_port_map(sysinfo, port_map)

        # Check that the ports are correctly named for ethsim mode
        rift.vcs.fastpath.assign_port_names(sysinfo, 'ethsim', port_names)

        tasklets = sysinfo.list_by_class(rift.vcs.fastpath.Fastpath)
        expected = [
                "vfap/1/1|eth_sim:name=a|vfap",
                "vfap/2/1|eth_sim:name=b|vfap",
                "trafgen/2/1|eth_sim:name=c|external"]

        self.assertEqual(expected[0], str(tasklets[0].port_map[0]))
        self.assertEqual(expected[1], str(tasklets[1].port_map[0]))
        self.assertEqual(expected[2], str(tasklets[1].port_map[1]))

        # Check that the ports are correctly named for raw socket mode
        rift.vcs.fastpath.assign_port_names(sysinfo, 'rawsocket', port_names)

        tasklets = sysinfo.list_by_class(rift.vcs.fastpath.Fastpath)
        expected = [
                "vfap/1/1|eth_raw:name=a|vfap",
                "vfap/2/1|eth_raw:name=b|vfap",
                "trafgen/2/1|eth_raw:name=c|external"]

        self.assertEqual(expected[0], str(tasklets[0].port_map[0]))
        self.assertEqual(expected[1], str(tasklets[1].port_map[0]))
        self.assertEqual(expected[2], str(tasklets[1].port_map[1]))

        # Check that the ports are correctly named for PCI mode
        rift.vcs.fastpath.assign_port_names(sysinfo, 'pci', port_names)

        tasklets = sysinfo.list_by_class(rift.vcs.fastpath.Fastpath)
        expected = [
                "vfap/1/1|eth_uio:pci=a|vfap",
                "vfap/2/1|eth_uio:pci=b|vfap",
                "trafgen/2/1|eth_uio:pci=c|external"]

        self.assertEqual(expected[0], str(tasklets[0].port_map[0]))
        self.assertEqual(expected[1], str(tasklets[1].port_map[0]))
        self.assertEqual(expected[2], str(tasklets[1].port_map[1]))


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
