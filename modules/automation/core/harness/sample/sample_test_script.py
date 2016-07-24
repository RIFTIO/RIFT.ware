#!/usr/bin/env python
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file sample_test_script
@author Jeremy Mordkoff (Jeremy.Mordkoff@riftio.com)
@date 11/20/14
@brief This is an example that shows how a test is handed the list of resources to use for a system test

"""


import argparse
import json
import logging 
import os
import sys
import time
import random

from rw_testconfig import TestConfiguration
from ndl import Testbed

logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s', level=logging.INFO) 
logger = logging.getLogger(__name__)

parser = argparse.ArgumentParser(description="sample test script")
''' sutfile is a json encoded dictionary that maps all of the logical entities as defined in 
    test configuration file to actual network switches and VM
'''
parser.add_argument('-s', '--sut_file', dest='sut_file', type=str, help='SUT file')
parser.add_argument('-c', '--config_file', dest='cfg_file', type=str, help='Test Config File')
parser.add_argument('-d', '--debug', dest='debug', action='store_true', help='debug logging', default=False)
parser.add_argument('-u', '--user', dest='user', type=str, help='user name for accessing testbed data', default=os.environ.get('USER','ruser'))
parser.add_argument('--opt', dest='opts', action='append', type=str )

cmdargs = parser.parse_args()

logger.debug("sutfile is %s, cfg file is %s" % ( cmdargs.sut_file, cmdargs.cfg_file ))
logger.info("opts are %s" % cmdargs.opts)

''' this will also validate that the sut_file has an entry for every logical 
    entity in the test config 
'''
testconfig = TestConfiguration(cmdargs.cfg_file, cmdargs.sut_file )
testbed = Testbed(user=cmdargs.user)

res = 0
VMS = [ 'VM' ]
NETS = []

for logical_vm in VMS: 
    ''' map the logical name to an actual resource and retrieve its testbed object
    '''
    actual_vm = testbed.find_host(testconfig.get_actual_vm(logical_vm))
    for logical_net in NETS:
        actual_net = testconfig.get_actual_network(logical_net)
        conns = actual_vm.get_connections_by_net(actual_net)
        if len(conns):
            for conn in conns:
                print "%s is connected to %s via port %s @ %d Mbps" % ( logical_vm, logical_net, conn.port, conn.speed)
        else:
            print "no connections found from %s (%s) to %s (%s)" % ( logical_vm, actual_vm.name, logical_net, actual_net )
            res = 1 
        
        
        
# os.kill(os.getpid(), 11)

x=random.randint(1,3)*3
print("sleeping %d secs" % x)
sys.stdout.flush()
sys.stderr.write("stderr")
time.sleep(x)
print "exitting"
sys.stdout.flush()
raise Exception()
sys.exit(res)
    
    
