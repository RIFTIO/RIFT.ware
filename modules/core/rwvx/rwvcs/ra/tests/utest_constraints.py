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
# @file utest_constraints.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 04/20/2015
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
import rift.vcs.compiler.exc
import rift.vcs.vms

logger = logging.getLogger(__name__)


class TestSystemConstraints(unittest.TestCase):
    def test_unique_uagent(self):
        # Define a toy system that contains 2 uagents
        sysinfo = rift.vcs.SystemInfo(
                colonies=[
                    rift.vcs.Collection(
                        name='test.collection',
                        subcomponents=[
                            rift.vcs.VirtualMachine(
                                name='test.vm',
                                tasklets=[
                                    rift.vcs.uAgentTasklet(),
                                    rift.vcs.uAgentTasklet(),
                                    ]
                                )
                            ]
                        )
                    ]
                )

        constraint = rift.vcs.compiler.constraints.UniqueUAgent()

        # The constraint should raise a constraint error because there are 2
        # instances of the uAgentTasklet in the system.
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(sysinfo)

        # After removing one of the uagent tasklets, the constraint should be
        # satisfied.
        sysinfo.remove(sysinfo.find_by_class(rift.vcs.uAgentTasklet))
        constraint(sysinfo)

        # If the final uagent is removed, the constraint is no longer satisfied
        # and it should raise a ConstraintError
        sysinfo.remove(sysinfo.find_by_class(rift.vcs.uAgentTasklet))
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(sysinfo)

    def test_adjacent_agent_and_restconf(self):
        # A virtual machine must contain both a uAgent tasklet and a restconf
        # tasklet or neither.
        constraint = rift.vcs.compiler.constraints.AdjacentAgentRestconfTasklets()

        # First, consider a system that contains a virtual machine with one
        # confd tasklet.
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(
                    rift.vcs.SystemInfo(
                        colonies=[
                            rift.vcs.Collection(
                                name='test.collection',
                                subcomponents=[
                                    rift.vcs.VirtualMachine(
                                        name='test.vm',
                                        procs=[
                                            rift.vcs.uAgentTasklet(),
                                            ]
                                        )
                                    ]
                                )
                            ]
                        )
                    )

        # Now, consider a system that contains a virtual machine with one
        # restconf tasklet.
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(
                    rift.vcs.SystemInfo(
                        colonies=[
                            rift.vcs.Collection(
                                name='test.collection',
                                subcomponents=[
                                    rift.vcs.VirtualMachine(
                                        name='test.vm',
                                        procs=[
                                            rift.vcs.RestconfTasklet(),
                                            ]
                                        )
                                    ]
                                )
                            ]
                        )
                    )

        # Finally, consider a system that contains a virtual machine with both
        # a confd tasklet and a restconf tasklet.
        constraint(
                rift.vcs.SystemInfo(
                    colonies=[
                        rift.vcs.Collection(
                            name='test.collection',
                            subcomponents=[
                                rift.vcs.VirtualMachine(
                                    name='test.vm',
                                    procs=[
                                        rift.vcs.uAgentTasklet(),
                                        rift.vcs.RestconfTasklet(),
                                        ]
                                    )
                                ]
                            )
                        ]
                    )
                )


class TestManifestConstraints(unittest.TestCase):
    def test_unique_instance_ids(self):
        sysinfo = rift.vcs.SystemInfo()
        manifest = rift.vcs.manifest.RaManifest(northbound_listing="doesn't matter", persist_dir_name="test")

        vm1 = rift.vcs.manifest.RaVm(name='test.vm')
        vm2 = rift.vcs.manifest.RaVm(name='test.vm')
        vm3 = rift.vcs.manifest.RaVm(name='test.vm')

        collection1 = rift.vcs.manifest.RaCollection(
                name='test.collection',
                tag='rwcolony',
                instance_id=1,
                )
        collection2 = rift.vcs.manifest.RaCollection(
                name='test.collection',
                tag='rwcolony',
                instance_id=2,
                )

        collection1.add_component(vm1)
        collection2.add_component(vm2)
        collection2.add_component(vm3)

        manifest.add_component(collection1)
        manifest.add_component(collection2)

        constraint = rift.vcs.compiler.constraints.UniqueInstanceIds()
        constraint(manifest, sysinfo)

        vm1.instance_id = 1
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

    def test_check_fastpath_induced_unique_names(self):
        # Create a system with 2 VMs, each containing a fastpath tasklet
        fpath1 = rift.vcs.Fastpath()
        fpath2 = rift.vcs.Fastpath()

        proc1 = rift.vcs.Proc(tasklets=[fpath1])
        proc2 = rift.vcs.Proc(tasklets=[fpath2])

        vm1 = rift.vcs.VirtualMachine(name='test.vm', procs=[proc1])
        vm2 = rift.vcs.VirtualMachine(name='test.vm', procs=[proc2])

        sysinfo = rift.vcs.SystemInfo(colonies=[vm1, vm2])

        constraint = rift.vcs.compiler.constraints.CheckFastpathInducedUniqueNames()

        # There are duplicate names in the system that violate the constraint
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(sysinfo)

        # Giving the the fastpath tasklets unique names is insufficient. All
        # component that are ancestors of a fastpath tasklets must have unique
        # names.
        fpath1.name = 'test.fpath-1'
        fpath2.name = 'test.fpath-2'

        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(sysinfo)

        # Assigning unique names to all of the components satisfies the
        # constraint.
        proc1.name = 'test.proc.fpath-1'
        proc2.name = 'test.proc.fpath-2'
        vm1.name = 'test.vm-1'
        vm2.name = 'test.vm-2'

        constraint(sysinfo)

    def test_check_number_of_brokers(self):
        sysinfo = rift.vcs.SystemInfo()
        manifest = rift.vcs.manifest.RaManifest(northbound_listing="doesn't matter", persist_dir_name="test")
        constraint = rift.vcs.compiler.constraints.CheckNumberOfBrokers()

        # It is valid for the manifest to have no broker in either multi-broker
        # or single-broker mode.
        manifest.multi_broker = True
        constraint(manifest, sysinfo)

        manifest.multi_broker = False
        constraint(manifest, sysinfo)

        # It is not valid for the manifest to have any brokers when
        # multi-broker mode is enabled.
        manifest.add_component(rift.vcs.manifest.RaBroker(name='broker'))

        sysinfo.multi_broker = True
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        sysinfo.multi_broker = False
        constraint(manifest, sysinfo)

        # It is not valid for the manifest to have more than one broker in
        # either multi-broker or single-broker mode.
        manifest.add_component(rift.vcs.manifest.RaBroker(name='broker'))

        sysinfo.multi_broker = True
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        sysinfo.multi_broker = False
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

    def test_check_number_of_dtsrouters(self):
        sysinfo = rift.vcs.SystemInfo()
        manifest = rift.vcs.manifest.RaManifest(northbound_listing="doesn't matter", persist_dir_name="test")
        constraint = rift.vcs.compiler.constraints.CheckNumberOfDtsRouters()

        # It is valid for the manifest to have no dtsrouter in either multi-dtsrouter
        # or single-dtsrouter mode.
        manifest.multi_dtsrouter = True
        constraint(manifest, sysinfo)

        manifest.multi_dtsrouter = False
        constraint(manifest, sysinfo)

        # It is not valid for the manifest to have any dtsrouters when
        # multi-dtsrouter mode is enabled.
        manifest.add_component(rift.vcs.manifest.RaDtsRouter(name='dtsrouter'))

        manifest.multi_dtsrouter = True
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        manifest.multi_dtsrouter = False
        constraint(manifest, sysinfo)

        # It is not valid for the manifest to have more than one dtsrouter in
        # either multi-dtsrouter or single-dtsrouter mode.
        manifest.add_component(rift.vcs.manifest.RaDtsRouter(name='dtsrouter'))

        manifest.multi_dtsrouter = True
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        manifest.multi_dtsrouter = False
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

    def test_check_number_of_rwlog(self):
        sysinfo = rift.vcs.SystemInfo()
        manifest = rift.vcs.manifest.RaManifest(northbound_listing="doesn't matter", persist_dir_name="test")
        constraint = rift.vcs.compiler.constraints.CheckNumberOfLogd()

        # It is never valid for the manifest to have no rwlogs and no
        # bootstrap.
        sysinfo.collapsed = True
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        sysinfo.collapsed = False
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        log = rift.vcs.manifest.Logd(name='blah')

        # Bootstrap and expanded is valid
        sysinfo.collapsed = False
        manifest.vm_bootstrap.append(log)
        constraint(manifest, sysinfo)

        # Expanded and rwlog is invalid
        manifest.add_component(log)
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        # Collapsed and rwlog is valid
        sysinfo.collapsed = True
        manifest.vm_bootstrap = []
        constraint(manifest, sysinfo)

    def test_check_component_names_are_not_none(self):
        sysinfo = rift.vcs.SystemInfo()
        manifest = rift.vcs.manifest.RaManifest(northbound_listing="doesn't matter",persist_dir_name="test")

        # Add a single, unnamed colony to the manifest
        colony = rift.vcs.manifest.RaColony(name=None)
        manifest.add_component(colony)

        constraint = rift.vcs.compiler.constraints.CheckComponentNamesAreNotNone()

        # The constraint should raise an exception because the colony is
        # unnamed.
        with self.assertRaises(rift.vcs.compiler.exc.ConstraintError):
            constraint(manifest, sysinfo)

        # Assign the colony a name and now the constraint should not raise an
        # exception.
        colony.name = 'test.colony'
        constraint(manifest, sysinfo)


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
