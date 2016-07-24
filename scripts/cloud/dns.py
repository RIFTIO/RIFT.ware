#!/usr/bin/env python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import re
import socket

from libcloud.compute.types import Provider
from libcloud.compute.providers import get_driver

def get_mgmt_ip(node):
    for i in node.private_ips:
        if re.match("10.0", i):
            return i
    return None

def vm_list(ipaddr=None):
    ''' 
    ipaddr should be the IP address of the controller node
    '''
    if ipaddr is None:
        ipaddr = socket.gethostbyname(socket.getfqdn())
    cls = get_driver(Provider.OPENSTACK)
    driver = cls('demo', 'mypasswd', ex_force_auth_url = 'http://%s:35357/v2.0/tokens' % ipaddr, ex_force_auth_version = '2.0_password', ex_tenant_name = 'demo')
    res = dict()
    nodes = driver.list_nodes()
    for n in nodes:
        if n.state != 4:
            res[n.name] = get_mgmt_ip(n)
    return res


if __name__ == '__main__':

    res = vm_list()
    print res
