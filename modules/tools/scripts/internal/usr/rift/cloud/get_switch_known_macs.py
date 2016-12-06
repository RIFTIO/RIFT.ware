#!/usr/bin/env python

# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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
			
		
	
	