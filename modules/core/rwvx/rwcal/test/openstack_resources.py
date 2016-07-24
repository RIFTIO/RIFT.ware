#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import RwcalYang
from gi.repository.RwTypes import RwStatus
import logging
import rw_peas
import rwlogger
import time
import argparse
import os
import sys
import uuid
from os.path import basename

FLAVOR_NAME = 'm1.medium'
DEFAULT_IMAGE='/net/sharedfiles/home1/common/vm/rift-root-latest.qcow2'

persistent_resources = {
    'vms'      : ['mission_control','launchpad',],
    'networks' : ['public', 'private', 'multisite'],
    'flavors'  : ['m1.tiny', 'm1.small', 'm1.medium', 'm1.large', 'm1.xlarge'],
    'images'   : ['rwimage','rift-root-latest.qcow2','rift-root-latest-trafgen.qcow2', 'rift-root-latest-trafgen-f.qcow2']
}

#
# Important information about openstack installation. This needs to be manually verified 
#
openstack_info = {
    'username'           : 'pluto',
    'password'           : 'mypasswd',
    'project_name'       : 'demo',
    'mgmt_network'       : 'private',
    'physical_network'   : 'physnet1',
    'network_type'       : 'VLAN',
    'segmentation_id'    : 42, ### What else?
    'subnets'            : ["11.0.0.0/24", "12.0.0.0/24", "13.0.0.0/24", "14.0.0.0/24"],
    'subnet_index'       : 0,
    }


logging.basicConfig(level=logging.INFO)

USERDATA_FILENAME = os.path.join(os.environ['RIFT_INSTALL'],
                                 'etc/userdata-template')


RIFT_BASE_USERDATA = '''
#cloud-config
runcmd:
 - sleep 5
 - /usr/rift/scripts/cloud/enable_lab
 - /usr/rift/etc/fix_this_vm
'''

try:
    fd = open(USERDATA_FILENAME, 'r')
except Exception as e:
    #logger.error("Received exception during opening of userdata (%s) file. Exception: %s" %(USERDATA_FILENAME, str(e)))
    sys.exit(-1)
else:
    LP_USERDATA_FILE = fd.read()
    # Run the enable lab script when the openstack vm comes up
    LP_USERDATA_FILE += "runcmd:\n"
    LP_USERDATA_FILE += " - /usr/rift/scripts/cloud/enable_lab\n"
    LP_USERDATA_FILE += " - /usr/rift/etc/fix_this_vm\n"



def get_cal_plugin():
    """
    Loads rw.cal plugin via libpeas
    """
    plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
    engine, info, extension = plugin()
    cal = plugin.get_interface("Cloud")
    # Get the RwLogger context
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
    try:
        rc = cal.init(rwloggerctx)
        assert rc == RwStatus.SUCCESS
    except:
        logger.error("ERROR:Cal plugin instantiation failed. Aborting tests")
    else:
        logger.info("Openstack Cal plugin successfully instantiated")
        return cal 
    
def get_cal_account(auth_url):
    """
    Returns cal account
    """
    account                        = RwcalYang.CloudAccount()
    account.account_type           = "openstack"
    account.openstack.key          = openstack_info['username']
    account.openstack.secret       = openstack_info['password']
    account.openstack.auth_url     = auth_url
    account.openstack.tenant       = openstack_info['project_name']
    account.openstack.mgmt_network = openstack_info['mgmt_network']
    return account


logger = logging.getLogger('rift.cal.openstackresources')

class OpenstackResources(object):
    """
    A stupid class to manage bunch of openstack resources
    """
    def __init__(self, controller):    
        self._cal      = get_cal_plugin()
        self._acct     = get_cal_account('http://'+controller+':5000/v3/')
        self._id       = 0
        self._image_id = None
        self._flavor_id = None
        
    def _destroy_vms(self):
        """
        Destroy VMs
        """
        logger.info("Initiating VM cleanup")
        rc, rsp = self._cal.get_vdu_list(self._acct)
        vdu_list = [vm for vm in rsp.vdu_info_list if vm.name not in persistent_resources['vms']]
        logger.info("Deleting VMs : %s" %([x.name for x in vdu_list]))
        
        for vdu in vdu_list:
            self._cal.delete_vdu(self._acct, vdu.vdu_id)

        logger.info("VM cleanup complete")

    def _destroy_networks(self):
        """
        Destroy Networks
        """
        logger.info("Initiating Network cleanup")
        rc, rsp = self._cal.get_virtual_link_list(self._acct)
        vlink_list = [vlink for vlink in rsp.virtual_link_info_list if vlink.name not in persistent_resources['networks']]

        logger.info("Deleting Networks : %s" %([x.name for x in vlink_list]))
        for vlink in vlink_list:
            self._cal.delete_virtual_link(self._acct, vlink.virtual_link_id)
        logger.info("Network cleanup complete")

    def _destroy_flavors(self):
        """
        Destroy Flavors
        """
        logger.info("Initiating flavor cleanup")
        rc, rsp = self._cal.get_flavor_list(self._acct)
        flavor_list = [flavor for flavor in rsp.flavorinfo_list if flavor.name not in persistent_resources['flavors']]
            
        logger.info("Deleting flavors : %s" %([x.name for x in flavor_list]))

        for flavor in flavor_list:
            self._cal.delete_flavor(self._acct, flavor.id)
            
        logger.info("Flavor cleanup complete")

    def _destroy_images(self):
        logger.info("Initiating image cleanup")
        rc, rsp = self._cal.get_image_list(self._acct)
        image_list = [image for image in rsp.imageinfo_list if image.name not in persistent_resources['images']]

        logger.info("Deleting images : %s" %([x.name for x in image_list]))
            
        for image in image_list:
            self._cal.delete_image(self._acct, image.id)
            
        logger.info("Image cleanup complete")
        
    def destroy_resource(self):
        """
        Destroy resources
        """
        logger.info("Cleaning up openstack resources")
        self._destroy_vms()
        self._destroy_networks()
        self._destroy_flavors()
        self._destroy_images()
        logger.info("Cleaning up openstack resources.......[Done]")

    def create_mission_control(self):
        vm_id = self.create_vm('mission_control',
                               userdata = RIFT_BASE_USERDATA)
        return vm_id
    

    def create_launchpad_vm(self, salt_master=None):
        node_id = str(uuid.uuid4())
        if salt_master is not None:
           userdata = LP_USERDATA_FILE.format(master_ip = salt_master,
                                           lxcname = node_id)
        else:
           userdata = RIFT_BASE_USERDATA

        vm_id = self.create_vm('launchpad',
                              userdata = userdata,
                              node_id = node_id)
#        vm_id = self.create_vm('launchpad2',
#                               userdata = userdata,
#                               node_id = node_id)
        return vm_id
    
    def create_vm(self, name, userdata, node_id = None):
        """
        Creates a VM. The VM name is derived from username

        """
        vm = RwcalYang.VDUInitParams()
        vm.name = name
        vm.flavor_id = self._flavor_id
        vm.image_id  = self._image_id
        if node_id is not None:
            vm.node_id = node_id
        vm.vdu_init.userdata = userdata
        vm.allocate_public_address = True
        logger.info("Starting a VM with parameter: %s" %(vm))
     
        rc, vm_id = self._cal.create_vdu(self._acct, vm)
        assert rc == RwStatus.SUCCESS
        logger.info('Created vm: %s with id: %s', name, vm_id)
        return vm_id
        
    def create_network(self, name):
        logger.info("Creating network with name: %s" %name)
        network                = RwcalYang.NetworkInfoItem()
        network.network_name   = name
        network.subnet         = openstack_info['subnets'][openstack_info['subnet_index']]

        if openstack_info['subnet_index'] == len(openstack_info['subnets']):
            openstack_info['subnet_index'] = 0
        else:
            openstack_info['subnet_index'] += 1
        
        if openstack_info['physical_network']:
            network.provider_network.physical_network = openstack_info['physical_network']
        if openstack_info['network_type']:
            network.provider_network.overlay_type     = openstack_info['network_type']
        if openstack_info['segmentation_id']:
            network.provider_network.segmentation_id  = openstack_info['segmentation_id']
            openstack_info['segmentation_id'] += 1

        rc, net_id = self._cal.create_network(self._acct, network)
        assert rc == RwStatus.SUCCESS

        logger.info("Successfully created network with id: %s" %net_id)
        return net_id
    
        

    def create_image(self, location):
        img = RwcalYang.ImageInfoItem()
        img.name = basename(location)
        img.location = location
        img.disk_format = "qcow2"
        img.container_format = "bare"

        logger.info("Uploading image : %s" %img.name)
        rc, img_id = self._cal.create_image(self._acct, img)
        assert rc == RwStatus.SUCCESS

        rs = None
        rc = None
        image = None
        for i in range(100):
            rc, rs = self._cal.get_image(self._acct, img_id)
            assert rc == RwStatus.SUCCESS
            logger.info("Image (image_id: %s) reached status : %s" %(img_id, rs.state))
            if rs.state == 'active':
                image = rs
                break
            else:
                time.sleep(2) # Sleep for a second

        if image is None:
            logger.error("Failed to upload openstack image: %s", img)
            sys.exit(1)

        self._image_id = img_id
        logger.info("Uploading image.......[Done]")
        
    def create_flavor(self):
        """
        Create Flavor suitable for rift_ping_pong VNF
        """
        flavor = RwcalYang.FlavorInfoItem()
        flavor.name = FLAVOR_NAME
        flavor.vm_flavor.memory_mb   = 16384 # 16GB
        flavor.vm_flavor.vcpu_count  = 4 
        flavor.vm_flavor.storage_gb  = 20 # 20 GB

        logger.info("Creating new flavor. Flavor Info: %s" %str(flavor.vm_flavor))

        rc, flavor_id = self._cal.create_flavor(self._acct, flavor)
        assert rc == RwStatus.SUCCESS
        logger.info("Creating new flavor.......[Done]")
        return flavor_id

    def find_image(self, name):
        logger.info("Searching for uploaded image: %s" %name)
        rc, rsp = self._cal.get_image_list(self._acct)
        image_list = [image for image in rsp.imageinfo_list if image.name ==  name]

        if not image_list:
            logger.error("Image %s not found" %name)
            return None

        self._image_id = image_list[0].id
        logger.info("Searching for uploaded image.......[Done]")
        return self._image_id

    def find_flavor(self, name=FLAVOR_NAME):
        logger.info("Searching for required flavor: %s" %name)
        rc, rsp = self._cal.get_flavor_list(self._acct)
        flavor_list = [flavor for flavor in rsp.flavorinfo_list if flavor.name == name]

        if not flavor_list:
            logger.error("Flavor %s not found" %name)
            self._flavor_id = self.create_flavor()
        else:
            self._flavor_id = flavor_list[0].id

        logger.info("Searching for required flavor.......[Done]")
        return self._flavor_id

        
    

def main():
    """
    Main routine
    """
    parser = argparse.ArgumentParser(description='Script to manage openstack resources')
    
    parser.add_argument('--controller',
                        action = 'store',
                        dest = 'controller',
                        type = str,
                        help='IP Address of openstack controller. This is mandatory parameter')

    parser.add_argument('--cleanup',
                        action = 'store',
                        dest = 'cleanup',
                        nargs = '+',
                        type = str,
                        help = 'Perform resource cleanup for openstack installation. \n Possible options are {all, flavors, vms, networks, images}')

    parser.add_argument('--persist-vms',
                        action = 'store',
                        dest = 'persist_vms',
                        help = 'VM instance name to persist')

    parser.add_argument('--salt-master',
                        action = 'store',
                        dest = 'salt_master',
                        type = str,
                        help='IP Address of salt controller. Required, if VMs are being created.')

    parser.add_argument('--upload-image',
                        action = 'store',
                        dest = 'upload_image',
                        help='Openstack image location to upload and use when creating vms.x')

    parser.add_argument('--use-image',
                        action = 'store',
                        dest = 'use_image',
                        help='Image name to be used for VM creation')

    parser.add_argument('--use-flavor',
                        action = 'store',
                        dest = 'use_flavor',
                        help='Flavor name to be used for VM creation')
    
    parser.add_argument('--mission-control',
                        action = 'store_true',
                        dest = 'mission_control',
                        help='Create Mission Control VM')


    parser.add_argument('--launchpad',
                        action = 'store_true',
                        dest = 'launchpad',
                        help='Create LaunchPad VM')

    parser.add_argument('--use-project',
                        action = 'store',
                        dest = 'use_project',
                        help='Project name to be used for VM creation')

    parser.add_argument('--clean-mclp',
                        action='store_true',
                        dest='clean_mclp',
                        help='Remove Mission Control and Launchpad VMs')

    argument = parser.parse_args()

    if argument.persist_vms is not None:
        global persistent_resources
        vm_name_list = argument.persist_vms.split(',')
        for single_vm in vm_name_list:
                persistent_resources['vms'].append(single_vm)
        logger.info("persist-vms: %s" % persistent_resources['vms'])

    if argument.clean_mclp:
        persistent_resources['vms'] = []

    if argument.controller is None:
        logger.error('Need openstack controller IP address')
        sys.exit(-1)

    
    if argument.use_project is not None:
        openstack_info['project_name'] = argument.use_project

    ### Start processing
    logger.info("Instantiating cloud-abstraction-layer")
    drv = OpenstackResources(argument.controller)
    logger.info("Instantiating cloud-abstraction-layer.......[Done]")

        
    if argument.cleanup is not None:
        for r_type in argument.cleanup:
            if r_type == 'all':
                drv.destroy_resource()
                break
            if r_type == 'images':
                drv._destroy_images()
            if r_type == 'flavors':
                drv._destroy_flavors()
            if r_type == 'vms':
                drv._destroy_vms()
            if r_type == 'networks':
                drv._destroy_networks()

    if argument.upload_image is not None:
        image_name_list = argument.upload_image.split(',')
        logger.info("Will upload %d image(s): %s" % (len(image_name_list), image_name_list))
        for image_name in image_name_list:
            drv.create_image(image_name)
            #print("Uploaded :", image_name)

    elif argument.use_image is not None:
        img = drv.find_image(argument.use_image)
        if img == None:
            logger.error("Image: %s not found" %(argument.use_image))
            sys.exit(-4)
    else:
        if argument.mission_control or argument.launchpad:
            img = drv.find_image(basename(DEFAULT_IMAGE))
            if img == None:
                drv.create_image(DEFAULT_IMAGE)

    if argument.use_flavor is not None:
        drv.find_flavor(argument.use_flavor)
    else:
        drv.find_flavor()
        
    if argument.mission_control == True:
        drv.create_mission_control()

    if argument.launchpad == True:
        drv.create_launchpad_vm(salt_master = argument.salt_master)
        
    
if __name__ == '__main__':
    main()
        
