
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import ncclient
import ncclient.asyncio_manager
import re
import time

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwNsmYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwConmanYang', '1.0')
gi.require_version('NsrYang', '1.0')

from gi.repository import (
    NsrYang as nsrY,
    RwYang,
    RwNsmYang as nsmY,
    RwDts as rwdts,
    RwTypes,
    RwConmanYang as conmanY,
)

import rift.tasklets

class ROConfigManager(object):
    def __init__(self, log, loop, dts, parent):
        self._log = log
        self._loop = loop
        self._dts = dts
        self.nsm = parent
        self._log.debug("Initialized ROConfigManager")

    def is_ready(self):
        return True

    @property
    def cm_state_xpath(self):
        return ("/rw-conman:cm-state/rw-conman:cm-nsr")

    @classmethod
    def map_config_status(cls, status):
        cfg_map = {
            'init': nsrY.ConfigStates.INIT,
            'received': nsrY.ConfigStates.CONFIGURING,
            'cfg_delay': nsrY.ConfigStates.CONFIGURING,
            'cfg_process': nsrY.ConfigStates.CONFIGURING,
            'cfg_process-failed': nsrY.ConfigStates.CONFIGURING,
            'cfg_sched': nsrY.ConfigStates.CONFIGURING,
            'connecting': nsrY.ConfigStates.CONFIGURING,
            'failed_connection': nsrY.ConfigStates.CONFIGURING,
            'netconf_connected': nsrY.ConfigStates.CONFIGURING,
            'netconf_ssh_connected': nsrY.ConfigStates.CONFIGURING,
            'restconf_connected': nsrY.ConfigStates.CONFIGURING,
            'cfg_send': nsrY.ConfigStates.CONFIGURING,
            'cfg_failed': nsrY.ConfigStates.FAILED,
            'ready_no_cfg': nsrY.ConfigStates.CONFIG_NOT_NEEDED,
            'ready': nsrY.ConfigStates.CONFIGURED,
        }

        return cfg_map[status]

    @asyncio.coroutine
    def update_ns_cfg_state(self, cm_nsr):
        if cm_nsr is None:
            return

        try:
            nsrid = cm_nsr['id']

            # Update the VNFRs' config status
            gen = []
            if 'cm_vnfr' in cm_nsr:
                gen = (vnfr for vnfr in cm_nsr['cm_vnfr']
                       if vnfr['id'] in self.nsm._vnfrs)

            for vnfr in gen:
                vnfrid = vnfr['id']
                new_status = ROConfigManager.map_config_status(vnfr['state'])
                self._log.debug("Updating config status of VNFR {} " \
                                "in NSR {} to {}({})".
                                format(vnfrid, nsrid, new_status,
                                       vnfr['state']))
                yield from \
                    self.nsm.vnfrs[vnfrid].set_config_status(new_status)

            # Update the NSR's config status
            new_status = ROConfigManager.map_config_status(cm_nsr['state'])
            self._log.debug("Updating config status of NSR {} to {}({})".
                                format(nsrid, new_status, cm_nsr['state']))
            yield from self.nsm.nsrs[nsrid].set_config_status(new_status)

        except Exception as e:
            self._log.error("Failed to process cm-state for nsr {}: {}".
                            format(nsrid, e))
            self._log.exception(e)

    @asyncio.coroutine
    def register(self):
        """ Register for cm-state changes """
        
        @asyncio.coroutine
        def on_prepare(xact_info, query_action, ks_path, msg):
            """ cm-state changed """

            #print("###>>> cm-state change ({}), msg_dict = {}".format(query_action, msg_dict))
            self._log.debug("Received cm-state on_prepare (%s:%s:%s)",
                            query_action,
                            ks_path,
                            msg)

            if (query_action == rwdts.QueryAction.UPDATE or
                query_action == rwdts.QueryAction.CREATE):
                # Update Each NSR/VNFR state
                msg_dict = msg.as_dict()
                yield from self.update_ns_cfg_state(msg_dict)
            elif query_action == rwdts.QueryAction.DELETE:
                self._log.debug("DELETE action in on_prepare for cm-state, ignoring")
            else:
                raise NotImplementedError(
                    "%s on cm-state is not supported",
                    query_action)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        try:
            handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare)
            self.dts_reg_hdl = yield from self._dts.register(self.cm_state_xpath,
                                                             flags=rwdts.Flag.SUBSCRIBER,
                                                             handler=handler)
        except Exception as e:
            self._log.error("Failed to register for cm-state changes as %s", str(e))
            
