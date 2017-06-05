#!/usr/bin/env python3
"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file prepare.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 2016-03-03
@brief Prepare an instance to run systemtests by creating the required images, VMs and networks
"""
import argparse
import copy
import functools
import hashlib
import logging
import logging.handlers
import json
import os
import pwd
import re
import signal
import socket
import subprocess
import sys
import time
import uuid

import rwlogger
import rw_peas

import rift.auto.mano

import gi
gi.require_version('RwCal', '1.0')

from gi.repository import (
    RwcalYang,
    RwTypes,
)


base_flavor = {'memory': 4096, 'cpus': 1, 'names':[], 'qualified_names':[]}
base_network = {'names':[], 'qualified_names':[]}

class ValidationError(Exception):
    '''Thrown if success of the bootstrap process is not verified'''
    pass

def parse_known_args(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--cloud-host', action='store', help='address of the cloud host')
    parser.add_argument('--docker', default=None, dest='docker', action='store', help='prepare hosts as docker containers on specified docker controller (currently not used for cloud account in tests)')
    parser.add_argument('-i', '--image', action='store', help='path to boot image')
    parser.add_argument('-n', '--nfs-root', action='store', help='location of RIFT_INSTALL in nfs mounted workspace')
    parser.add_argument('-u', '--user', action='store', help='user to log into controller as')
    parser.add_argument('-t', '--tenant', action='store', help='tenant to run tests on')
    parser.add_argument('-v', '--verbose')
    parser.add_argument('-r', '--racfg', action='append', help='racfg to consider when preparing resources')
    parser.add_argument('--rpm', action='store_true', help='Install rpms from workspace and run system from installed rpms')
    parser.add_argument('--rpms-from', action='store', help='Use rpms from specified workspace rather than current')
    parser.add_argument('-s', '--sut-file', action='store', help='sut file to write to')
    
    location_group = parser.add_mutually_exclusive_group(required=False)
    location_group.add_argument(
        '--local',
        dest='prepare_location',
        action='store_const',
        const='local',
        help=('''
Spawn initial test resources locally. This currently implies docker is used when preparing launchpad.
If no docker controller address is specified the current environment is assumed to be the location where the launchpad should be run.'''
        ),
    )
    location_group.add_argument(
        '--remote',
        dest='prepare_location',
        action='store_const',
        const='remote',
        help="Spawn initial test resources remotely. This currently implies openstack is used when preparing launchpad.",
    )
    parser.set_defaults(prepare_location='local')

    return parser.parse_known_args(argv)


def choose_suitable_flavor(flavors, target):
    chosen = None
    for flavor in flavors:
        skipped = False
        for k,v in target.items():
            if k == 'name':
                continue
            if k not in ['memory', 'cpus', 'storage'] and k in flavor and flavor[k] != v:
                skipped = True
                break
        if not skipped:
            chosen = flavor
            break
    return chosen
    
def upgrade_flavor(flavor, target):
    for k,v in target.items():
        if k not in flavor:
            flavor[k] = v
            continue
    if 'memory' in target and flavor['memory'] < target['memory']:
        flavor['memory'] = target['memory']
    if 'cpus' in target and flavor['cpus'] < target['cpus']:
        flavor['cpus'] = target['cpus']
    if 'storage' in target and flavor['storage'] < target['storage']:
        flavor['storage'] = target['storage']
    return flavor

def generate_flavors(required_flavors, cfg):
    if 'vms' not in cfg:
        return required_flavors

    flavors = cfg['vms']

    while len(flavors) > len(required_flavors):
        required_flavors.append(copy.deepcopy(base_flavor))
    unassigned_flavors = required_flavors.copy()

    while flavors:
        target_flavor = flavors.pop()
        chosen_flavor = choose_suitable_flavor(unassigned_flavors, target_flavor)

        if chosen_flavor:
            unassigned_flavors.remove(chosen_flavor)
        else:
            chosen_flavor = copy.deepcopy(base_flavor)
            required_flavors.append(chosen_flavor)

        required_flavors.remove(chosen_flavor)
        chosen_flavor = upgrade_flavor(chosen_flavor, target_flavor)

        if 'name' in target_flavor.keys():
            if target_flavor['name'] not in chosen_flavor['names']:
                chosen_flavor['names'].append(target_flavor['name'])
            chosen_flavor['qualified_names'].append("%s.%s" % (cfg['test_name'], target_flavor['name']))

        required_flavors.insert(0, chosen_flavor)
    return required_flavors

def choose_suitable_network(networks, target):
    # No smarts here yet, this basically just chooses a network if it is almost a perfect match
    # to what is needed...
    chosen = None
    for network in networks:
        skipped = False
        for k,v in target.items():
            if k not in ['name'] and k in network and network[k] != v:
                skipped = True
                break
        if not skipped:
            chosen = network
            break
    return chosen

def upgrade_network(network, target):
    for k,v in target.items():
        if k not in network:
            network[k] = v
            continue
    return network

def _generate_subnet():
    for base in range(10,254):
        yield "%d.0.0.0/24" % (base)
    raise StopIteration()
generate_subnet = _generate_subnet()

def generate_networks(required_networks, cfg):
    networks = cfg.get('networks')
    if networks is None:
        return required_networks

    while len(networks) > len(required_networks):
        required_networks.append(copy.deepcopy(base_network))
    unassigned_networks = required_networks.copy()

    while networks:
        target_network = networks.pop()
        if type(target_network) == type(""):
            target_network = {'name':target_network}
        if not 'subnet' in target_network:
            target_network['subnet'] = next(generate_subnet)

        chosen_network = choose_suitable_network(unassigned_networks, target_network)
        if chosen_network:
            unassigned_networks.remove(chosen_network)
        else:
            chosen_network = copy.deepcopy(base_network)
            required_networks.append(chosen_network)

        required_networks.remove(chosen_network)
        chosen_network = upgrade_network(chosen_network, target_network)

        if 'name' in target_network.keys():
            if target_network['name'] not in chosen_network['names']:
                chosen_network['names'].append(target_network['name'])
            chosen_network['qualified_names'].append("%s.%s" % (cfg['test_name'], target_network['name']))

        required_networks.insert(0, chosen_network)
    return required_networks

def get_cal_interface():
    plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
    engine, info, extension = plugin()
    cal = plugin.get_interface("Cloud")
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
    rc = cal.init(rwloggerctx)
    assert rc == RwTypes.RwStatus.SUCCESS
    return cal

def wait_for_image_state(account, image_id, state, timeout=300):
    start_time = time.time()
    state = state.lower()
    current_state = 'unknown'
    while current_state != state:
        rc, image_info = cal.get_image(account, image_id)
        current_state = image_info.state.lower()
        if current_state in ['failed']:
           raise ValidationError('Image [{}] entered failed state while waiting for state [{}]'.format(image_id, state))
        time_elapsed = time.time() - start_time
        time_remaining = timeout - time_elapsed
        if time_remaining <= 0:
            logger.error('Image still in state [{}] after [{}] seconds'.format(current_state, timeout))
            raise TimeoutError('Image [{}] failed to reach state [{}] within timeout [{}]'.format(image_id, state, timeout))
        if current_state != state:
            time.sleep(10)

    return image_info

def wait_for_vm_state(account, vm_id, state, timeout=300):
    start_time = time.time()
    state = state.lower()
    current_state = 'unknown'
    while current_state != state:
        rc, vm_info = cal.get_vdu(account, vm_id)
        assert rc == RwTypes.RwStatus.SUCCESS
        current_state = vm_info.state.lower()
        if current_state in ['failed']:
           raise ValidationError('VM [{}] entered failed state while waiting for state [{}]'.format(vm_id, state))
        time_elapsed = time.time() - start_time
        time_remaining = timeout - time_elapsed
        if time_remaining <= 0:
            logger.error('VM still in state [{}] after [{}] seconds'.format(current_state, timeout))
            raise TimeoutError('VM [{}] failed to reach state [{}] within timeout [{}]'.format(vm_id, state, timeout))
        if current_state != state:
            time.sleep(10)

    return vm_info

if __name__ == '__main__':
    logging.basicConfig(
            format='%(asctime)-15s|%(module)s|%(levelname)s|%(message)s',
            level=logging.INFO,
    )

    (args, unparsed_args) = parse_known_args()
    logging_level = logging.DEBUG if args.verbose else logging.INFO
    logger = logging.getLogger()
    logger.setLevel(logging_level)

    rsyslog_host = os.getenv('RW_AUTO_RSYSLOG_HOST')
    rsyslog_port = os.getenv('RW_AUTO_RSYSLOG_PORT')
    if rsyslog_host and rsyslog_port:
        logger.addHandler(
                logging.handlers.SysLogHandler(address=(rsyslog_host, int(rsyslog_port)))
        )

    if not args.cloud_host:
        logger.error('missing required argument cloud-host')
        sys.exit(1)

    user = args.user or args.tenant or os.getenv('AUTO_USER') or os.getenv('USER', None)
    if not user:
        logger.error('Failed to determine user')
        sys.exit(1)

    tenant = args.tenant or user
    if not tenant:
        logger.error('Failed to determine tenant')
        sys.exit(1)

    os.environ['AUTO_USER'] = user

    suts = {
        "nets":{},
        "vms":{},
        "vm_addrs":{},
    }
    image_ids = []
    flavor_ids = []
    network_ids = []
    vm_ids = []



    def handle_term_signal(_signo, _stack_frame):
        sys.exit(2)

    signal.signal(signal.SIGINT, handle_term_signal)
    signal.signal(signal.SIGTERM, handle_term_signal)


    racfgs = args.racfg
    if not racfgs:
        systest_dirs = [
            '%s/usr/rift/systemtest' % (os.environ['RIFT_INSTALL']),
            '%s/modules/automation/systemtest' % (os.environ['RIFT_ROOT']),
        ]
        for systest_dir in systest_dirs:
            result = subprocess.check_output('find %s -name "*.racfg"' % (systest_dir), shell=True)
        racfgs = [racfg for racfg in result.decode('ascii').split('\n') if racfg != '']


    if args.prepare_location == 'local':
        for racfg in racfgs:
            with open(racfg) as fd:
                contents = fd.read()
                cfg = json.loads(contents)

                target_vm = None
                if "target" in cfg:
                    target_vm = cfg["target"]
                else:
                    for vm in cfg["vms"]:
                        target_vm = vm["name"]
                        if target_vm == "rift_auto_launchpad":
                            break

                vm_id = 0
                for vm in cfg["vms"]:
                    if args.docker:
                        docker_host_cmd = (
                            'ssh {docker_host} -o StrictHostkeyChecking=no -o ConnectionAttempts=60 -- '
                        ).format(
                            docker_host=args.docker,
                        )
                        container_name = (
                            'auto--{tenant}--{vm_id}--{task_id}'
                        ).format(
                            tenant=tenant,
                            vm_id=vm_id,
                            task_id=os.getenv('AUTO_TASK_ID', '0'),
                        )
                        container_name = container_name.replace('_','-')
                        platform_cmd = (
                            "{rift_install}/bin/host-platform"
                        ).format(
                            rift_install=os.environ['RIFT_INSTALL']
                        )
                        platform = subprocess.check_output(platform_cmd, shell=True).decode('ascii').strip()

                        container_image = '{platform}-ui'.format(platform=platform)
                        if platform == 'ub16':
                            container_image = 'ub16:master-fixed'
                        elif platform == 'fc20':
                            container_image = 'fc20:master-fixed'

                        create_cmd = '{docker_host_cmd} /usr/rift/bin/rbc container create {container_image} {container_name}'.format(
                            docker_host_cmd=docker_host_cmd,
                            container_image=container_image,
                            container_name=container_name,
                        )

                        container_created=False
                        attempts=3
                        for attempt in range(0,attempts):
                            try:
                                result = subprocess.check_output(create_cmd, shell=True)
                                container_created=True
                                break
                            except Exception as e:
                                logger.error("Exception encountered while creating container", exc_info=True)
                            time.sleep(5)
                        if not container_created:
                            raise ValidationError("Failed to create container")


                        vm_id += 1
                        match = re.search('Lease of (\S*) obtained', result.decode('ascii'))
                        if not match:
                            raise ValidationError('Failed to obtain launchpad container address')
                        time.sleep(10)
                        container_address = match.group(1)
                    else:
                        container_address = '127.0.0.1'

                    cmd_cleanup = 'ssh-keygen -R {container_address}'
                    logger.info("Cleaning up known host entry for %s", container_address)
                    rc = subprocess.call(cmd_cleanup.format(container_address=container_address), shell=True, stdout=subprocess.DEVNULL)
                    if rc != 0:
                        logger.error("Failed to remove known host entry for %s", container_address)

                    container_cmd = 'ssh {container_address} -o StrictHostkeyChecking=no -o ConnectionAttempts=60 -- '.format(container_address=container_address)
                    if args.rpm:
                        rpm_location = args.rpms_from or os.environ['RIFT_BUILD']
                        install_cmd = (
                            '{container_cmd} sudo rpm -Uvh --nodeps --force '
                            '$(find {rpm_location} -name "*.rpm" | grep -v CPack | grep -v foss)'
                        ).format(
                            container_cmd=container_cmd,
                            rpm_location=rpm_location,
                        )
                        subprocess.check_call(install_cmd, shell=True)

                        make_artifact_dir_cmd = (
                            '{container_cmd} sudo mkdir -p /usr/rift/.artifacts/{required_dirs}'
                        ).format(
                            container_cmd=container_cmd,
                            required_dirs='{moduletest,systemtest}'
                        )

                        subprocess.check_call(make_artifact_dir_cmd, shell=True)
                        pwuid = pwd.getpwuid( os.geteuid() )
                        fix_permissions_cmd = (
                            '{container_cmd} sudo chown -R {uid}:{gid} /usr/rift'
                        ).format(
                            container_cmd=container_cmd,
                            uid=pwuid.pw_uid,
                            gid=pwuid.pw_gid,
                        )
                        subprocess.check_call(fix_permissions_cmd, shell=True)

                    if rsyslog_host and rsyslog_port:
                        rsyslog_cmd = (
                            '{container_cmd} sudo \'bash -c "cat >/etc/rsyslog.d/20-rift.conf ' 
                            '<<RSYSLOG_CONFIG\n'
                                '\\\\\\$ActionForwardDefaultTemplate RSYSLOG_ForwardFormat\n'
                                '*.* @{host}:{port}\n'
                            'RSYSLOG_CONFIG"\''
                        ).format(
                            container_cmd=container_cmd,
                            host=rsyslog_host,
                            port=rsyslog_port,
                        )
                        subprocess.check_call(rsyslog_cmd, shell=True)
                        restart_cmd = (
                            '{container_cmd} sudo service rsyslog restart'
                        ).format(
                            container_cmd=container_cmd,
                        )
                        subprocess.check_call(restart_cmd, shell=True)

                    prepare_cmd = (
                        '{container_cmd} sudo {rift_install}/container_tools/mkvmimg --container --modes ext --modes ui --modes systemtest --modes build'
                    ).format(
                        container_cmd=container_cmd,
                        rift_install=os.environ['RIFT_INSTALL'],
                    )
                    subprocess.check_call(prepare_cmd, shell=True)

                    if args.docker:
                        qualified_name = '{test_name}.{vm_name}'.format(test_name=cfg['test_name'], vm_name=vm["name"])
                        suts['vms'][vm["name"]] = "docker_container"
                        suts['vm_addrs'][qualified_name] = container_address
                    else:
                        qualified_name = '{test_name}.{target_vm}'.format(test_name=cfg['test_name'], target_vm=target_vm)
                        suts['vms'][target_vm] = "localhost"
                        suts['vm_addrs'][qualified_name] = '127.0.0.1'
                        break

        json_suts = json.dumps(suts, sort_keys=False, indent=4, separators=(',', ': '))
        logger.info(json_suts)

        if args.sut_file:
            logger.info("Generated sutfile: %s", args.sut_file)
            sut_file = open(args.sut_file, 'w+b')
            sut_file.write(bytes(json_suts, 'ascii'))
            sut_file.close()

        sys.exit(0)



    account = RwcalYang.CloudAccount.from_dict({
            'name': 'rift-auto-openstack',
            'account_type': 'openstack',
            'openstack': {
              'key':  user, 
              'secret': "mypasswd",
              'auth_url': 'http://{cloud_host}:5000/v3/'.format(cloud_host=args.cloud_host),
              'tenant': tenant,
              'mgmt_network': os.getenv('MGMT_NETWORK', 'private')}})

    cal = get_cal_interface()


    default_network = {
    }

    default_flavor = {
        'memory':8192,
        'cpus':4,
        'storage':40,
    }

    required_flavors = []
    required_networks = []
    for racfg in racfgs:
        with open(racfg) as fd:
            contents = fd.read()
            cfg = json.loads(contents)
            required_flavors = generate_flavors(required_flavors, cfg)
            required_networks = generate_networks(required_networks, cfg)

    flavor_by_cal_flavor = {}
    cal_flavors = []
    for required_flavor in required_flavors:
        flavor = default_flavor.copy()
        flavor.update(required_flavor)
        flavor_id = uuid.uuid4().hex[:10]
        cal_flavor = RwcalYang.FlavorInfoItem.from_dict({
                'name':rift.auto.mano.resource_name('flavor-{}'.format(flavor_id)),
                'vm_flavor': {
                    'memory_mb':flavor['memory'],
                    'vcpu_count':flavor['cpus'],
                    'storage_gb':flavor['storage'],
                }
            })
        cal_flavors.append(cal_flavor)
        flavor_by_cal_flavor[cal_flavor] = required_flavor

    private_network_id = None
    rc, network_list = cal.get_network_list(account)
    for network_info in network_list.networkinfo_list:
        if network_info.network_name in ['private']:
            private_network_id = network_info.network_id

    network_by_cal_network = {}
    cal_networks = []
    for required_network in required_networks:
        network = default_network.copy()
        network.update(required_network)

        cal_network = RwcalYang.NetworkInfoItem.from_dict({
            'network_name':rift.auto.mano.resource_name(network['name']),
            'subnet':network['subnet'],
        })


        provider_network = network.get('provider_network')
        if provider_network:
            for network_attr in ['physical_network', 'overlay_type', 'segmentation_id']:
                value = provider_network.get(network_attr)
                if value is not None:
                    setattr(cal_network.provider_network, network_attr, value)

        cal_networks.append(cal_network)
        network_by_cal_network[cal_network] = required_network

    flavor_by_openstack_id = {}
    for cal_flavor in cal_flavors:
        logger.info("Creating VM Flavor %s", str(cal_flavor.name))
        rc, flavor_id = cal.create_flavor(account, cal_flavor)
        assert rc == RwTypes.RwStatus.SUCCESS
        flavor_ids.append(flavor_id)
        flavor_by_openstack_id[flavor_id] = flavor_by_cal_flavor[cal_flavor]


    network_by_openstack_id = {}
    for cal_network in cal_networks:
        logger.info("Creating Network %s", str(cal_network.network_name))
        rc, network_id = cal.create_network(account, cal_network)
        assert rc == RwTypes.RwStatus.SUCCESS
        network_ids.append(network_id)
        network_by_openstack_id[network_id] = network_by_cal_network[cal_network]


    def image_path_to_name(image_path):
        (name, rest) = os.path.basename(image_path).split('.', 1)
        return name

    images_to_create = []
    default_image_path = args.image or '%s/images/rift-ui-lab-latest.qcow2' % (os.environ['RIFT_ROOT'])

    default_image = RwcalYang.ImageInfoItem.from_dict({
        'name':rift.auto.mano.resource_name('image-%s' % (image_path_to_name(default_image_path))),
        'location':default_image_path,
        'disk_format':'qcow2',
        'container_format':'bare'
    })
    images_to_create.append(default_image)

    for flavor in required_flavors:
        if 'image' in flavor:
            flavor_image = RwcalYang.ImageInfoItem.from_dict({
                'name':rift.auto.mano.resource_name('image-%s' % (image_path_to_name(flavor['image']))),
                'location':flavor['image'],
                'disk_format':'qcow2',
                'container_format':'bare'
            })
            images_to_create.append(flavor_image)


    def create_image(cal, account, image, use_existing=True):

        def md5sum(path):
            with open(path, mode='rb') as fd:
                md5 = hashlib.md5()
                for buf in iter(functools.partial(fd.read, 4096), b''):
                    md5.update(buf)
            return md5.hexdigest()

        existing = {}
        if use_existing:
            if not image.checksum:
                image.checksum = md5sum(image.location)
                
            rc, imagelist = cal.get_image_list(account)
            existing_images = {image.checksum:image.id for image in imagelist.imageinfo_list}

            if image.checksum and image.checksum in existing_images:
                logger.info("Using existing image with matching checksum [%s]", image.checksum)
                return existing_images[image.checksum]

        rc, image_id = cal.create_image(account, image)
        assert rc == RwTypes.RwStatus.SUCCESS        
        return image_id
        

    for image in images_to_create:
        logger.info("Creating VM Image %s", str(image.name))
        image_id = create_image(cal, account, image)
        image_ids.append(image_id)
        image_info = wait_for_image_state(account, image_id, 'active')

    for image_id in image_ids:
        image_info = wait_for_image_state(account, image_id, 'active')


    site = ""
    if os.getenv("SITE") == "blr":
        site = "-s blr"

    userdata_run = []
    if not args.rpm:
        userdata_run.append('/usr/rift/scripts/cloud/enable_lab {site}'.format(site=site))

    userdata_run.extend([
        '{rift_install}/bin/UpdateHostsFile'.format(rift_install=os.environ['RIFT_INSTALL']),
        'yum install wget --assumeyes',
    ])

    if args.rpm:
        args.nfs_root = '/home/rift'

    if args.nfs_root:
        link_dir = os.path.dirname(os.environ['RIFT_ROOT'])
        link_root_cmd = (
                "mkdir -p {link_dir} && ln -s {nfs_root} -T {rift_root}"
                ).format(
                link_dir=link_dir,
                nfs_root=args.nfs_root,
                rift_root=os.environ['RIFT_ROOT'],
        )
        userdata_run.append(link_root_cmd)

    if args.rpm:
        test_host_addr = socket.gethostbyname(socket.gethostname())
        script_dir = '{rift_root}/scripts'.format(rift_root=os.environ['RIFT_ROOT'])
        rpm_dir = '{rift_root}/.install/rpmbuild/RPMS/x86_64'.format(rift_root=os.environ['RIFT_ROOT'])

        connect_test_host = 'ssh -o StrictHostkeyChecking=no -o ConnectionAttempts=120 {test_host_addr} -- ls'.format(test_host_addr=test_host_addr)
        create_dests  = 'mkdir -p {script_dir} {rpm_dir}'.format(script_dir=script_dir, rpm_dir=rpm_dir)
        copy_scripts = 'scp -r -o StrictHostkeyChecking=no {test_host_addr}:{script_dir}/* {script_dir}'.format(test_host_addr=test_host_addr, script_dir=script_dir)
        copy_rpms    = 'scp -r -o StrictHostkeyChecking=no {test_host_addr}:{rpm_dir}/* {rpm_dir}'.format(test_host_addr=test_host_addr, rpm_dir=rpm_dir)
        install_rpms = 'rpm -ivh --force {rpm_dir}/*.rpm'.format(rpm_dir=rpm_dir)

        userdata_run.extend([
            connect_test_host,
            create_dests,
            copy_scripts,
            copy_rpms,
            install_rpms,
        ])

    userdata_run.append((
            '{rift_install}/rift-shell -e -r -- '
            'container_tools/mkvmimg --container --modes ext --modes ui --modes systemtest --modes build'
        ).format(
            rift_install=os.environ['RIFT_INSTALL']
        )
    )

    userdata_run.append((
        '{rift_install}/rift-shell -e -r -- bin/enable_lab {site}'
        ).format(
            rift_install=os.environ['RIFT_INSTALL'],
            site=site
        )
    )

    if args.rpm:
        pwuid = pwd.getpwuid( os.geteuid() )
        userdata_run.append((
            'chown -R {uid}:{gid} '
                '{rift_install} '
                '{rift_artifacts} '
            ).format(
                uid=pwuid.pw_uid,
                gid=pwuid.pw_gid,
                rift_install=os.environ['RIFT_INSTALL'],
                rift_artifacts=os.environ['RIFT_ARTIFACTS'],
            )
        )

    userdata = "#cloud-config"

    if rsyslog_host and rsyslog_port:
        userdata += '''
    rsyslog:
      - "$ActionForwardDefaultTemplate RSYSLOG_ForwardFormat"
      - "*.* @{host}:{port}"'''.format(
            host=rsyslog_host,
            port=rsyslog_port,
        )

    flavor_by_vm_id = {}
    for flavor_id in flavor_ids:
        flavor = flavor_by_openstack_id[flavor_id]

        if 'name' in flavor:
            name = flavor['name']
        else:
            name = 'auto-vm'

        vm_info = RwcalYang.VMInfoItem.from_dict({
            'vm_name':rift.auto.mano.resource_name(name),
            'vm_id':str(uuid.uuid4().hex[:10]),
            'flavor_id':str(flavor_id),
            'image_id':image_id,
            'allocate_public_address':True,
            'cloud_init': {
                'userdata':''
            }
        })
        
        interface_cfg_base = '''
      - content: |
            DEVICE="eth{instance_id}"
            BOOTPROTO="dhcp"
            ONBOOT="yes"
            TYPE="Ethernet"
            PEERDNS="no"
        path: "/etc/sysconfig/network-scripts/ifcfg-eth{instance_id}"
        permissions: 0420'''
        network_userdata = ''
        private_network = vm_info.network_list.add()
        private_network.network_id = private_network_id
        instance_id = 1
        # Network data structure needs work, mapping requires some hoop jumping
        for network in required_networks:
            if network['name'] in flavor:
                for network_id in network_ids:
                    potential_network = network_by_openstack_id[network_id]
                    if potential_network['name'] != network['name']:
                        continue
                    vm_network = vm_info.network_list.add()
                    vm_network.network_id = network_id
                    logger.info('added network with id %s', network_id)
                    network_userdata += interface_cfg_base.format(instance_id=instance_id)
                    instance_id += 1

        if network_userdata:
            vm_info.cloud_init.userdata = '''
{userdata}

    write_files:{network_userdata}
            '''.format(
                  userdata=userdata,
                  network_userdata=network_userdata,
            )
        else:
            vm_info.cloud_init.userdata = userdata
 
        if userdata_run:
            vm_info.cloud_init.userdata += '''
    runcmd:'''

            for command in userdata_run:
                vm_info.cloud_init.userdata += '''
      - {command}'''.format(command=command)

            if network_userdata:
                vm_info.cloud_init.userdata += '''
      - service network restart'''


        logger.info('userdata: %s', vm_info.cloud_init.userdata)
        vm_created = False
        attempts = 0
        max_attempts = 3
        while not vm_created and attempts < max_attempts:
            attempts += 1
            try:
                logger.info("Creating VM %s", vm_info.vm_name)
                rc, vm_id = cal.create_vm(account, vm_info)
                logger.info("RC: %s", rc)
                assert rc == RwTypes.RwStatus.SUCCESS
                wait_for_vm_state(account, vm_id, 'active')
                print("VM Created successfully.")
                vm_created = True
            except:
                logger.warn("VM Failed to reach active state")
                cal.delete_vdu(account, vm_id)

        if attempts >= max_attempts:
            raise ValidationError('VM [{}] failed to reach active state within [{}] attempts.'.format(vm_info.vm_name, max_attempts))

        vm_ids.append(vm_id)
        flavor_by_vm_id[vm_id] = flavor


    for vm_id in vm_ids:
        wait_for_vm_state(account, vm_id, 'active')

    for vm_id in vm_ids:
        for network_id in network_ids:
            if network_id not in network_by_openstack_id:
                continue
            network_def = network_by_openstack_id[network_id]
            if 'ports' not in network_def:
                continue

            for idx in range(1, int(network_def['ports'])):
                port = RwcalYang.PortInfoItem.from_dict({
                    'port_name':rift.auto.mano.resource_name('port_{}'.format(idx)),
                    'network_id':network_id,
                    'vm_id':vm_id, 
                })
                rc, port_id = cal.create_port(account, port)



    elapsed = 0
    timeout = 300
    start = time.time()

    logger.info("Waiting for Resources to have a public ip address assigned")
    while elapsed < timeout:
        elapsed = time.time() - start

        rc, vdu_list = cal.get_vdu_list(account)
        assert rc == RwTypes.RwStatus.SUCCESS

        vms_pending_public_ip = [vdu_info for vdu_info in vdu_list.vdu_info_list if vdu_info.vdu_id in vm_ids and vdu_info.public_ip == ""]
        if not vms_pending_public_ip:
            break

        logger.info("Resource %s has no public ip address", vms_pending_public_ip[0].name)
        time.sleep(5)

    if vms_pending_public_ip:
        raise ValidationError("Failed to verify all resources have public ip address")



    vdus = [vdu_info for vdu_info in vdu_list.vdu_info_list if vdu_info.vdu_id in vm_ids]

    cmd_cleanup = 'ssh-keygen -R {mgmt_ip}'
    for vdu in vdus:
        logger.info("Cleaning up known host entry for %s", vdu.public_ip)
        rc = subprocess.call(cmd_cleanup.format(mgmt_ip=vdu.public_ip), shell=True, stdout=subprocess.DEVNULL)
        if rc != 0:
            logger.error("Failed to remove known host entry for %s", vdu.name)


    cmd_accessible  = '/usr/rift/bin/ssh_root {mgmt_ip} -q -n -o UserKnownHostsFile=/dev/null -o BatchMode=yes -o StrictHostKeyChecking=no -- ls'
    cmd_initialized = '/usr/rift/bin/ssh_root {mgmt_ip} -q -n -o BatchMode=yes -o StrictHostKeyChecking=no -- stat /var/lib/cloud/instance/boot-finished > /dev/null'
    cmd_lab_enabled = 'ssh {mgmt_ip} -q -n -o BatchMode=yes -o StrictHostKeyChecking=no -- ls'
    cmd_log_cloud_init = '/usr/rift/bin/ssh_root {mgmt_ip} -q -n -o BatchMode=yes -o StrictHostKeyChecking=no -- cat /var/log/cloud-init.log'

    accessible  = {vdu.vdu_id:False for vdu in vdus}
    initialized = {vdu.vdu_id:False for vdu in vdus}
    lab_enabled = {vdu.vdu_id:False for vdu in vdus}

    elapsed = 0
    timeout = 1200
    start = time.time()

    while elapsed < timeout:
        elapsed = time.time() - start
        all_resources_ready = True

        for vdu in vdus:
            if accessible[vdu.vdu_id]:
                continue
            rc = subprocess.call(cmd_accessible.format(mgmt_ip=vdu.public_ip), shell=True, stdout=subprocess.DEVNULL)
            if rc != 0:
                logger.info("Resource %s not accessible", vdu.name)
                all_resources_ready = False
                time.sleep(10)
                break
            logger.info("Resource %s accessible", vdu.name)
            accessible[vdu.vdu_id] = True

        if not all_resources_ready:
            continue

        for vdu in vdus:
            if initialized[vdu.vdu_id]:
                continue
            rc = subprocess.call(cmd_initialized.format(mgmt_ip=vdu.public_ip), shell=True, stdout=subprocess.DEVNULL)
            if rc != 0:
                logger.info("Resource %s not initialized", vdu.name)
                all_resources_ready = False
                time.sleep(10)
                break
            logger.info("Resource %s initialized", vdu.name)
            initialized[vdu.vdu_id] = True

        if not all_resources_ready:
            continue

        for vdu in vdus:
            if lab_enabled[vdu.vdu_id]:
                continue
            rc = subprocess.call(cmd_lab_enabled.format(mgmt_ip=vdu.public_ip), shell=True, stdout=subprocess.DEVNULL)
            if rc != 0:
                logger.info("Resource %s not lab enabled", vdu.name)
                all_resources_ready = False
                time.sleep(10)
                break
            logger.info("Resource %s lab enabled", vdu.name)
            lab_enabled[vdu.vdu_id] = True

        if not all_resources_ready:
            continue

        for vdu in vdus:
            subprocess.call(cmd_log_cloud_init.format(mgmt_ip=vdu.public_ip), shell=True)

        # all_resources_ready
        break


    if not all_resources_ready:
        raise ValidationError("Failed to verify all resources started")


    default_vm = 'VM'
    default_vm_id = 1
    for vdu in vdus:
        flavor = flavor_by_vm_id[vdu.vdu_id]

        if not flavor['names']:
            suts['vms'][default_vm] = 'dynamic_openstack_host'
            suts['vm_addrs'][default_vm] = vdu.public_ip
            default_vm = 'VM_%d' % (default_vm_id)
            default_vm_id += 1

        for name in flavor['names']:
            suts['vms'][name] = 'dynamic_openstack_host'

        for name in flavor['qualified_names']:
            suts['vm_addrs'][name] = vdu.public_ip

    for cal_network in cal_networks:
        network = network_by_cal_network[cal_network]
        suts['nets'][network['name']] = cal_network.network_name

    json_suts = json.dumps(suts, sort_keys=False, indent=4, separators=(',', ': '))
    logger.info(json_suts)

    if args.sut_file:
        logger.info("Generated sutfile: %s", args.sut_file)
        sut_file = open(args.sut_file, 'w+b')
        sut_file.write(bytes(json_suts, 'ascii'))
        sut_file.close()

