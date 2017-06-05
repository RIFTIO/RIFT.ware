"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file mano.py
@author Varun Prasad (varun.prasad@riftio.com)
@date 19/01/2016

"""

import collections
import contextlib
import logging
import os
import subprocess
import time
import uuid

from gi import require_version
require_version('RwCal', '1.0')
require_version('RwcalYang', '1.0')
require_version('RwTypes', '1.0')
require_version('RwNsrYang', '1.0')

from gi.repository import RwcalYang, RwTypes, RwNsrYang
import rw_peas
import rwlogger

import rift.auto.vm_utils

logger = logging.getLogger()

def resource_name(base, user=None):
    name = '{user}-{task_id}-{base}'.format(
            user=user or os.getenv('AUTO_USER', os.getenv('USER', 'nouser')),
            task_id=os.getenv('AUTO_TASK_ID', '0'),
            base=base
    )
    return name

class TimeoutError(Exception):
    '''Thrown if object does not reach the desired state within the timeout'''
    pass


class ValidationError(Exception):
    '''Thrown if success of the bootstrap process is not verified'''
    pass


class Openstack(object):
    _DEFAULT_USERNAME = "pluto"
    _DEFAULT_PASSWORD = "mypasswd"

    def __init__(self, host, user, tenants):
        self.host = host
        self.tenants = tenants
        self.user = user
        self.accounts = self._generate_accounts()
        self.cal = self._get_cal_interface()

    def _generate_accounts(self):
        """
        Generates the list of cal accounts for which setup and cleanup
        has to be performed.
        """
        accounts = []
        auth_url = 'http://{}:5000/v3/'.format(self.host)

        for tenant, network in self.tenants:
            account = RwcalYang.CloudAccount.from_dict({
                    'name': 'rift.auto.openstack',
                    'account_type': 'openstack',
                    'openstack': {
                      'key':  self.user or self._DEFAULT_USERNAME,
                      'secret': self._DEFAULT_PASSWORD,
                      'auth_url': auth_url,
                      'tenant': tenant,
                      'mgmt_network': network}})

            accounts.append(account)

        return accounts

    def _get_cal_interface(self):
        """Get an instance of the rw.cal interface

        Load an instance of the rw.cal plugin via libpeas
        and returns the interface to that plugin instance

        Returns:
            rw.cal interface to created rw.cal instance
        """
        plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
        engine, info, extension = plugin()
        cal = plugin.get_interface("Cloud")
        rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
        rc = cal.init(rwloggerctx)
        assert rc == RwTypes.RwStatus.SUCCESS

        return cal

    @staticmethod
    def parse_tenants(tenant_list):
        """
        Args:
            tenant_list (str): format: "demo:private, mano:private"

        Raises:
            ValueError: If the format is incorrect

        Returns:
            list of tuples: (tenant, network)
        """
        tenants = []
        if not tenant_list:
            return tenants

        try:
            for tenant in tenant_list.split(","):
                tenant = tenant.strip()
                tenant, network = tenant.split(":")
                tenants.append((tenant, network))
        except ValueError:
            raise ValueError("Tenant list should be specified as <TENANT>:<NETWORK>")

        return tenants


class OpenstackCleanup(Openstack):
    """
    A convenience class on top of cal interface to provide cleanup
    functions.
    """
    DEFAULT_FLAVORS = ['m1.tiny', 'm1.small', 'm1.medium', 'm1.large', 'm1.xlarge']
    DEFAULT_IMAGES = ['rwimage', 'rift.auto.image',
                      'rift-root-latest-multivm-vnf.qcow2', 'rift-ui-latest.qcow2']
    DEFAULT_NETWORKS =["public", "private"]

    def __init__(self, host, user, tenants, dry_run=False):
        """
        Args:
            account (RwcalYang.CloudAccount): Accound details
            dry_run (bool, optional): If enable, only log statements are
                    printed.
        """
        super().__init__(host, user, tenants)
        self.dry_run = dry_run

    def delete_vms(self, skip_list=None):
        """Delete all the VM in the tenant.

        Args:
            skip_list (list, optional): A list of VM names. Skips the
                    VMs present in the list
        """
        skip_list = skip_list or []

        for account in self.accounts:
            rc, vdulist = self.cal.get_vdu_list(account)
            for vduinfo in vdulist.vdu_info_list:
                if vduinfo.name in skip_list:
                    continue
                if self.user not in vduinfo.name:
                    continue
                logger.info("Deleting VM: {}".format(vduinfo.name))
                if self.dry_run:
                    continue
                self.cal.delete_vdu(account, vduinfo.vdu_id)

    def delete_networks(self, skip_list=None):
        """Delete all the networks.

        Args:
            skip_list (list, optional): A list of Network names. Skips the
                    networks present in the list
        """
        skip_list = skip_list or []
        for account in self.accounts:
            rc, rsp = self.cal.get_virtual_link_list(account)

            for vlink in rsp.virtual_link_info_list:
                if vlink.name in skip_list:
                    continue
                if self.user not in vlink.name:
                    continue
                logger.info("Deleting Network: {}".format(vlink.name))
                if self.dry_run:
                    continue
                self.cal.delete_virtual_link(
                        account,
                        vlink.virtual_link_id)

    def delete_ports_on_default_network(self, skip_list=None):
        """Delete all the ports on the mgmt_network, except the ones
        that are active.

        The ports that are active are identified using the presence of
        VM ID.

        Args:
            skip_list (list, optional): A list of port names. Skips the
                    ports present in the list
        """
        skip_list = skip_list or []
        for account in self.accounts:
            rc, rsp = self.cal.get_port_list(account)

            for port in rsp.portinfo_list:
                if port.port_name not in skip_list and len(port.vm_id) == 0:
                    logger.info("Deleting Port: {}".format(port.port_name))
                    if self.dry_run:
                        continue
                    self.cal.delete_port(account, port.port_id)

    def delete_flavors(self, skip_list=None):
        """Delete all the flavors.

        Args:
            skip_list (list, optional): A list of flavor names. Skips the
                    flavors present in the list
        """
        skip_list = skip_list or []
        for account in self.accounts:
            rc, rsp = self.cal.get_flavor_list(account)

            for flavor in rsp.flavorinfo_list:
                if flavor.name in skip_list:
                    continue
                if self.user not in flavor.name:
                    continue
                logger.info("Deleting Flavor: {}".format(flavor.name))
                if self.dry_run:
                    continue
                self.cal.delete_flavor(account, flavor.id)

    def delete_images(self, skip_list=None):
        """Delete all the images.

        Args:
            skip_list (list, optional): A list of image names. Skips the
                    images present in the list
        """
        skip_list = skip_list or []

        for account in self.accounts:
            rc, rsp = self.cal.get_image_list(account)

            for image in rsp.imageinfo_list:
                if image.name in skip_list:
                    continue
                if self.user not in image.name:
                    continue
                logger.info("Deleting Image: {}".format(image.name))
                if self.dry_run:
                    continue
                self.cal.delete_image(account, image.id)


def reset_openstack(openstack_cleanup):
    """
    A sample reset openstack callback that can be used by the client
    """
    openstack_cleanup.delete_vms()

    openstack_cleanup.delete_networks(skip_list=OpenstackCleanup.DEFAULT_NETWORKS)
    openstack_cleanup.delete_ports_on_default_network()

    openstack_cleanup.delete_images(skip_list=OpenstackCleanup.DEFAULT_IMAGES)
    openstack_cleanup.delete_flavors(skip_list=OpenstackCleanup.DEFAULT_FLAVORS)


class OpenstackManoSetup(Openstack):
    """Provide a single entry point for setting up an openstack controller
    with launchpad.

    Usage:
        mano = OpenstackManoSetup(account)
        with mano.setup() as (launchpad, slaves):
            <SYS CMD>
    """
    _DEFAULT_IMG = RwcalYang.ImageInfoItem.from_dict({
            'name': 'rift.auto.image',
            'location': os.path.join(os.getenv("RIFT_ROOT"), "images/rift-ui-lab-latest.qcow2"),
            'disk_format': 'qcow2',
            'container_format': 'bare'})

    _DEFAULT_FLAVOR = RwcalYang.FlavorInfoItem.from_dict({
            'name': 'rift.auto.flavor.{}'.format(str(uuid.uuid4())),
            'vm_flavor': {
                'memory_mb': 8192,  # 8 GB
                'vcpu_count': 4,
                'storage_gb': 40,  # 40 GB
            }
        })

    _LAUNCHPAD_NAME         = "rift_auto_launchpad"
    _LAUNCHPAD_NAME_STANDBY = "rift_auto_launchpad_standby"

    def __init__(self, host, user, tenants, site=None, image=None, flavor=None, use_existing=False):
        """
        Args:
            account (RwcalYang.CloudAccount): Account details
            site (str, optional): If specified as "blr", enable_lab CMD
                is called with -s option.
            image (RwcalYang.ImageInfoItem, optional): If specified the image
                is created usin the config.
            flavor (RwcalYang.FlavorInfoItem, optional): If specified the
                flavor config is used to create all resources.
            use_exiting(bool, optional): If set no new instance are created,
                looks for exiting ones.
        """
        super().__init__(host, user, tenants)
        # we just need a tenant to set up the LP and MC vms, so taking the
        # first one.
        self.account = self.accounts[0]
        self._site = site
        self.image = image or self._DEFAULT_IMG
        self.flavor = flavor or self._DEFAULT_FLAVOR
        self._use_existing = use_existing

    @property
    def master_name(self):
        """Returns the Master resource's name
        """
        return self._LAUNCHPAD_NAME

    def check_for_updated_image(self, image):
        """Verify the currently loaded image is up-to-date

        Arguments:
            image - image reference to check against the currently loaded image

        Return:
            image_id - id of the currently loaded image or None if the image is not
                     up-to-date
        """
        for account in self.accounts:
            rc, rsp = self.cal.get_image_list(account)

            for curr_image in rsp.imageinfo_list:
                if curr_image.name == image.name:
                   local_cksum = rift.auto.vm_utils.md5checksum(image.location)
                   if local_cksum == curr_image.checksum:
                       return curr_image.id
                   else:
                       # delete out-dated image
                       logger.debug("Deleting openstack image: %s", curr_image.name)
                       self.cal.delete_image(account, curr_image.id)
                       break
        return None

    def create_image(self, image=None):
        """Creates an image using the config provided.

        Arguments:
            image (RwcalYang.ImageInfoItem, optional): If specified the image
                is created using that image
        Raises:
            TimeoutError: If the image does not reach a valid state
            ValidationError: If the image upload failed.

        Returns:
            str: image_id
        """
        if image is None:
            image = self.image

        current_image_id = self.check_for_updated_image(image)
        if current_image_id is not None:
            return current_image_id

        def wait_for_image_state(account, image_id, state, timeout=300):
            state = state.lower()
            current_state = 'unknown'

            while current_state != state:
                rc, image_info = self.cal.get_image(account, image_id)
                current_state = image_info.state.lower()

                if current_state in ['failed']:
                   raise ValidationError('Image [{}] entered failed state while waiting for state [{}]'.format(image_id, state))

                if current_state != state:
                    time.sleep(1)

            if current_state != state:
                logger.error('Image still in state [{}] after [{}] seconds'.format(current_state, timeout))
                raise TimeoutError('Image [{}] failed to reach state [{}] within timeout [{}]'.format(image_id, state, timeout))

            return image_info

        logger.debug("Uploading VM Image: %s", image.name)
        rc, image_id = self.cal.create_image(self.account, image)
        assert rc == RwTypes.RwStatus.SUCCESS
        image_info = wait_for_image_state(self.account, image_id, 'active')

        return image_id

    def create_flavor(self):
        """Creates a flavor in the openstack instance.

        Returns:
            str: flavor id
        """
        logger.debug("Creating VM Flavor")
        rc, flavor_id = self.cal.create_flavor(self.account, self.flavor)
        assert rc == RwTypes.RwStatus.SUCCESS

        return flavor_id

    def _wait_for_vdu_state(self, vdu_id, state, timeout=300):
        """Wait till the VM enter an 'active' state.

        Args:
            vdu_id (str): ID of the VM
            state (str): expected state
            timeout (int, optional): defaults to 300

        Raises:
            TimeoutError: If the VM does not reach a valid state
            ValidationError: If VM reaches a failed state.

        Returns:
            vdu_info(RwcalYang.VDUInfoParams)
        """
        state = state.lower()
        current_state = 'unknown'
        while current_state != state:
            rc, vdu_info = self.cal.get_vdu(self.account, vdu_id)

            assert rc == RwTypes.RwStatus.SUCCESS
            current_state = vdu_info.state.lower()

            if current_state in ['failed']:
               raise ValidationError('VM [{}] entered failed state while waiting for state [{}]'.format(vdu_id, state))

            if current_state != state:
                time.sleep(1)

        if current_state != state:
            raise TimeoutError('VM [{}] failed to reach state [{}] within timeout [{}]'.format(vdu_id, state, timeout))

        return vdu_info

    def create_master_resource(self, name):
        """Creates the master resource instance.

        Returns:
            VduInfo: vdu_info of master resource
        """
        vdu_id = str(uuid.uuid4())
        userdata = """#cloud-config

    runcmd:
     - /usr/rift/scripts/cloud/enable_lab {site}
    """

        site = ""
        if self._site == "blr":
            site = "-s blr"
        userdata = userdata.format(site=site)

        vdu_info_dict = {
            'name': resource_name(name, self.user),
            'node_id': vdu_id,
            'flavor_id': str(self.flavor_id),
            'image_id': self.image_id,
            'allocate_public_address': False,
            'vdu_init': {
                'userdata': userdata}
        }

        vdu_info = RwcalYang.VDUInitParams.from_dict(vdu_info_dict)

        logger.debug("Creating Master VM with VDU params: %s", vdu_info_dict)
        rc, vdu_id = self.cal.create_vdu(self.account, vdu_info)

        assert rc.status == RwTypes.RwStatus.SUCCESS

        vdu_info = self._wait_for_vdu_state(vdu_id, 'active')
        logger.debug("Master VM Active!")

        return VduInfo(vdu_info)

    def create_slave_resource(self, vdu_name, master_vm):
        """Create a slave resource with salt config pointing to master's IP.

        Args:
            vdu_name (str): Name of the VDU
            master_vm (VduInfo): VduInfo instance for Master Vm

        Returns:
            VduInfo(VduInfo): VduInfo of the newly created slave resource.

        """
        vdu_id = str(uuid.uuid4())

        site = ""
        if self._site == "blr":
            site = "-s blr"

        vdu_userdata = """#cloud-config

    salt_minion:
      conf:
        master: {mgmt_ip}
        id: {vdu_id}
        acceptance_wait_time: 1
        recon_default: 100
        recon_max: 1000
        recon_randomize: False
        log_level: debug

    runcmd:
     - echo Sleeping for 5 seconds and attempting to start minion
     - sleep 5
     - /bin/systemctl start salt-minion.service

    runcmd:
     - /usr/rift/scripts/cloud/enable_lab {site}
    """.format(mgmt_ip=master_vm.vdu_info.management_ip,
               vdu_id=vdu_id,
               site=site)

        vdu_info = RwcalYang.VDUInitParams.from_dict({
            'name': resource_name(vdu_name, self.user),
            'node_id': vdu_id,
            'flavor_id': str(self.flavor_id),
            'image_id': self.image_id,
            'allocate_public_address': False,
            'vdu_init': {'userdata': vdu_userdata},
        })

        logger.debug("Creating vdu resource {}".format(vdu_name))
        rc, vdu_id = self.cal.create_vdu(self.account, vdu_info)

        vdu_info = self._wait_for_vdu_state(vdu_id, 'active')
        logger.debug("Slave VM {} active".format(vdu_name))

        return VduInfo(vdu_info)

    def _wait_till_resources_ready(self, resources, timeout):
        """A convenience method to check if all resource are ready (ssh'able &
            cloud-init is done running.)

        Args:
            resources (list): A list of all resources to be checked.
            timeout (int): Timeout in seconds

        Raises:
            ValidationError: If all the resources are not ready within the
            timeout duration.

        """
        start = time.time()
        elapsed = 0
        while resources and elapsed < timeout:
            resource = resources.popleft()
            if not resource.is_ready():
                resources.append(resource)

            time.sleep(5)
            elapsed = time.time() - start

        if resources:
            raise ValidationError("Failed to verify all VM resources started")

    def _get_vdu(self, name):
        """Finds and returns the VM from the list of VMs present in the tenant.

        Args:
            name (str): Name of the VM

        Returns:
            VduInfo: VduInfo instance of the VM.
        """
        for account in self.accounts:
            rc, vdulist = self.cal.get_vdu_list(account)
            for vduinfo in vdulist.vdu_info_list:
                if vduinfo.name in [name, resource_name(name, self.user)]:
                    return VduInfo(vduinfo)

        return None

    @contextlib.contextmanager
    def setup(self, slave_resources=0, timeout=300, cleanup_clbk=None):
        """
        Args:
            slave_resources (int, optional): Number of additional slave resource
                to be created.
            timeout (int, optional): timeout in seconds
            cleanup_clbk (callable, optional): Callback to perform any teardown
                operations.
                Signature: cleanup_clbk(cleanup)
                    cleanup(OpenstackCleanup): Account details

        Yields:
            A tuple of VduInfo- Master, Slaves.
        """
        try:
            if self._use_existing:
                    yield self._get_vdu(self._LAUNCHPAD_NAME), [self._get_vdu(self._LAUNCHPAD_NAME_STANDBY)]

            else:
                self.image_id = self.create_image()
                self.flavor_id = self.create_flavor()

                master = self.create_master_resource(self.master_name)
                resources = collections.deque([master])
                #TODO: Need to extend this for LSS model
                slaves = []
                if slave_resources:
                    slave = self.create_slave_resource(self._LAUNCHPAD_NAME_STANDBY, master)
                    slaves.append(slave)

                resources.extend(slaves)

                self._wait_till_resources_ready(resources, timeout=timeout)

                yield master, slaves

        finally:
            if cleanup_clbk is not None:
                # Clean up should be done for all the accounts.
                cleanup_clbk(OpenstackCleanup(self.host, self.user, self.tenants))


class VduInfo(object):
    """
    A wrapper on the vdu_info with some additional utilities.
    """
    def __init__(self, vdu_info):
        """
        Args:
            vdu_info (RwcalYang.VDUInfoParams): Vdu info
        """
        self.vdu_info = vdu_info
        self._ip = vdu_info.public_ip or vdu_info.management_ip
        self._is_accessible = False

    @property
    def id(self):
        """Node ID of the VM in openstack
        """
        return self.vdu_info.vdu_id

    @property
    def ip(self):
        """IP address of the VM
        """
        return self._ip

    @property
    def is_accessible(self):
        """Check if the machine is ssh'able.
        """
        if self._is_accessible:
            return self._is_accessible

        check_host_cmd = '/usr/rift/bin/ssh_root {ip} -q -n -o BatchMode=yes -o StrictHostKeyChecking=no ls > /dev/null'
        rc = subprocess.call(check_host_cmd.format(ip=self._ip), shell=True)
        logger.info("Checking if {} is accessible".format(self._ip))



        if rc != 0:
            return False

        self._is_accessible = True
        return self._is_accessible

    def is_ready(self):
        """Checks if the cloud-init script is done running.

        Checks if the boot-finished file is present.

        Returns:
            bool: True if present.
        """
        if not self.is_accessible:
            return False

        is_ready_cmd = '/usr/rift/bin/ssh_root {ip} -q -n -o BatchMode=yes -o StrictHostKeyChecking=no stat /var/lib/cloud/instance/boot-finished > /dev/null'
        rc = subprocess.call(is_ready_cmd.format(ip=self._ip), shell=True)

        logger.info("Checking if {} is ready".format(self._ip))
        if rc != 0:
            return False

        return True
