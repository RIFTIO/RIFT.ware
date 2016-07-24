
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import re
import logging
import rw_status
import rwlogger
import subprocess, os

import gi
gi.require_version('RwVeVnfmVnf', '1.0')
gi.require_version('RwTypes', '1.0')
from gi.repository import (
    GObject,
    RwVeVnfmVnf,
    RwTypes)

logger = logging.getLogger('rwve-vnfm-vnf-rest')


rwstatus = rw_status.rwstatus_from_exc_map({ IndexError: RwTypes.RwStatus.NOTFOUND,
                                             KeyError: RwTypes.RwStatus.NOTFOUND,
                                             NotImplementedError: RwTypes.RwStatus.NOT_IMPLEMENTED,})

class RwVeVnfmVnfRestPlugin(GObject.Object, RwVeVnfmVnf.Vnf):
    """This class implements the Ve-Vnfm VALA methods."""

    def __init__(self):
        GObject.Object.__init__(self)

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(rwlogger.RwLogger(subcategory="rwve-vnfm-vnf-rest",
                                                log_hdl=rwlog_ctx,))

    @rwstatus
    def do_get_monitoring_param(self):
        pass
        
