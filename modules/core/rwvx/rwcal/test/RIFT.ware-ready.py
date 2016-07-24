#!/usr/bin/env python3

import re
import sys
from rift.rwcal.openstack.openstack_drv import OpenstackDriver



def test_openstack(drv):
    print("checking endpoints")
    for ep in [ 'compute', 'image', 'network', 'metering' ]: 
        url = drv.ks_drv.get_service_endpoint(ep, 'publicURL')
        print("%s: %s" % ( ep, url))
        if re.search(url, '127.0.0'): 
            raise Exception("endpoint %s is using a loopback URL: %s" % ( ep, url))

    def verify(name, min, count):
        if count < min:
            raise Exception("only %d instances of %s found. Minimum is %d" % ( count, name, min))
        print("found %d %s" % ( count, name ))
        
    verify("images"     , 1, len(drv.glance_image_list()))
    verify("flavors "    , 1, len(drv.nova_flavor_list()))
    verify("floating ips "    , 1, len(drv.nova_floating_ip_list()))
    verify("servers"     , 0, len(drv.nova_server_list()))
    verify("networks"     , 1, len(drv.neutron_network_list()))
    verify("subnets"     , 1, len(drv.neutron_subnet_list()))
    verify("ports"         , 1, len(drv.neutron_port_list()))
    #verify("ceilometers"     , 1, len(drv.ceilo_meter_list()))
    


if len(sys.argv) != 6:
    print("ARGS are admin_user admin_password auth_url tenant_name mgmt_network_name")
    print("e.g. %s pluto mypasswd http://10.95.4.2:5000/v3 demo private" % __file__ )
    sys.exit(1)

args=tuple(sys.argv[1:6])
print("Using args \"%s\"" % ",".join(args))

try:
    v3 = OpenstackDriver(*args)
except Exception as e:
    print("\n\nunable to instantiate a endpoint: %s" % e)
else:
    print("\n\n endpoint instantiated")
    try:
        test_openstack(v3)
    except Exception as e:
        print("\n\nendpoint verification failed: %s" % e)
    else:
        print("\n\nSUCCESS! openstack is working")
        sys.exit(0)



sys.exit(1)


# need to check if any public urls are loopbacks
# need to check DNS is set up right 
#    neutron subnet-show private_subnet
#    host repo.riftio.com  10.64.1.3

