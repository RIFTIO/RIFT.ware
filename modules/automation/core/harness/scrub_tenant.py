#!/usr/bin/env python3
"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file scrub_tenant.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 2016-03-10
@brief Clean up resources found on a particular openstack tenant
"""

import argparse
import logging
import os
import re
import signal
import subprocess
import sys

import rwlogger
import rw_peas

from gi import require_version
require_version('RwCal', '1.0')
require_version('RwcalYang', '1.0')
require_version('RwTypes', '1.0')

from gi.repository import (
    RwcalYang,
    RwTypes,
)

def parse_known_args(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--cloud-host', action='store', help='address of the openstack host')
    parser.add_argument('--docker', default=None, dest='docker', action='store', help='prepare hosts as docker containers on specified docker controller (currently not used for cloud account in tests)')
    parser.add_argument('-u', '--user', action='store', help='user to log into controller as')
    parser.add_argument('-i', '--task-id', action='store', help='id of the task to be cleaned up')
    parser.add_argument('-t', '--tenant', action='store', help='tenant to run tests on')
    parser.add_argument('--confirm', action='store_true', help='Prompt before deleting resources')
    parser.add_argument('-v', '--verbose')
    return parser.parse_known_args(argv)

def get_cal_interface():
    plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
    engine, info, extension = plugin()
    cal = plugin.get_interface("Cloud")
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
    rc = cal.init(rwloggerctx)
    assert rc == RwTypes.RwStatus.SUCCESS
    return cal

if __name__ == '__main__':
    (args, unparsed_args) = parse_known_args()

    logging_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format='%(asctime)-15s %(module)s %(levelname)s %(message)s', level=logging_level)
    logger = logging.getLogger()
    logger.setLevel(logging_level)

    if not args.cloud_host:
        logger.error('missing required argument cloud-host')
        sys.exit(1)

    def handle_term_signal(_signo, _stack_frame):
        sys.exit(2)

    signal.signal(signal.SIGINT, handle_term_signal)
    signal.signal(signal.SIGTERM, handle_term_signal)

    user = args.user or args.tenant or os.getenv('AUTO_USER') or os.getenv('USER', None)
    if not user:
        logger.error('Failed to determine user')
        sys.exit(1)

    tenant = args.tenant or user
    if not tenant:
        logger.error('Failed to determine tenant')
        sys.exit(1)

    os.environ['AUTO_USER'] = user

    task_id = args.task_id or os.getenv('AUTO_TASK_ID') or ''
    if task_id:
        sig = '-'.join([user, task_id])
    else:
        sig = user

    logger.info('Cleaning up resources matching {sig}'.format(sig=sig))


    cal = get_cal_interface()
    account = RwcalYang.CloudAccount.from_dict({
            'name': 'rift-auto-openstack',
            'account_type': 'openstack',
            'openstack': {
              'key':  user,
              'secret': "mypasswd",
              'auth_url': 'http://{cloud_host}:5000/v3/'.format(cloud_host=args.cloud_host),
              'tenant': tenant,
              'mgmt_network': os.getenv('MGMT_NETWORK', 'private')}})

    vm_names = []
    vms_to_delete = set()
    flavors_to_delete = set()
    images_to_delete = set()
    networks_to_delete = set()
    ports_to_delete = set()

    rc, vdu_list = cal.get_vdu_list(account)
    if rc != RwTypes.RwStatus.SUCCESS or not vdu_list:
        vdu_list = None

    if vdu_list:
        for info in vdu_list.vdu_info_list:
            if 'node_id' not in info:
                continue
            if sig not in info.node_id:
                continue
            vm_names.append(info.name)
            vms_to_delete.add(info.vdu_id)
            flavors_to_delete.add(info.flavor_id)
            images_to_delete.add(info.image_id)

    if args.confirm:
        if not vms_to_delete:
            print("Nothing to do, tenant already clean")
            sys.exit(0)

        print("\nPreparing to delete VMs: [%s]\n" % (', '.join(vm_names)))
        response = input('Delete VMs and associated resources? [y/N]: ')
        if response.upper() not in ['Y','YES']:
            sys.exit(0)

    for instance_id in flavors_to_delete:
        if instance_id in ['1','2','3','4','5']:
            continue
        try:
            rc, flavor = cal.get_flavor(account, instance_id)
            if not flavor:
                logger.error('Failed to find flavor with id: %s', instance_id)
                continue
            if sig not in flavor.name:
                continue
            logger.info('Deleting flavor with instance id: %s', instance_id)
            cal.delete_flavor(account, instance_id)
        except Exception as e:
            logger.error('Failed to delete flavor - %s', e, exc_info=True)

    for instance_id in images_to_delete:
        try:
            rc, image = cal.get_image(account, instance_id)
            if not image:
                logger.error('Failed to find image with instance id: %s', instance_id)
                continue
            if sig not in image.name:
                continue
            if image.state == 'ACTIVE':
                logger.warn('Would have deleted image [%s] but it is still ACTIVE', image.id)
                continue
            logger.info('Deleting image with instance id: %s', instance_id)
            cal.delete_image(account, instance_id)
        except Exception as e:
            logger.error("Failed to delete image - %s", e, exc_info=True)

    for instance_id in vms_to_delete:
        try:
            logger.info('Deleting vm with instance id: %s', instance_id)
            cal.delete_vdu(account, instance_id)
        except Exception as e:
            logger.error("Failed to delete vm - %s", e, exc_info=True)

    rc, network_list = cal.get_network_list(account)
    if rc != RwTypes.RwStatus.SUCCESS or not network_list:
        network_list = None

    if network_list:
        for info in network_list.networkinfo_list:
            if sig not in info.network_name:
                continue
            if info.network_name in ['private', 'private2', 'private3', 'private4', 'public']:
                continue
            networks_to_delete.add(info.network_id)


    rc, port_list = cal.get_port_list(account)
    if rc != RwTypes.RwStatus.SUCCESS or not port_list:
        port_list = None

    if port_list:
        for info in port_list.portinfo_list:
            if info.network_id not in networks_to_delete:
                continue
            if info.port_state == 'ACTIVE':
                logger.warn('Would have deleted port [%s] but it is still ACTIVE', info.network_id)
                continue
            ports_to_delete.add(info.port_id)

    drv = cal._use_driver(account)
    unused_floating_ips = [floating_ip for floating_ip in drv.nova_floating_ip_list() if floating_ip.instance_id is None]
    for floating_ip in unused_floating_ips:
        logger.info('Deleting unused floating ip address: %s', floating_ip.ip)
        drv.nova_drv.floating_ip_delete(floating_ip)
    drv = None

    for instance_id in ports_to_delete:
        try:
            logger.info('Deleting port with instance id: %s', instance_id)
            cal.delete_port(account, instance_id)
        except Exception as e:
            logger.error("Failed to delete Port - %s", e, exc_info=True)

    for instance_id in networks_to_delete:
        try:
            logger.info('Deleting network with instance id: %s', instance_id)
            cal.delete_network(account, instance_id)
        except Exception as e:
            logger.error("Failed to delete Network - %s", e, exc_info=True)

    if args.docker:
        sig = "auto-"
        if tenant and task_id:
            sig = 'auto--{tenant}--.*?{task_id}'.format(tenant=tenant, task_id=task_id)
        elif tenant:
            sig = 'auto--{tenant}--'.format(tenant=tenant)
        elif task_id:
            sig = 'auto--.*?{task_id}'.format(task_id=task_id)

        sig = sig.replace('_', '-')

        def rbc_cleanup():
            ssh_cmd = 'ssh {docker} -o StrictHostkeyChecking=no -- '.format(docker=args.docker)
            list_cmd = '{ssh_cmd} /usr/rift/bin/rbc container list'.format(ssh_cmd=ssh_cmd)
            result = subprocess.check_output(list_cmd, shell=True).decode('ascii')
            containers_to_delete = re.findall('^\s*({sig}[\S-]*)\s*\S+'.format(sig=sig), result, re.MULTILINE)
            for container in containers_to_delete:
                delete_cmd = '{ssh_cmd} /usr/rift/bin/rbc container delete {container}'.format(
                    ssh_cmd=ssh_cmd,
                    container=container
                )
                subprocess.check_call(delete_cmd, shell=True)

        attempts = 3
        for attempt in range(0,attempts):
            logger.info("Cleaning up rbc resources - attempt %d/%d", attempt, attempts)
            try:
                rbc_cleanup()
                break
            except Exception as e:
                logger.error("Exception encountered while cleaning up rbc resources", exc_info=True)
            time.sleep(5)
