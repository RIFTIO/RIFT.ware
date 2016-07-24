#!/usr/bin/env python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#



import subprocess
import re
import os
import sys

macs = dict()
cmds='''terminal length 0
show mac-address-table
q
'''
re1 = re.compile('\s*[0-9]+\s+([0-9a-f:]+)\s+Dynamic\s+Te\s+0/([0-9]+)\s+Active')
devnull = open(os.devnull, "w")
DEBUG = 'DEBUG' in sys.argv
for swnum in range(3):
 	sw = "f10-grunt%02d" % swnum
	p = subprocess.Popen(["ssh", "-o", "StrictHostKeyChecking=no", "admin@%s" % sw ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=devnull )
	
	(stdout, stderr) = p.communicate(cmds)
	# eliminate any ports that have more than 2 mac addresses
	counts=[0 for m in range(64)]
	for line in stdout.split('\n'):
		m = re1.match(line)
		if m is not None:
			counts[int(m.group(2))] += 1
		else:
			if DEBUG: print line
	
	for line in stdout.split('\n'):
		m = re1.match(line)
		if m is not None:
			port = int(m.group(2))
			if counts[port] < 3:
				macs[m.group(1)] = sw
			else:
				if DEBUG: print 'skipping %s' % port
				
for mac, sw in macs.iteritems():
	print "%s %s" % ( mac, sw)
			
		
	
	