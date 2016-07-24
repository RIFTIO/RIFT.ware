#!/usr/bin/python3

import os
import sys
import uuid
import rw_peas
from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import RwcalYang
from gi.repository.RwTypes import RwStatus
import argparse
import logging
import rwlogger
import boto3
import botocore

persistent_resources = {
    'vms'      : [],
    'networks' : [],
}

MISSION_CONTROL_NAME = 'mission-control'
LAUNCHPAD_NAME = 'launchpad'

RIFT_IMAGE_AMI = 'ami-7070231a'

logging.basicConfig(level=logging.ERROR)
logger = logging.getLogger('rift.cal.awsresources')
logger.setLevel(logging.INFO)

def get_cal_plugin():
    """
        Load AWS cal plugin
    """
    plugin = rw_peas.PeasPlugin('rwcal_aws', 'RwCal-1.0')
    engine, info, extension = plugin()
    cal = plugin.get_interface("Cloud")
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
    try:
        rc = cal.init(rwloggerctx)
        assert rc == RwStatus.SUCCESS
    except Exception as e:
        logger.error("ERROR:Cal plugin instantiation failed with exception %s",repr(e))
    else:
        logger.info("AWS Cal plugin successfully instantiated")
        return cal

def get_cal_account(**kwargs):
    """
    Returns AWS cal account
    """
    account                        = RwcalYang.CloudAccount()
    account.account_type           = "aws"
    account.aws.key = kwargs['key']
    account.aws.secret = kwargs['secret']
    account.aws.region = kwargs['region']
    if 'ssh_key' in kwargs and kwargs['ssh_key'] is not None:
        account.aws.ssh_key = kwargs['ssh_key']
    account.aws.availability_zone = kwargs['availability_zone']
    if 'vpcid' in kwargs and kwargs['vpcid'] is not None: 
        account.aws.vpcid =  kwargs['vpcid']
    if 'default_subnet_id' in kwargs and kwargs['default_subnet_id'] is not None:
        account.aws.default_subnet_id = kwargs['default_subnet_id']
    return account

class AWSResources(object):
    """
    Class with methods to manage AWS resources
    """
    def __init__(self,**kwargs):
        self._cal      = get_cal_plugin()
        self._acct     = get_cal_account(**kwargs)

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
        driver = self._cal._get_driver(self._acct)
        subnets = driver.get_subnet_list()
        subnet_list = [subnet for subnet in subnets if subnet.default_for_az is False]

        logger.info("Deleting Networks : %s" %([x.id for x in subnet_list]))
        for subnet in subnet_list:
            self._cal.delete_virtual_link(self._acct, subnet.subnet_id)
        logger.info("Network cleanup complete")

    def destroy_resource(self):
        """
        Destroy resources
        """
        logger.info("Cleaning up AWS resources")
        self._destroy_vms()
        self._destroy_networks()
        logger.info("Cleaning up AWS resources.......[Done]")

    def _destroy_mission_control(self):
        """
        Destroy Mission Control VM
        """
        logger.info("Initiating MC VM cleanup")
        rc, rsp = self._cal.get_vdu_list(self._acct)
        vdu_list = [vm for vm in rsp.vdu_info_list if vm.name == MISSION_CONTROL_NAME]
        logger.info("Deleting VMs : %s" %([x.name for x in vdu_list]))

        for vdu in vdu_list:
            self._cal.delete_vdu(self._acct, vdu.vdu_id)
        logger.info("MC VM cleanup complete")

    def _destroy_launchpad(self):
        """
        Destroy Launchpad VM
        """
        logger.info("Initiating LP VM cleanup")
        rc, rsp = self._cal.get_vdu_list(self._acct)
        vdu_list = [vm for vm in rsp.vdu_info_list if vm.name == LAUNCHPAD_NAME]
        logger.info("Deleting VMs : %s" %([x.name for x in vdu_list]))

        for vdu in vdu_list:
            self._cal.delete_vdu(self._acct, vdu.vdu_id)
        logger.info("LP VM cleanup complete")
        

    def create_mission_control(self):
        """
        Create Mission Control VM in AWS
        """ 
        logger.info("Creating mission control VM")
        vdu = RwcalYang.VDUInitParams()
        vdu.name = MISSION_CONTROL_NAME
        vdu.image_id = RIFT_IMAGE_AMI
        vdu.flavor_id = 'c3.large'
        vdu.allocate_public_address = True
        vdu.vdu_init.userdata = "#cloud-config\n\nruncmd:\n - echo Sleeping for 5 seconds and attempting to start salt-master\n - sleep 5\n - /bin/systemctl restart salt-master.service\n"

        rc,rs=self._cal.create_vdu(self._acct,vdu)
        assert rc == RwStatus.SUCCESS
        self._mc_id = rs

        driver = self._cal._get_driver(self._acct)
        inst=driver.get_instance(self._mc_id)
        inst.wait_until_running()

        rc,rs =self._cal.get_vdu(self._acct,self._mc_id)
        assert rc == RwStatus.SUCCESS
        self._mc_public_ip = rs.public_ip
        self._mc_private_ip = rs.management_ip
        
        logger.info("Started Mission Control VM with id %s and IP Address %s\n",self._mc_id, self._mc_public_ip)

    def create_launchpad_vm(self, salt_master = None):        
        """
        Create Launchpad VM in AWS
        Arguments
            salt_master (String): String with Salt master IP typically MC VM private IP
        """
        logger.info("Creating launchpad VM")
        USERDATA_FILENAME = os.path.join(os.environ['RIFT_INSTALL'],
                                 'etc/userdata-template')

        try:
            fd = open(USERDATA_FILENAME, 'r')
        except Exception as e:
                sys.exit(-1)
        else:
            LP_USERDATA_FILE = fd.read()
            # Run the enable lab script when the openstack vm comes up
            LP_USERDATA_FILE += "runcmd:\n"
            LP_USERDATA_FILE += " - echo Sleeping for 5 seconds and attempting to start elastic-network-interface\n"
            LP_USERDATA_FILE += " - sleep 5\n"
            LP_USERDATA_FILE += " - /bin/systemctl restart elastic-network-interfaces.service\n"

        if salt_master is None:
            salt_master=self._mc_private_ip
        node_id = str(uuid.uuid4())

        vdu = RwcalYang.VDUInitParams()
        vdu.name = LAUNCHPAD_NAME
        vdu.image_id = RIFT_IMAGE_AMI
        vdu.flavor_id = 'c3.xlarge'
        vdu.allocate_public_address = True
        vdu.vdu_init.userdata = LP_USERDATA_FILE.format(master_ip = salt_master,
                                          lxcname = node_id)
        vdu.node_id = node_id

        rc,rs=self._cal.create_vdu(self._acct,vdu)
        assert rc == RwStatus.SUCCESS
        self._lp_id = rs

        driver = self._cal._get_driver(self._acct)
        inst=driver.get_instance(self._lp_id)
        inst.wait_until_running()

        rc,rs =self._cal.get_vdu(self._acct,self._lp_id)
        assert rc == RwStatus.SUCCESS

        self._lp_public_ip = rs.public_ip
        self._lp_private_ip = rs.management_ip
        logger.info("Started Launchpad VM with id %s and IP Address %s\n",self._lp_id, self._lp_public_ip)
         
    def upload_ssh_key_to_ec2(self):
        """
         Upload SSH key to EC2 region
        """
        driver = self._cal._get_driver(self._acct)
        key_name = os.getlogin() + '-' + 'sshkey' 
        key_path = '%s/.ssh/id_rsa.pub' % (os.environ['HOME'])
        if os.path.isfile(key_path):
            logger.info("Uploading ssh public key file in path %s with keypair name %s", key_path,key_name)
            with open(key_path) as fp:
                driver.upload_ssh_key(key_name,fp.read())
        else:
            logger.error("Valid Public key file %s not found", key_path)


def main():
    """
    Main routine
    """
    parser = argparse.ArgumentParser(description='Script to manage AWS resources')

    parser.add_argument('--aws-key',
                        action = 'store',
                        dest = 'aws_key',
                        type = str,
                        help='AWS key')

    parser.add_argument('--aws-secret',
                        action = 'store',
                        dest = 'aws_secret',
                        type = str,
                        help='AWS secret')

    parser.add_argument('--aws-region',
                        action = 'store',
                        dest = 'aws_region',
                        type = str,
                        help='AWS region')

    parser.add_argument('--aws-az',
                        action = 'store',
                        dest = 'aws_az',
                        type = str,
                        help='AWS Availability zone')

    parser.add_argument('--aws-sshkey',
                        action = 'store',
                        dest = 'aws_sshkey',
                        type = str,
                        help='AWS SSH Key to login to instance')

    parser.add_argument('--aws-vpcid',
                        action = 'store',
                        dest = 'aws_vpcid',
                        type = str,
                        help='AWS VPC ID to use to indicate non default VPC')

    parser.add_argument('--aws-default-subnet',
                        action = 'store',
                        dest = 'aws_default_subnet',
                        type = str,
                        help='AWS Default subnet id in VPC to be used for mgmt network')

    parser.add_argument('--mission-control',
                        action = 'store_true',
                        dest = 'mission_control',
                        help='Create Mission Control VM')

    parser.add_argument('--launchpad',
                        action = 'store_true',
                        dest = 'launchpad',
                        help='Create LaunchPad VM')

    parser.add_argument('--salt-master',
                        action = 'store',
                        dest = 'salt_master',
                        type = str,
                        help='IP Address of salt controller. Required, if only launchpad  VM is being created.')

    parser.add_argument('--cleanup',
                        action = 'store',
                        dest = 'cleanup',
                        nargs = '+',
                        type = str,
                        help = 'Perform resource cleanup for AWS installation. \n Possible options are {all, mc, lp,  vms, networks }')

    parser.add_argument('--upload-ssh-key',
                         action = 'store_true',
                         dest = 'upload_ssh_key',
                         help = 'Upload users SSH public key ~/.ssh/id_rsa.pub')  

    argument = parser.parse_args()

    if (argument.aws_key is None or argument.aws_secret is None or argument.aws_region is None or
       argument.aws_az is None):
        logger.error("Missing mandatory params. AWS Key, Secret, Region, AZ and SSH key are mandatory params")
        sys.exit(-1)

    if (argument.cleanup is None and argument.mission_control is None and argument.launchpad is None 
        and argument.upload_ssh_key is None):
        logger.error('Insufficient parameters')
        sys.exit(-1)

    ### Start processing
    logger.info("Instantiating cloud-abstraction-layer")
    drv = AWSResources(key=argument.aws_key, secret=argument.aws_secret, region=argument.aws_region, availability_zone = argument.aws_az, 
                       ssh_key = argument.aws_sshkey, vpcid = argument.aws_vpcid, default_subnet_id = argument.aws_default_subnet)
    logger.info("Instantiating cloud-abstraction-layer.......[Done]")

    if argument.upload_ssh_key:
         drv.upload_ssh_key_to_ec2()

    if argument.cleanup is not None:
        for r_type in argument.cleanup:
            if r_type == 'all':
                drv.destroy_resource()
                break
            if r_type == 'vms':
                drv._destroy_vms()
            if r_type == 'networks':
                drv._destroy_networks()
            if r_type == 'mc':
                drv._destroy_mission_control()
            if r_type == 'lp':
                drv._destroy_launchpad()

    if argument.mission_control == True:
        drv.create_mission_control()

    if argument.launchpad == True:
        if argument.salt_master is None and argument.mission_control is False:
            logger.error('Salt Master IP address not provided to start Launchpad.')
            sys.exit(-2)

        drv.create_launchpad_vm(argument.salt_master)

if __name__ == '__main__':
    main()
