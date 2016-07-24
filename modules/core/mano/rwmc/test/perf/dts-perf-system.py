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

logger = logging.getLogger(__name__)

class Webserver(rift.vcs.NativeProcess):
    def __init__(self, host, name="rw.perf.webserver"):
        super(Webserver, self).__init__(
                name=name,
                exe="./usr/local/bin/dts-perf-webserver.py",
                args="--host={}".format(host),
                )


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')

    # @HACK over-ride the mode (if is not important for this system)
    argv.extend(('--mode', 'ethsim'))

    # Create a parser which includes all generic demo arguments
    parser = rift.vcs.demo.DemoArgParser(conflict_handler='resolve')

    args = parser.parse_args(argv)

    # @HACK There should be one host IP provided; Use it as the host of the
    # webserver.
    host = args.ip_list[0]

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
                                    name='vm-mission-control',
                                    ip='127.0.0.1',
                                    tasklets=[
                                        rift.vcs.uAgentTasklet(),
                                        ],
                                    procs=[
                                        rift.vcs.CliTasklet(),
                                        rift.vcs.MissionControl(),
                                        rift.vcs.DtsRouterTasklet(),
                                        rift.vcs.MsgBrokerTasklet(),
                                        Webserver(host),
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

    #load demo info and create Demo object
    demo = rift.vcs.demo.Demo(sysinfo=sysinfo,
                              port_map=port_map,
                              port_names=port_names,
                              port_groups=port_groups)

    # Create the prepared system from the demo
    system = rift.vcs.demo.prepared_system_from_demo_and_args(demo, args, 
              northbound_listing="cli_rwfpath_schema_listing.txt",
              netconf_trace_override=True)

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
