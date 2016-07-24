#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import logging
import os
import sys
import unittest

import xmlrunner

import rift.vcs.port

logger = logging.getLogger(__name__)

class TestPort(unittest.TestCase):
    def test_create_port(self):
        port_name = rift.vcs.port.PortName("ethsim", "fakename")
        new_port = rift.vcs.port.Port(port_name=port_name)

        self.assertEqual(new_port.port_name, str(port_name))
        self.assertEqual(new_port.logical_name, None)
        self.assertEqual(new_port.port_type, False)

        new_port.logical_name = "logical"
        self.assertEqual(new_port.logical_name, "logical")

        new_port.port_type = "faketype"
        self.assertEqual(new_port.port_type, "faketype")

        fabric_port = rift.vcs.port.Fabric("logical", port_name)
        self.assertEqual(fabric_port.logical_name, "logical")
        self.assertEqual(fabric_port.port_name, str(port_name))
        self.assertEqual(fabric_port.port_type, "vfap")

        external_port = rift.vcs.port.External("logical", port_name)
        self.assertEqual(external_port.logical_name, "logical")
        self.assertEqual(external_port.port_name, str(port_name))
        self.assertEqual(external_port.port_type, "external")

    def test_port_equality(self):
        port_a = rift.vcs.port.Port(port_name=rift.vcs.port.PortName("ethsim", "fakename"))
        port_b = rift.vcs.port.Port(port_name=rift.vcs.port.PortName("ethsim", "fakename"))
        self.assertEqual(port_a, port_b)

        port_a.logical_name = "fakelogicalname"
        self.assertNotEqual(port_a, port_b)

        port_b = rift.vcs.port.Port(port_name=rift.vcs.port.PortName("ethsim", "fakename"),
                                 logical_name="fakelogicalname")
        self.assertEqual(port_a, port_b)

        port_a = rift.vcs.port.Fabric("logical", port_name=rift.vcs.port.PortName("ethsim", "fakename"))
        port_b = rift.vcs.port.External("logical", port_name=rift.vcs.port.PortName("ethsim", "fakename"))
        self.assertNotEqual(port_a, port_b)


class TestPortName(unittest.TestCase):
    def test_string_conversion(self):
        """
        This test checks the a port name expressed as a string can be converted
        correctly into a PortName object.

        """
        port = rift.vcs.port.PortName.from_string('eth_uio:pci=fakename')
        self.assertEqual('pci', port.mode)
        self.assertEqual('fakename', port.name)

        port = rift.vcs.port.PortName.from_string('eth_sim:name=fakename')
        self.assertEqual('ethsim', port.mode)
        self.assertEqual('fakename', port.name)

        port = rift.vcs.port.PortName.from_string('eth_raw:name=fakename')
        self.assertEqual('rawsocket', port.mode)
        self.assertEqual('fakename', port.name)

        with self.assertRaises(ValueError):
            rift.vcs.port.PortName.from_string('eth_sim:eth=fakename')

        with self.assertRaises(ValueError):
            rift.vcs.port.PortName.from_string('eth_foo:name=fakename')


class TestPortGroup(unittest.TestCase):
    def test_port_group(self):
        port_a = rift.vcs.port.Port(port_name=rift.vcs.port.PortName("ethsim", "fakename_a"))
        group = rift.vcs.port.PortGroup("group_name")
        self.assertEqual(group.name, "group_name")
        group.add_port(port_a)
        self.assertIn(port_a, group)

        with self.assertRaises(TypeError):
            group.add_port("invalid_port_type")


        ports = group.get_ports()
        self.assertEqual([port_a], ports)

        ports.remove(port_a)
        # Port should still be in the group since get_ports returns a copy
        self.assertIn(port_a, group)

        # Adding the same port again should be fine
        group.add_port(port_a)

        # Only a single instance of that port in the group though.
        ports = group.get_ports()
        self.assertEqual(1, ports.count(port_a))

        port_b = rift.vcs.port.Port(port_name=rift.vcs.port.PortName("ethsim", "fakename_b"))
        group.add_port(port_b)
        self.assertEqual(set([port_a, port_b]), set(group.get_ports()))


class TestPortNetwork(unittest.TestCase):
    def test_port_network(self):
        port_a = rift.vcs.port.Port(port_name=rift.vcs.port.PortName("ethsim", "fakename"))
        group = rift.vcs.port.PortGroup("group_name")
        group.add_port(port_a)
        network = rift.vcs.port.PortNetwork()
        network.add_port_group(group)
        self.assertEqual(network.find_group(port_a), group)

        with self.assertRaises(TypeError):
            network.add_port_group("invalid port group")

    def test_connected_ports(self):
        port_a = rift.vcs.port.External(port_name=rift.vcs.port.PortName("ethsim", "fakename_a"))
        port_b = rift.vcs.port.External(port_name=rift.vcs.port.PortName("ethsim", "fakename_b"))
        group_a = rift.vcs.port.PortGroup("group_name", [port_a, port_b])
        network = rift.vcs.port.PortNetwork([group_a])
        self.assertEqual(network.find_group(port_a), group_a)
        self.assertEqual(network.find_group(port_b), group_a)
        self.assertTrue(network.ports_in_same_group([port_a, port_b]))

        port_c = rift.vcs.port.External(port_name=rift.vcs.port.PortName("ethsim", "fakename_c"))
        group_a.add_port(port_c)
        self.assertTrue(network.ports_in_same_group([port_a, port_b, port_c]))

        group_b = rift.vcs.port.PortGroup("group_name_b", [port_a, port_b])
        port_d = rift.vcs.port.External(port_name=rift.vcs.port.PortName("ethsim", "fakename_d"))
        group_b = rift.vcs.port.PortGroup("group_name_b", [port_d])
        network.add_port_group(group_b)
        self.assertFalse(network.ports_in_same_group([port_a, port_d]))


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
