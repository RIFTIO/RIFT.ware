# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# 

import os,sys,platform
import socket
import time
import re
import logging

from pprint import pprint
import argparse

from gi.repository import RwcalYang
from gi.repository.RwTypes import RwStatus
import rw_peas
import rwlogger
import time

global nova
nova = None

def wait_till_active(driver, account, vm_id_list, timeout):                                                                                                              
    """
    Wait until VM reaches ACTIVE state. 
    """
    # Wait while VM goes to required state

    start = time.time()
    end = time.time() + timeout
    done = False;

    while ( time.time() < end ) and not done:
       done = True      
       for vm_id in vm_id_list:
           rc, rs = driver.get_vm(account, vm_id)
           assert rc == RwStatus.SUCCESS
           if rs.state != 'ACTIVE':
               done = False		   
               time.sleep(2)


def get_image_name(node):
    images = driver.list_images()
    for i in images:
        if i.id == node.extra['imageId']:
            return i.name
    return None

def get_flavor_name(flavorid):
    global nova
    if nova is None:
        nova = ra_nova_connect(project='admin')
    for f in nova.flavors.list(True):
         if f.id == flavorid: 
             return f.name
    return None

def hostname():
    return socket.gethostname().split('.')[0]

def vm_register(id, driver, account, cmdargs, header=True):
    if testbed is None:
        print("Cannot register VM without reservation system")
        return False

    if cmdargs.reserve_new_vms:
        user=os.environ['USER']
    else:
        user=None
    fmt="%-28s %-12s %-12s %-15s"
    if header:
        print('VM                           controller   compute      mgmt ip')
        print('---------------------------- ------------ ------------ ---------------')
    rc, nodes = driver.get_vm_list(account)
    assert rc == RwStatus.SUCCESS
    for node in nodes.vminfo_list:
        if id == 'all' or node.vm_id == id:
            flavor = driver.get_flavor(account, node.flavor_id)
            assert rc == RwStatus.SUCCESS
            ip = node.management_ip
            
            huge = 'DISABLED'	    
            if flavor.guest_epa.mempage_size == 'LARGE':
                huge = flavor.guest_epa.mempage_size							    	    
            #compute = utils.find_resource(nova.servers, node.id)
            #compute_name = compute._info['OS-EXT-SRV-ATTR:hypervisor_hostname'].split('.')[0]
            compute_name = hostname()	    
            try:
                testbed.add_resource(node.vm_name, hostname(), ip, flavor.vm_flavor.memory_mb, flavor.vm_flavor.vcpu_count, user, flavor.name, compute=compute_name, huge_pages=huge )
                print(fmt % ( node.vm_name, hostname(), compute_name, ip )) 
            except Exception as e:
                print("WARNING: Error \"%s\"adding resource to reservation system" % e)

class OFromDict(object):
  def __init__(self, d):
    self.__dict__ = d


def vm_create_subcommand(driver, account, cmdargs):
    """Process the VM create subcommand."""
    if cmdargs.name and cmdargs.count != 1:
        sys.exit("Error: when VM name is specified, the count must be 1")

    rc, sizes = driver.get_flavor_list(account)
    assert rc == RwStatus.SUCCESS

    try:
        size = [s for s in sizes.flavorinfo_list if s.name == cmdargs.flavor][0]
    except IndexError:
        sys.exit("Error: Failed to create VM, couldn't find flavor %s" % \
                 cmdargs.flavor)
    print(size)
    rc, images = driver.get_image_list(account)
    assert rc == RwStatus.SUCCESS
    if images is None:
    	sys.exit("Error: No images found")
    try:
        image = [i for i in images.imageinfo_list if cmdargs.image in i.name][0]
    except IndexError:
        sys.exit("Error: Failed to create VM, couldn't find image %s" % \
                 cmdargs.image)
    print(image)

    # VM name is not specified, so determine a unique VM name
    # VM name should have the following format:
    #     rwopenstack_<host>_vm<id>, e.g., rwopenstack_grunt16_vm1
    # The following code gets the list of existing VMs and determines
    # a unique id for the VM name construction.
    rc, nodes = driver.get_vm_list(account)
    assert rc == RwStatus.SUCCESS
    prefix = 'rwopenstack_%s_vm' % hostname()
    vmid = 0;
    for n in nodes.vminfo_list:
        if n.vm_name.startswith(prefix):
            temp_str = n.vm_name[len(prefix):]
            if temp_str == '':
                temp = 1
            else:
                temp = int(n.vm_name[len(prefix):])

            if (temp > vmid):
                vmid = temp

    nodelist = []
    for i in range(0, cmdargs.count):
            if cmdargs.name:
                vm_name = cmdargs.name
            else:
                vm_name = '%s%d' % (prefix, vmid+i+1)
 
            rc, netlist = driver.get_network_list(account)
            assert rc == RwStatus.SUCCESS	
            for network in netlist.networkinfo_list:
                 print(network)    

            vm = RwcalYang.VMInfoItem()
            vm.vm_name = vm_name
            vm.flavor_id = size.id
            vm.image_id  = image.id
            vm.cloud_init.userdata = ''

            nets = dict()
            for network in netlist.networkinfo_list:
                if network.network_name != "public":
                    nwitem = RwcalYang.VMInfoItem_NetworkList()			
                    nwitem.network_id = network.network_id		    
                    nets[network.network_name] = nwitem
                     
            logger.debug('creating VM using nets %s' % cmdargs.networks )
            for net in cmdargs.networks.split(','):
                if not net in nets:
                    print(("Invalid network name '%s'" % net))
                    print(('available nets are %s' % ','.join(list(nets.keys())) ))
                    sys.exit(1)
                if net != cmdargs.mgmt_network:
                    vm.network_list.append(nets[net])

            print(vm.network_list)
            rc, node_id = driver.create_vm(account, vm) 

            # wait for 1 to be up before starting the rest
            # this is an attempt to make sure the image is cached
            nodelist.append(node_id)
            if i == 0 or cmdargs.wait_after_create is True:
                #wait_until_running([node], timeout=300)
                wait_till_active(driver, account, nodelist, timeout=300)		
            print(node_id)
    if cmdargs.reservation_server_url is not None:
            if not cmdargs.wait_after_create:
                print("Waiting for VMs to start")
                wait_till_active(driver, account, nodelist, timeout=300)		
                print("VMs are up")
            header=True
            for node in nodelist:
                vm_register(node, driver, account, cmdargs, header)
                header=False
                

def vm_destroy_subcommand(driver, account, cmdargs):
    rc, nodes = driver.get_vm_list(account)
    assert rc == RwStatus.SUCCESS	
    ct = len(nodes.vminfo_list)
    if cmdargs.destroy_all or cmdargs.wait:
        rc=0
        for n in nodes.vminfo_list:
            if testbed is not None:
                try:
                    testbed.remove_resource(n.vm_name)
                except:
                    print("WARNING: error deleting resource from reservation system")
            if RwStatus.SUCCESS != driver.delete_vm(account, n.vm_id):
                print('Error: failed to destroy node %s' % n.vm_name)
                rc=1
        if rc:
            sys.exit(1)
        if cmdargs.wait:
            while ct > 0:
                sys.stderr.write("waiting for %d VMs to exit...\n" % ct)
                time.sleep(1)
                try:
                    rc, nodesnw = driver.get_vm_list(account)
                    assert rc == RwStatus.SUCCESS	
                    ct = len(nodesnw.vminfo_list )
                except:
                    pass
        
    else:
        vm_re = re.compile('^%s$' % cmdargs.vm_name)
        ct = 0
        for n in nodes.vminfo_list:
            if vm_re.match(n.vm_name):
                ct += 1
                if testbed is not None:
                    try:
                        testbed.remove_resource(n.vm_name)
                    except:
                        print("WARNING: error deleting resource from reservation system")
                if RwStatus.SUCCESS != driver.delete_vm(account, n.vm_id):
                    print('Error: failed to destroy node %s' % n.vm_name)
                    return
                print('destroyed %s' % n.vm_name)
        if ct == 0:
            print("No VMs matching \"%s\" found" % ( cmdargs.vm_name ))
        
                    
def vm_rebuild_subcommand(driver, account, cmdargs):
    images = driver.list_images()
    found=0
    for i in images:
        if i.name == cmdargs.image_name:
            found=1
            break
    if found != 1:
        print('Error: Rebuild failed - image %s not found' % cmdargs.image_name)
        sys.exit(1)
    image=i
    nodes = driver.list_nodes()
    if cmdargs.rebuild_all:
        rc=0
        for n in nodes:
            if not driver.ex_rebuild(n,image):
                print('Error: failed to rebuild node %s' % n.name)
                rc=1
            if rc:
               sys.exit(1)
            rebuilt=0
            while rebuilt != 1:
                time.sleep(10)
                nw_nodes = driver.list_nodes()
                for nw in nw_nodes:
                    if nw.name == n.name:
                        if nw.state == n.state:
                            rebuilt=1
                        break  
    else:
        vm_re = re.compile('^%s$' % cmdargs.vm_name)
        ct = 0
        for n in nodes:
            if vm_re.match(n.name):
                ct += 1
                if not driver.ex_rebuild(n,image):
                    print('Error: failed to rebuild node %s' % n.name)
                    return
                print('Rebuilt %s' % n.name)
                rebuilt=0
                while rebuilt != 1:
                    time.sleep(10)
                    nw_nodes = driver.list_nodes()
                    for nw in nw_nodes:
                        if nw.name == n.name:
                            if nw.state == n.state:
                                rebuilt=1
                            break  
        if ct == 0:
            print("No VMs matching \"%s\" found" % ( cmdargs.vm_name ))
        
                    

def vm_reboot_subcommand(driver, account, cmdargs):
    rc, nodes = driver.get_vm_list(account)
    assert rc == RwStatus.SUCCESS	
    if cmdargs.reboot_all:
        for n in nodes.vminfo_list:
            '''
            if not n.reboot():
                print 'Error: failed to reboot node %s' % n.name
            else:
                print "rebooted %s" % n.name
            '''
            time.sleep(cmdargs.sleep_time)
    else:
        for n in nodes.vminfo_list:
            if n.vm_name == cmdargs.vm_name:
                if RwStatus.SUCCESS !=  driver.reboot_vm(account,n.vm_id):
                    print('Error: failed to reboot node %s' % n.vm_name)
                else:
                    print("rebooted %s" % n.vm_name)
                    

def vm_start_subcommand(driver, account, cmdargs):
    rc, nodes = driver.get_vm_list(account)
    assert rc == RwStatus.SUCCESS	
    if cmdargs.start_all:
        for n in nodes.vminfo_list:
            print(dir(n))
            if RwStatus.SUCCESS != driver.start_vm(account, n.vm_id):
                print('Error: failed to start node %s' % n.vm_name)
            else:
                print("started %s" % n.vm_name)
    else:
        for n in nodes.vminfo_list:
            if n.vm_name == cmdargs.vm_name:
                if RwStatus.SUCCESS != driver.start_vm(account, n.vm_id):
                    print('Error: failed to start node %s' % n.vm_name)
                else:
                    print("started %s" % n.vm_name)
                    
def vm_subcommand(driver, account, cmdargs):
    """Process the vm subcommand"""

    if cmdargs.which == 'list':
        rc, nodes = driver.get_vm_list(account)
        assert rc == RwStatus.SUCCESS	
        for n in nodes.vminfo_list:
            print(n)		
            if n.state == 4:
                if not cmdargs.ipsonly:
                    print("%s is shutoff" % n.vm_name)
            elif cmdargs.ipsonly:
                i = n.management_ip
                if i is not None:
                    print(i)
            else: 
                if n.management_ip is not None:
                    if len(n.private_ip_list) > 0:
                        print("%s %s,%s" % (n.vm_name, n.management_ip, ",".join([i.get_ip_address() for i in n.private_ip_list])))
                    else:
                        print("%s %s" % (n.vm_name, n.management_ip))
                else:
                    print("%s NO IP" % n.vm_name)

    elif cmdargs.which == 'create':
        vm_create_subcommand(driver, account, cmdargs)

    elif cmdargs.which == 'reboot':
        vm_reboot_subcommand(driver, account, cmdargs)
    elif cmdargs.which == 'start':
        vm_start_subcommand(driver, account, cmdargs)
    elif cmdargs.which == 'destroy':
        vm_destroy_subcommand(driver, account, cmdargs)
    #elif cmdargs.which == 'rebuild':
    #    vm_rebuild_subcommand(driver, account, cmdargs)

def image_delete_subcommand(driver, account, cmdargs):
    rc,images = driver.get_image_list(account)
    assert rc == RwStatus.SUCCESS
    account.openstack.key          = 'admin'
    if cmdargs.delete_all:
        for i in images.imageinfo_list:
            if RwStatus.SUCCESS != driver.delete_image(account, i.id):
                print('Error: failed to delete image %s' % i.name)
    else:
        for i in images.imageinfo_list:
            if i.name == cmdargs.image_name:
                if RwStatus.SUCCESS != driver.delete_image(account, i.id):
                    print('Error: failed to delete image %s' % i.name)

def image_subcommand(driver, account, cmdargs):
    """Process the image subcommand"""
    if cmdargs.which == 'list':
        rc, images = driver.get_image_list(account)
        assert rc == RwStatus.SUCCESS

        for i in images.imageinfo_list:
            print(i)

    elif cmdargs.which == 'delete':
        image_delete_subcommand(driver, account, cmdargs)

    elif cmdargs.which == 'create':
        account.openstack.key          = 'admin'
        rc, images = driver.get_image_list(account)
        assert rc == RwStatus.SUCCESS
        for i in images.imageinfo_list:
            if i.name == cmdargs.image_name:
                print("FATAL: image \"%s\" already exists" % cmdargs.image_name)
                return 1
        
        print("creating image \"%s\" using %s ..." % \
              (cmdargs.image_name, cmdargs.file_name))
        img = RwcalYang.ImageInfoItem()
        img.name = cmdargs.image_name
        img.location = cmdargs.file_name
        img.disk_format = "qcow2"
        img.container_format = "bare"
        rc, img_id = driver.create_image(account, img)	
        print("... done. image_id is %s" % img_id)
        return img_id

    elif cmdargs.which == 'getid':
        rc, images = driver.get_image_list(account)
        assert rc == RwStatus.SUCCESS
        found=0
        for i in images.imageinfo_list:
            if i.name == cmdargs.image_name:
                print(i.id)
                found += 1
        if found != 1:
            sys.exit(1)
        
def flavor_subcommand(driver, account, cmdargs):
    """Process the flavor subcommand"""
    if cmdargs.which == 'list':
        rc, sizes = driver.get_flavor_list(account)
        assert rc == RwStatus.SUCCESS
        for f in sizes.flavorinfo_list:
            rc, flv = driver.get_flavor(account, f.id)	    
            print(flv)	    
    elif cmdargs.which == 'create':
        account.openstack.key          = 'admin'    
        flavor                                     = RwcalYang.FlavorInfoItem()
        flavor.name                                = cmdargs.flavor_name
        flavor.vm_flavor.memory_mb                 = cmdargs.memory_size
        flavor.vm_flavor.vcpu_count                = cmdargs.vcpu_count
        flavor.vm_flavor.storage_gb                = cmdargs.disc_size
        if cmdargs.hugepages_kilo:
            flavor.guest_epa.mempage_size              = cmdargs.hugepages_kilo
        if cmdargs.numa_nodes:
            flavor.guest_epa.numa_node_policy.node_cnt = cmdargs.numa_nodes
        if cmdargs.dedicated_cpu:
            flavor.guest_epa.cpu_pinning_policy        = 'DEDICATED'
        if cmdargs.pci_count:
            dev = flavor.guest_epa.pcie_device.add()
            dev.device_id = 'PCI_%dG_ALIAS' % (cmdargs.pci_speed)
            dev.count = cmdargs.pci_count 
        if cmdargs.colleto:
            dev = flavor.guest_epa.pcie_device.add()
            dev.device_id = 'COLETO_VF_ALIAS'
            dev.count = cmdargs.colleto 
        if cmdargs.trusted_host:
            flavor.guest_epa.trusted_execution = True 

        rc, flavor_id = driver.create_flavor(account, flavor)
        assert rc == RwStatus.SUCCESS

        print("created flavor %s id %s" % (cmdargs.flavor_name, flavor_id)) 

    elif cmdargs.which == 'delete':
        account.openstack.key          = 'admin'    
        rc, sizes = driver.get_flavor_list(account)
        assert rc == RwStatus.SUCCESS
        for f in sizes.flavorinfo_list:
            if f.name == cmdargs.flavor_name:
                rc = driver.delete_flavor(account, f.id)
                assert rc == RwStatus.SUCCESS

def hostagg_subcommand(driver, account, cmdargs):
    """Process the hostagg subcommand"""
    if cmdargs.which == 'list':
        nova = ra_nova_connect(project='admin')
        for f in nova.aggregates.list():
            print("%-12s %-12s" % \
                  (f.name, f.availability_zone))
                
    elif cmdargs.which == 'create':
        nova = ra_nova_connect(project='admin')
        hostagg = nova.aggregates.create(cmdargs.hostagg_name, 
                                     cmdargs.avail_zone)
        print("created hostagg %s in %s" % (hostagg.name, hostagg.availability_zone)) 

    elif cmdargs.which == 'delete':
        nova = ra_nova_connect(project='admin')
        for f in nova.aggregates.list():
            if f.name == cmdargs.hostagg_name:
                if cmdargs.force_delete_hosts:
                    for h in f.hosts:
                        f.remove_host(h)

                f.delete()

    elif cmdargs.which == 'addhost':
        nova = ra_nova_connect(project='admin')
        for f in nova.aggregates.list():
            if f.name == cmdargs.hostagg_name:
                f.add_host(cmdargs.host_name)

    elif cmdargs.which == 'delhost':
        nova = ra_nova_connect(project='admin')
        for f in nova.aggregates.list():
            if f.name == cmdargs.hostagg_name:
                f.remove_host(cmdargs.host_name)

    elif cmdargs.which == 'setmetadata':
        nova = ra_nova_connect(project='admin')
        for f in nova.aggregates.list():
            if f.name == cmdargs.hostagg_name:
                d = dict([cmdargs.extra_specs.split("="),])		    
                f.set_metadata(d)

def quota_subcommand(driver, account, cmdargs):
    """Process the quota subcommand"""
    nova = ra_nova_connect(project='admin')
    cfgfile = get_openstack_file(None,  cmdargs.project)
    kwargs = load_params(cfgfile)

    keystone = keystone_client.Client(username=kwargs.get('OS_USERNAME'),
                               password=kwargs.get('OS_PASSWORD'),
                               tenant_name=kwargs.get('OS_TENANT_NAME'),
                               auth_url=kwargs.get('OS_AUTH_URL'))
    if cmdargs.which == 'set':
        nova.quotas.update(keystone.tenant_id, 
                           ram=cmdargs.memory, 
                           floating_ips=cmdargs.ips, 
                           instances=cmdargs.vms, 
                           cores=cmdargs.vcpus)
    elif cmdargs.which == 'get':
        print("get quotas for tenant %s %s" % \
              (cmdargs.project, keystone.tenant_id))
        q = nova.quotas.get(keystone.tenant_id)
        for att in [ 'ram', 'floating_ips', 'instances', 'cores' ]: 
            print("%12s: %6d" % ( att, getattr(q, att) ))
        
def rules_subcommand(driver, account, cmdargs):
    nova = ra_nova_connect(project='demo')
    group=nova.security_groups.find(name='default')
    if cmdargs.which == 'set':
        try:
            nova.security_group_rules.create(group.id,ip_protocol='tcp', from_port=1, to_port=65535 )
        except BadRequest:
            pass
        try: 
            nova.security_group_rules.create(group.id, ip_protocol='icmp',from_port=-1, to_port=-1 )
        except BadRequest:
            pass
            
    elif cmdargs.which == 'list':
        for r in group.rules:
            if r['from_port'] == -1:
                print("rule %d proto %s from IP %s" % ( r['id'], r['ip_protocol'], r['ip_range']['cidr'] ))
            else:
                print("rule %d proto %s from port %d to %d from IP %s" % ( r['id'], r['ip_protocol'], r['from_port'], r['to_port'], r['ip_range']['cidr'] ))


def register_subcommand(driver, account, cmdargs):
    cmdargs.reserve_new_vms = False
    vm_register('all', driver, account, cmdargs)       
           
##
# Command line argument specification
##
desc="""This tool is used to manage the VMs"""
kilo=platform.dist()[1]=='21'
parser = argparse.ArgumentParser(description=desc)
subparsers = parser.add_subparsers()
ipaddr = socket.gethostbyname(socket.getfqdn())
reservation_server_url = os.environ.get('RESERVATION_SERVER', 'http://reservation.eng.riftio.com:80')
# ipaddr = netifaces.ifaddresses('br0')[netifaces.AF_INET][0]['addr']
#default_auth_url = 'http://%s:5000/v3/' % ipaddr
default_auth_url = 'http://10.66.4.27:5000/v3/'

parser.add_argument('-t', '--provider-type', dest='provider_type',
                    type=str, default='OPENSTACK', 
                    help='Cloud provider type (default: %(default)s)')
parser.add_argument('-u', '--user-name', dest='user', 
                    type=str, default='demo', 
                    help='User name (default: %(default)s)')
parser.add_argument('-p', '--password', dest='passwd', 
                    type=str, default='mypasswd', 
                    help='Password (default: %(default)s)')
parser.add_argument('-m', '--mgmt-nw', dest='mgmt_network', 
                    type=str, default='private', 
                    help='mgmt-network (default: %(default)s)')
parser.add_argument('-a', '--auth-url', dest='auth_url', 
                    type=str, default=default_auth_url, 
                    help='Password (default: %(default)s)')
parser.add_argument('-r', '--reservation_server_url', dest='reservation_server_url', 
                    type=str, default=reservation_server_url, 
                    help='reservation server url, use None to disable (default %(default)s)' )
parser.add_argument('-d', '--debug', dest='debug', action='store_true', help='raise the logging level')

##
# Subparser for VM
##
vm_parser = subparsers.add_parser('vm')
vm_subparsers = vm_parser.add_subparsers()

# Create VM subparser
vm_create_parser = vm_subparsers.add_parser('create')
vm_create_parser.add_argument('-c', '--count',
                              type=int, default=1,
                              help='The number of VMs to launch '
                                   '(default: %(default)d)')
vm_create_parser.add_argument('-i', '--image', 
							  default='rwopenstack_vm',
                              help='Specify the image for the VM  (default: %(default)s)')
vm_create_parser.add_argument('-n', '--name',
                              help='Specify the name of the VM')
vm_create_parser.add_argument('-f', '--flavor',
                              help='Specify the flavor for the VM')
vm_create_parser.add_argument('-R', '--reserve', dest='reserve_new_vms', 
                    action='store_true', help='reserve any newly created VMs')
vm_create_parser.add_argument('-s', '--single', dest='wait_after_create', 
                    action='store_true', help='wait for each VM to start before creating the next')
vm_create_parser.add_argument('-N', '--networks', dest='networks', type=str, 
                                default='private,private2,private3,private4',
                                help='comma separated list of networks to connect these VMs to (default: %(default)s)' )

vm_create_parser.set_defaults(which='create')
# Reboot VM subparser
vm_reboot_parser = vm_subparsers.add_parser('reboot')
group = vm_reboot_parser.add_mutually_exclusive_group()
group.add_argument('-n', '--vm-name', dest='vm_name',
                   type=str,
                   help='Specify the name of the VM')
group.add_argument('-a', '--reboot-all', 
                   dest='reboot_all', action='store_true',
                   help='Reboot all VMs')
vm_reboot_parser.add_argument('-s', '--sleep', dest='sleep_time', type=int, default=4, help='time in seconds to sleep between reboots')
vm_reboot_parser.set_defaults(which='reboot')


"""
# start VM subparser
vm_start_parser = vm_subparsers.add_parser('start')
group = vm_start_parser.add_mutually_exclusive_group()
group.add_argument('-n', '--vm-name', dest='vm_name',
                   type=str,
                   help='Specify the name of the VM')
group.add_argument('-a', '--start-all', 
                   dest='start_all', action='store_true',
                   help='Start all VMs')
vm_start_parser.set_defaults(which='start')
"""

# Destroy VM subparser
vm_destroy_parser = vm_subparsers.add_parser('destroy')
group = vm_destroy_parser.add_mutually_exclusive_group()
group.add_argument('-n', '--vm-name', dest='vm_name',
                   type=str,
                   help='Specify the name of the VM (accepts regular expressions)')
group.add_argument('-a', '--destroy-all', 
                   dest='destroy_all', action='store_true',
                   help='Delete all VMs')
group.add_argument('-w', '--wait', 
                   dest='wait', action='store_true',
                   help='destroy all and wait until all VMs have exited')
vm_destroy_parser.set_defaults(which='destroy')

# Rebuild VM subparser
vm_rebuild_parser = vm_subparsers.add_parser('rebuild')
group = vm_rebuild_parser.add_mutually_exclusive_group()
group.add_argument('-n', '--vm-name', dest='vm_name',
                   type=str,
                   help='Specify the name of the VM (accepts regular expressions)')
group.add_argument('-a', '--rebuild-all', 
                   dest='rebuild_all', action='store_true',
                   help='Rebuild all VMs')
vm_rebuild_parser.add_argument('-i', '--image-name', dest='image_name',
                              type=str,
                              help='Specify the name of the image')
vm_rebuild_parser.set_defaults(which='rebuild')

# List VM subparser
vm_list_parser = vm_subparsers.add_parser('list')
vm_list_parser.set_defaults(which='list')
vm_list_parser.add_argument('-i', '--ips_only', dest='ipsonly', 
                            action='store_true', 
                            help='only list IP addresses')

vm_parser.set_defaults(func=vm_subcommand)

##
# Subparser for image
##
image_parser = subparsers.add_parser('image')
image_subparsers = image_parser.add_subparsers()

# List image subparser
image_list_parser = image_subparsers.add_parser('list')
image_list_parser.set_defaults(which='list')

# Delete image subparser
image_destroy_parser = image_subparsers.add_parser('delete')
group = image_destroy_parser.add_mutually_exclusive_group()
group.add_argument('-n', '--image-name', dest='image_name',
                   type=str,
                   help='Specify the name of the image')
group.add_argument('-a', '--delete-all', 
                   dest='delete_all', action='store_true',
                   help='Delete all images')
image_destroy_parser.set_defaults(which='delete')

# create image
image_create_parser = image_subparsers.add_parser('create')
image_create_parser.set_defaults(which='create')
image_create_parser.add_argument('-n', '--image-name', dest='image_name',
                                  type=str,
                                  default="rwopenstack_vm",
                                  help='Specify the name of the image')
image_create_parser.add_argument('-f', '--filename', dest='file_name',
                                  type=str, 
                                  default='/net/sharedfiles/home1/common/vm/rift-root-current.qcow2',
                                  help='name of the existing qcow2 image file')


image_create_parser = image_subparsers.add_parser('getid')
image_create_parser.set_defaults(which='getid')
image_create_parser.add_argument('-n', '--image-name', dest='image_name',
                                  type=str,
                                  default="rwopenstack_vm",
                                  help='Specify the name of the image')
image_parser.set_defaults(func=image_subcommand)

##
# Subparser for flavor
##
flavor_parser = subparsers.add_parser('flavor')
flavor_subparsers = flavor_parser.add_subparsers()

# List flavor subparser
flavor_list_parser = flavor_subparsers.add_parser('list')
flavor_list_parser.set_defaults(which='list')

# Create flavor subparser
flavor_create_parser = flavor_subparsers.add_parser('create')
flavor_create_parser.set_defaults(which='create')
flavor_create_parser.add_argument('-n', '--flavor-name', dest='flavor_name',
                                  type=str,
                                  help='Specify the name of the flavor')
flavor_create_parser.add_argument('-m', '--memory-size', dest='memory_size',
                                  type=int, default=1024,
                                  help='Specify the size of the memory in MB '
                                       '(default: %(default)d)')
flavor_create_parser.add_argument('-d', '--disc-size', dest='disc_size',
                                  type=int, default=16,
                                  help='Specify the size of the disc in GB '
                                       '(default: %(default)d)')
flavor_create_parser.add_argument('-v', '--vcpu-count', dest='vcpu_count',
                                  type=int, default=1,
                                  help='Specify the number of VCPUs '
                                       '(default: %(default)d)')
flavor_create_parser.add_argument('-p', '--pci-count', dest='pci_count',
                                  type=int, default=0,
                                  help='Specify the number of PCI devices '
                                       '(default: %(default)d)')
flavor_create_parser.add_argument('-s', '--pci-speed', dest='pci_speed',
                                  type=int, default=10,
                                  help='Specify the speed of the PCI devices in Gbps (default: %(default)d)')
flavor_create_parser.add_argument('-e', '--hostagg-extra-specs', dest='extra_specs',
                                  type=str, 
                                  help='Specify the extra spec ')
flavor_create_parser.add_argument('-b', '--back-with-hugepages', dest='enable_hugepages',
                                  action='store_true',
                                  help='Enable memory backing with hugepages')
flavor_create_parser.add_argument('-B', '--back-with-hugepages-kilo', dest='hugepages_kilo',
                                  type=str,
                                  help='Enable memory backing with hugepages for kilo')
flavor_create_parser.add_argument('-D', '--dedicated_cpu', dest='dedicated_cpu',
                                  action='store_true',
                                  help='Dedicated CPU usage')
flavor_create_parser.add_argument('-T', '--cpu_threads', dest='cpu_threads',
                                  type=str, 
                                  help='CPU threads usage')
flavor_create_parser.add_argument('-N', '--numa_nodes', dest='numa_nodes',
                                  type=int, 
                                  help='Configure numa nodes')
flavor_create_parser.add_argument('-t', '--trusted-host', dest='trusted_host',  action='store_true', help='restrict instances to trusted hosts')
flavor_create_parser.add_argument('-c', '--crypto-cards', dest='colleto',  type=int, default=0,  \
                                    help='how many colleto creek VFs should be passed thru to the VM')

# Delete flavor subparser
flavor_delete_parser = flavor_subparsers.add_parser('delete')
flavor_delete_parser.set_defaults(which='delete')
flavor_delete_parser.add_argument('-n', '--flavor-name', dest='flavor_name',
                                  type=str,
                                  help='Specify the name of the flavor')

flavor_parser.set_defaults(func=flavor_subcommand)

##
# Subparser for host-aggregate 
##
hostagg_parser = subparsers.add_parser('hostagg')
hostagg_subparsers = hostagg_parser.add_subparsers()

# List host-aggregate subparser
hostagg_list_parser = hostagg_subparsers.add_parser('list')
hostagg_list_parser.set_defaults(which='list')

# Create hostagg subparser
hostagg_create_parser = hostagg_subparsers.add_parser('create')
hostagg_create_parser.set_defaults(which='create')
hostagg_create_parser.add_argument('-n', '--hostagg-name', dest='hostagg_name',
                                  type=str,
                                  help='Specify the name of the hostagg')
hostagg_create_parser.add_argument('-a', '--avail-zone', dest='avail_zone',
                                  type=str,
                                  help='Specify the name of the availability zone')
# Delete hostagg subparser
hostagg_delete_parser = hostagg_subparsers.add_parser('delete')
hostagg_delete_parser.set_defaults(which='delete')
hostagg_delete_parser.add_argument('-n', '--hostagg-name', dest='hostagg_name',
                                  type=str,
                                  help='Specify the name of the hostagg')
hostagg_delete_parser.add_argument('-f', '--force-delete-hosts', dest='force_delete_hosts',
                                  action='store_true',
                                  help='Delete the existing hosts')

# Add host subparser
hostagg_addhost_parser = hostagg_subparsers.add_parser('addhost')
hostagg_addhost_parser.set_defaults(which='addhost')
hostagg_addhost_parser.add_argument('-n', '--hostagg-name', dest='hostagg_name',
                                  type=str,
                                  help='Specify the name of the hostagg')
hostagg_addhost_parser.add_argument('-c', '--compute-host-name', dest='host_name',
                                  type=str,
                                  help='Specify the name of the host to be added')

# Remove host subparser
hostagg_delhost_parser = hostagg_subparsers.add_parser('delhost')
hostagg_delhost_parser.set_defaults(which='delhost')
hostagg_delhost_parser.add_argument('-n', '--hostagg-name', dest='hostagg_name',
                                  type=str,
                                  help='Specify the name of the hostagg')
hostagg_delhost_parser.add_argument('-c', '--compute-host-name', dest='host_name',
                                  type=str,
                                  help='Specify the name of the host to be removed')

# Set meta-data subparser
hostagg_setdata_parser = hostagg_subparsers.add_parser('setmetadata')
hostagg_setdata_parser.set_defaults(which='setmetadata')
hostagg_setdata_parser.add_argument('-n', '--hostagg-name', dest='hostagg_name',
                                  type=str,
                                  help='Specify the name of the hostagg')
hostagg_setdata_parser.add_argument('-d', '--meta-data', dest='extra_specs',
                                  type=str,
                                  help='Specify the meta-data to be associated to this host aggregate')

hostagg_parser.set_defaults(func=hostagg_subcommand)

##
# Subparser for quota
##
quota_parser = subparsers.add_parser('quota')
quota_subparser = quota_parser.add_subparsers()
quota_set_parser = quota_subparser.add_parser('set')

# quota set subparser
quota_set_parser.set_defaults(which='set')
quota_set_parser.add_argument('-p', '--project', dest='project', 
                              type=str, default='demo', 
                              help='project name that you wish to set '
                                   'the quotas for')
quota_set_parser.add_argument('-c', '--vcpus', dest='vcpus', 
                              type=int, default=48, 
                              help='Maximum number of virtual CPUs that can '
                                   'be assigned to all VMs in aggregate')
quota_set_parser.add_argument('-v', '--vms', dest='vms', 
                              type=int, default=24, 
                              help='Maximum number of VMs that can be created ' 
                                   'on this openstack instance '
                                   '(which may be more than 1 machine)')
quota_set_parser.add_argument('-i', '--ips', dest='ips', 
                              type=int, default=250, 
                              help='Maximum number of Floating IP Addresses '
                                   'that can be assigned to all VMs '
                                   'in aggregate')
quota_set_parser.add_argument('-m', '--memory', dest='memory', 
                              type=int, default=122880, 
                              help='Maximum amount of RAM in MB that can be '
                                   'assigned to all VMs in aggregate')

# quota get subparser
quota_get_parser = quota_subparser.add_parser('get')
quota_get_parser.add_argument('-p', '--project', dest='project', 
                              type=str, default='demo', 
                              help='project name that you wish to get '
                                   'the quotas for')
quota_get_parser.set_defaults(which='get')
quota_parser.set_defaults(func=quota_subcommand)

##
# rules subparser
##
rules_parser = subparsers.add_parser('rules')
rules_parser.set_defaults(func=rules_subcommand)
rules_subparser = rules_parser.add_subparsers()
rules_set_parser = rules_subparser.add_parser('set')
rules_set_parser.set_defaults(which='set')
rules_list_parser = rules_subparser.add_parser('list')
rules_list_parser.set_defaults(which='list')

register_parser = subparsers.add_parser('register')
register_parser.set_defaults(func=register_subcommand)
 
cmdargs = parser.parse_args()


if __name__ == "__main__":
    logger=logging.getLogger(__name__)
    if cmdargs.debug:
        logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s', level=logging.DEBUG) 
    else:
        logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s', level=logging.WARNING) 

    if cmdargs.provider_type == 'OPENSTACK':
        #cls = get_driver(Provider.OPENSTACK)
        pass
    elif cmdargs.provider_type == 'VSPHERE':
        cls = get_driver(Provider.VSPHERE)
    else:
        sys.exit("Cloud provider %s is NOT supported yet" % cmdargs.provider_type)

    if cmdargs.reservation_server_url == "None" or cmdargs.reservation_server_url == "":
        cmdargs.reservation_server_url = None
    if cmdargs.reservation_server_url is not None:
        sys.path.append('/usr/rift/lib')
        try:
            import ndl
        except Exception as e:
            logger.warning("Error loading Reservation library")
            testbed=None
        else:
            testbed=ndl.Testbed()
            testbed.set_server(cmdargs.reservation_server_url)
            


    if cmdargs.provider_type == 'OPENSTACK':
        account                        = RwcalYang.CloudAccount()
        account.account_type           = "openstack"
        account.openstack.key          = cmdargs.user
        account.openstack.secret       = cmdargs.passwd
        account.openstack.auth_url     = cmdargs.auth_url
        account.openstack.tenant       = cmdargs.user
        account.openstack.mgmt_network = cmdargs.mgmt_network

        plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
        engine, info, extension = plugin()
        driver = plugin.get_interface("Cloud")
        # Get the RwLogger context
        rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
        try:
            rc = driver.init(rwloggerctx)
            assert rc == RwStatus.SUCCESS
        except:
            logger.error("ERROR:Cal plugin instantiation failed. Aborting tests")
        else:
            logger.info("Openstack Cal plugin successfully instantiated")

        cmdargs.func(driver, account, cmdargs)

    elif cmdargs.provider_type == 'VSPHERE':
        driver = cls(cmdargs.user, cmdargs.passwd, host='vcenter' )
        cmdargs.func(driver, cmdargs)
