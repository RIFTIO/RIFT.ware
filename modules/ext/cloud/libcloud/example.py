#!/usr/bin/env python
from libcloud.compute.providers import get_driver
from libcloud.compute.types import Provider
import libcloud.security
import os
from pprint import pprint
import sys

#libcloud.enable_debug(sys.stdout)

# At the time this example was written, https://nova-api.trystack.org:5443
# was using a certificate issued by a Certificate Authority (CA) which is
# not included in the default Ubuntu certificates bundle (ca-certificates).
# Note: Code like this poses a security risk (MITM attack) and that's the
# reason why you should never use it for anything else besides testing. You
# have been warned.
libcloud.security.VERIFY_SSL_CERT = False

OpenStack = get_driver(Provider.OPENSTACK)

driver = OpenStack('admin', 'mypasswd',
#driver = OpenStackNodeDriver('admin', '18267ce5260b404b',
                   ex_force_auth_url='http://10.64.4.23:35357/v2.0/tokens',
#                   ex_tenant_name ='demo',
                   ex_tenant_name ='admin',
                   ex_force_auth_version='2.0_password')

#pprint(driver.list_nodes())
#pprint(driver.list_images())
sizes=driver.list_sizes()
for size in sizes:
	print("%20s %10s %5d %5d %5d %s" % ( size.name, size.bandwidth, size.disk, size.ram, size.vcpus, size.extra ))


print(" ADD ... ")
try:
	driver.ex_create_flavor( name='test', ram=8000, vcpus=7, disk=8, extra={ 'mem:hugepages': 'enabled'} )
except Exception as e:
	print "error %s create test" % e

sizes=driver.list_sizes()
for size in sizes:
	print("%20s %10s %5d %5d %5d %s" % ( size.name, size.bandwidth, size.disk, size.ram, size.vcpus, size.extra ))

print("DELETE ")
for size in sizes:
	if size.name == 'test': 
		driver.ex_delete_flavor(size)

sizes=driver.list_sizes()
for size in sizes:
	print("%20s %10s %5d %5d %5d %s" % ( size.name, size.bandwidth, size.disk, size.ram, size.vcpus, size.extra ))

'''
bandwidth is None
disk is 6
driver is <libcloud.compute.drivers.openstack.OpenStack_1_1_NodeDriver object at 0x7f5c1d58c650>
extra is {}
get_uuid is <bound method OpenStackNodeSize.get_uuid of <OpenStackNodeSize: id=09c0825b-d661-41e9-8876-b8f506e5292f, name=rw.medium1, ram=8192, disk=6, bandwidth=None, price=0.0, driver=OpenStack, vcpus=1,  ...>>
id is 09c0825b-d661-41e9-8876-b8f506e5292f
name is rw.medium1
price is 0.0
ram is 8192
uuid is 749175bb3d305b59704e4f98055e5d9e2be52342
vcpus is 1

s=sizes[0]
print dir(s)
for x in dir(s): 
	try:
		print "%s is %s" % ( x, s.__getattribute__(x)  )
	except:
		print "cannot print %s" % x
'''
#pprint(driver.ex_list_networks())
#pprint(driver.ex_list_security_groups())
#images = driver.list_images()
#sizes=driver.list_sizes()
#size = [s for s in sizes if s.id == '2'][0]
#pprint(size)
#image = [i for i in images if 'cirr' in i.name][0]
#pprint(image)
#node = driver.create_node(name='libcloud', size=size, image=image,ex_keyname="pkstkkey")
##print(node)
#nodes = driver.list_nodes()
#node = [i for i in nodes if 'libcloud' in i.name][0]
#pprint(node)
##driver.ex_rebuild(node,image,ex_key_name="pkstkkey")
#driver.destroy_node(node)
#pprint(driver.list_nodes())
#pprint(driver.ex_get_metadata(node))
#pprint(driver.ex_get_node_details(node))
#pprint(driver.list_key_pairs())
