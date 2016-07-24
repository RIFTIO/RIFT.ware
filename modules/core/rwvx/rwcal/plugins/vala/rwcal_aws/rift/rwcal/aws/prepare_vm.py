#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import rift.rwcal.aws as aws_drv
import logging
import argparse
import rwlogger
import sys, os, time

logging.basicConfig(level=logging.DEBUG)

logger = logging.getLogger()
rwlog_handler = rwlogger.RwLogger(category="rw-cal-log",
                                  subcategory="aws",)
logger.addHandler(rwlog_handler)
#logger.setLevel(logging.DEBUG)


        
def prepare_vm_after_boot(drv,argument):
    vm_inst = drv.get_instance(argument.server_id)
    logger.info("Waiting for VM instance to get to running state")
    vm_inst.wait_until_running()
    logger.info("VM instance is now in running state") 
    if argument.vdu_name:
        vm_inst.create_tags(Tags=[{'Key': 'Name','Value':argument.vdu_name}])
    if argument.vdu_node_id is not None:
        vm_inst.create_tags(Tags=[{'Key':'node_id','Value':argument.vdu_node_id}])    
    
    for index,port_id in enumerate(argument.vdu_port_list):
        logger.info("Attaching network interface with id %s to VDU instance %s",port_id,vm_inst.id)
        drv.attach_network_interface(NetworkInterfaceId = port_id,InstanceId = vm_inst.id,DeviceIndex=index+1)
    

def main():
    """
    Main routine
    """
    parser = argparse.ArgumentParser(description='Script to create AWS resources')
    parser.add_argument('--aws_key',
                        action = "store",
                        dest = "aws_key",
                        type = str,
                        help='AWS Key')

    parser.add_argument('--aws_secret',
                        action = "store",
                        dest = "aws_secret",
                        type = str,
                        help = "AWS Secret")

    parser.add_argument('--aws_region',
                        action = "store",
                        dest = "aws_region",
                        type = str,
                        help = "AWS Region")

    parser.add_argument('--server_id',
                        action = "store",
                        dest = "server_id",
                        type = str,
                        help = "Server ID on which boot operations needs to be performed")
    
    parser.add_argument('--vdu_name',
                        action = "store",
                        dest = "vdu_name",
                        type = str,
                        help = "VDU name")

    parser.add_argument('--vdu_node_id',
                        action = "store",
                        dest = "vdu_node_id",
                        help = "Node id for vdu")

    parser.add_argument('--vdu_port_list',
                        action = "append",
                        dest = "vdu_port_list",
                        default = [],
                        help = "Port id list for vdu")

    argument = parser.parse_args()

    if not argument.aws_key:
        logger.error("ERROR: AWS key is not configured")
        sys.exit(1)
    else:
        logger.debug("Using AWS key: %s" %(argument.aws_key))

    if not argument.aws_secret:
        logger.error("ERROR: AWS Secret is not configured")
        sys.exit(1)
    else:
        logger.debug("Using AWS Secret: %s" %(argument.aws_secret))

    if not argument.aws_region:
        logger.error("ERROR: AWS Region is not configured")
        sys.exit(1)
    else:
        logger.debug("Using AWS Region: %s" %(argument.aws_region))

    if not argument.server_id:
        logger.error("ERROR: Server ID is not configured")
        sys.exit(1)
    else:
        logger.debug("Using Server ID : %s" %(argument.server_id))
        
    try:
        pid = os.fork()
        if pid > 0:
            # exit for parent
            sys.exit(0)
    except OSError as e:
        logger.error("fork failed: %d (%s)\n" % (e.errno, e.strerror))
        sys.exit(2)
        
    drv = aws_drv.AWSDriver(key = argument.aws_key,
                            secret  = argument.aws_secret,
                            region  = argument.aws_region)
    prepare_vm_after_boot(drv, argument)
    sys.exit(0)
    
if __name__ == "__main__":
    main()
        

