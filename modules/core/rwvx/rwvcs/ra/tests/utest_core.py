#!/usr/bin/env python3
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file utest_core.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 09/18/2014
#
"""

import argparse
import logging
import os
import sys
import unittest

import xmlrunner

import rift.vcs
import rift.vcs.core
import rift.vcs.pool

logger = logging.getLogger(__name__)


class MockTrafgenVM(rift.vcs.VirtualMachine):
    pass


class MockTrafsinkVM(rift.vcs.VirtualMachine):
    pass


class MockLoadBalancerVM(rift.vcs.VirtualMachine):
    pass


class MockFastpathTasklet(rift.vcs.Tasklet):
    pass


class MockAppTasklet(rift.vcs.Tasklet):
    pass


class TestSystemInfo(unittest.TestCase):
    def setUp(self):
        self.sysinfo = rift.vcs.SystemInfo(
                mode='pci',
                collapsed=True,
                zookeeper=None,
                colonies=[
                    rift.vcs.Colony(
                        clusters=[
                            rift.vcs.Cluster(
                                virtual_machines=[
                                    MockTrafgenVM(procs=[
                                        MockFastpathTasklet(),
                                        MockAppTasklet()]),
                                    MockTrafsinkVM(procs=[
                                        MockFastpathTasklet(),
                                        MockAppTasklet()]),
                                    MockLoadBalancerVM(procs=[
                                        MockFastpathTasklet(),
                                        MockAppTasklet()]),
                                    ])
                            ])
                    ])

    def test_virtual_machine_add_proc(self):
        """
        The VirtualMachine class has a function called 'add_proc' that, as a
        convenience, adds tasklets and native process as well as procs.
        Moreover it creates a proc when adding a tasklet.
        """
        vm = rift.vcs.VirtualMachine()
        vm.add_proc(rift.vcs.Proc())
        vm.add_proc(rift.vcs.Tasklet())
        vm.add_proc(rift.vcs.NativeProcess())

        # There should be 3 comoponents in the VM: 2 Procs (1 that is added
        # explicitly and 1 that is added to contain the tasklet) and 1 NativeProcess.
        self.assertEqual(3, len(vm.subcomponents))
        self.assertEqual(2, len(rift.vcs.core.list_by_type(vm, rift.vcs.Proc)))
        self.assertEqual(1, len(rift.vcs.core.list_by_type(vm, rift.vcs.NativeProcess)))

    def test_list_by_class_count(self):
        """
        This test checks that list_by_class returns the correct number of
        objects for a given class type. This function should return objects
        whose class is of the specified type or a class derived from that type.

        """
        self.assertEqual(1, len(self.sysinfo.list_by_class(rift.vcs.Colony)))
        self.assertEqual(1, len(self.sysinfo.list_by_class(rift.vcs.Cluster)))
        self.assertEqual(3, len(self.sysinfo.list_by_class(rift.vcs.VirtualMachine)))
        self.assertEqual(6, len(self.sysinfo.list_by_class(rift.vcs.Proc)))
        self.assertEqual(6, len(self.sysinfo.list_by_class(rift.vcs.Tasklet)))

    def test_list_by_class(self):
        """
        This tests checks that the list_by_class utility function correctly
        traverses a component hierarchy and returns a list of components with
        the correct matching class.
        """
        class Container(object):
            def __init__(self, components=None):
                self.subcomponents = [] if components is None else components

        class Foo(Container):
            pass

        class Bar(Foo):
            pass

        container = Container(components=[
            Container(),
            Container(components=[
                Bar(),
                Bar(),
                Foo(),
                Bar(),
                Bar(),
                Foo()])
            ])

        self.assertEqual(4, len(rift.vcs.core.list_by_class(container, Bar)))
        self.assertEqual(6, len(rift.vcs.core.list_by_class(container, Foo)))

    def test_list_by_type_count(self):
        """
        This test checks that list_by_type returns the correct number of
        objects for a given class type. This function should return objects
        whose type matches the query type exactly.

        """
        # Core types
        self.assertEqual(1, len(self.sysinfo.list_by_type(rift.vcs.Colony)))
        self.assertEqual(1, len(self.sysinfo.list_by_type(rift.vcs.Cluster)))
        self.assertEqual(0, len(self.sysinfo.list_by_type(rift.vcs.VirtualMachine)))
        self.assertEqual(6, len(self.sysinfo.list_by_type(rift.vcs.Proc)))
        self.assertEqual(0, len(self.sysinfo.list_by_type(rift.vcs.Tasklet)))

        # Derived types
        self.assertEqual(1, len(self.sysinfo.list_by_type(MockTrafgenVM)))
        self.assertEqual(1, len(self.sysinfo.list_by_type(MockTrafsinkVM)))
        self.assertEqual(1, len(self.sysinfo.list_by_type(MockLoadBalancerVM)))
        self.assertEqual(3, len(self.sysinfo.list_by_type(MockFastpathTasklet)))
        self.assertEqual(3, len(self.sysinfo.list_by_type(MockAppTasklet)))

    def test_list_by_type(self):
        """
        This tests checks that the list_by_type utility function correctly
        traverses a component hierarchy and returns a list of components with
        the correct matching type.
        """
        class Container(object):
            def __init__(self, components=None):
                self.subcomponents = [] if components is None else components

        class Foo(Container):
            pass

        class Bar(Foo):
            pass

        container = Container(components=[
            Container(),
            Container(components=[
                Bar(),
                Bar(),
                Foo(),
                Bar(),
                Bar(),
                Foo()])
            ])

        self.assertEqual(4, len(rift.vcs.core.list_by_type(container, Bar)))
        self.assertEqual(2, len(rift.vcs.core.list_by_type(container, Foo)))

    def test_list_by_name_count(self):
        """
        The list_by_name function performs a simple substring search to identify
        components by name.

        """
        # Check that there are no components that match the search term
        components = self.sysinfo.list_by_name('ba')
        self.assertEqual(0, len(components))

        # Now set the names of the virtual machines in the system.
        vms = self.sysinfo.colonies[0].clusters[0].virtual_machines
        vms[0].name = 'foo'
        vms[1].name = 'bar'
        vms[2].name = 'baz'

        # Repeat the search. This time there should be 2 matching components.
        components = self.sysinfo.list_by_name('ba')
        self.assertEqual(2, len(components))
        self.assertEqual('bar', components[0].name)
        self.assertEqual('baz', components[1].name)

    def test_find_by_name(self):
        """
        The find_by_name function performs a simple substring search to find
        the first component which has substring name.

        """
        # Check that there are no components that match the search term
        component = self.sysinfo.find_by_name('foo')
        self.assertEqual(None, component)

        # Now set the names of the virtual machines in the system.
        vms = self.sysinfo.colonies[0].clusters[0].virtual_machines
        vms[0].name = 'foo'
        vms[1].name = 'barfoo'
        vms[2].name = 'foobar'

        component = self.sysinfo.find_by_name('foo')
        self.assertEqual(component, vms[0])

        component = self.sysinfo.find_by_name('bar')
        self.assertEqual(component, vms[1])

        component = self.sysinfo.find_by_name('oba')
        self.assertEqual(component, vms[2])

    def test_ip_addresses(self):
        """
        This test checks that the IP addresses returned by the system info
        object are consistent with the addresses that are assigned to the
        virtual machines in the system.

        """
        # First, create an address pool and then assign an IP to each virtual
        # machine in the system.
        pool = rift.vcs.pool.FakeAddressPool()
        for vm in self.sysinfo.list_by_class(rift.vcs.VirtualMachine):
            vm.ip = pool.acquire()

        # Retrieve the list of IP addresses
        ip_addresses = self.sysinfo.ip_addresses

        # Check that we have recovered the correct addresses
        self.assertEqual(3, len(ip_addresses))
        for ip in pool._released:
            self.assertIn(ip, ip_addresses)

    def test_iter(self):
        """
        This test checks SystemInfo provides an iterator that correct iterates
        over all of the components in the system.

        """
        # Create a list of the class types in the system and the order that they
        # should be returned in (depth-first traversal).
        expected_class = [
                rift.vcs.Tasklet,
                rift.vcs.Tasklet,
                rift.vcs.VirtualMachine,
                rift.vcs.Tasklet,
                rift.vcs.Tasklet,
                rift.vcs.VirtualMachine,
                rift.vcs.Tasklet,
                rift.vcs.Tasklet,
                rift.vcs.VirtualMachine,
                rift.vcs.Cluster,
                rift.vcs.Colony]

        # Iterate over the system components
        actual_class = [type(obj) for obj in self.sysinfo]

        # Check that the results are correct
        for expected, actual in zip(expected_class, actual_class):
            self.assertTrue(expected, actual)

    def test_find_by_type(self):
        """
        This tests checks that the find_by_type utility function correctly
        traverses a component hierarchy and returns the first component with
        matching type.
        """
        class Container(object):
            def __init__(self, components=None):
                self.subcomponents = [] if components is None else components

        class Foo(Container):
            pass

        class Bar(Foo):
            pass

        container = Container(components=[
            Container(),
            Container(components=[
                Bar(),
                Foo()])
            ])

        self.assertIs(container.subcomponents[1].subcomponents[1],
                rift.vcs.core.find_by_type(container, Foo))

    def test_find_by_class(self):
        """
        This tests checks that the find_by_class utility function correctly
        traverses a component hierarchy and returns the first component with
        matching class.
        """
        class Container(object):
            def __init__(self, components=None):
                self.subcomponents = [] if components is None else components

        class Foo(Container):
            pass

        class Bar(Foo):
            pass

        container = Container(components=[
            Container(),
            Container(components=[
                Foo(),
                Bar()])
            ])

        self.assertIs(container.subcomponents[1].subcomponents[0],
                rift.vcs.core.find_by_class(container, Foo))

    def test_remove_component(self):
        """
        This test checks that the 'remove' function correctly removes
        components from a SystemInfo object.

        """
        # Find the load balancer component
        load_balancer = self.sysinfo.find_by_class(MockLoadBalancerVM)
        self.assertIsNotNone(load_balancer)
        self.assertEqual(6, len(self.sysinfo.list_by_class(rift.vcs.Tasklet)))

        # Remove the load balancer
        self.sysinfo.remove(load_balancer)
        self.assertIsNone(self.sysinfo.find_by_class(MockLoadBalancerVM))

        # Check that the tasklet count has been reduced
        self.assertEqual(4, len(self.sysinfo.list_by_class(rift.vcs.Tasklet)))

    def test_find_parent(self):
        """
        This test checks that the 'parent' function correctly identifies a
        components parent in the SystemInfo object.

        """
        # Find the load balancer component
        load_balancer = self.sysinfo.find_by_class(MockLoadBalancerVM)
        self.assertIsNotNone(load_balancer)

        # Check that the load balancer is contained by a cluster
        parent = self.sysinfo.parent(load_balancer)
        self.assertIsNotNone(parent)
        self.assertTrue(isinstance(parent, rift.vcs.Cluster))


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
