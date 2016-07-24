#!/usr/bin/env python3
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file utest_compiler.py
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
import rift.vcs.compiler
import rift.vcs.compiler.constraints
import rift.vcs.compiler.exc
import rift.vcs.core
import rift.vcs.manifest
import rift.vcs.pool

logger = logging.getLogger(__name__)


class TestManifestCompiler(unittest.TestCase):
    def setUp(self):
        self.sysinfo = rift.vcs.SystemInfo(
                colonies=[],
                mode='pci',
                collapsed=True,
                zookeeper=None)

        # Define the topology
        self.sysinfo.subcomponents = [
                rift.vcs.Colony(clusters=[
                    rift.vcs.Cluster(virtual_machines=[
                        rift.vcs.VirtualMachine(),
                        rift.vcs.VirtualMachine()
                        ]),
                    rift.vcs.Cluster(virtual_machines=[
                        rift.vcs.VirtualMachine(),
                        rift.vcs.VirtualMachine(),
                        rift.vcs.VirtualMachine()
                        ])
                    ])
                ]

        # Assign identifiers to each component
        for index, component in enumerate(self.sysinfo.list_by_class(object)):
            component.uid = index + 1

        # Assign IP addresses to each of the virtual machines
        pool = rift.vcs.pool.FakeAddressPool()
        for vm in self.sysinfo.list_by_class(rift.vcs.VirtualMachine):
            vm.ip = pool.acquire()

    def test_compile(self):
        """
        This test checks that the components in the compiled manifest are
        correct.

        """
        # Compile the manifest
        compiler = rift.vcs.compiler.ManifestCompiler()
        manifest = compiler.compile(self.sysinfo)

        # Define the list of component types that expected and the oreder in
        # which they are expected.
        expected_types = (
                rift.vcs.manifest.RaVm,
                rift.vcs.manifest.RaVm,
                rift.vcs.manifest.RaCluster,
                rift.vcs.manifest.RaVm,
                rift.vcs.manifest.RaVm,
                rift.vcs.manifest.RaVm,
                rift.vcs.manifest.RaCluster,
                rift.vcs.manifest.RaColony)

        # Define a generator over the components in the manifest
        components = (component
                for colony in manifest.components
                for component in rift.vcs.core.component_iterator(colony))

        # Iterate over the components and the expected types and check that the
        # match up.
        for component, expected in zip(components, expected_types):
            self.assertEqual(type(component), expected)


class TestLegacyManifestCompiler(unittest.TestCase):
    def setUp(self):
        self.sysinfo = rift.vcs.SystemInfo(
                colonies=[],
                mode='pci',
                collapsed=True,
                zookeeper=None)

        self.sysinfo.subcomponents = [
                rift.vcs.Colony(clusters=[
                    rift.vcs.Cluster(virtual_machines=[
                        rift.vcs.VirtualMachine(
                            ip='127.0.0.1',
                            tasklets=[
                                rift.vcs.MsgBrokerTasklet(),
                                rift.vcs.DtsRouterTasklet(),
                                rift.vcs.uAgentTasklet(port=8080),
                                rift.vcs.RestconfTasklet(),
                                ],
                            procs=[
                                rift.vcs.RiftCli(),
                                rift.vcs.Webserver(),
                                rift.vcs.RedisServer(),
                                ],
                            )
                        ])
                    ])
                ]

    def test_webserver_compilation(self):
        """
        This test checks that the webserver tasklet is correctly handled by the
        legacy compiler.
        """

        # There should be one uagent and one webserver
        self.assertEqual(1, len(self.sysinfo.list_by_class(rift.vcs.uAgentTasklet)))
        self.assertEqual(1, len(self.sysinfo.list_by_class(rift.vcs.Webserver)))

        # Compile the manfiest
        compiler = rift.vcs.compiler.LegacyManifestCompiler()
        _, manifest = compiler.compile(self.sysinfo)

        # The webserver is the one of the 2 native processes on the VM
        colony = manifest.components[0]
        native = rift.vcs.core.list_by_class(colony, rift.vcs.manifest.RaNativeProcess)
        self.assertEqual(3, len(native))


class TestUAgentConstraint(unittest.TestCase):
    def setUp(self):
        self.sysinfo = rift.vcs.SystemInfo(
                colonies=[],
                mode='pci',
                collapsed=True,
                zookeeper=None)

        self.virtual_machine = rift.vcs.VirtualMachine(ip='127.0.0.1')
        self.sysinfo.subcomponents = [
                rift.vcs.Colony(clusters=[
                    rift.vcs.Cluster(virtual_machines=[
                        self.virtual_machine
                        ])
                    ])
                ]

    def test_too_few(self):
        """
        An exception should be raised if the system contains too few uAgents,
        i.e. zero.

        """
        constraint = rift.vcs.compiler.constraints.UniqueUAgent()
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(self.sysinfo)

    def test_too_many(self):
        """
        An exception should be raised if the system contains too many uAgents,
        i.e. more than one.

        """
        self.virtual_machine.add_proc(rift.vcs.uAgentTasklet())
        self.virtual_machine.add_proc(rift.vcs.uAgentTasklet())

        constraint = rift.vcs.compiler.constraints.UniqueUAgent()
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(self.sysinfo)

    def test_one_uagent(self):
        """
        When the system contains just one uAgent the constraints should 
        """
        self.virtual_machine.add_proc(rift.vcs.uAgentTasklet())

        constraint = rift.vcs.compiler.constraints.UniqueUAgent()
        constraint(self.sysinfo)


class TestGenerateCollectionInstanceIDs(unittest.TestCase):
    def setUp(self):
        self.sysinfo = rift.vcs.SystemInfo(
                colonies=[],
                mode='pci',
                collapsed=True,
                zookeeper=None)

        self.virtual_machine = rift.vcs.VirtualMachine(uid=2, ip='127.0.0.1')
        self.sysinfo.subcomponents = [
                rift.vcs.Colony(clusters=[
                    rift.vcs.Cluster(virtual_machines=[
                        self.virtual_machine
                        ])
                    ])
                ]

    def test(self):
        """
        Check that the transform assigns the expected IDs to the colony and
        cluster objects.
        """
        transform = rift.vcs.compiler.transforms.GenerateCollectionInstanceIDs()
        transform(self.sysinfo)

        self.assertEqual(1, self.sysinfo.find_by_class(rift.vcs.Cluster).uid)
        self.assertEqual(3, self.sysinfo.find_by_class(rift.vcs.Colony).uid)


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
