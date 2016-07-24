
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import re
import logging
import rw_status
import rwlogger
import subprocess, os

import gi
gi.require_version('RwVeVnfmEm', '1.0')
gi.require_version('RwTypes', '1.0')
from gi.repository import (
    GObject,
    RwVeVnfmEm,
    RwTypes)

logger = logging.getLogger('rw_ve_vnfm_em.rest')


rwstatus = rw_status.rwstatus_from_exc_map({ IndexError: RwTypes.RwStatus.NOTFOUND,
                                             KeyError: RwTypes.RwStatus.NOTFOUND,
                                             NotImplementedError: RwTypes.RwStatus.NOT_IMPLEMENTED,})

class RwVeVnfmEmRestPlugin(GObject.Object, RwVeVnfmEm.ElementManager):
    """This class implements the Ve-Vnfm VALA methods."""

    def __init__(self):
        GObject.Object.__init__(self)


    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(rwlogger.RwLogger(subcategory="rwcal-aws",
                                                log_hdl=rwlog_ctx,))
    @rwstatus
    def do_vnf_lifecycle_event(self):
        pass
        
