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
# Author(s): Austin Cormier
# Creation Date: 09/09/2014
# 
#
# Automation VM Utility function/classes to easily perform common automation
# operations across many VM's

import collections
import hashlib
import itertools
import logging
import os
import shlex
import subprocess
import sys
import time
import concurrent.futures

import rift.auto.ip_utils

logger = logging.getLogger(__name__)

RIFT_INSTALL = os.environ["RIFT_INSTALL"]

# TODO: This should probably be in a environment variable
KMOD_DIR = os.path.join(RIFT_INSTALL, "usr/lib/modules/3.12.9-301.fc20.x86_64/extra/dpdk/kmod")
KMOD_MODULES = ["igb_uio", "rte_kni"]

def md5checksum(fname):
    """
    This calculates md5 check for a given file

    Argument:
        fname - file name for the checksum

    Return:
        checksum as hex
    """
    hash_md5 = hashlib.md5()
    with open(fname, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def run_command_on_vm(ip_address, command_str, root=False):
    '''
    this is not as straightforward as it seems
    we can be root on a host, root on a VM, a regular user on the VM or a regular user on a host
    '''
    if root and os.geteuid() != 0:
        logger.info('Skipped running command because user is non-root - %s', command_str)
        return 0
    else:
        fmt = "ssh -q -o StrictHostKeyChecking=no -o BatchMode=yes -o ConnectTimeout=10 -t -t {ip_address} \"{command}\""
    ssh_command = fmt.format(ip_address=ip_address, command=command_str)
    logger.info('ssh command: {}'.format(ssh_command))

    command_args = shlex.split(ssh_command)
    result = subprocess.check_output(command_args)
    logger.info(result)
    return result

def rmmmod_module(ip_address, module_name):
    rmmod_cmd = "sudo rmmod {module_name}".format(module_name=module_name)
    run_command_on_vm(ip_address, rmmod_cmd, root=True)


def insmod_module(ip_address, module_path, module_name):
    insmod_cmd = "sudo insmod {module_path}/{module_name}.ko".format(module_path=module_path,
                                                                     module_name=module_name)
    run_command_on_vm(ip_address, insmod_cmd, root=True)


def modprobe_module(ip_address, module_name):
    modprobe_cmd = "sudo modprobe {module_name}".format(module_name=module_name)
    run_command_on_vm(ip_address, modprobe_cmd, root=True)


def install_kmod_drivers(ip_address):
    """ Installs the neccessary kmod drivers from the current workspace on a VM

    Args:
      ip_address:  The VM IP address to install the kmod drivers on.

    Returns:
      True if latest kmod drivers were successfully installed
      False otherwise
    """
    rift_install = os.environ["RIFT_INSTALL"]
    script = "%s/scripts/cloud/install_kernel_mods --rift-install %s" % (rift_install, rift_install)
    try:
        run_command_on_vm(ip_address, script, root=True )
    except subprocess.CalledProcessError:
        logger.critical("error running %s on %s", script, ip_address)
        return False
    return True

    '''

    for module in KMOD_MODULES.reverse():
        try:
            rmmmod_module(ip_address, module)
        except subprocess.CalledProcessError:
            pass

    try:
        modprobe_module(ip_address, "uio")
    except subprocess.CalledProcessError:
        logger.critical("modprobe uio failed")
        return False

    for module in KMOD_MODULES:
        try:
            insmod_module(ip_address, KMOD_DIR, module)
        except subprocess.CalledProcessError:
            logger.critical("error loading kernel module %s" % module)
            return False

    return True
    '''


def can_ssh_into_ip(ip):
    """ Tests SSHing into an ip address and running sudo.

    Returns:
      True if we are able to SSH into a VM and run "sudo echo hi"
      False otherwise

    """
    try:
        run_command_on_vm(ip, "sudo echo -n")
        run_command_on_vm(ip, "sudo echo -n", root=True)
    except subprocess.CalledProcessError:
        return False

    return True


def has_pgrep_command(ip):
    """ Tests whether a vm has the pgrep command.

    Returns:
      True if pgrep is in the PATH, False otherwise.
    """

    try:
        run_command_on_vm(ip, "which pgrep")
    except subprocess.CalledProcessError:
        return False

    return True


def kill_rwmain(ip):
    """ Kills and ensures that rwmain is not running on a VM
        NOTE
            If pgrep returned an error, this means that no rwmain
            processes were found.
    """
    try:
        run_command_on_vm(ip, "pgrep rwmain | sudo xargs -i kill -9 {}", root=True)
    except subprocess.CalledProcessError:
        pass

    try:
        run_command_on_vm(ip, "pgrep rwmain")
    except subprocess.CalledProcessError:
        return True

    # lets try again in a few seconds rather than failing right away
    time.sleep(3)

    try:
        run_command_on_vm(ip, "pgrep rwmain")
    except subprocess.CalledProcessError:
        return True

    return False


def kill_tmux(ip):
    try:
        run_command_on_vm(ip, "killall -9 tmux", root=True)
    except subprocess.CalledProcessError:
        pass

    return True


def delete_netns(ip):
    try:
        run_command_on_vm(ip, "for netns in `ip netns`; do ip netns delete $netns; done",
                          root=True)
    except subprocess.CalledProcessError:
        pass

    return True

def run_pgrep_process_matcher(vm_ip, process_matchers):
    """Run pgrep process matcher on a VM.

    Use the pgrep regex match patterns specified in the process_matchers to
    check if a process is running on the VM.

    Arguments:
        vm_ip            - ip address of the VM
        process_matchers - a list of ProcessMatcher objects specifying the
                           processes to match and their associated pgrep patterns

    Returns:
        a set of matched process names
    """

    matched_processes = set()
    pgrep_pattern = '|'.join(matcher.pattern for matcher in process_matchers)
    pgrep_command = "pgrep --list-full -f '%s'" % pgrep_pattern
    try:
        pgrep_result = run_command_on_vm(vm_ip, pgrep_command, root=True)
        for pgrep_line in pgrep_result.splitlines():
            for matcher in process_matchers:
                if matcher.process_name in str(pgrep_line):
                    matched_processes.add(matcher.process_name)
                    break
    except subprocess.CalledProcessError:
        pass
    return matched_processes


class VmPreparer(object):
    """ Prepares a set of VM's in order to run in expanded mode."""
    def __init__(self, ip_list=None, delete_ip_netns=True):
        """ Create a VMPreparer Object

        Args:
          ip_list: A list of VM ip_addresses with which to apply operations to.
          delete_ip_netns: Delete ip network namespaces if True

        Raises:
          RuntimeError: If any ip is an invalid ipv4 addresses.
        """

        if ip_list is None:
            ip_list = ["127.0.0.1"]

        for ip in ip_list:
            if not rift.auto.ip_utils.is_valid_ipv4_address(ip):
                raise ValueError("Invalid VM IP address: %s" % ip)

        self._ip_list = ip_list
        self._delete_ip_netns = delete_ip_netns

    def check_ssh_connections(self):
        """ Ensure that we are able to SSH into all VM's

        Raises:
          RuntimeError: If we are unable to SSH into any VM.
        """

        logger.info("Testing SSH Connections to IP's: {}".format(self._ip_list))
        with concurrent.futures.ProcessPoolExecutor() as executer:
            for ip, result in zip(self._ip_list, executer.map(can_ssh_into_ip, self._ip_list)):
                if not result:
                    raise RuntimeError("Unable to pgrep command on VM: %s" % ip)

            for ip, result in zip(self._ip_list, executer.map(has_pgrep_command, self._ip_list)):
                if not result:
                    raise RuntimeError("Unable to SSH into VM: %s" % ip)

        os.system("stty sane")

    def install_kmod_drivers(self):
        """Attempt to reinstall kmod drivers from the current workspace.

        Raises:
          RuntimeError: If installing latest kmod drivers fails for any VM.
        """
        logger.info("Installing Kmod drivers to IP's: {}".format(self._ip_list))
        with concurrent.futures.ProcessPoolExecutor() as executer:
            for ip, result in zip(self._ip_list, executer.map(install_kmod_drivers, self._ip_list)):
                if not result:
                    raise RuntimeError("Unable to install kmod drivers on VM: %s" % ip)

        os.system("stty sane")

    def kill_rwmain_processes(self):
        """Kill any running rwmain processes on the VM's

        Raises:
          RuntimeError: If into a VM fails.
        """
        logger.info("Killing Rwmain processes on IP's: {}".format(self._ip_list))
        with concurrent.futures.ProcessPoolExecutor() as executer:
            for ip, result in zip(self._ip_list, executer.map(kill_rwmain, self._ip_list)):
                if not result:
                    raise RuntimeError("Unable to kill rwmain processes on VM: %s" % ip)

        os.system("stty sane")

    def kill_tmux_processes(self):
        """Kill any running tmux processes on the VM's

        Raises:
          RuntimeError: If SSHing into a VM fails.
        """
        logger.info("Killing Tmux processes on IP's: {}".format(self._ip_list))
        with concurrent.futures.ProcessPoolExecutor() as executer:
            for ip, result in zip(self._ip_list, executer.map(kill_tmux, self._ip_list)):
                if not result:
                    raise RuntimeError("Unable to kill tmux processes on VM: %s" % ip)

        os.system("stty sane")

    def delete_ip_netns(self):
        """Delete all ip network namespaces on the VM's

        Raises:
          RuntimeError: If SSHing into a VM fails.
        """
        logger.info("Deleting ip network namespaces on IP's: {}".format(self._ip_list))
        with concurrent.futures.ProcessPoolExecutor() as executer:
            for ip, result in zip(self._ip_list, executer.map(delete_netns, self._ip_list)):
                if not result:
                    raise RuntimeError("Unable to delete ip network namespaces on VM: %s" % ip)

        os.system("stty sane")

    def prepare_vms(self):
        self.check_ssh_connections()
        self.kill_rwmain_processes()
        self.kill_tmux_processes()
        if os.environ.get('NO_KERNEL_MODS', '0') == '0':
            self.install_kmod_drivers()
        if self._delete_ip_netns:
            self.delete_ip_netns()
        run_command_on_vm(self._ip_list[0], "rm -rf  %s/zk/server*" % os.environ.get('RIFT_INSTALL'), True)
        run_command_on_vm(self._ip_list[0], "rm -rf  %s/var/rift/tmp*" % os.environ.get('RIFT_INSTALL'), True)


class ProcessMatcher(object):
    """
    This class is a helper class that specifies a process name and its
    associated pgrep regex match pattern to find the process using pgrep.
    """

    def __init__(self, process_name, pattern):
        """Create a ProcessMatcher object.

        As an example, the following ProcessMatcher object can be used to
        seach dnsmasq process,

            ProcessMatcher('dnsmasq', r'^\S*?\bdnsmasq\b')

        Arguments:
            process_name - the name of the process to match
            pattern      - the pgrep regex pattern to find the process

        """
        self.process_name = process_name
        self.pattern = pattern


class ProcessChecker(object):
    """
    This class checks if specified processes are running.
    """

    def __init__(self, ip_list, process_matchers):
        """Create a ProcessChecker object.

        Arguments:
            ip_list          - a list of ip addresses of VMs to check processes
            process_matchers - a list of ProcessMatcher objects specifying the
                               processes to match and their associated pgrep
                               patterns

        """
        self._ip_list = ip_list
        self._process_matchers = process_matchers

    def check(self):
        """Check if the processes exist on the specified VM based on the provided
        process_matchers.

        Returns:
            a list of existing processes and the ip addresses of the VMs on which
            the processes are running. Each list entry is a namedtuple consisting
            of 'process_name' and 'vm_ip' attributes.
        """
        matched_processes = []
        ProcessIpPair = collections.namedtuple('ProcessIpPair', 'process_name vm_ip')
        with concurrent.futures.ProcessPoolExecutor() as executer:
            for ip, result in zip(self._ip_list, executer.map(run_pgrep_process_matcher,
                    self._ip_list, itertools.repeat(self._process_matchers))):
                 for process_name in result:
                     matched_processes.append(ProcessIpPair(process_name, ip))
        return matched_processes


if __name__ == "__main__":
    ip_list = sys.argv[1:]

    if len(ip_list) < 1:
        "USAGE: ./vm_utils.py <ip> [<ip>].."
        sys.exit(1)

    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')
    logging.getLogger().setLevel(logging.DEBUG)

    utils = VmPreparer(ip_list)
    utils.check_ssh_connection()
    utils.kill_rwmain_processes()
    utils.install_kmod_drivers()
