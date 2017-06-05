#!/usr/bin/env python3
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file utest_transforms.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 04/21/2015
#
"""

import argparse
import logging
import os
import sys
import unittest

import xmlrunner

import rift.vcs
import rift.vcs.compiler.constraints
import rift.vcs.compiler.transforms
import rift.vcs.compiler.exc
import rift.vcs.vms


logger = logging.getLogger(__name__)


class TestSystemTransforms(unittest.TestCase):
    def test_assign_fastpath_ids(self):
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Collection(
                        name='test.collection',
                        subcomponents=[
                            rift.vcs.vms.LeadVM(),
                            rift.vcs.vms.LeadVM(),
                            rift.vcs.vms.LeadVM(),
                            ]
                        )
                    ]
                )

        # Before the transform is applied, none of the fastpath tasklets should
        # have a UID.
        tasklets = sysinfo.list_by_class(rift.vcs.Fastpath)
        for tasklet in tasklets:
            self.assertIsNone(tasklet.uid)

        transform = rift.vcs.compiler.transforms.AssignFastpathIds()
        transform(sysinfo)

        # After the transform, each fastpath tasklet should have been assigned
        # a UID.
        for uid, tasklet in enumerate(tasklets, start=1):
            self.assertEqual(uid, tasklet.uid)

    def test_assign_cluster_ids(self):
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Cluster(
                        name='test.cluster',
                        virtual_machines=[
                            rift.vcs.vms.LeadVM(),
                            rift.vcs.vms.LeadVM(),
                            rift.vcs.vms.LeadVM(),
                            ]
                        )
                    ]
                )

        # Check that the cluster has not been assigned a UID
        cluster = sysinfo.find_by_class(rift.vcs.Cluster)
        self.assertIsNone(cluster.uid)

        transform = rift.vcs.compiler.transforms.AssignClusterIds()
        transform(sysinfo)

        # After applying the transform, the cluster should have an ID of 1
        # since it is the only component to have a UID.
        self.assertEqual(1, cluster.uid)

    def test_assign_fastpath_induced_unique_names(self):
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Cluster(
                        name='test.cluster',
                        virtual_machines=[
                            rift.vcs.vms.LeadVM(name='test.vm'),
                            rift.vcs.vms.LeadVM(name='test.vm'),
                            ]
                        )
                    ]
                )

        # Construct a list of the components that are ancestors of the fastpath
        # tasklets as well as the fastpath tasklets themselves.
        ancestors = sysinfo.list_by_class(rift.vcs.Fastpath)
        for component in sysinfo:
            if hasattr(component, 'subcomponents'):
                if any((c in component.subcomponents for c in ancestors)):
                    ancestors.append(component)

        # There should be 7 ancestors, but only 4 of them have unique component
        # names.
        self.assertEqual(7, len(ancestors))
        self.assertEqual(4, len(set((a.name for a in ancestors))))

        transform = rift.vcs.compiler.transforms.AssignFastpathInducedUniqueNames()
        transform(sysinfo)

        # After applying the transform, all of the ancestor should have unique
        # component names.
        self.assertEqual(7, len(set((a.name for a in ancestors))))

    def test_assign_web_servers_uagent_port(self):
        webserver = rift.vcs.Webserver()
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Collection(
                        name='test.collection',
                        subcomponents=[
                            rift.vcs.VirtualMachine(
                                name='test.vm',
                                procs=[webserver],
                                )
                            ]
                        )
                    ]
                )

        transform = rift.vcs.compiler.transforms.AssignWebServersUAgentPort()

        # If there is no uagent in the system, this transform should raise an
        # error.
        with self.assertRaises(rift.vcs.compiler.exc.TransformError):
            transform(sysinfo)

        # Create a uagent tasklets and added it to the virtual machine.
        uagent = rift.vcs.uAgentTasklet()
        uagent.port = 2134

        vm = sysinfo.find_by_class(rift.vcs.VirtualMachine)
        vm.subcomponents.append(uagent)

        # Check that the uagent port on the webserver has not been assigned
        # yet.
        self.assertIsNone(webserver.uagent_port)

        transform(sysinfo)

        # After the transform, the webserver should have correct uagent port.
        self.assertEqual(uagent.port, webserver.uagent_port)

    def test_generate_collection_instance_ids(self):
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Colony(name='test.colony-1'),
                    rift.vcs.Colony(
                        name='test.colony-2',
                        clusters=[
                            rift.vcs.Cluster(name='test.cluster-1'),
                            rift.vcs.Cluster(name='test.cluster-2'),
                            rift.vcs.Collection(name='test.collection'),
                            ]
                        )
                    ]
                )

        # Verify that none of the components have been assigned UIDs
        for component in sysinfo:
            self.assertIsNone(component.uid)

        transform = rift.vcs.compiler.transforms.GenerateCollectionInstanceIDs()
        transform(sysinfo)

        # Each collections should now have an UID.
        for uid, component in enumerate(sysinfo, start=1):
            self.assertEqual(uid, component.uid)

    def test_assign_webservers_confd_host(self):
        webserver1 = rift.vcs.Webserver()
        webserver2 = rift.vcs.Webserver()
        uagent = rift.vcs.uAgentTasklet()

        vm1 = rift.vcs.VirtualMachine(ip='127.0.0.1', procs=[webserver1])
        vm2 = rift.vcs.VirtualMachine(ip='127.0.0.2', procs=[webserver2, uagent])

        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Collection(
                        name='test.collection',
                        subcomponents=[vm1, vm2],
                        )
                    ]
                )

        # By default, the webservers set the confd host to 'localhost'
        self.assertEqual('localhost', webserver1.confd_host)
        self.assertEqual('localhost', webserver2.confd_host)

        transform = rift.vcs.compiler.transforms.AssignWebserversConfdHost()
        transform(sysinfo)

        # After the transform, the webservers both have the IP of the VM that
        # contains the uagent process.
        self.assertEqual('127.0.0.2', webserver1.confd_host)
        self.assertEqual('127.0.0.2', webserver2.confd_host)

    def test_enforce_message_broker_policy(self):
        colony = rift.vcs.core.Colony(name='test.colony')
        vm = rift.vcs.core.VirtualMachine(name='test.vm')
        broker = rift.vcs.tasklets.MsgBrokerTasklet()

        colony.append(vm)
        vm.add_tasklet(broker)

        sysinfo = rift.vcs.core.SystemInfo(colonies=[colony],)
        transform = rift.vcs.compiler.transforms.EnforceMessageBrokerPolicy()

        # When the system is in single-broker mode, the transform should have
        # no effect.
        sysinfo.multi_broker = False
        transform(sysinfo)
        self.assertEqual(1, len(sysinfo.list_by_class(rift.vcs.tasklets.MsgBrokerTasklet)))
        self.assertEqual(0, len(sysinfo.vm_bootstrap))

        # When the system is in multi-broker mode, the transform should remove
        # the broker.
        sysinfo.multi_broker = True
        transform(sysinfo)
        self.assertEqual(0, len(sysinfo.list_by_class(rift.vcs.tasklets.MsgBrokerTasklet)))
        self.assertEqual(1, len(sysinfo.vm_bootstrap))

    def test_enforce_dtsrouter_policy(self):
        colony = rift.vcs.core.Colony(name='test.colony')
        vm = rift.vcs.core.VirtualMachine(name='test.vm')
        dtsrouter = rift.vcs.tasklets.DtsRouterTasklet()

        colony.append(vm)
        vm.add_tasklet(dtsrouter)

        sysinfo = rift.vcs.core.SystemInfo(colonies=[colony],)
        transform = rift.vcs.compiler.transforms.EnforceDtsRouterPolicy()

        # When the system is in single-dtsrouter mode, the transform should have
        # no effect.
        sysinfo.multi_dtsrouter = False
        transform(sysinfo)
        self.assertEqual(1, len(sysinfo.list_by_class(rift.vcs.tasklets.DtsRouterTasklet)))
        self.assertEqual(0, len(sysinfo.vm_bootstrap))

        # When the system is in multi-dtsrouter mode, the transform should remove
        # the dtsrouter.
        sysinfo.multi_dtsrouter = True
        transform(sysinfo)
        self.assertEqual(0, len(sysinfo.list_by_class(rift.vcs.tasklets.DtsRouterTasklet)))
        self.assertEqual(1, len(sysinfo.vm_bootstrap))

    def test_assign_dtsrouter_vm(self):
        manifest = rift.vcs.manifest.RaManifest(northbound_listing=None, persist_dir_name='test')
        transform = rift.vcs.compiler.transforms.AssignDtsRouterVM()

        # A CompilationError is raised if the system does not contain a 
        # dtsrouter component.
        with self.assertRaises(rift.vcs.compiler.exc.CompilationError):
            transform(manifest, rift.vcs.SystemInfo())

        dtsrouter = rift.vcs.manifest.RaDtsRouter(name='test.dtsrouter')
        manifest.add_component(dtsrouter)

        # If the dtsrouter component is not contained by a virtual machine
        # a CompilationError is raised.
        with self.assertRaises(rift.vcs.compiler.exc.CompilationError):
            transform(manifest, rift.vcs.SystemInfo())

        # Remove the dtsrouter from the manifest and add a colony with two virtual
        # machines. Put the dtsrouter in one of the virtual machines.
        manifest.remove_component(dtsrouter)

        vm1 = rift.vcs.manifest.RaVm(name='test.vm1')
        vm2 = rift.vcs.manifest.RaVm(name='test.vm2')

        vm1.add_component(dtsrouter)

        colony = rift.vcs.manifest.RaColony(name='test.colony')

        colony.add_component(vm1)
        colony.add_component(vm2)

        manifest.add_component(colony)

        transform(manifest, rift.vcs.SystemInfo())

        # The VM containing the message dtsrouter should be assigned as the
        # dtsrouter VM.
        self.assertEqual(manifest.dtsrouter_vm, vm1)

    def test_add_rwlog(self):
        colony = rift.vcs.core.Colony(name='test.colony')
        vm = rift.vcs.core.VirtualMachine(name='test.vm')
        log = rift.vcs.tasklets.LogdTasklet()

        colony.append(vm)
        vm.add_tasklet(log)

        transform = rift.vcs.compiler.transforms.AddLogd()

        # The log instance should always be removed.  In collapsed
        # mode a new log instance will be added to the first vm,
        # in expanded mode it will be added to bootstrap.
        sysinfo = rift.vcs.core.SystemInfo(collapsed=True, colonies=[colony],)
        transform(sysinfo)
        new_log = sysinfo.find_by_class(rift.vcs.tasklets.LogdTasklet)
        self.assertIsNotNone(new_log)
        self.assertNotEqual(new_log, log)

        sysinfo = rift.vcs.core.SystemInfo(collapsed=False, colonies=[colony],)
        transform(sysinfo)
        new_log = sysinfo.find_by_class(rift.vcs.tasklets.LogdTasklet)
        self.assertIsNone(new_log)
        self.assertEqual(1, len(sysinfo.vm_bootstrap))
        self.assertIsInstance(sysinfo.vm_bootstrap[0], rift.vcs.tasklets.LogdTasklet)

    def test_assign_netconf_host_to_cli(self):
        cli_proc = rift.vcs.RiftCli()
        uagent = rift.vcs.uAgentTasklet()

        vm1 = rift.vcs.VirtualMachine(ip='127.0.0.1', procs=[cli_proc])
        vm2 = rift.vcs.VirtualMachine(ip='127.0.0.2', procs=[uagent])

        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Collection(
                        name='test.collection',
                        subcomponents=[vm1, vm2],
                        )
                    ]
                )

        # By default, the RiftCli set the netconf host to '127.0.0.1'
        self.assertEqual('127.0.0.1', cli_proc.netconf_host)

        transform = rift.vcs.compiler.transforms.AssignNetconfHostToRiftCli()
        transform(sysinfo)

        # After the transform, the RiftCli netconf host is changed
        self.assertEqual('127.0.0.1', cli_proc.netconf_host)


class TestManifestTransforms(unittest.TestCase):
    def test_startup_colony(self):
        cli = rift.vcs.manifest.RaCliProc(name='test.cli')
        vm = rift.vcs.manifest.RaVm(name='test.vm')

        cluster = rift.vcs.manifest.RaCluster(name='test.cluster')
        cluster.add_component(vm)

        colony = rift.vcs.manifest.RaColony(name='test.colony-1')
        colony.add_component(cluster)

        manifest = rift.vcs.manifest.RaManifest(northbound_listing=None, persist_dir_name='test')
        manifest.add_component(colony)
        manifest.init = cli

        # Initially the init component will have no startups
        self.assertEqual(0, len(manifest.init.startups))

        transform = rift.vcs.compiler.transforms.StartupColony()
        transform(manifest, rift.vcs.SystemInfo())

        # After the transform the init component will have 1 startup and it
        # should be the colony.
        self.assertEqual(1, len(manifest.init.startups))
        self.assertEqual(colony, manifest.init.startups[0])

    def test_assign_broker_vm(self):
        manifest = rift.vcs.manifest.RaManifest(northbound_listing=None, persist_dir_name='test')
        transform = rift.vcs.compiler.transforms.AssignBrokerVM()

        # A CompilationError is raised if the system does not contain a message
        # broker component.
        with self.assertRaises(rift.vcs.compiler.exc.CompilationError):
            transform(manifest, rift.vcs.SystemInfo())

        broker = rift.vcs.manifest.RaBroker(name='test.broker')
        manifest.add_component(broker)

        # If the message broker component is not contained by a virtual machine
        # a CompilationError is raised.
        with self.assertRaises(rift.vcs.compiler.exc.CompilationError):
            transform(manifest, rift.vcs.SystemInfo())

        # Remove the broker from the manifest and add a colony with two virtual
        # machines. Put the broker in one of the virtual machines.
        manifest.remove_component(broker)

        vm1 = rift.vcs.manifest.RaVm(name='test.vm1')
        vm2 = rift.vcs.manifest.RaVm(name='test.vm2')

        vm1.add_component(broker)

        colony = rift.vcs.manifest.RaColony(name='test.colony')

        colony.add_component(vm1)
        colony.add_component(vm2)

        manifest.add_component(colony)

        transform(manifest, rift.vcs.SystemInfo())

        # The VM containing the message broker should be assigned as the
        # broker VM.
        self.assertEqual(manifest.broker_vm, vm1)

    def test_assign_init_vm(self):
        vm1 = rift.vcs.manifest.RaVm(name='test.vm-1')
        vm2 = rift.vcs.manifest.RaVm(name='test.vm-2')

        cluster = rift.vcs.manifest.RaCluster(name='test.cluster')
        cluster.add_component(vm1)
        cluster.add_component(vm2)

        colony = rift.vcs.manifest.RaColony(name='test.colony')
        colony.add_component(cluster)

        manifest = rift.vcs.manifest.RaManifest(northbound_listing=None, persist_dir_name='test')
        manifest.add_component(colony)
        manifest.mgmt_ip_list = []

        transform = rift.vcs.compiler.transforms.AssignInitVM()

        with self.assertRaises(rift.vcs.compiler.exc.CompilationError):
            transform(manifest, rift.vcs.SystemInfo())

        uagent = rift.vcs.manifest.RaUagent(name='test.uagent', port=6578)
        colony.add_component(uagent)

        with self.assertRaises(rift.vcs.compiler.exc.CompilationError):
            transform(manifest, rift.vcs.SystemInfo())

        colony.subcomponents.remove(uagent)
        vm1.add_component(uagent)

        transform(manifest, rift.vcs.SystemInfo())

    def test_assign_ip_addresses(self):
        manifest = rift.vcs.manifest.RaManifest(northbound_listing=None, persist_dir_name='test')
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Collection(
                        name='test.collection',
                        subcomponents=[
                            rift.vcs.VirtualMachine(ip='127.0.0.1'),
                            rift.vcs.VirtualMachine(ip='127.0.0.2'),
                            rift.vcs.VirtualMachine(ip='127.0.0.3'),
                            ]
                        )
                    ]
                )

        # Retrieve a list of the VMs in the system
        vms = sysinfo.list_by_class(rift.vcs.VirtualMachine)

        # The manifest should have no IP addresses
        self.assertEqual(0, len(manifest.ipaddresses))

        transform = rift.vcs.compiler.transforms.AssignIPAddresses()
        transform(manifest, sysinfo)

        # The manifest should have a list of all the IPs of the VMs in the
        # system.
        self.assertEqual(len(vms), len(manifest.ipaddresses))

        for vm, ip in zip(vms, manifest.ipaddresses):
            self.assertEqual(vm.ip, ip)


def main(argv = sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')

    args = parser.parse_args(argv)

    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    testRunner = xmlrunner.XMLTestRunner(output=os.environ['RIFT_MODULE_TEST'])
    unittest.main(argv=[__file__] + argv, testRunner=testRunner)


if __name__ == '__main__':
    main()

# vim: sw=4
