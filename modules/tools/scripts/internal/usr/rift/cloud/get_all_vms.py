#!/usr/bin/env python

# STANDARD_RIFT_IO_COPYRIGHT

'''
get_all_vms.py

7/28/14 -- JLM -- Genesis

Poll all openstack controllers and get a list of their VMs
'''

from novaclient.client import Client
import os
import re
import subprocess



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
    

def ra_nova_connect(host=None):
    if host is None:
        # local
        params = load_params("/home/stack/devstack/accrc/demo/demo")
    else:
        # remote
        cfgfile = "/tmp/%s.openstack" % host
        if not os.path.exists(cfgfile):
            subprocess.call(["sftp", "root@%s:/home/stack/devstack/accrc/demo/demo" % host, cfgfile])
        if not os.path.exists(cfgfile):
            raise Exception, "Error retrieving openstack config from %s" % host
        params = load_params(cfgfile)
    
    nova = Client(2, params['OS_USERNAME'], params['OS_PASSWORD'], "demo", params['OS_AUTH_URL'] )
    return nova

        
if __name__ == "__main__":
    for grunt_num in range(1,21):
        grunt = "grunt%d" % grunt_num
        try:
            nova = ra_nova_connect(grunt)
            for server in nova.servers.list():
                ''' @type d dict
                '''
                d = server.to_dict()
                if server.status == "SHUTOFF":
                    print "server %s is SHUTOFF" % d['name']
                    continue
                try:
                        f = nova.flavors.find(id=server.flavor['id'])
                        cpus = f.vcpus
                        ram = f.ram
                except:
                        cpus = 0
                        ram = 0 
        
                i = d['addresses']
                for l in i.keys():
                  for p in i[l]:
                    print "server %s VM %s is %s has %d cpus and %d MB ram and is at %s on %s" % ( grunt, d['name'], server.status, cpus, ram, p['addr'], l )
        except:
            print "%s may not be an openstack controler" % grunt
        
