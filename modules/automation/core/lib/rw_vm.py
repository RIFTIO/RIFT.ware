
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

'''
Description:    Misc constants, functions, classes related to VMs.
Author:         Andrew Golovanov
Version:        1.0
Creation Date:  6/21/2014
Copyright:      RIFTio, Inc.


POSSIBLY OBSOLETE -- JLM 10/1/14
'''

#
#--- External modules
#
import sys, os, re, datetime as dt
import pexpect as pe
import gevent, gevent.monkey
import rw_checks as rwchk

gevent.monkey.patch_os()
gevent.monkey.patch_select()
gevent.monkey.patch_time()

#
#--- Local constants
#
DBGMODE    = '-DEBUG' in sys.argv

reLOGIN    = re.compile('(?m)^Last login:.+from.+')
reVMPROMPT = re.compile('\[(([\w]+)@rwopenstack-(\w+)-(vm\d+)[^\[\]]+)\]\$ ')
reAUTH     = re.compile('(?s)authenticity.+continue connecting \(yes/no\)\? ')
rePASSWD   = re.compile('(?i)password:')
reANY      = re.compile('.+')

#
#--- Classes
#
class vm_sync(object):
    '''Represents a single remote VM instance'''

    def __init__(self, vm_host, user=None, pwd=None, timeout=15):
        '''
        Arguments:

        vmHost  - a name or IP address of the VM
        user    - user name for login; if None, current user will be used
        pwd     - password for login; if None, passwordless access is assumed
        timeout - default timeout for the VM session (secs)
        '''
        self.vm  = vm_host
        self.usr = user
        self.pwd = pwd
        self.tm  = timeout

        # Connect to the VM
        self.connect()

    def __del__(self):
        self.disconnect()

    def connect(self):
        '''Connects to the VM via ssh/pexpect'''
        cmd = 'ssh {}'.format(self.vm) if self.usr is None else 'ssh {}@{}'.format(
            self.usr, self.vm)
        self.ses = pe.spawn(cmd, timeout=self.tm, logfile=open(
            os.path.join(os.path.dirname(sys.argv[0]), 'pexpect.log'), 'w') if DBGMODE else None)
        while True:
            gevent.sleep(0)
            i = self.ses.expect_list([reVMPROMPT, reLOGIN, reAUTH, rePASSWD, pe.TIMEOUT, pe.EOF])
            if i <= 0:
                self.user      = self.ses.match.group(2)
                self.phys_host = self.ses.match.group(3)
                self.virt_host = self.ses.match.group(4)
                self.prompt    = self.ses.match.group(1)
                print('Successful Login to "{}" (user "{}", phy host "{}")'.format(
                    self.virt_host, self.user, self.phys_host))
                break
            elif i == 1:
                continue
            elif i == 2:
                self.ses.sendline('yes')
            elif i == 3:
                if self.pwd is None:
                    raise Exception('Missing required password for login')
                self.ses.sendline(self.pwd)
            elif i == 4:
                raise Exception('Timeout ({}) detected'.format(self.ses.timeout))
            elif i == 5:
                raise Exception('EOF detected - connection refused')

    def disconnect(self):
        '''Disconnects from the VM'''
        try:
            if self.ses.isalive():
                print('Disconnecting from VM "{}"'.format(self.virt_host))
                self.ses.sendline('exit')
                gevent.sleep(0)
                self.ses.expect_list([reANY])
                self.ses.close(True)
                if self.ses.isalive():
                    try:    self.ses.terminate(True)
                    except: pass
        except:
            pass

def start_vm_pool(vm_list, stagger=0, async=True, user=None, pwd=None, timeout=15):
    gl_list = list()
    for vm in vm_list:
        g = gevent.Greenlet(perf_check, vm, user, pwd, timeout)
        if async is True:
            g.start()
            gevent.sleep(0)
        else:
            g.run()
        gl_list.append(g)
        if stagger:
            gevent.sleep(stagger)
    print('Number of started greenlets:', len(gl_list))
    alive_glts = [x for x in gl_list if not x.ready()]
    return alive_glts

def wait_for_vm_pool(gl_list, timeout=240):
    print('Waiting for all ({}) greenlets to complete (timeout: {} secs)'.format(len(gl_list), timeout))
    t1 = dt.datetime.now()
    while (dt.datetime.now()-t1).total_seconds() < timeout:
        alive_glts = [x for x in gl_list if not x.ready()]
        if alive_glts:
            gevent.sleep(0.5)
            continue
        break
    else:
        print('Killing all greenlets as waiting timeout ({}) expired'.format(timeout))
        gevent.killall(gl_list)
        print('Killed them all')

def perf_check(vm, user, pwd, timeout):
    try:
        print('-- [{}] Connecting'.format(vm))
        gevent.sleep(0)
        vm_obj =  vm_sync(vm, user, pwd, timeout)
        print('-- [{}] Running check'.format(vm))
        gevent.sleep(0)
        if rwchk.chkOsSudo(ses=vm_obj.ses, sesPrmpt=vm_obj.prompt, log=None)():
            p = '--'
            r = 'passed'
        else:
            p = '**'
            r = '*** failed ***'
        print('{} [{}] Check {}'.format(p, vm, r))
    except Exception as e:
        print('** [{}] Exception: {}'.format(vm, str(e)))
    finally:
        print('-- [{}] Disconnecting'.format(vm))
        try:
            vm_obj.disconnect()
            print('-- [{}] Disconnected'.format(vm))
        except Exception as e:
            pass
