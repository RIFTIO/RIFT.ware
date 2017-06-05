
# STANDARD_RIFT_IO_COPYRIGHT

import os
import glob
import subprocess
import shlex

import abc
import collections
import inspect
import tempfile
import weakref
import logging

class UnlockedError(Exception):
    pass

def report_crash(vm_name):
    """
    This function does the 'crash report'
    """
    crashes = []
    ldir = "/var/log/rift/vm-cores"
    ldir_cores = glob.glob(os.path.join(ldir, 'reported-%s-core.*.txt' %(vm_name)))
    curdir_cores = glob.glob(os.path.join(os.path.realpath('.'), 'reported-%s-core.*.txt' %(vm_name)))
    #print(ldir_cores)
    #print(curdir_cores)
    for f in ldir_cores + curdir_cores:
        try:
            stat = os.stat(f)
            if stat.st_uid == 0:
                sudo_chmod_cmd = "/usr/bin/sudo --non-interactive /bin/chmod 777 {}".format(f)
                subprocess.check_call(shlex.split(sudo_chmod_cmd))
            with open(f,'r') as fp:
                crashes.append(fp.read())
        except Exception as e:
            logging.error(str(e))

    return crashes
