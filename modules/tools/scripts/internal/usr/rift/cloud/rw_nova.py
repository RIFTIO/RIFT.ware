#!/usr/bin/env python

# STANDARD_RIFT_IO_COPYRIGHT

'''
utility functions
'''

import os
import re
import subprocess
from novaclient.client import Client
try:
    from neutronclient.v2_0 import client as NeutronClient
except:
    pass



    



def load_params(filename):
    params = dict()
    with open(filename, "r") as f:
        for line in f.readlines():
            m = re.match('^export (.*)="(.*)"$', line)
            if m is not None:
                params[m.group(1)] = m.group(2)
                #print "%s is %s" % ( m.group(1), m.group(2) )
                continue
            m = re.match('^export (.*)=(.*)$', line)
            if m is not None:
                params[m.group(1)] = m.group(2)
                #print "%s is %s" % ( m.group(1), m.group(2) )
                continue
    return params

def get_openstack_file(host, project):
    if host is None:
        ''' we are on the grunt
        '''
        if os.path.exists('/home/stack'):
            subprocess.call(['sudo', 'chmod', '755', '/home/stack'])
            cfgfile = '/home/stack/devstack/accrc/%s/%s' % ( project, project )
        else:
            cfgfile = '/root/keystonerc_%s' % ( project )
            subprocess.call(['sudo', 'chmod', '755', '/root', cfgfile ])
        if not os.path.exists(cfgfile):
            raise Exception, "Openstack config file \"%s\" not found. Install openstack or specify host" % cfgfile
    else:
        # remote
        cfgfile = "/tmp/%s.%s.openstack" % ( host, project )
        filename = '/home/stack/devstack/accrc/%s/%s' % ( project, project )
        if not os.path.exists(cfgfile):
            subprocess.call(["sftp", "-i", "%s/.ssh/id_grunt" % os.environ['HOME'], "root@%s:%s" % ( host, filename), cfgfile])
        if not os.path.exists(cfgfile):
            filename = '/root/keystonerc_%s' % ( project )
            subprocess.call(["sftp", "-i", "%s/.ssh/id_grunt" % os.environ['HOME'], "root@%s:%s" % ( host, filename), cfgfile])
        if not os.path.exists(cfgfile):
            raise Exception, "Error retrieving openstack config from %s" % host

    return cfgfile

def get_glance_endpoint_and_token(host=None, project='demo'):
    from keystoneclient.v2_0 import client as ksclient
    cfgfile = get_openstack_file(host, project)
    kwargs = load_params(cfgfile)
    
    _ksclient = ksclient.Client(username=kwargs.get('OS_USERNAME'),
                               password=kwargs.get('OS_PASSWORD'),
                               tenant_id=kwargs.get('tenant_id'),
                               tenant_name=kwargs.get('OS_TENANT_NAME'),
                               auth_url=kwargs.get('OS_AUTH_URL'),
                               cacert=kwargs.get('OS_CACERT'),
                               insecure=kwargs.get('insecure'))

    token = _ksclient.auth_token
    endpoint_kwargs = {             'service_type': kwargs.get('service_type') or 'image',
                                    'endpoint_type': kwargs.get('endpoint_type') or 'publicURL',
                    }
    endpoint = _ksclient.service_catalog.url_for(**endpoint_kwargs)
    return endpoint, token

        

def ra_nova_connect(host=None, project='demo'):
    
    cfgfile = get_openstack_file(host, project)
    params = load_params(cfgfile)
    nova = Client(2, params['OS_USERNAME'], params['OS_PASSWORD'], project, params['OS_AUTH_URL'] )
    return nova


def ra_nova_get_vcpus(vmname_or_ip):

    nova = ra_nova_connect()    
    # hostname can have dashes but they will be _ here so...
    s2 = re.sub("-", "_", vmname_or_ip)
    for server in nova.servers.list():
        d=server.to_dict()
        if d['name'] == vmname_or_ip or d['name'] == s2:
            return nova.flavors.find(id=server.flavor['id']).vcpus
        for p in d['addresses']['private']:
            if p['addr'] == vmname_or_ip:
                return nova.flavors.find(id=server.flavor['id']).vcpus
    return None

def ra_neutron_connect(host=None, project='demo'):
    
    cfgfile = get_openstack_file(host, project)
    params = load_params(cfgfile)
    client = NeutronClient.Client(username=params['OS_USERNAME'], password=params['OS_PASSWORD'], tenant_name=project, auth_url=params['OS_AUTH_URL'] )
    return client


if __name__ == "__main__": 
    import sys
    
    if len(sys.argv) > 1: 
        nova = ra_nova_connect(sys.argv[1])
    else:
        nova = ra_nova_connect()
        
    for server in nova.servers.list():
        d = server.to_dict()
        print "%s %s" % ( d['name'], d['status'])
