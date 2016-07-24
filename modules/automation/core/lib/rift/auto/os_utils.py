#!/usr/bin/env python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import os
import re
import logging
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

if __name__ == '__main__':
    port = get_confd_port()
    fixup_confd()
    print("port is %d, confd has been modified" % port)


