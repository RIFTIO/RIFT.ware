#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import rift.rwcal.openstack as openstack_drv
import logging
import argparse
import sys, os, time
import rwlogger

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger()

rwlog_handler = rwlogger.RwLogger(category="rw-cal-log",
                                  subcategory="openstack",)
logger.addHandler(rwlog_handler)
#logger.setLevel(logging.DEBUG)


def assign_floating_ip_address(drv, argument):
    if not argument.floating_ip:
        return

    server = drv.nova_server_get(argument.server_id)
    logger.info("Assigning the floating_ip: %s to VM: %s" %(argument.floating_ip, server['name']))
    
    for i in range(120):
        server = drv.nova_server_get(argument.server_id)
        for network_name,network_info in server['addresses'].items():
            if network_info:
                if network_name == argument.mgmt_network:
                    for n_info in network_info:
                        if 'OS-EXT-IPS:type' in n_info and n_info['OS-EXT-IPS:type'] == 'fixed':
                            management_ip = n_info['addr']
                            drv.nova_floating_ip_assign(argument.server_id,
                                                        argument.floating_ip,
                                                        management_ip)
                            logger.info("Assigned floating_ip: %s to management_ip: %s" %(argument.floating_ip, management_ip))
                        return
        logger.info("Waiting for management_ip to be assigned to server: %s" %(server['name']))
        time.sleep(1)
    else:
        logger.info("No management_ip IP available to associate floating_ip for server: %s" %(server['name']))
    return


def create_port_metadata(drv, argument):
    if argument.port_metadata == False:
        return

    ### Get Management Network ID
    network_list = drv.neutron_network_list()
    mgmt_network_id = [net['id'] for net in network_list if net['name'] == argument.mgmt_network][0]
    port_list = [ port for port in drv.neutron_port_list(**{'device_id': argument.server_id})
                  if port['network_id'] != mgmt_network_id ]
    meta_data = {}

    meta_data['rift-meta-ports'] = str(len(port_list))
    port_id = 0
    for port in port_list:
        info = []
        info.append('"port_name":"'+port['name']+'"')
        if 'mac_address' in port:
            info.append('"hw_addr":"'+port['mac_address']+'"')
        if 'network_id' in port:
            #info.append('"network_id":"'+port['network_id']+'"')
            net_name = [net['name'] for net in network_list if net['id'] == port['network_id']]
            if net_name:
                info.append('"network_name":"'+net_name[0]+'"')
        if 'fixed_ips' in port:
            ip_address = port['fixed_ips'][0]['ip_address']
            info.append('"ip":"'+ip_address+'"')
            
        meta_data['rift-meta-port-'+str(port_id)] = '{' + ','.join(info) + '}'
        port_id += 1
        
    nvconn = drv.nova_drv._get_nova_connection()
    nvconn.servers.set_meta(argument.server_id, meta_data)
    
        
def prepare_vm_after_boot(drv,argument):
    ### Important to call create_port_metadata before assign_floating_ip_address
    ### since assign_floating_ip_address can wait thus delaying port_metadata creation

    ### Wait for 2 minute for server to come up -- Needs fine tuning
    wait_time = 120 
    sleep_time = 1
    for i in range(int(wait_time/sleep_time)):
        server = drv.nova_server_get(argument.server_id)
        if server['status'] == 'ACTIVE':
            logger.info("Server %s to reached active state" %(server['name']))
            break
        elif server['status'] == 'BUILD':
            logger.info("Waiting for server: %s to build. Current state: %s" %(server['name'], server['status']))
            time.sleep(sleep_time)
        else:
            logger.info("Server %s reached state: %s" %(server['name'], server['status']))
            sys.exit(3)
    else:
        logger.error("Server %s did not reach active state in %d seconds. Current state: %s" %(server['name'], wait_time, server['status']))
        sys.exit(4)
    
    #create_port_metadata(drv, argument)
    assign_floating_ip_address(drv, argument)
    

def main():
    """
    Main routine
    """
    parser = argparse.ArgumentParser(description='Script to create openstack resources')
    parser.add_argument('--auth_url',
                        action = "store",
                        dest = "auth_url",
                        type = str,
                        help='Keystone Auth URL')

    parser.add_argument('--username',
                        action = "store",
                        dest = "username",
                        type = str,
                        help = "Username for openstack installation")

    parser.add_argument('--password',
                        action = "store",
                        dest = "password",
                        type = str,
                        help = "Password for openstack installation")

    parser.add_argument('--tenant_name',
                        action = "store",
                        dest = "tenant_name",
                        type = str,
                        help = "Tenant name openstack installation")

    parser.add_argument('--mgmt_network',
                        action = "store",
                        dest = "mgmt_network",
                        type = str,
                        help = "mgmt_network")
    
    parser.add_argument('--server_id',
                        action = "store",
                        dest = "server_id",
                        type = str,
                        help = "Server ID on which boot operations needs to be performed")
    
    parser.add_argument('--floating_ip',
                        action = "store",
                        dest = "floating_ip",
                        type = str,
                        help = "Floating IP to be assigned")

    parser.add_argument('--port_metadata',
                        action = "store_true",
                        dest = "port_metadata",
                        default = False,
                        help = "Create Port Metadata")

    argument = parser.parse_args()

    if not argument.auth_url:
        logger.error("ERROR: AuthURL is not configured")
        sys.exit(1)
    else:
        logger.info("Using AuthURL: %s" %(argument.auth_url))

    if not argument.username:
        logger.error("ERROR: Username is not configured")
        sys.exit(1)
    else:
        logger.info("Using Username: %s" %(argument.username))

    if not argument.password:
        logger.error("ERROR: Password is not configured")
        sys.exit(1)
    else:
        logger.info("Using Password: %s" %(argument.password))

    if not argument.tenant_name:
        logger.error("ERROR: Tenant Name is not configured")
        sys.exit(1)
    else:
        logger.info("Using Tenant Name: %s" %(argument.tenant_name))

    if not argument.mgmt_network:
        logger.error("ERROR: Management Network Name is not configured")
        sys.exit(1)
    else:
        logger.info("Using Management Network: %s" %(argument.mgmt_network))
        
    if not argument.server_id:
        logger.error("ERROR: Server ID is not configured")
        sys.exit(1)
    else:
        logger.info("Using Server ID : %s" %(argument.server_id))
        
        
    try:
        pid = os.fork()
        if pid > 0:
            # exit for parent
            sys.exit(0)
    except OSError as e:
        logger.error("fork failed: %d (%s)\n" % (e.errno, e.strerror))
        sys.exit(2)
        
    drv = openstack_drv.OpenstackDriver(username = argument.username,
                                        password = argument.password,
                                        auth_url = argument.auth_url,
                                        tenant_name = argument.tenant_name,
                                        mgmt_network = argument.mgmt_network)
    prepare_vm_after_boot(drv, argument)
    sys.exit(0)
    
if __name__ == "__main__":
    main()
        

