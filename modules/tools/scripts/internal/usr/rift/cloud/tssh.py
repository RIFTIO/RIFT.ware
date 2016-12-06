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

# By WW 2015
# This is a multi thread ssh tool. Please know what you are doing before using this tool.
# To use, generate a list of IP or hostname, one each line and piple into this tool.
# For example, below command will run uptime on grunt1-10 with 5 session in pararall:
# for i in {1..10}; do echo grunt$i ; done | ./tssh.py -t 10 -k ~/.ssh/vmkey "uptime"


import subprocess
import threading
import sys,os,shlex
import argparse

basecommand = "ssh -o StrictHostKeyChecking=no "
header = "echo -n  \"%s: \";  "


class Ssher(object):
    hosts = [] # List of all hosts/servers in our input queue
    cmd = ""
    user = ""

    # How many ssh process at the time.
    thread_count = 4

    # Lock object to keep track the threads in loops, where it can potentially be race conditions.
    lock = threading.Lock()

    def Runner(self, server):
        
        if self.ping(server):
        	command = shlex.split(basecommand )
		if len(self.keyfile) > 1 :
			command += [ "-i" ,  self.keyfile  ]
        	command += [ self.user + "@" + server]
        	command += [ header % server  + str(self.cmd) +";"]
  		#print command
        	output = subprocess.Popen(command, stdout=subprocess.PIPE).stdout
        	for i in output:
        	    print i.strip()
	else:
		print "%s: Not reachable" % server

    def ping(self, ip):
        # Use the system ping command with count of 1 and wait time of 1.
        ret = subprocess.call(['ping', '-c', '2', '-W', '1', ip],
                              stdout=open('/dev/null', 'w'), stderr=open('/dev/null', 'w'))

        return ret == 0 # Return True if our ping command succeeds


    def pop_queue(self):
        server = None

        self.lock.acquire() # Grab or wait+grab the lock.

        if self.hosts:
            server = self.hosts.pop()

        self.lock.release() # Release the lock, so another thread could grab it.

        return server

    def dequeue(self):
        while True:
            server = self.pop_queue()

            if not server:
                return None

            self.Runner(server)

    def start(self):
        threads = []

        for i in range(self.thread_count):
            # Create self.thread_count number of threads that together will
            # cooperate removing every server in the list. Each thread will do the
            # job as fast as it can.
            t = threading.Thread(target=self.dequeue)
            t.start()
            threads.append(t)

        # Wait until all the threads are done. .join() is blocking.
        [ t.join() for t in threads ]


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description=''' 
This is a multi thread ssh tool. Please know what you are doing before using this tool.
 To use, generate a list of IP or hostname, one each line and piple into this tool.
 For example, below command will run uptime on grunt1-10 with 5 session in pararall: 
 for i in {1..10}; do echo grunt$i ; done | ./tssh.py -t 10 -k ~/.ssh/vmkey "uptime" 
''')
    parser.add_argument('command', action="store", type=str,
                        help="Command to execute in the ssh targets")
    parser.add_argument('-t', action="store", dest="threads_no", type=int, default=6,
                        help="Number of threads")
    parser.add_argument('-u', action="store", dest="user", type=str, default="root",
                        help="user name")
    parser.add_argument('-k', action="store", dest="keyfile", type=str, default="",
                        help="key file")
    
    args = parser.parse_args()

    servers = []
    while True:
        try:
            server = raw_input()
            servers.append( server )
        except EOFError:
            break

    tssh = Ssher()
    tssh.thread_count = args.threads_no 
    
    tssh.hosts =  servers 
    tssh.cmd = args.command
    tssh.user = args.user
    tssh.keyfile = args.keyfile

    tssh.start()
