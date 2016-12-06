
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
