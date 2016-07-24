#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
"""
Provides 4 env variables for development convenience:
NOCREATE    : If set, mission_control and launchpad are not created,
              instead existing launchpad & MC instances will be used.
SITE        : If set to "blr" , enable lab along with -s "blr" option
MGMT_NETWORK: If set, this will be used as mgmt_network in cloud account details
PROJECT_NAME: If set, this will be used as the tenant name.

"""
import argparse
import logging
import os
import signal
import subprocess
import sys

from gi import require_version
require_version('RwcalYang', '1.0')
from gi.repository import RwcalYang
import rift.auto.mano as mano


def parse_known_args(argv=sys.argv[1:]):
    """Create a parser and parse system test arguments

    Arguments:
        argv - list of args to parse

    Returns:
        A tuple containg two elements
            A list containg the unparsed portion of argv
            A namespace object populated with parsed args as attributes
    """

    parser = argparse.ArgumentParser()
    parser.add_argument(
            '-c', '--cloud-host',
            help='address of the openstack host')

    parser.add_argument(
            '--dts-cmd',
            help='command to setup a trace of a dts client')

    parser.add_argument(
            '-n', '--num-vm-resources',
            default=0,
            type=int,
            help='This flag indicates the number of additional vm resources to create')

    parser.add_argument(
            '--run-as-root',
            help='run system as root')

    parser.add_argument(
            '--systest-script',
            help='system test wrapper script')

    parser.add_argument(
            '--systest-args',
            help='system test wrapper script')

    parser.add_argument(
            '--system-script',
            help='script to bring up system')

    parser.add_argument(
            '--system-args',
            help='arguments to the system script')

    parser.add_argument(
            '--tenant',
            action='append',
            help='tenant to use in openstack instance')

    parser.add_argument(
            '--test-script',
            help='script to test the system')

    parser.add_argument(
            '--test-args',
            help='arguments to the test script')

    parser.add_argument(
            '--user',
            action='store',
            help='user to log in to openstack instance as',
            default=os.environ.get('USER'))

    parser.add_argument(
            '--post-restart-test-script',
            help='script to test the system, after restart')

    parser.add_argument(
            '--post-restart-test-args',
            help='args for the script to be tested post restart')

    parser.add_argument(
            '--up-cmd',
            help='command to run to wait until system is up')

    parser.add_argument(
            '-v', '--verbose',
            action='store_true',
            help='This flag sets the logging level to DEBUG')

    parser.add_argument(
            '--wait',
            action='store_true',
            help='wait for interrupt after tests complete')

    parser.add_argument(
            '--lp-standalone',
            action='store_true',
            help='Start launchpad in standalone mode.')

    parser.add_argument(
            '--tenants',
            type=mano.Openstack.parse_tenants,
            help="Tenant in the cloud-host in which the test should be run, "
                 "can also be passed as comma separated list for multi-cloud "
                 "account  tests.")

    parser.add_argument(
            '--use-existing',
            action='store_true',
            help='Use existing VMs.')

    parser.add_argument(
            '--mvv-image',
            type=lambda x: os.path.exists(x) and x or parser.error("MVV image file does not exist"),
            help='Path of the VM image file for Multi-VM VNF')

    return parser.parse_known_args(argv)


if __name__ == '__main__':
    (args, unparsed_args) = parse_known_args()
    args.verbose=True

    flavor_id = None
    lp_vdu_id = None
    mc_vdu_id = None
    image_id = None

    vdu_resources = []

    def handle_term_signal(_signo, _stack_frame):
        sys.exit(2)


    signal.signal(signal.SIGINT, handle_term_signal)
    signal.signal(signal.SIGTERM, handle_term_signal)

    test_execution_rc = 1

    logging_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s', level=logging_level)
    logger = logging.getLogger()
    logger.setLevel(logging_level)

    tenants = args.tenants + [(tenant, 'private') for tenant in args.tenant]
    use_existing = args.use_existing or os.getenv("NOCREATE", None) is not None
    openstack = mano.OpenstackManoSetup(
            args.cloud_host, args.user, tenants,
            lp_standalone=args.lp_standalone,
            site=os.getenv("SITE"),
            use_existing=use_existing)

    cleanup_clbk = None
    if not use_existing:
        mano.reset_openstack(mano.OpenstackCleanup(args.cloud_host, args.user, tenants))
        cleanup_clbk = mano.reset_openstack

    if args.mvv_image is not None:
        vm_image = RwcalYang.ImageInfoItem.from_dict({
            'name': os.path.basename(args.mvv_image),
            'location': args.mvv_image,
            'disk_format': 'qcow2',
            'container_format': 'bare'})
        logger.debug("Creating VM image for Multi-vm vnf")
        openstack.create_image(vm_image)

    with openstack.setup(cleanup_clbk=cleanup_clbk) as (master, slaves):
        rift_root = os.environ.get('HOME_RIFT', os.environ.get('RIFT_ROOT'))
        system_cmd = ("/usr/rift/bin/ssh_root {master_ip} -q -o BatchMode=yes -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -- "
                "{rift_root}/rift-shell -e -r -- {system_script} {system_args}").format(
                master_ip=master.ip,
                rift_root=rift_root,
                system_script=args.system_script,
                system_args=args.system_args
        )

        test_cmd_template = ("{test_script} --confd-host {master_ip} "
                "{launchpad_data} {other_args}")

        confd_ips = [master.ip]
        launchpad_data = ""

        if args.lp_standalone:
            launchpad_data = "--lp-standalone"
        else:
            launchpad = slaves.pop(0)
            confd_ips.append(launchpad.ip)
            launchpad_data = "--launchpad-vm-id {}".format(launchpad.id)

        test_cmd = test_cmd_template.format(
                test_script=args.test_script,
                master_ip=master.ip,
                launchpad_data=launchpad_data,
                other_args=args.test_args)


        systemtest_cmd = ('{systest_script} {other_args} '
                '--up_cmd "{up_cmd}" '
                '--system_cmd "{system_cmd}" '
                '--test_cmd "{test_cmd}" '
                '--confd_ip {confd_ips} ').format(
                        systest_script=args.systest_script,
                        up_cmd=args.up_cmd.replace('CONFD_HOST', master.ip),
                        system_cmd=system_cmd,
                        test_cmd=test_cmd,
                        confd_ips=",".join(confd_ips),
                        other_args=args.systest_args
                        )

        if args.post_restart_test_script is not None:
            post_restart_test_cmd = test_cmd_template.format(
                    test_script=args.post_restart_test_script,
                    master_ip=master.ip,
                    launchpad_data=launchpad_data,
                    other_args=args.post_restart_test_args,
                    )
            systemtest_cmd += '--post_restart_test_cmd "{}" '.format(post_restart_test_cmd)


        logger.debug('Executing Systemtest with command: %s', systemtest_cmd)
        test_execution_rc = subprocess.call(systemtest_cmd, shell=True)

        if args.wait:
            # Wait for signal to cleanup
            signal.pause()

    exit(test_execution_rc)
