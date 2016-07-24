
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import datetime
import logging
import time
import unittest

import novaclient.exceptions as nova_exception
import paramiko
import rw_peas
import rwlogger
from keystoneclient import v3 as ksclient

from gi.repository import RwcalYang
from gi.repository.RwTypes import RwStatus
from rift.rwcal.openstack.openstack_drv import KeystoneDriver, NovaDriver

logger = logging.getLogger('rwcal-openstack')

#
# Important information about openstack installation. This needs to be manually verified 
#
openstack_info = {
    'username'           : 'pluto',
    'password'           : 'mypasswd',
    'auth_url'           : 'http://10.66.4.14:5000/v3/',
    'project_name'       : 'demo',
    'mgmt_network'       : 'private',
    'reserved_flavor'    : 'm1.medium',
    'reserved_image'     : 'rift-root-latest.qcow2',
    'physical_network'   : None,
    'network_type'       : None,
    'segmentation_id'    : None
    }


def get_cal_account():
    """
    Creates an object for class RwcalYang.CloudAccount()
    """
    account                        = RwcalYang.CloudAccount()
    account.account_type           = "openstack"
    account.openstack.key          = openstack_info['username']
    account.openstack.secret       = openstack_info['password']
    account.openstack.auth_url     = openstack_info['auth_url']
    account.openstack.tenant       = openstack_info['project_name']
    account.openstack.mgmt_network = openstack_info['mgmt_network']
    return account

def get_cal_plugin():
    """
    Loads rw.cal plugin via libpeas
    """
    plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
    engine, info, extension = plugin()

    # Get the RwLogger context
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")

    cal = plugin.get_interface("Cloud")
    try:
        rc = cal.init(rwloggerctx)
        assert rc == RwStatus.SUCCESS
    except:
        logger.error("ERROR:Cal plugin instantiation failed. Aborting tests")
    else:
        logger.info("Openstack Cal plugin successfully instantiated")
    return cal 


class OpenStackTest(unittest.TestCase):
    IMG_Checksum = "12312313123131313131" # Some random number to test image tagging
    NodeID = "123456789012345" # Some random number to test VM tagging
    MemoryPageSize = "LARGE"
    CpuPolicy = "DEDICATED"
    CpuThreadPolicy = "SEPARATE"
    CpuThreads = 1
    NumaNodeCount = 2
    HostTrust = "trusted"
    PCIPassThroughAlias = "PCI_10G_ALIAS"
    SEG_ID = openstack_info['segmentation_id']
    
    def setUp(self):
        """
        Assumption:
         - It is assumed that openstack install has a flavor and image precreated.
         - Flavor_name: x1.xlarge
         - Image_name : rwimage

        If these resources are not then this test will fail.
        """
        self._acct = get_cal_account()
        logger.info("Openstack-CAL-Test: setUp")
        self.cal   = get_cal_plugin()
        logger.info("Openstack-CAL-Test: setUpEND")
        
        # First check for VM Flavor and Image and get the corresponding IDs
        rc, rs = self.cal.get_flavor_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)

        flavor_list = [ flavor for flavor in rs.flavorinfo_list if flavor.name == openstack_info['reserved_flavor'] ]
        self.assertNotEqual(len(flavor_list), 0)
        self._flavor = flavor_list[0]

        rc, rs = self.cal.get_image_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)

        image_list = [ image for image in rs.imageinfo_list if image.name == openstack_info['reserved_image'] ]
        self.assertNotEqual(len(image_list), 0)
        self._image = image_list[0]

        rc, rs = self.cal.get_network_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        networks = [ network for network in rs.networkinfo_list if (network.network_name == 'rift.cal.unittest.network' or network.network_name == 'rift.cal.virtual_link') ]
        for network in networks:
            self.cal.delete_virtual_link(self._acct, network.network_id)
            
    def tearDown(self):
        logger.info("Openstack-CAL-Test: tearDown")
        

    @unittest.skip("Skipping test_list_flavors")        
    def test_list_flavor(self):
        """
        List existing flavors from openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting List Flavors Test")
        rc, rsp = self.cal.get_flavor_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d flavors" %(len(rsp.flavorinfo_list)))
        for flavor in rsp.flavorinfo_list:
            rc, flv = self.cal.get_flavor(self._acct, flavor.id)
            self.assertEqual(rc, RwStatus.SUCCESS)
            self.assertEqual(flavor.id, flv.id)
        
    @unittest.skip("Skipping test_list_images")                    
    def test_list_images(self):
        """
        List existing images from openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting List Images Test")
        rc, rsp = self.cal.get_image_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d images" %(len(rsp.imageinfo_list)))
        #for image in rsp.imageinfo_list:
        #    rc, img = self.cal.get_image(self._acct, image.id)
        #    self.assertEqual(rc, RwStatus.SUCCESS)
        #    self.assertEqual(image.id, img.id)
        
    @unittest.skip("Skipping test_list_vms")                
    def test_list_vms(self):
        """
        List existing VMs from openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting List VMs Test")
        rc, rsp = self.cal.get_vm_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d VMs" %(len(rsp.vminfo_list)))
        for vm in rsp.vminfo_list:
            rc, server = self.cal.get_vm(self._acct, vm.vm_id)
            self.assertEqual(vm.vm_id, server.vm_id)
            
    @unittest.skip("Skipping test_list_networks")                            
    def test_list_networks(self):
        """
        List existing Network from openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting List Networks Test")
        rc, rsp = self.cal.get_network_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d Networks" %(len(rsp.networkinfo_list)))
        for network in rsp.networkinfo_list:
            rc, net = self.cal.get_network(self._acct, network.network_id)
            self.assertEqual(network.network_id, net.network_id)
        
    @unittest.skip("Skipping test_list_ports")                                    
    def test_list_ports(self):
        """
        List existing Ports from openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting List Ports Test")
        rc, rsp = self.cal.get_port_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        assert(rc == RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d Ports" %(len(rsp.portinfo_list)))
        for port in rsp.portinfo_list:
            rc, p = self.cal.get_port(self._acct, port.port_id)
            self.assertEqual(port.port_id, p.port_id)

    def _get_image_info_request(self):
        """
        Returns request object of type RwcalYang.ImageInfoItem()
        """
        img = RwcalYang.ImageInfoItem()
        img.name = "rift.cal.unittest.image"
        img.location = '/net/sharedfiles/home1/common/vm/rift-root-latest.qcow2'
        img.disk_format = "qcow2"
        img.container_format = "bare"
        img.checksum = OpenStackTest.IMG_Checksum
        return img

    def _get_image_info(self, img_id):
        """
        Checks the image status until it becomes active or timeout occurs (100sec)
        Returns the image_info dictionary
        """
        rs = None
        rc = None
        for i in range(100):
            rc, rs = self.cal.get_image(self._acct, img_id)
            self.assertEqual(rc, RwStatus.SUCCESS)
            logger.info("Openstack-CAL-Test: Image (image_id: %s) reached state : %s" %(img_id, rs.state))
            if rs.state == 'active':
                break
            else:
                time.sleep(2) # Sleep for a second
        return rs
    
    @unittest.skip("Skipping test_create_delete_image")                            
    def test_create_delete_image(self):
        """
        Create/Query/Delete a new image in openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting Image create test")
        img = self._get_image_info_request()
        rc, img_id = self.cal.create_image(self._acct, img)
        logger.info("Openstack-CAL-Test: Created Image with image_id: %s" %(img_id))
        self.assertEqual(rc, RwStatus.SUCCESS)
        img_info = self._get_image_info(img_id)
        self.assertNotEqual(img_info, None)
        self.assertEqual(img_id, img_info.id)
        logger.info("Openstack-CAL-Test: Image (image_id: %s) reached state : %s" %(img_id, img_info.state))
        self.assertEqual(img_info.has_field('checksum'), True)
        self.assertEqual(img_info.checksum, OpenStackTest.IMG_Checksum)
        logger.info("Openstack-CAL-Test: Initiating Delete Image operation for image_id: %s" %(img_id))
        rc = self.cal.delete_image(self._acct, img_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Image (image_id: %s) successfully deleted" %(img_id))

    def _get_flavor_info_request(self):
        """
        Returns request object of type RwcalYang.FlavorInfoItem()
        """
        flavor                                     = RwcalYang.FlavorInfoItem()
        flavor.name                                = 'rift.cal.unittest.flavor'
        flavor.vm_flavor.memory_mb                 = 16384 # 16GB
        flavor.vm_flavor.vcpu_count                = 4 
        flavor.vm_flavor.storage_gb                = 40 # 40GB
        flavor.guest_epa.mempage_size              = OpenStackTest.MemoryPageSize
        flavor.guest_epa.cpu_pinning_policy        = OpenStackTest.CpuPolicy
        flavor.guest_epa.cpu_thread_pinning_policy = OpenStackTest.CpuThreadPolicy
        flavor.guest_epa.numa_node_policy.node_cnt = OpenStackTest.NumaNodeCount
        for i in range(OpenStackTest.NumaNodeCount):
            node = flavor.guest_epa.numa_node_policy.node.add()
            node.id = i
            if i == 0:
                node.vcpu = [0,1]
            elif i == 1:
                node.vcpu = [2,3]
            node.memory_mb = 8196
        dev = flavor.guest_epa.pcie_device.add()
        dev.device_id = OpenStackTest.PCIPassThroughAlias
        dev.count = 1
        return flavor
        
    @unittest.skip("Skipping test_create_delete_flavor")                            
    def test_create_delete_flavor(self):
        """
        Create/Query/Delete a new flavor in openstack installation
        """
        logger.info("Openstack-CAL-Test: Starting Image create/delete test")

        ### Delete any previously created flavor with name rift.cal.unittest.flavor
        rc, rs = self.cal.get_flavor_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        flavor_list = [ flavor for flavor in rs.flavorinfo_list if flavor.name == 'rift.cal.unittest.flavor' ]
        if flavor_list:
            rc = self.cal.delete_flavor(self._acct, flavor_list[0].id)
            self.assertEqual(rc, RwStatus.SUCCESS)
        
        flavor = self._get_flavor_info_request()
        rc, flavor_id = self.cal.create_flavor(self._acct, flavor)
        self.assertEqual(rc, RwStatus.SUCCESS)
        
        logger.info("Openstack-CAL-Test: Created new flavor with flavor_id : %s" %(flavor_id))
        rc, rs = self.cal.get_flavor(self._acct, flavor_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        self.assertEqual(rs.id, flavor_id)

        # Verify EPA Attributes
        self.assertEqual(rs.guest_epa.mempage_size, OpenStackTest.MemoryPageSize)
        self.assertEqual(rs.guest_epa.cpu_pinning_policy, OpenStackTest.CpuPolicy)
        self.assertEqual(rs.guest_epa.cpu_thread_pinning_policy, OpenStackTest.CpuThreadPolicy)
        self.assertEqual(rs.guest_epa.numa_node_policy.node_cnt, OpenStackTest.NumaNodeCount)
        self.assertEqual(len(rs.guest_epa.pcie_device), 1)
        self.assertEqual(rs.guest_epa.pcie_device[0].device_id, OpenStackTest.PCIPassThroughAlias)
        self.assertEqual(rs.guest_epa.pcie_device[0].count, 1)
        logger.info("Openstack-CAL-Test: Initiating delete for flavor_id : %s" %(flavor_id))
        rc = self.cal.delete_flavor(self._acct, flavor_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        # Check that flavor does not exist anymore in list_flavor
        rc, rs = self.cal.get_flavor_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        flavor_list = [ flavor for flavor in rs.flavorinfo_list if flavor.id == flavor_id ]
        # Flavor List should be empty
        self.assertEqual(len(flavor_list), 0)
        logger.info("Openstack-CAL-Test: Flavor (flavor_id: %s) successfully deleted" %(flavor_id))

    def _get_vm_info_request(self, flavor_id, image_id):
        """
        Returns request object of type RwcalYang.VMInfoItem
        """
        vm = RwcalYang.VMInfoItem()
        vm.vm_name = 'rift.cal.unittest.vm'
        vm.flavor_id = flavor_id
        vm.image_id  = image_id
        vm.cloud_init.userdata = ''
        vm.user_tags.node_id  = OpenStackTest.NodeID
        return vm

    def _check_vm_state(self, vm_id, expected_state):
        """
        Wait until VM reaches particular state (expected_state). 
        """
        # Wait while VM goes to required state

        for i in range(50): # 50 poll iterations...
            rc, rs = self.cal.get_vm(self._acct, vm_id)
            self.assertEqual(rc, RwStatus.SUCCESS)
            logger.info("Openstack-CAL-Test: VM vm_id : %s. Current VM state : %s " %(vm_id, rs.state))
            if rs.state == expected_state:
                break
            else:
                time.sleep(1)

        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        self.assertEqual(rs.state, expected_state)

    def _create_vm(self, flavor, image, port_list = None):
        """
        Create VM and perform validity checks
        """
        logger.info("Openstack-CAL-Test: Using image : %s and flavor : %s " %(image.name, flavor.name))
        vm = self._get_vm_info_request(flavor.id, image.id)

        if port_list:
            for port_id in port_list:
                port = vm.port_list.add()
                port.port_id = port_id 

        rc, vm_id = self.cal.create_vm(self._acct, vm)
        self.assertEqual(rc, RwStatus.SUCCESS)

        ### Check if VM creation is successful
        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Successfully created VM with vm_id : %s. Current VM state : %s " %(vm_id, rs.state))

        ### Ensure the VM state is active
        self._check_vm_state(vm_id, 'ACTIVE')

        ### Ensure that userdata tags are set as expected
        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        self.assertEqual(rs.user_tags.has_field('node_id'), True)
        self.assertEqual(getattr(rs.user_tags, 'node_id'), OpenStackTest.NodeID)
        logger.info("Openstack-CAL-Test: Successfully verified the user tags for VM-ID: %s" %(vm_id))
        return rs, vm_id

    def _delete_vm(self, vm_id):
        """
        Delete VM and perform validity checks
        """
        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)

        logger.info("Openstack-CAL-Test: Initiating VM Delete operation on VM vm_id : %s. Current VM state : %s " %(vm_id, rs.state))

        rc = self.cal.delete_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)

        for i in range(50):
            # Check if VM still exists
            rc, rs = self.cal.get_vm_list(self._acct)
            self.assertEqual(rc, RwStatus.SUCCESS)
            vm_list = [vm for vm in rs.vminfo_list if vm.vm_id == vm_id]
            if not len(vm_list):
                break
        
        rc, rs = self.cal.get_vm_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        vm_list = [vm for vm in rs.vminfo_list if vm.vm_id == vm_id]
        self.assertEqual(len(vm_list), 0)
        logger.info("Openstack-CAL-Test: VM with vm_id : %s successfully deleted" %(vm_id))

    def _stop_vm(self, vm_id):
        """
        Stop VM and perform validity checks
        """
        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Initiating Stop VM operation on VM vm_id : %s. Current VM state : %s " %(vm_id, rs.state))
        rc = self.cal.stop_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        ### Ensure that VM state is SHUTOFF
        self._check_vm_state(vm_id, 'SHUTOFF')
        
        
    def _start_vm(self, vm_id):
        """
        Starts VM and performs validity checks
        """
        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Initiating Start VM operation on VM vm_id : %s. Current VM state : %s " %(vm_id, rs.state))
        rc = self.cal.start_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)

        ### Ensure that VM state is ACTIVE
        self._check_vm_state(vm_id, 'ACTIVE')

        
    def _reboot_vm(self, vm_id):
        """
        Reboot VM and perform validity checks
        """
        rc, rs = self.cal.get_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Initiating Reboot VM operation on VM vm_id : %s. Current VM state : %s " %(vm_id, rs.state))
        rc = self.cal.reboot_vm(self._acct, vm_id)
        self.assertEqual(rc, RwStatus.SUCCESS)

        ### Ensure that VM state is ACTIVE
        self._check_vm_state(vm_id, 'ACTIVE')

    def assert_vm(self, vm_data, flavor):
        """Verify the newly created VM for attributes specified in the flavor.

        Args:
            vm_data (VmData): Instance of the newly created VM
            flavor (FlavorInfoItem): Config flavor.
        """
        vm_config = flavor

        # Page size seems to be 4096, regardless of the page size name.
        page_lookup = {"large": '4096', "small": '4096'}
        FIELDS = ["vcpus", "cpu_threads", "memory_page_size", "disk",
                  "numa_node_count", "memory", "pci_passthrough_device_list"]

        for field in FIELDS:
            if field not in vm_config:
                continue

            vm_value = getattr(vm_data, field)
            config_value = getattr(vm_config, field)

            if field == "memory_page_size":
                config_value = page_lookup[config_value]

            if field == "memory":
                config_value = int(config_value/1000)

            if field == "pci_passthrough_device_list":
                config_value = len(config_value)
                vm_value = len(vm_value)

            self.assertEqual(vm_value, config_value)

    @unittest.skip("Skipping test_vm_epa_attributes")
    def test_vm_epa_attributes(self):
        """
        Primary goal: To create a VM with the specified EPA Attributes
        Secondary goal: To verify flavor creation/delete
        """

        logger.info("Openstack-CAL-Test: Starting VM(EPA) create/delete test")
        flavor = self._get_flavor_info_request()
   
        rc, flavor_id = self.cal.do_create_flavor(self._acct, flavor)
        self.assertEqual(rc, RwStatus.SUCCESS)
        flavor.id = flavor_id

        data, vm_id = self._create_vm(flavor, self._image)

        vm_data = VmData(data.host_name, data.management_ip)
        self.assert_vm(vm_data, flavor)

        self._delete_vm(vm_id)

        rc = self.cal.do_delete_flavor(self._acct, flavor_id)
        self.assertEqual(rc, RwStatus.SUCCESS)

    @unittest.skip("Skipping test_expiry_token")
    def test_expiry_token(self):
        """
        Primary goal: To verify if we are refreshing the expired tokens.
        """
        logger.info("Openstack-CAL-Test: Starting token refresh test")
        drv = KeystoneDriver(
                openstack_info['username'],
                openstack_info['password'],
                openstack_info['auth_url'],
                openstack_info['project_name'])
        # Get hold of the client instance need for Token Manager
        client = drv._get_keystone_connection()

        auth_ref = client.auth_ref
        token = auth_ref['auth_token']

        # Verify if the newly acquired token works.
        nova = NovaDriver(drv)
        flavors = nova.flavor_list()
        self.assertTrue(len(flavors) > 1)

        # Invalidate the token
        token_manger = ksclient.tokens.TokenManager(client)
        token_manger.revoke_token(token)

        time.sleep(10)

        unauth_exp = False
        try:
            flavors = nova.flavor_list()
            print (flavors)
        except nova_exception.AuthorizationFailure:
            unauth_exp = True

        self.assertTrue(unauth_exp)

        # Explicitly reset the expire time, to test if we acquire a new token
        now = datetime.datetime.utcnow()
        time_str = format(now, "%Y-%m-%dT%H:%M:%S.%fZ")
        drv._get_keystone_connection().auth_ref['expires_at'] = time_str

        flavors = nova.flavor_list()
        self.assertTrue(len(flavors) > 1)

    @unittest.skip("Skipping test_vm_operations")                            
    def test_vm_operations(self):
        """
        Primary goal: Create/Query/Delete VM in openstack installation.
        Secondary goal: VM pause/resume operations on VM.

        """
        logger.info("Openstack-CAL-Test: Starting VM Operations test")

        # Create VM
        data, vm_id = self._create_vm(self._flavor, self._image)

        # Stop VM
        self._stop_vm(vm_id)
        # Start VM
        self._start_vm(vm_id)

        vm_data = VmData(data.host_name, data.management_ip)
        self.assert_vm(vm_data, self._flavor)

        # Reboot VM
        self._reboot_vm(vm_id)
        ### Delete the VM
        self._delete_vm(vm_id)

        
    def _get_network_info_request(self):
        """
        Returns request object of type RwcalYang.NetworkInfoItem
        """
        network                            = RwcalYang.NetworkInfoItem()
        network.network_name               = 'rift.cal.unittest.network'
        network.subnet                     = '192.168.16.0/24'
        if openstack_info['physical_network']:
            network.provider_network.physical_network = openstack_info['physical_network']
        if openstack_info['network_type']:
            network.provider_network.overlay_type     = openstack_info['network_type']
        if OpenStackTest.SEG_ID:
            network.provider_network.segmentation_id  = OpenStackTest.SEG_ID
            OpenStackTest.SEG_ID += 1
        return network


    def _create_network(self):
        """
        Create a network and verify that network creation is successful
        """
        network = self._get_network_info_request()

        ### Create network
        logger.info("Openstack-CAL-Test: Creating a network with name : %s" %(network.network_name))
        rc, net_id = self.cal.create_network(self._acct, network)
        self.assertEqual(rc, RwStatus.SUCCESS)

        ### Verify network is created successfully
        rc, rs = self.cal.get_network(self._acct, net_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Successfully create Network : %s  with id : %s." %(network.network_name, net_id ))

        return net_id

    def _delete_network(self, net_id):
        """
        Delete network and verify that delete operation is successful
        """
        rc, rs = self.cal.get_network(self._acct, net_id)
        self.assertEqual(rc, RwStatus.SUCCESS)

        logger.info("Openstack-CAL-Test: Deleting a network with id : %s. " %(net_id))
        rc = self.cal.delete_network(self._acct, net_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        
        # Verify that network is no longer available via get_network_list API
        rc, rs = self.cal.get_network_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        network_info = [ network for network in rs.networkinfo_list if network.network_id == net_id ]
        self.assertEqual(len(network_info), 0)
        logger.info("Openstack-CAL-Test: Successfully deleted Network with id : %s" %(net_id))
        
        
    @unittest.skip("Skipping test_network_operations")                            
    def test_network_operations(self):
        """
        Create/Delete Networks
        """
        logger.info("Openstack-CAL-Test: Starting Network Operation test")

        ### Create Network
        net_id = self._create_network()

        ### Delete Network
        self._delete_network(net_id)

    def _get_port_info_request(self, network_id, vm_id):
        """
        Returns an object of type RwcalYang.PortInfoItem
        """
        port = RwcalYang.PortInfoItem()
        port.port_name = 'rift.cal.unittest.port'
        port.network_id = network_id
        if vm_id != None:
            port.vm_id = vm_id
        return port

    def _create_port(self, net_id, vm_id = None):
        """
        Create a port in network with network_id: net_id and verifies that operation is successful
        """
        if vm_id != None:
            logger.info("Openstack-CAL-Test: Creating a port in network with network_id: %s and VM with vm_id: %s" %(net_id, vm_id))
        else:
            logger.info("Openstack-CAL-Test: Creating a port in network with network_id: %s" %(net_id))

        ### Create Port
        port = self._get_port_info_request(net_id, vm_id)
        rc, port_id = self.cal.create_port(self._acct, port)
        self.assertEqual(rc, RwStatus.SUCCESS)

        ### Get Port
        rc, rs = self.cal.get_port(self._acct, port_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Successfully create Port with id : %s. Port State :  %s" %(port_id, rs.port_state))

        return port_id

    def _delete_port(self, port_id):
        """
        Deletes a port and verifies that operation is successful
        """
        rc, rs = self.cal.get_port(self._acct, port_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Deleting Port with id : %s. Port State :  %s" %(port_id, rs.port_state))

        ### Delete Port
        self.cal.delete_port(self._acct, port_id)
        
        rc, rs = self.cal.get_port_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        port_list = [ port for port in rs.portinfo_list if port.port_id == port_id ]
        self.assertEqual(len(port_list), 0)
        logger.info("Openstack-CAL-Test: Successfully Deleted Port with id : %s" %(port_id))

    def _monitor_port(self, port_id, expected_state):
        """
        Monitor the port state until it reaches expected_state
        """
        for i in range(50):
            rc, rs = self.cal.get_port(self._acct, port_id)
            self.assertEqual(rc, RwStatus.SUCCESS)
            logger.info("Openstack-CAL-Test: Port with id : %s. Port State :  %s" %(port_id, rs.port_state))
            if rs.port_state == expected_state:
                break
        rc, rs = self.cal.get_port(self._acct, port_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        self.assertEqual(rs.port_state, expected_state)
        logger.info("Openstack-CAL-Test: Port with port_id : %s reached expected state  : %s" %(port_id, rs.port_state))
            
    @unittest.skip("Skipping test_port_operations_with_vm")
    def test_port_operations_with_vm(self):
        """
        Create/Delete Ports in a network and associate it with a VM
        """
        logger.info("Openstack-CAL-Test: Starting Port Operation test with VM")

        ### First create a network
        net_id = self._create_network()

        ### Create a VM
        data, vm_id = self._create_vm(self._flavor, self._image)

        ### Now create Port which connects VM to Network
        port_id = self._create_port(net_id, vm_id)

        ### Verify that port goes to active state
        self._monitor_port(port_id, 'ACTIVE')

        ### Delete VM
        self._delete_vm(vm_id)
        
        ### Delete Port
        self._delete_port(port_id)

        ### Delete the network
        self._delete_network(net_id)

    @unittest.skip("Skipping test_create_vm_with_port")
    def test_create_vm_with_port(self):
        """
        Create VM and add ports to it during boot time.
        """
        logger.info("Openstack-CAL-Test: Starting Create VM with port test")

        ### First create a network
        net_id = self._create_network()

        ### Now create Port which connects VM to Network
        port_id = self._create_port(net_id)

        ### Create a VM
        data, vm_id = self._create_vm(self._flavor, self._image, [port_id])

        ### Verify that port goes to active state
        self._monitor_port(port_id, 'ACTIVE')

        ### Delete VM
        self._delete_vm(vm_id)
        
        ### Delete Port
        self._delete_port(port_id)

        ### Delete the network
        self._delete_network(net_id)

    @unittest.skip("Skipping test_get_vdu_list")
    def test_get_vdu_list(self):
        """
        Test the get_vdu_list API
        """
        logger.info("Openstack-CAL-Test: Test Get VDU List APIs")
        rc, rsp = self.cal.get_vdu_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d VDUs" %(len(rsp.vdu_info_list)))
        for vdu in rsp.vdu_info_list:
            rc, vdu2 = self.cal.get_vdu(self._acct, vdu.vdu_id)
            self.assertEqual(vdu2.vdu_id, vdu.vdu_id)


    @unittest.skip("Skipping test_get_virtual_link_list")
    def test_get_virtual_link_list(self):
        """
        Test the get_virtual_link_list API
        """
        logger.info("Openstack-CAL-Test: Test Get virtual_link List APIs")
        rc, rsp = self.cal.get_virtual_link_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Received %d virtual_links" %(len(rsp.virtual_link_info_list)))
        for virtual_link in rsp.virtual_link_info_list:
            rc, virtual_link2 = self.cal.get_virtual_link(self._acct, virtual_link.virtual_link_id)
            self.assertEqual(virtual_link2.virtual_link_id, virtual_link.virtual_link_id)

    def _get_virtual_link_request_info(self):
        """
        Returns object of type RwcalYang.VirtualLinkReqParams
        """
        vlink = RwcalYang.VirtualLinkReqParams()
        vlink.name = 'rift.cal.virtual_link'
        vlink.subnet = '192.168.1.0/24'
        if openstack_info['physical_network']:
            vlink.provider_network.physical_network = openstack_info['physical_network']
        if openstack_info['network_type']:
            vlink.provider_network.overlay_type     = openstack_info['network_type'].upper()
        if OpenStackTest.SEG_ID:
            vlink.provider_network.segmentation_id  = OpenStackTest.SEG_ID
            OpenStackTest.SEG_ID += 1
        return vlink
        
    def _get_vdu_request_info(self, virtual_link_id):
        """
        Returns object of type RwcalYang.VDUInitParams
        """
        vdu = RwcalYang.VDUInitParams()
        vdu.name = "cal.vdu"
        vdu.node_id = OpenStackTest.NodeID
        vdu.image_id = self._image.id
        vdu.flavor_id = self._flavor.id
        vdu.vdu_init.userdata = ''
        vdu.allocate_public_address = True
        c1 = vdu.connection_points.add()
        c1.name = "c_point1"
        c1.virtual_link_id = virtual_link_id
        c1.type_yang = 'VIRTIO'
        return vdu

    def _get_vdu_modify_request_info(self, vdu_id, virtual_link_id):
        """
        Returns object of type RwcalYang.VDUModifyParams
        """
        vdu = RwcalYang.VDUModifyParams()
        vdu.vdu_id = vdu_id
        c1 = vdu.connection_points_add.add()
        c1.name = "c_modify1"
        c1.virtual_link_id = virtual_link_id
       
        return vdu 
        
    #@unittest.skip("Skipping test_create_delete_virtual_link_and_vdu")
    def test_create_delete_virtual_link_and_vdu(self):
        """
        Test to create VDU
        """
        logger.info("Openstack-CAL-Test: Test Create Virtual Link API")
        vlink_req = self._get_virtual_link_request_info()

        rc, rsp = self.cal.create_virtual_link(self._acct, vlink_req)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Created virtual_link with Id: %s" %rsp)
        vlink_id = rsp
        
        #Check if virtual_link create is successful
        rc, rsp = self.cal.get_virtual_link(self._acct, rsp)
        self.assertEqual(rc, RwStatus.SUCCESS)
        self.assertEqual(rsp.virtual_link_id, vlink_id)

        # Now create VDU
        vdu_req = self._get_vdu_request_info(vlink_id)
        logger.info("Openstack-CAL-Test: Test Create VDU API")

        rc, rsp = self.cal.create_vdu(self._acct, vdu_req)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Created vdu with Id: %s" %rsp)

        vdu_id = rsp

        ## Check if VDU create is successful
        rc, rsp = self.cal.get_vdu(self._acct, rsp)
        self.assertEqual(rsp.vdu_id, vdu_id)

        ### Wait until vdu_state is active
        for i in range(50):
            rc, rs = self.cal.get_vdu(self._acct, vdu_id)
            self.assertEqual(rc, RwStatus.SUCCESS)
            logger.info("Openstack-CAL-Test: VDU with id : %s. Reached State :  %s" %(vdu_id, rs.state))
            if rs.state == 'active':
                break
        rc, rs = self.cal.get_vdu(self._acct, vdu_id)
        self.assertEqual(rc, RwStatus.SUCCESS)
        self.assertEqual(rs.state, 'active')
        logger.info("Openstack-CAL-Test: VDU with id : %s reached expected state  : %s" %(vdu_id, rs.state))
        logger.info("Openstack-CAL-Test: VDUInfo: %s" %(rs))
        
        vlink_req = self._get_virtual_link_request_info()

        ### Create another virtual_link
        rc, rsp = self.cal.create_virtual_link(self._acct, vlink_req)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Created virtual_link with Id: %s" %rsp)
        vlink_id2= rsp

        ### Now exercise the modify_vdu_api
        vdu_modify = self._get_vdu_modify_request_info(vdu_id, vlink_id2)
        rc = self.cal.modify_vdu(self._acct, vdu_modify)
        self.assertEqual(rc, RwStatus.SUCCESS)
        logger.info("Openstack-CAL-Test: Modified vdu with Id: %s" %vdu_id)

        ### Lets delete the VDU
        self.cal.delete_vdu(self._acct, vdu_id)

        ### Lets delete the Virtual Link
        self.cal.delete_virtual_link(self._acct, vlink_id)

        ### Lets delete the Virtual Link-2
        self.cal.delete_virtual_link(self._acct, vlink_id2)

        time.sleep(5)
        ### Verify that VDU and virtual link are successfully deleted
        rc, rsp = self.cal.get_vdu_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)
        for vdu in rsp.vdu_info_list:
            self.assertNotEqual(vdu.vdu_id, vdu_id)

        rc, rsp = self.cal.get_virtual_link_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS)

        for virtual_link in rsp.virtual_link_info_list:
            self.assertNotEqual(virtual_link.virtual_link_id, vlink_id)

        logger.info("Openstack-CAL-Test: VDU/Virtual Link create-delete test successfully completed")


class VmData(object):
    """A convenience class that provides all the stats and EPA Attributes
    from the VM provided
    """
    def __init__(self, host, mgmt_ip):
        """
        Args:
            host (str): host name.
            mgmt_ip (str): The IP of the newly created VM.
        """
        # Sleep for 20s to ensure the VM is UP and ready to run commands
        time.sleep(20)
        logger.info("Connecting to host: {} and IP: {}".format(host, mgmt_ip))
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.WarningPolicy())
        self.client.connect(host)
        self.ip = mgmt_ip

        # Get all data from the newly created VM.
        self._data = self._get_data()
        self._page_size = self._exec_and_clean("getconf PAGE_SIZE")
        self._disk_space = self._exec_and_clean(
                "df -kh --output=size /",
                line_no=1)
        self._pci_data = self._exec('lspci -m | grep "10-Gigabit"')

    def _get_data(self,):
        """Runs the command and store the output in a python dict.

        Returns:
            dict: Containing all key => value pairs.
        """
        content = {}
        cmds = ["lscpu", 'less /proc/meminfo']
        for cmd in cmds:
            ssh_out = self._exec(cmd)
            content.update(self._convert_to_dict(ssh_out))
        return content

    def _exec_and_clean(self, cmd, line_no=0):
        """A convenience method to run a command and extract the specified line
        number.

        Args:
            cmd (str): Command to execute
            line_no (int, optional): Default to 0, extracts the first line.

        Returns:
            str: line_no of the output of the command.
        """
        output = self._exec(cmd)[line_no]
        output = ' '.join(output.split())
        return output.strip()

    def _exec(self, cmd):
        """Thin wrapper that runs the command and returns the stdout data

        Args:
            cmd (str): Command to execute.

        Returns:
            list: Contains the command output.
        """
        _, ssh_out, _ = self.client.exec_command(
                "/usr/rift/bin/ssh_root {} {}".format(self.ip,
                                                      cmd))
        return ssh_out.readlines()

    def _convert_to_dict(self, content):
        """convenience method that cleans and stores the line into dict.
        data is split based on ":" or " ".

        Args:
            content (list): A list containing the stdout.

        Returns:
            dict: containing stat attribute => value.
        """
        flattened = {}
        for line in content:
            line = ' '.join(line.split())
            if ":" in line:
                key, value = line.split(":")
            else:
                key, value = line.split(" ")
            key, value = key.strip(), value.strip()
            flattened[key] = value
        return flattened

    @property
    def disk(self):
        disk = self._disk_space.replace("G", "")
        return int(disk)

    @property
    def numa_node_count(self):
        numa_cores = self._data['NUMA node(s)']
        numa_cores = int(numa_cores)
        return numa_cores

    @property
    def vcpus(self):
        cores = int(self._data['CPU(s)'])
        return cores

    @property
    def cpu_threads(self):
        threads = int(self._data['Thread(s) per core'])
        return threads

    @property
    def memory(self):
        memory = self._data['MemTotal']
        memory = int(memory.replace("kB", ""))/1000/1000
        return int(memory)

    @property
    def memory_page_size(self):
        return self._page_size

    @property
    def pci_passthrough_device_list(self):
        return self._pci_data


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()
