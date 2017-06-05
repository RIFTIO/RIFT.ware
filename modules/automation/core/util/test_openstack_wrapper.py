#!/usr/bin/env python3

# STANDARD_RIFT_IO_COPYRIGHT
"""
Provides 4 env variables for development convenience:
NOCREATE    : If set, launchpad are not created,
              instead an existing launchpad instance will be used.
SITE        : If set to "blr" , enable lab along with -s "blr" option
MGMT_NETWORK: If set, this will be used as mgmt_network in cloud account details
PROJECT_NAME: If set, this will be used as the tenant name.

"""
import argparse
import json
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

    parser.add_argument(
            '--ha-mode',
            action='store',
            help='The high availability mode to run the system test')

    return parser.parse_known_args(argv)


if __name__ == '__main__':
    (args, unparsed_args) = parse_known_args()
    args.verbose=True

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
            site=os.getenv("SITE"),
            use_existing=use_existing)

    cleanup_clbk = None
    if not use_existing:
        mano.reset_openstack(mano.OpenstackCleanup(args.cloud_host, args.user, tenants))
        cleanup_clbk = mano.reset_openstack

    slave_resources = 0
    if args.ha_mode:
        if args.ha_mode == "LS":
            slave_resources = 1
        elif args.ha_mode == "LSS":
            slave_resources = 2  
        else:
            logger.error("Invalid ha_mode specified: {0}".format(args.ha-mode))
            sys.exit(1)

    print ("slave-resources : {}".format(slave_resources))
    if args.mvv_image is not None:
        vm_image = RwcalYang.ImageInfoItem.from_dict({
            'name': os.path.basename(args.mvv_image),
            'location': args.mvv_image,
            'disk_format': 'qcow2',
            'container_format': 'bare'})
        logger.debug("Creating VM image for Multi-vm vnf")
        openstack.create_image(vm_image)

    with openstack.setup(cleanup_clbk=cleanup_clbk,
                         slave_resources=slave_resources) as (master, slaves):

        master_ip = '127.0.0.1'
        sutfile = os.environ.get('RW_AUTO_SUT_FILE', None)
        if sutfile:
            with open(sutfile, "r") as f:
                suts = json.loads(f.read())
                for resource_name, ip in suts['vm_addrs'].items():
                    if 'rift_auto_launchpad' in resource_name:
                        logger.info("Found launchpad in sutfile")
                        master_ip = ip
        else:
            if master:
                master_ip = master.ip

        rift_root = os.environ.get('HOME_RIFT', os.environ.get('RIFT_ROOT'))

        use_rpm = os.environ.get('AUTO_USE_RPM', None)
        shell_cmd = "{rift_root}/rift-shell -e -r ".format(rift_root=rift_root)
        if use_rpm:
            shell_cmd = (
                "{shell_cmd} -i {install_dir} -b {build_dir} -a {artifacts_dir}"
            ).format(
                shell_cmd=shell_cmd,
                install_dir=rift_root,
                build_dir=os.path.join(rift_root,'.build'),
                artifacts_dir=os.path.join(rift_root,'.artifacts')
            )

        system_cmd = (
            "/usr/rift/bin/ssh_root {master_ip} "
            "-q "
            "-o BatchMode=yes "
            "-o UserKnownHostsFile=/dev/null "
            "-o StrictHostKeyChecking=no "
            "-- "
            "{shell_cmd} -- "
            "{system_script} {system_args}"
        ).format(
            master_ip=master_ip,
            shell_cmd=shell_cmd,
            system_script=args.system_script,
            system_args=args.system_args
        )

        test_cmd_template = ("{test_script} --confd-host {master_ip} "
                "{launchpad_data} {other_args}")

        confd_ips = [master_ip]
        launchpad_data = ""

        test_cmd = test_cmd_template.format(
                test_script=args.test_script,
                master_ip=master_ip,
                launchpad_data=launchpad_data,
                other_args=args.test_args)


        all_vms = [master] + slaves
        system_args = args.system_args

        # Specific to HA test case
        if args.ha_mode:
            # Get the private IP from the VM. The IP
            # provided by the openstack wrapper could be a Floating IP
            # in which case pacmaker would not be able to figure out 
            # the IP address correctly. RIFT-14269
            def get_private_ip(ip, cloud_host):
                cmd = ('ssh_root {vm_ip} -q -o BatchMode=yes -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -- '
                'ip route get {cloud_host} | awk \'{{print $NF; exit}}\'').format(
                    vm_ip = ip,
                    cloud_host = cloud_host,)
                print ("Get IP address cmd: {}".format(cmd))
                priv_ip = subprocess.check_output(cmd, shell=True)
                print ("Got IP {}".format(priv_ip))
                priv_ip = priv_ip[:-1].decode("utf-8")
                return priv_ip

            system_args += " --ha-mode " + args.ha_mode
            system_args += " --mgmt-ip-list " + ','.join(get_private_ip(str(s.ip), args.cloud_host) for s in all_vms)
      
            #TODO: This is very specific to LS mode
            # need to make it generic for LSS mode
            rift_install = os.environ.get('RIFT_INSTALL')
            ha_test_path = rift_install + "/usr/rift/systemtest/pytest/system/"
            test_cmd = \
            "python3 {test_path}/test_start_standby.py --remote-ip {remote_ip}".format(
                test_path=ha_test_path,
                remote_ip=str(all_vms[1].ip)
              ) + " ; " + test_cmd

        system_cmd = ("ssh_root {vm_ip} -q -o BatchMode=yes -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -- "
                "{rift_root}/rift-shell -e -r -- {system_script} {system_args}").format(
                    vm_ip=master_ip,
                    rift_root=rift_root,
                    system_script=args.system_script,
                    system_args=system_args)


        systemtest_cmd = ('{systest_script} {other_args} '
                '--up_cmd "{up_cmd}" '
                '--system_cmd "{system_cmd}" '
                '--test_cmd "{test_cmd}" '
                '--confd_ip {confd_ips} ').format(
                    systest_script=args.systest_script,
                    up_cmd=args.up_cmd.replace('CONFD_HOST', master_ip),
                    system_cmd=system_cmd,
                    test_cmd=test_cmd,
                    confd_ips=",".join(confd_ips),
                    other_args=args.systest_args
                    )

        if args.ha_mode:
            reboot_cmd = ("ssh_root {ip} -q -o BatchMode=yes -o "
                  " UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -- "
                  " reboot").format(
                  ip=str(all_vms[0].ip),)

            rift_install = os.environ.get('RIFT_INSTALL')
            ha_test_path = rift_install + "/usr/rift/systemtest/pytest/system/"


            #TODO: Need to enable this back
            """
            new_test_cmd = test_cmd_template.format(
                test_script = args.test_script,
                master_ip = all_vms[1].ip,
                launchpad_data=launchpad_data,
                other_args=args.test_args)

            post_test_cmd = \
                "python3 {test_path}/test_failover.py --master-ip {master_ip} --standby-ip {standby_ip}".format(
                      test_path=ha_test_path,
                      master_ip=str(all_vms[0].ip),
                      standby_ip=str(all_vms[1].ip)
                    )
            """
            systemtest_cmd += ' --post_reboot_test_cmd "{new_test_cmd}" --reboot_cmd "{reboot_cmd}" '\
                         .format(new_test_cmd="sleep 90",
			                           reboot_cmd=reboot_cmd)

        if args.post_restart_test_script is not None:
            post_restart_test_cmd = test_cmd_template.format(
                    test_script=args.post_restart_test_script,
                    master_ip=master_ip,
                    launchpad_data=launchpad_data,
                    other_args=args.post_restart_test_args,
                    )
            systemtest_cmd += '--post_restart_test_cmd "{}" '.format(post_restart_test_cmd)


        logger.debug('Executing Systemtest with command: %s', systemtest_cmd)

        # Execute on master
        test_execution_rc = subprocess.call(systemtest_cmd, shell=True)

        if args.wait:
            # Wait for signal to cleanup
            signal.pause()

    exit(test_execution_rc)
