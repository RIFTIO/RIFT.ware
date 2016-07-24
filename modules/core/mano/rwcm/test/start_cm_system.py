#!/usr/bin/env python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import logging
import os
import sys

import rift.vcs
import rift.vcs.demo
import rift.vcs.vms

from rift.vcs.ext import ClassProperty

logger = logging.getLogger(__name__)


class ConfigManagerTasklet(rift.vcs.core.Tasklet):
    """
    This class represents SO tasklet.
    """

    def __init__(self, name='rwcmtasklet', uid=None):
        """
        Creates a PingTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
        """
        super(ConfigManagerTasklet, self).__init__(name=name, uid=uid)

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwconmantasklet')
    plugin_name = ClassProperty('rwconmantasklet')


# Construct the system. This system consists of 1 cluster in 1
# colony. The master cluster houses CLI and management VMs
sysinfo = rift.vcs.SystemInfo(
        colonies=[
            rift.vcs.Colony(
                clusters=[
                    rift.vcs.Cluster(
                        name='master',
                        virtual_machines=[
                            rift.vcs.VirtualMachine(
                                name='vm-so',
                                ip='127.0.0.1',
                                tasklets=[
                                    rift.vcs.uAgentTasklet(),
                                    ],
                                procs=[
                                    rift.vcs.CliTasklet(manifest_file="cli_rwcm.xml"),
                                    rift.vcs.DtsRouterTasklet(),
                                    rift.vcs.MsgBrokerTasklet(),
                                    rift.vcs.RestconfTasklet(),
                                    ConfigManagerTasklet()
                                    ],
                                ),
                            ]
                        )
                    ]
                )
            ]
        )


# Define the generic portmap.
port_map = {}


# Define a mapping from the placeholder logical names to the real
# port names for each of the different modes supported by this demo.
port_names = {
    'ethsim': {
    },
    'pci': {
    }
}


# Define the connectivity between logical port names.
port_groups = {}

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')

    # Create a parser which includes all generic demo arguments
    parser = rift.vcs.demo.DemoArgParser()

    args = parser.parse_args(argv)

    #load demo info and create Demo object
    demo = rift.vcs.demo.Demo(sysinfo=sysinfo,
                              port_map=port_map,
                              port_names=port_names,
                              port_groups=port_groups)

    # Create the prepared system from the demo
    system = rift.vcs.demo.prepared_system_from_demo_and_args(demo, args, netconf_trace_override=True)

    # Start the prepared system
    system.start()


if __name__ == "__main__":
    try:
        main()
    except rift.vcs.demo.ReservationError:
        print("ERROR: unable to retrieve a list of IP addresses from the reservation system")
        sys.exit(1)
    except rift.vcs.demo.MissingModeError:
        print("ERROR: you need to provide a mode to run the script")
        sys.exit(1)
    finally:
        os.system("stty sane")
