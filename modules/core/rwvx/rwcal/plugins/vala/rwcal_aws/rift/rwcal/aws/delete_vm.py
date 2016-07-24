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

        
def cleanup_vm(drv,argument):
    vm_inst = drv.get_instance(argument.server_id)
    logger.info("Waiting for VM instance to get to terminating state")
    vm_inst.wait_until_terminated()
    logger.info("VM inst is now in terminating state") 

    for port_id in argument.vdu_port_list:
        logger.info("Deleting network interface with id %s",port_id)
        port = drv.get_network_interface(port_id)
        if port:
            if port.association and 'AssociationId' in port.association:
                drv.disassociate_public_ip_from_network_interface(NetworkInterfaceId=port.id)
            drv.delete_network_interface(port.id)
        else:
            logger.error("Newtork interface with id %s not found when deleting interface",port_id)
    

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
                        help = "Server ID on which delete operations needs to be performed")
    
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
    cleanup_vm(drv, argument)
    sys.exit(0)
    
if __name__ == "__main__":
    main()
        

