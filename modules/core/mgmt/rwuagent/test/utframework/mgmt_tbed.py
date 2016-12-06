#!/usr/bin/env python3

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


import logging
import os
import pprint
import sys
import argparse

import rift.vcs
import rift.vcs.vms

import rift.vcs.demo
from rift.vcs.ext import ClassProperty
from argparse import Action

class TestTasklet(rift.vcs.Tasklet):
    """
    This class represents a TestTasklet for uagent UT framework.
    """

    def __init__(self, name="RW.TestTasklet", uid=None):
        """Creates a TestTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier

        """
        super(TestTasklet, self).__init__(name=name, uid=uid)

    plugin_name = ClassProperty("testtasklet")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/testtasklet")

class TestDriverTasklet(rift.vcs.Tasklet):
    """
    This class represents the TestDriverTasklet for uagent UT framework.
    """

    def __init__(self, name="RW.TestDriverTasklet", uid=None):
        """Creates a TestDriverTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier

        """
        super(TestDriverTasklet, self).__init__(name=name, uid=uid)

    plugin_name = ClassProperty("testdriver_tasklet")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/testdriver_tasklet")

class Demo(rift.vcs.demo.Demo):
    def __init__(self, test_name=None):

        procs = [
            rift.vcs.DtsRouterTasklet(),
            rift.vcs.MsgBrokerTasklet(),
            rift.vcs.RestconfTasklet(),
            rift.vcs.RestPortForwardTasklet(),
            rift.vcs.RiftCli(),
            rift.vcs.uAgentTasklet(),
            TestTasklet(),
            TestDriverTasklet()
            ]

        super(Demo, self).__init__(


            sysinfo = rift.vcs.SystemInfo(
                    colonies=[rift.vcs.Colony(
                        clusters=[rift.vcs.Cluster(
                            name='master',
                            virtual_machines=[
                                rift.vcs.VirtualMachine(
                                    name="VM.MGMT",
                                    ip='127.0.0.1',
                                    tasklets=[
                                        ],
                                    procs=procs
                                        ),
                                    ]),
                            ]),
                    ],
                    test_name=test_name),
                              
            # Define the generic portmap.
            port_map = {},

            # Define a mapping from the placeholder logical names to the real
            # port names for each of the different modes supported by this demo.
            port_names = {
                'ethsim': {
                },
                'pci': {
                }
            },

            # Define the connectivity between logical port names.
            port_groups = {},
        )


def main(argv=sys.argv[1:]):

    parser = rift.vcs.demo.DemoArgParser()
    args = parser.parse_args(argv)
    args.collapsed = True
    args.skip_prepare_vm = True
    
    #os.environ['RIFT_NO_SUDO_REAPER'] = '1'
    #os.environ['RW_VAR_RIFT'] = '/tmp'

    system = rift.vcs.demo.prepared_system_from_demo_and_args(Demo(args.test_name), args,
                        northbound_listing="mgmtagt_tbed_schemas.txt",
                        netconf_trace_override=True)

    system.start()
  
if __name__ == "__main__":
    main()
