
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import itertools
import logging
import os
import uuid
import time

import ipaddress

import gi
gi.require_version('RwTypes', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwSdn', '1.0')
from gi.repository import (
    GObject,
    RwSdn, # Vala package
    RwTypes,
    RwsdnYang,
    #IetfL2TopologyYang as l2Tl,
    RwTopologyYang as RwTl,
    )

import rw_status
import rwlogger

from rift.topmgr.sdnsim import SdnSim


logger = logging.getLogger('rwsdn.sdnsim')


class UnknownAccountError(Exception):
    pass


class MissingFileError(Exception):
    pass


rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    UnknownAccountError: RwTypes.RwStatus.NOTFOUND,
    MissingFileError: RwTypes.RwStatus.NOTFOUND,
    })


class SdnSimPlugin(GObject.Object, RwSdn.Topology):

    def __init__(self):
        GObject.Object.__init__(self)
        self.sdnsim = SdnSim()
        

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    subcategory="sdnsim",
                    log_hdl=rwlog_ctx,
                )
            )

    @rwstatus(ret_on_failure=[None])
    def do_validate_sdn_creds(self, account):
        """
        Validates the sdn account credentials for the specified account.
        Performs an access to the resources using Keystone API. If creds
        are not valid, returns an error code & reason string

        @param account - a SDN account

        Returns:
            Validation Code and Details String
        """
        status = RwsdnYang.SdnConnectionStatus()
        print("SDN Successfully connected")
        status.status = "success"
        status.details = "Connection was successful"
        #logger.debug('Done with validate SDN creds: %s', type(status))
        return status


    @rwstatus(ret_on_failure=[None])
    def do_get_network_list(self, account):
        """
        Returns the list of discovered networks

        @param account - a SDN account

        """
        logger.debug('Get network list: ')
        nwtop = self.sdnsim.get_network_list( account)
        logger.debug('Done with get network list: %s', type(nwtop))
        return nwtop
