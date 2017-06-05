#!/usr/bin/env python

# STANDARD_RIFT_IO_COPYRIGHT


import os
import re
import logging
from pwd import getpwuid
import gi.repository.rwlib as rwlib
import gi.repository.RwTypes as rwtypes

logger = logging.getLogger(__name__)

def get_confd_port():
    status, port= rwlib.unique_port(3300)
    if status != rwtypes.RwStatus.SUCCESS:
        raise Exception("rwlib unique_port failed")
    return port

def fixup_confd():
    '''
    fix up confd.conf.collapsed to use a unique port
    '''
    port = get_confd_port()
    conffile = os.path.join(os.environ.get('RIFT_ROOT'), ".install/usr/local/confd/etc/confd/confd.conf.collapsed")
    if not os.path.exists(conffile):
        raise Exception("confd files %s not found" % conffile)
    port_re = re.compile("(.*<port>).*(</port>.* <!-- NETCONF PORT.*)")
    with open(conffile, "r") as fp:
        txt = fp.readlines()
    for linenum in range(len(txt)):
        m = port_re.match(txt[linenum])
        if m is not None:
            txt[linenum] = m.group(1) + str(port) + m.group(2)
            break
    with open(conffile, "w") as fp:
        fp.writelines(txt)
    logger.debug("updated %s to use port %d" % ( conffile, port))

def get_mem_usage():
    """Returns used memory(MB) of the system. System can be a docker container or a VM.
    It collects the mem usage(rss) through /sys/fs/cgroup/memory/memory.stat.
    """
    total_rss, total_cache = system_rss_cache()
    print('Current mem_usage: total_rss - {rss}, total_cache - {cache}'.format(rss=total_rss, cache=total_cache))
    return total_rss

def print_mem_usage():
    """Prints the user name, pid, Rss, Pss, command line for all accessible processes in Rss descending order.
    Also, prints memory usage of the whole system.
    """
    total_rss, total_cache = system_rss_cache()
    process_attrs = []
    # Loop through /proc/<pid> and get owner, pss, rss, cmdline of that process
    for pid in filter(lambda x: x.isdigit(), os.listdir('/proc')):
        try:
            p_rss, p_pss = process_rss_pss(pid)
            process_attrs.append((process_owner(pid), pid, p_rss, p_pss, process_cmdline(pid)))
        except:
            # Ignoring this process: either permission is denied or the process no more exists
            pass

    # Get the max length for each of the columns to use them during fixed width formatting
    owner_len, pid_len, rss_len, pss_len, _ = [max(len(str(x)) for x in line) for line in zip(*process_attrs)]
    
    fmt = '%%-%ds  %%%ds   %%%ds   %%%ds  %%s' % (owner_len, pid_len, rss_len, pss_len)
    print('System mem usage')
    print('='*50)
    print('total_rss - %d MB' %total_rss)
    print('total_cache - %d MB' %total_cache)
    print('='*50)
    print('Memory usage of processes')
    print('='*50)
    print(fmt % ('USER', 'PID', 'RSS', 'PSS', 'CMD_LINE'))

    # Sort the list in Rss descending order 
    process_attrs.sort(key=lambda x: x[2], reverse=True)

    for (user, pid, rss, pss, cmd) in process_attrs:
        if cmd != '':
            print(fmt % (user, pid, int(rss), int(pss), cmd))

def process_cmdline(pid):
    """Returns command line of the process"""
    with open('/proc/%s/cmdline' % pid) as fp:
        return fp.read().replace('\0', ' ').strip()

def process_owner(pid):
    """Returns owner of the process"""
    return getpwuid(os.stat('/proc/%s' % pid).st_uid).pw_name

def process_rss_pss(pid):
    """Returns Rss, Pss of the process in mega-bytes.
    """
    with open('/proc/%s/smaps' % pid) as fp:
        smaps_ = fp.read()

    pss = int(sum([int(x) for x in re.findall('^Pss:\s+(\d+)', smaps_, re.M)])/1024)
    rss = int(sum([int(x) for x in re.findall('^Rss:\s+(\d+)', smaps_, re.M)])/1024)

    return rss, pss

def system_rss_cache():
    with open('/sys/fs/cgroup/memory/memory.stat') as fobj:
        memory_stat = fobj.read()

    total_rss = re.findall('^total_rss\s+(\d+)' , memory_stat, re.M)[0]
    total_cache = re.findall('^total_cache\s+(\d+)' , memory_stat, re.M)[0]

    return int(int(total_rss)/1024/1024), int(int(total_cache)/1024/1024)

def check_mem_usage_threshold(cap1, cap2, threshold_pct=5, threshold_mb=1024):
    """Compares two memory usage captures

    Args:
        cap1: memory usage capture 1
        cap2: memory usage capture 2
        threshold_pct: Allowed increased change in percentage between cap1 and cap2 
        threshold_mb: Allowed increased change in memory usage(in MB) between cap1 and cap2
    """
    print('Comparing mem usage captures: {capture1} {capture2}'.format(capture1=cap1, capture2=cap2))
    mem_usage_diff = cap2-cap1
    pc_change = 100 * mem_usage_diff / cap1

    if mem_usage_diff > threshold_mb or pc_change > threshold_pct:
        print('There is an increase in memory usage by {}% between two captures: {}, {}'.format(pc_change, cap1, cap2))
        return True

    return False

if __name__ == '__main__':
    port = get_confd_port()
    fixup_confd()
    print("port is %d, confd has been modified" % port)


