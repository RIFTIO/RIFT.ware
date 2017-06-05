#!/usr/bin/env python3

# STANDARD_RIFT_IO_COPYRIGHT

import logging
import os
import sys
import unittest

import xmlrunner

import rift.vcs
import rift.vcs.compiler
import rift.vcs.manifest
from rift.vcs.manifest import (
        RaCliProc,
        RaCluster,
        RaColony,
        RaManifest,
        RaManifestObject,
        RaNativeProcess,
        RaProc,
        RaVm,
        )
import rift.vcs.vms
import gi
gi.require_version('RwManifestYang', '1.0')
from gi.repository.RwManifestYang import NetconfTrace


logger = logging.getLogger(__name__)


class TestManifest(unittest.TestCase):
    def setUp(self):
        # Create a system that consists of a cli tasklet
        # running inside a VM, that is running within a colony.
        self.cli = RaCliProc(name='test.cli')
        self.colony = RaColony(name='test.colony')
        self.cluster = RaCluster(name='test.cluster')
        self.manifest = RaManifest(northbound_listing=None, persist_dir_name='test')
        self.assertEqual(self.manifest.netconf_trace, "AUTO")

        # Put the CLI tasklet into a proc
        self.cli_proc = RaProc(name='test.proc.cli')
        self.cli_proc.add_component(self.cli)

        # Add the CLI process to the VM
        self.vm = RaVm(name='test.vm')
        self.vm.add_component(self.cli_proc)

        self.cluster.add_component(self.vm)
        self.colony.add_component(self.cluster)
        self.manifest.add_component(self.colony)

    def test_iter(self):
        # The components of the manifest are returned by a depth-first
        # traversal.
        reference = [
                'test.cli',
                'test.proc.cli',
                'test.vm',
                'test.cluster',
                'test.colony',
                ]

        for actual, expected in zip(self.manifest, reference):
            self.assertEqual(actual.name, expected)

    def test_remove_component(self):
        # Remove the VM
        self.manifest.remove_component(self.vm)

        # The VM component and all of its children should have been removed
        # from the manifest
        self.assertIsNone(self.manifest.find_by_class(RaCliProc))
        self.assertIsNone(self.manifest.find_by_class(RaProc))

        # The only remaining components are the cluster and the colony
        self.assertIsNotNone(self.manifest.find_by_class(RaCluster))
        self.assertIsNotNone(self.manifest.find_by_class(RaColony))

    def test_remove_empty_procs(self):
        self.manifest.remove_component(self.cli)
        self.manifest.remove_empty_procs()
        self.assertIsNone(self.manifest.find_by_class(RaProc))

    def test_assign_leading_vm(self):
        self.manifest.assign_leading_vm()

        reference = [
                'test.cli',
                'test.proc.cli',
                'test.vm',
                'test.colony',
                ]

        for actual, expected in zip(self.manifest, reference):
            self.assertEqual(actual.name, expected)

    def test_list_by_class(self):
        # All components derive from RaManifestObject, so each component should
        # be in the list.
        components = self.manifest.list_by_class(RaManifestObject)
        self.assertEqual(5, len(components))

        # There is only one RaProc component in the manifest
        procs = self.manifest.list_by_class(RaProc)
        self.assertEqual(1, len(procs))

    def test_find_by_class(self):
        self.assertTrue(self.manifest.find_by_class(RaCliProc) == self.cli)
        self.assertTrue(self.manifest.find_by_class(RaProc) == self.cli_proc)
        self.assertTrue(self.manifest.find_by_class(RaColony) == self.colony)
        self.assertTrue(self.manifest.find_by_class(RaNativeProcess) == self.cli)

    def test_find_ancestor_by_class(self):
        self.assertEqual(self.cli_proc,
                self.manifest.find_ancestor_by_class(RaProc, self.cli))

    def test_find_enclosing_cluster(self):
        self.assertEqual(self.cluster,
                self.manifest.find_enclosing_cluster(self.cli))

    def test_find_enclosing_colony(self):
        self.assertEqual(self.colony,
                self.manifest.find_enclosing_colony(self.cli))


class TestManifestComponentPruning(unittest.TestCase):
    def DISABLED_test(self):
        def count_components(manifest, type_name):
            count = 0
            for component in manifest.inventory.component:
                if component.HasField(type_name):
                    count += 1

            return count

        # Create a system that contains a colony, a cluster, and 2 virtual
        # machines.
        cli = rift.vcs.vms.CliVM()
        mgmt = rift.vcs.vms.MgmtVM()

        cluster = rift.vcs.Cluster(
                name="test.cluster",
                virtual_machines=[cli, mgmt],
                )

        colony = rift.vcs.Colony(
                name="test.colony",
                clusters=[cluster],
                )

        sysinfo = rift.vcs.SystemInfo(
                mode='ethsim',
                collapsed=True,
                colonies=[colony],
                )

        # Adding a new tasklet
        cli.add_tasklet(rift.vcs.DtsPerfTasklet())

        # Check that the system has a DTS router tasklet and 2 virtual
        # machines.
        self.assertEqual(1, len(sysinfo.list_by_class(rift.vcs.DtsPerfTasklet)))
        self.assertEqual(2, len(sysinfo.list_by_class(rift.vcs.VirtualMachine)))

        # Compile the manifest into a protobuf
        compiler = rift.vcs.compiler.LegacyManifestCompiler()
        manifest = compiler.compile(sysinfo)[1].as_pb()

        # Check that the manifest has the expected number of tasklets and
        # virtual machines.
        self.assertEqual(7, count_components(manifest, 'rwtasklet'))
        self.assertEqual(2, count_components(manifest, 'rwvm'))

        # Add anpother of same tasklet and a virtual machine to the system
        cli.add_tasklet(rift.vcs.DtsPerfTasklet())
        cluster.add_virtual_machine(rift.vcs.VirtualMachine(name="foo"))

        # There should now be 2 instances of the DtsRouterTasklet and 3 virtual
        # machines.
        self.assertEqual(2, len(sysinfo.list_by_class(rift.vcs.DtsPerfTasklet)))
        self.assertEqual(3, len(sysinfo.list_by_class(rift.vcs.VirtualMachine)))

        # Compile the manifest into a protobuf
        manifest = compiler.compile(sysinfo)[1].as_pb()

        # Check that the number of tasklet components unchanged, but the number
        # of virtual machine components has increased by 1.
        self.assertEqual(7, count_components(manifest, 'rwtasklet'))
        self.assertEqual(3, count_components(manifest, 'rwvm'))

    def DISABLED_test_find_ancestor(self):
        # Create a system that contains a colony, a cluster, and 2 virtual
        # machines.
        cli = rift.vcs.vms.CliVM()
        mgmt = rift.vcs.vms.MgmtVM()

        cluster = rift.vcs.Cluster(
                name="test.cluster",
                virtual_machines=[cli, mgmt],
                )

        colony = rift.vcs.Colony(
                name="test.colony",
                clusters=[cluster],
                )

        sysinfo = rift.vcs.SystemInfo(
                mode='ethsim',
                collapsed=True,
                colonies=[colony],
                )

        # Compile the manifest
        compiler = rift.vcs.compiler.LegacyManifestCompiler()
        _, manifest = compiler.compile(sysinfo)

        # "" the CLI, colony, and cluster components
        cli = manifest.find_by_class(rift.vcs.manifest.RaCliProc)
        colony = manifest.find_by_class(rift.vcs.manifest.RaColony)
        cluster = manifest.find_by_class(rift.vcs.manifest.RaCluster)

        # Verify that the enclosing function for colonies and clusters return
        # the correct components
        self.assertTrue(manifest.find_enclosing_colony(cli) is colony)
        self.assertTrue(manifest.find_enclosing_cluster(cli) is cluster)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))


if __name__ == "__main__":
    main()
