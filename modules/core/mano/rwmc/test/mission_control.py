#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import ipaddress
import logging
import os
import shlex
import socket
import subprocess
import sys

import rift.vcs
import rift.vcs.demo
import rift.vcs.vms

import rift.rwcal.cloudsim
import rift.rwcal.cloudsim.net

logger = logging.getLogger(__name__)


class MinionConnectionError(Exception):
    pass


class MissionControlUI(rift.vcs.NativeProcess):
    def __init__(self, name="RW.MC.UI"):
        super(MissionControlUI, self).__init__(
                name=name,
                exe="./usr/share/rw.ui/skyquake/scripts/launch_ui.sh",
                )

    @property
    def args(self):
        return ' '


class Demo(rift.vcs.demo.Demo):
    def __init__(self, use_mock=False, skip_ui=False, disable_cnt_mgr=False):

        procs = [
            rift.vcs.RiftCli(),
            rift.vcs.DtsRouterTasklet(),
            rift.vcs.MsgBrokerTasklet(),
            rift.vcs.RestconfTasklet(),
            rift.vcs.RestPortForwardTasklet(),
            rift.vcs.CalProxy(),
            ]

        if not use_mock:
            procs.append(rift.vcs.MissionControl())
            if not disable_cnt_mgr:
                procs.append(rift.vcs.ContainerManager())
        else:
            procs.extend([rift.vcs.CrossbarServer(), rift.vcs.DtsMockServerTasklet()])

        if not skip_ui:
            procs.extend([MissionControlUI()])

        super(Demo, self).__init__(
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
                                            procs=procs,
                                            ),
                                        ]
                                    )
                                ]
                            )
                        ]
                    ),

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


def check_salt_master_running():
    cmd = "systemctl status salt-master.service | grep Active | awk '{print $2}'"
    salt_master_status = subprocess.check_output(cmd, universal_newlines=True, shell=True).rstrip('\n')
    if salt_master_status != 'active':
        logger.error("Salt master is not running on the host.")
        logger.error("Start the salt master (systemctl start salt-master.service) and re-run mission control.")
        exit(1)


def clear_salt_keys():
    # clear all the previosly installed salt keys
    logger.info("Removing all unconnected salt keys")
    stdout = subprocess.check_output(
            shlex.split('salt-run manage.down'),
            universal_newlines=True,
            )

    down_minions = stdout.splitlines()

    for line in down_minions:
        salt_id = line.strip().replace("- ", "")
        logger.info("Removing old unconnected salt id: %s", salt_id)
        minion_keys_stdout = subprocess.check_output(
                shlex.split('salt-key -f {}'.format(salt_id)),
                universal_newlines=True)

        minion_keys = minion_keys_stdout.splitlines()
        for key_line in minion_keys:
            if "Keys" in key_line:
                continue

            key_split = key_line.split(":")
            if len(key_split) < 2:
                continue

            key = key_split[0]

            # Delete the minion key
            logger.info("Deleting minion %s key: %s", salt_id, key)
            subprocess.check_call(shlex.split('salt-key -d {} -y'.format(key)))


def is_node_connected(node_id):
    try:
        stdout = subprocess.check_output(
                shlex.split('salt %s test.ping' % node_id),
                universal_newlines=True,
                )
    except subprocess.CalledProcessError:
        msg = "test.ping command failed against node_id: %s" % node_id
        logger.warning(msg)
        raise MinionConnectionError(msg)

    up_minions = stdout.splitlines()
    for line in up_minions:
        if "True" in line:
            return True

    return False

def construct_lp_public_ip_env_var(lp_public_ip):
    ipaddress.IPv4Address(lp_public_ip)
    os.environ["RIFT_LP_PUBLIC_IP"] = lp_public_ip

def construct_lp_node_env_var(lp_salt_node_ip_ids):
    format_msg = "--lp-node-id parameter must be in the following format <vm_ip>:<salt_node_id>"
    env_node_ip_str = ""
    for node_ip_id in lp_salt_node_ip_ids:
        if ":" not in node_ip_id:
            raise ValueError(format_msg)

        ip_id_list = node_ip_id.split(":")
        if len(ip_id_list) != 2:
            raise ValueError(format_msg)

        node_ip, node_id = ip_id_list

        # Validate the VM ip address provided
        ipaddress.IPv4Address(node_ip)

        if not is_node_connected(node_id):
            logger.warning("Salt minion id %s is not connected", node_id)

        env_node_ip_str += "{}|{}:".format(node_ip, node_id)

    env_node_ip_str = env_node_ip_str.rstrip(":")

    os.environ["RIFT_LP_NODES"] = env_node_ip_str


def main(argv=sys.argv[1:]):
    logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)-15s %(levelname)s %(message)s')

    # Create a parser which includes all generic demo arguments
    parser = rift.vcs.demo.DemoArgParser(conflict_handler='resolve')

    parser.add_argument(
            "--mock",
            help="Start the DTS mock server",
            action="store_true",
            )

    parser.add_argument(
            "--no-cntr-mgr",
            action="store_true",
            help="Disable the container manager"
            )

    parser.add_argument(
            "--skip-ui",
            help="Do not start UI services (MissionControlUI and Composer)",
            action="store_true",
            )

    parser.add_argument(
            "--lp-node-id",
            help="Use provided vm ip and salt node id's as launchpad VM's if "
                 "no static resources allocated. Pass in as <vm_ip>:<salt_node_id>",
            action='append',
            )

    parser.add_argument(
            "--lp-public-ip",
            help="Use provided vm public/floating ip as launchpad VM's public ip. "
                 "Pass in as <vm_public_ip>",
            )


    args = parser.parse_args(argv)

    # Disable loading any kernel modules for the mission control VM
    os.environ["NO_KERNEL_MODS"] = "1"

    if args.lp_node_id:
        construct_lp_node_env_var(args.lp_node_id)

    if args.lp_public_ip:
        construct_lp_public_ip_env_var(args.lp_public_ip)

    if not args.mock:
        # Ensure that salt master is running.
        check_salt_master_running()

        # Clear salt keys to clear out any old/duplicate keys
        #clear_salt_keys()

        # Initialize the virsh ahead of time to ensure container NAT
        # is functional.  This really should go into cloudsim container
        # initialization.
        if not args.no_cntr_mgr:
            rift.rwcal.cloudsim.net.virsh_initialize_default()

    # load demo info and create Demo object
    demo = Demo(use_mock=args.mock, skip_ui=args.skip_ui, disable_cnt_mgr=args.no_cntr_mgr)

    # Create the prepared system from the demo
    system = rift.vcs.demo.prepared_system_from_demo_and_args(demo, args, 
                  northbound_listing="cli_rwmc_schema_listing.txt",
                  netconf_trace_override=True)

    confd_ip = socket.gethostbyname(socket.gethostname())
    rift.vcs.logger.configure_sink(config_file=None, confd_ip=confd_ip)

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
