
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
Description:    Constants, functions, classes for common environment checks.
                Can be invoked from testing or operational code.
Version:        0.1
Creation Date:  May 23, 2014
Author:         Andrew Golovanov
License:        RIFTio, Inc.

How to add new checks:
---------------------
- create a new subclass of the _chkBase class
- create a description of the check (as a doc string under the class' header)
- override chkEnv() and optionally fixEnv() methods
  (see the descriptions in the _chkBase class for details)
- if methods of your subclass use optional arguments (passed in to the __call__
  method and saved in the self.args data member), make sure you document those
  additional arguments for users in the description of your subclass

Communicating with local vs. remote hosts:
-----------------------------------------
Same checks may need to be applicable on local and (in other cases) on
remote hosts. To facilitate such versatility, the _chkBase class provides
special method "talk" which hides communication details with a host and can
send shell commands and receive responses to/from either the local host or
the remote host depending on the "ses" argument provided by instantiating
the subclass.

You should use the talk method in your re-defined subclass methods getEnv
and fixEnv if you need to have your checks working with both local and
remote hosts.

Debugging new checks:
--------------------
If command line contains -DEBUG option, then the flag DBGMODE is set to
True, and this activates extra logging for every command and response
in the talk method. In addition, you may use the DBGMODE flag
to provide increased output from the subclasses.
'''

#--- EXTERNAL MODULES

import sys, re, subprocess as sbpr

#--- CONSTANTS

DBGMODE = '-DEBUG' in sys.argv
DBGPREF = '[***DEBUG***] {}'

#--- BASE CHECK CLASS

class _chkBase(object):
    '''
    A base check template.
    This class is supposed to be subclassed, and the corresponding methods
    need to be overridden to implement the specifics of a particulat check.

    Basic workflow supported by this class:

        2. chkEnv(): check environment against some expected conditions
        3. fixEnv(): try to change environment if the previous step failed
                     and make sure that the check has eventually succeeded

    If chkEnv() is not overridden in the subclass:
        - the check is considered not implemented

    If fixEnv() is not overridden in the subclass:
        - this is a read-only (passive) check that verifies but does not
          change the environment

    If both methods are overridden in the subclass:
        - this is a forcing (active) check that tries to change the environment

    Usage:
    -----
    To use any checks based on this class, call instance of the subclass,
    for example:

        myCheck(ses=sesObj)()

    Within the first pair of parentheses you may provide non-default arguments
    passed in to the constructor. Within the second pair of parentheses
    you may provide optional arguments that will be saved as a tuple
    in the self.args data member and can be used in the methods of subclass.

    Return:
    ------
    This call will return True if the check succeeded or False otherwise.
    It may raise exceptions if the check is not implemented, or some input
    arguments to the constructor are wrong, or if an exception was intentionally
    raised in the methods of subclass.

    Fix Flag:
    --------
    Sometimes the calling script needs to know whether actual changes ("fix") were
    made in fixEnv method of this check. The data member self.fix is True when
    actual changes were made and False if there were no changes. To get this
    information use slightly different pattern:

         o = myCheck()      # initialize the check
         o()                # execute the check
         if o.fix is True:
             ... do something here ...
    '''

    def chkEnv(self):
        '''
        === This method is supposed to be overridden from a subclass. ===
        Performs checks on the data retrieved from environment.

        If not overridden, will immediately raise an exception.
        If overridden, must have the following signature:

        Arguments:      none
        Return:         True if the check succeeded or False otherwise,
        '''
        raise Exception('{} not implemented'.format(self.pref))

    def fixEnv(self):
        '''
        === This method is supposed to be overridden from a subclass. ===
        This method tries to change the environment in order
        to make the check pass.

        If not overridden, this method returns False because it is called
        only when the previous step (chkEnv) has indicated a failure and
        the default implementation of this method does not know how to fix
        the issue in the general case.

        If overridden, must have the following signature:

        Arguments:      none
        Return:         True if fix needs to be confirmed with an additional
                        check or False otherwise

        Note: don't forget to set self.fix to True if actual changes to the
              environment were made
        '''
        return False

    def __init__(self, log=True, ses=None, sesPrmpt=None):
        '''
        Arguments:

            log      - defines logging of major steps from this class only
                       (i.e. subclass can use its own explicit logging):
                       None     - no logging from this class
                       True     - use the standard print function for logging
                       <object> - an external logging function that takes
                                  a single string argument (a message string)
            ses      - defines the communications with the host where the
                       environment is retrieved from:
                       None     - communicate with the local host via subprocess
                       <object> - an opened session object to the remote host;
                                  currently following types of remote sessions
                                  are supported:
                                      - pexpect.spawn
            sesPrmpt - optional prompt that indicates the end of output after
                       executing every command on the remote host; if not
                       specified, the standard bash shell prompt will be used
                       instead
        '''
        # Save constructor arguments for use in sub-checks
        # that may be called from within this class
        self._args = (log, ses, sesPrmpt)
        # Logging
        self.pref = 'Check "{}":'.format(self.__class__.__name__)
        if DBGMODE:
            self.dprf = DBGPREF.format(self.pref)
        if log not in (True, None) and not callable(log):
            raise Exception('{} invalid value of the "log" argument: {} (type: {})'.format(
                self.pref, str(log), type(log)))
        self.__logfunc = log

        # Session
        if ses and not (hasattr(ses, 'expect_list') and hasattr(ses, 'sendline')):
            raise Exception('{} unsupported session type "{}"'.format(type(ses)))
        self.__ses = ses

        # Session prompt
        if ses:
            self.__sesPrmpt = [re.compile(sesPrmpt) if sesPrmpt else re.compile('(?m)^[^$\n]*[$](?!\S) ?')]
        else:
            self.__sesPrmpt = None

        # Fix flag
        self.fix = False

    def __call__(self, *args):
        '''
        Implements basic workflow.

        Arguments:

             args    - this tuple of optional positional arguments is saved "as is"
                       in the data member self.args and can be used in the methods
                       of subclass; one use case for these optional arguments may be
                       passing expected values to the chkEnv method; note that it is
                       completely up to the subclass methods how these arguments are used
        '''
        self.args = args

        res = self.chkEnv()             # Perform the check(s)
        if res is True:
            self.log('{} succeeded'.format(self.pref))
        else:
            self.log('{} failed'.format(self.pref))
            if self.fixEnv() is True:   # Try to fix
                res = self.chkEnv()     # Perform the second check
                self.log('{} fix {}'.format(self.pref, 'succeeded' if res is True else 'failed'))
        return res

    def log(self, msg):
        '''
        This method performs logging from this class and also can be used in subclasses.
        '''
        if self.__logfunc:
            if callable(self.__logfunc):
                self.__logfunc('{}\n'.format(msg))
            else:
                print(msg)

    def talk(self, cmd, sesPrmpt=None, tmOut=None, strip=True):
        '''
        This method sends a command to and receives (and returns) a response
        from either the local or a remote host depending on the session object
        provided by instantiating the class.

        This method may be used in subclass methods to communicate with either
        the local or a remote host (in the universal way).

        Arguments:

            cmd      - command to execute on the host
            sesPrmpt - optional prompt that indicates the end of output after
                       executing the command on the remote host (ignored when
                       the command is executed on the local host); if None,
                       then the prompt defined in the constructor will be used
            tmOut    - optional max timeout (secs) when waiting for the prompt
                       after executing the command on the remote host (ignored
                       when the command is executed on the local host);
                       if None, a default session timeout will be used
            strip    - if True, strip whitespaces on both sides of the output
                       string

        Return:

            output received after executing the command

        Exceptions:

            this method may also return exception if the command timed out
            on the remote host or returned non-zero exit status on the local host
        '''
        ses = self.__ses
        if DBGMODE:
            self.log('{} Using {} host\n{} Request:  "{}"'.format(self.dprf, 'remote' if ses else 'local', self.dprf, cmd))
        if ses:
            # For remote host currently assuming pexpect session only
            ses.sendline(cmd)
            ses.expect_list(
                self.__sesPrmpt if sesPrmpt is None else [re.compile(sesPrmpt)],
                -1 if tmOut is None else float(tmOut))
            res = ses.before.partition('\n')[2]
        else:
            # Local host
            res = sbpr.check_output(cmd, shell=True)
        if DBGMODE:
            self.log('{} Response: "{}"'.format(self.dprf, res))
        return res.strip() if strip else res

    def _chkArgs(self, argn=None, softCheck=True):
        '''
        Auxiliary method to raise exception when check requires additional arguments
        and there were none provided.

        Arguments:

            argn      - optional number of required arguments; if None, no check by this parameter
            softCheck - if True, argn is considered a minimally required number of arguments;
                        if False, argn is considered as the exact number of required arguments
        '''
        if argn is None:
            if not self.args:
                raise Exception('{} missing argument(s)'.format(self.pref))
        else:
            txt = None
            if softCheck     and len(self.args) <  int(argn): txt = '>= '
            if not softCheck and len(self.args) != int(argn): txt = ''
            if txt is not None:
                raise Exception('{} invalid number of arguments: expected {}{}, got {}'.format(self.pref, txt, argn, len(self.args)))

#--------------------------------------------------------------------------------
#--- IMPLEMENTED CHECKS (SUBCLASSES OF _chkBase)
#--------------------------------------------------------------------------------

#--- OS Checks

class chkOsSudo(_chkBase):
    '''
    Simplest possible check that confirms that current user does have sudo permissions.
    This is a passive check as it does not have the "fix" part.

    Additional arguments:    none
    Usage example:           chkOsSudo()()
    '''
    def chkEnv(self):
        try:
            r = 'Sorry' not in self.talk('sudo -v 2>&1')
        except:
            r = False
        return r

class chkOsRelease(_chkBase):
    '''
    Verify that the output from "uname -r" () contains one or more specific substrings.
    Note this check illustrates how to use extra arguments.

    Additional arguments:    one or multiple strings to lookup in the output of "uname -r";
                             if multiple strings are provided, check succeeds if any
                             of them was found in the output of "uname -r"
    Usage example:           chkKernelRelease()('fc20', 'el6')
    '''
    def chkEnv(self):
        self._chkArgs(1)    # Arguments are required by this check
        skrn = map(str, self.args)
        su = self.talk('uname -r')
        for s in skrn:
            res = s in su
            if res: break
        return res

class chkOsCpuVendor(_chkBase):
    '''
    Verify that the CPU info contains a particular vendor ID.

    Additional arguments:    an expected vendor ID string
    Usage example:           chkCpuVendor()('GenuineIntel')
    '''
    def chkEnv(self):
        self._chkArgs(1, False)   # Arguments are required by this check
        cv = self.talk('cat /proc/cpuinfo | grep vendor_id | uniq')
        return str(self.args[0]) in cv.partition(':')[2]

class chkOsPkgs(_chkBase):
    '''
    Verify that required packages are installed. If not, will try to install them via yum.
    Note that the fix requires sudo permissions and will raise an exception otherwise.

    Additional arguments:    one or more package names
    Usage example:           chkOsPkgs()('qemu', 'qemu-kvm-tools', 'libvirt-daemon-qemu')
    '''
    def chkEnv(self):
        self._chkArgs(1)    # Arguments are required by this check
        self.missPkgs = self.talk('rpm -q {} | grep "not installed" | gawk "{}"'.format(
            ' '.join(self.args), '{print \$2}')).split()
        self.missPkgs = ' '.join(self.missPkgs)
        if DBGMODE:
            self.log('{} Missing packages: {}'.format(self.dprf, self.missPkgs))
        return not self.missPkgs
    def fixEnv(self):
        if chkOsSudo(*self._args)() is False:
            raise Exception('No sudo permission for the user to fix this check')
        try:
            r = self.talk('sudo yum -y install {}'.format(self.missPkgs), sesPrmpt='Complete!')
            self.fix = True
            if DBGMODE: self.log('{} {}'.format(self.dprf, r))
        except Exception as e:
            if DBGMODE: self.log('{} {}'.format(self.dprf, str(e)))
            r = None
        return r is not None

class chkOsFaclUsr(_chkBase):
    '''
    Set file access control for a specified user, permission and path.
    This "check" fails if the user has no permissions for this operation.

    Note this is not a real check because it immediately tries to provide
    the fix even within the chkEnv method.

    Additional arguments (3): username, permission ('r', 'w', 'x'), pathname
    Usage example:            chkOsFaclUsr()('qemu', 'x', '/root')
    '''
    def chkEnv(self):
        self._chkArgs(3, False)    # Arguments are required by this check
        try:
            r = self.talk('setfacl -m u:{}:{} {}'.format(*self.args))
            if DBGMODE: self.log('{} {}'.format(self.dprf, r))
        except Exception as e:
            if DBGMODE: self.log('{} {}'.format(self.dprf, str(e)))
            r = None
        return r is not None

class chkOsNuma(_chkBase):
    '''
    Verify that NUMA is supported by the OS/BIOS:
    compares a number of sockets vs. a number of numa nodes
    and makes sure that this number is non-zero.

    Additional arguments(1): optional: any non-zero value which evaluates to True
                             if the number of numa nodes should be discovered via
                             lscpu instead of lstopo (which is the default way)
    Usage example:           chkOsNuma()()
    '''
    def chkEnv(self):
        sn        = self.talk('lscpu | grep Socket | gawk "{}"'.format('{print \$2}'))
        nodeCmd   = 'lscpu | grep "NUMA node(s)" | gawk "{}"'.format('{print \$3}') \
            if len(self.args) > 0 and self.args[0] else 'lstopo 2> /dev/null | grep -i numanode | wc -l'
        nn        = self.talk(nodeCmd)
        r = sn == nn
        if r:
            if int(sn) <= 0:
                r = False
                self.log('{} number of CPU sockets ({}) is not positive'.format(self.pref, sn))
        else:
            self.log('\n{} '.format(self.pref).join((
                'NUMA node count ({}), does not match the socket count ({})'.format(nn, sn),
                'Update the BIOS setting "NUMA Optimized" to "Enabled"',
                'Details: http://boson.eng.riftio.com/wiki/doku.php?id=server_settings',
                )))
        return r

class chkOsIommu(_chkBase):
    '''
    Verify that kernel is started with IOMMU support.

    Additional arguments:    none
    Usage example:           chkOsIommu()()
    '''
    def chkEnv(self):
        r = self.talk('cat /proc/cmdline')
        if DBGMODE: self.log('{} {}'.format(self.dprf, r))
        return 'intel_iommu=on' in r and 'iommu=pt' in r

class chkOsBridgeIntf(_chkBase):
    '''
    Verify that the public interface is part of the bridge

    Additional arguments(2): bridge name, interface name
    Usage example:           chkOsBridgeIntf()('br100', 'ens2f1')
    '''
    def chkEnv(self):
        self._chkArgs(2, False)    # Arguments are required by this check
        try:    r = bool(self.talk('/usr/sbin/brctl show {} | grep {}'.format(*self.args)))
        except: r = False
        return r

class chkOsHugePages(_chkBase):
    '''
    Verifies that the current number of huge pages displayed in /proc/meminfo.

    If an optional required number of huge pages is specified, compares
    with that number and tries to set the number of configured huge pages
    to that number but ONLY when the number of configured huge pages is
    lower than the required one.

    If a required number of huge pages is not specified, than compares current
    number of huge pages with one configured in vm.nr_hugepages. If the required
    number is not specified, this check does not try to reconfigure the kernel.

    Additional arguments:    optional: required number of huge pages (integer)
    Usage example:           chkOsHugePages()(4608)
    '''
    def chkEnv(self):
        na = int(self.talk('grep HugePages_Total /proc/meminfo | gawk "{}"'.format('{print \$2}')))
        nc = int(self.talk('sysctl -n vm.nr_hugepages') if len(self.args) <= 0 else str(self.args[0]))
        self.log('{} {} huge pages: {}, actual huge pages: {}'.format(
            self.pref, 'configured' if len(self.args) <= 0 else 'required', nc, na))
        return nc <= na
    def fixEnv(self):
        if len(self.args) <= 0:
            return False
        if chkOsSudo(*self._args)() is False:
            raise Exception('{} no permissions to configure huge page number'.format(self.pref))
        try:
            self.talk('sudo sysctl -w vm.nr_hugepages={}'.format(self.args[0]))
            self.fix = True
            r = True
        except Exception as e:
            r = False
            self.log('{} {}'.format(self.pref, str(e)))
        return r

class chkOsIpForw(_chkBase):
    '''
    Tries to allow IP forwarding in the kernel (requires sudo permissions).

    Additional arguments:    none
    Usage example:           chkOsIpForw()()
    '''
    def chkEnv(self):
        r = self.talk('sysctl -n net.ipv4.ip_forward')
        if DBGMODE: self.log('{} net.ipv4.ip_forward = {}'.format(self.dprf, r))
        return r == '1'
    def fixEnv(self):
        try:
            self.talk('sysctl -w net.ipv4.ip_forward=1')
            self.fix = True
            if DBGMODE: self.log('{} {}'.format(self.dprf, r))
            r = True
        except:
            self.log('{} no permissions to set IP forwarding'.format(self.pref))
            r = False
        return r

class chkOsIpRpf(_chkBase):
    '''
    Verifies and sets (if necessary) the all and the default reverse path filtering
    in the kernel.

    Additional arguments:    all_value (0 or 1), default_value (0 or 1)
    Usage example:           chkOsIpRpf()(1, 1)
    '''
    def chkEnv(self):
        self._chkArgs(2, False)        # Exactly two arguments are required by this check
        allRpf = self.talk('sysctl -n net.ipv4.conf.all.rp_filter')
        defRpf = self.talk('sysctl -n net.ipv4.conf.default.rp_filter')
        return allRpf == str(self.args[0]) and defRpf == str(self.args[1])
    def fixEnv(self):
        if chkOsSudo(*self._args)() is False:
            raise Exception('{} no permissions to set RPF in the kernel'.format(self.pref))
        try:
            self.talk('sudo sysctl -w net.ipv4.conf.all.rp_filter={}'.format(self.args[0]))
            self.fix = True
            r = True
        except:
            r = False
        try:
            self.talk('sudo sysctl -w net.ipv4.conf.default.rp_filter={}'.format(self.args[1]))
            self.fix = True
            r = r and True
        except:
            r = False
        return r

class chkOsServ(_chkBase):
    '''
    Verifies if specified service(s) is(are) active.

    Additional arguments:    a name of the service
    Usage example:           chkOsNetwServ()('network', 'nfs-idmap')
    '''
    def chkEnv(self):
        self._chkArgs(1)        # At least one argument is required by this check
        naSrv = list()
        for srv in self.args:
            try:    r = 'active' in self.talk('systemctl is-active {}.service'.format(srv))
            except: r = False
            if not r: naSrv.append(srv)
        if naSrv:
            self.log('{} non-active services: {}'.format(self.pref, ', '.join(naSrv)))
        return len(naSrv) <= 0

#--- VM Checks

class chkVmHost(_chkBase):
    '''
    This check succeeds if the host is a VM and fails if it is bare metal.
    Requires sudo permissions.
    This is a passive check as it does not have the "fix" part.

    Additional arguments:    none
    Usage example:           chkVmHost()()
    '''
    def chkEnv(self):
        if chkOsSudo(*self._args)() is False:
            raise Exception('Sudo permissions required for this check')
        try:
            r = self.talk('sudo dmidecode -s system-product-name').lower()
            r = 'openstack' in r or 'kvm' in r or 'virtual' in r or 'bochs' in r or 'vmware' in r
        except:
            r = False
        return r
