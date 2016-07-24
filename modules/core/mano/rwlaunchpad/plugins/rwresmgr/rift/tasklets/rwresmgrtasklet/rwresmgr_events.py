
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import sys

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwcalYang', '1.0')
from gi.repository import (
    RwDts as rwdts,
    RwYang,
    RwResourceMgrYang,
    RwLaunchpadYang,
    RwcalYang,
)

from gi.repository.RwTypes import RwStatus
import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class ResourceMgrEvent(object):
    VDU_REQUEST_XPATH = "D,/rw-resource-mgr:resource-mgmt/vdu-event/vdu-event-data"
    VLINK_REQUEST_XPATH = "D,/rw-resource-mgr:resource-mgmt/vlink-event/vlink-event-data"

    def __init__(self, dts, log, loop, parent):
        self._log = log
        self._dts = dts
        self._loop = loop
        self._parent = parent
        self._vdu_reg = None
        self._link_reg = None

        self._vdu_reg_event = asyncio.Event(loop=self._loop)
        self._link_reg_event = asyncio.Event(loop=self._loop)

    @asyncio.coroutine
    def wait_ready(self, timeout=5):
        self._log.debug("Waiting for all request registrations to become ready.")
        yield from asyncio.wait([self._link_reg_event.wait(), self._vdu_reg_event.wait()],
                                timeout=timeout, loop=self._loop)

    def create_record_dts(self, regh, xact, path, msg):
        """
        Create a record in DTS with path and message
        """
        self._log.debug("Creating Resource Record xact = %s, %s:%s",
                        xact, path, msg)
        regh.create_element(path, msg)

    def delete_record_dts(self, regh, xact, path):
        """
        Delete a VNFR record in DTS with path and message
        """
        self._log.debug("Deleting Resource Record xact = %s, %s",
                        xact, path)
        regh.delete_element(path)

    @asyncio.coroutine
    def register(self):
        @asyncio.coroutine
        def onlink_event(dts, g_reg, xact, xact_event, scratch_data):
            @asyncio.coroutine
            def instantiate_realloc_vn(link):
                """Re-populate the virtual link information after restart

                Arguments:
                    vlink 

                """
                # wait for 3 seconds
                yield from asyncio.sleep(3, loop=self._loop)

                response_info = yield from self._parent.reallocate_virtual_network(link.event_id,
                                                                                 link.cloud_account,
                                                                                 link.request_info, link.resource_info,
                                                                                 )
            if (xact_event == rwdts.MemberEvent.INSTALL):
              link_cfg = self._link_reg.elements
              for link in link_cfg:
                self._loop.create_task(instantiate_realloc_vn(link))
            return rwdts.MemberRspCode.ACTION_OK

        @asyncio.coroutine
        def onvdu_event(dts, g_reg, xact, xact_event, scratch_data):
            @asyncio.coroutine
            def instantiate_realloc_vdu(vdu):
                """Re-populate the VDU information after restart

                Arguments:
                    vdu 

                """
                # wait for 3 seconds
                yield from asyncio.sleep(3, loop=self._loop)

                response_info = yield from self._parent.allocate_virtual_compute(vdu.event_id,
                                                                                 vdu.cloud_account,
                                                                                 vdu.request_info
                                                                                 )
            if (xact_event == rwdts.MemberEvent.INSTALL):
              vdu_cfg = self._vdu_reg.elements
              for vdu in vdu_cfg:
                self._loop.create_task(instantiate_realloc_vdu(vdu))
            return rwdts.MemberRspCode.ACTION_OK

        def on_link_request_commit(xact_info):
            """ The transaction has been committed """
            self._log.debug("Received link request commit (xact_info: %s)", xact_info)
            return rwdts.MemberRspCode.ACTION_OK

        @asyncio.coroutine
        def on_link_request_prepare(xact_info, action, ks_path, request_msg):
            self._log.debug("Received virtual-link on_prepare callback (xact_info: %s, action: %s): %s",
                            xact_info, action, request_msg)

            response_info = None
            response_xpath = ks_path.to_xpath(RwResourceMgrYang.get_schema()) + "/resource-info"

            schema = RwResourceMgrYang.VirtualLinkEventData().schema()
            pathentry = schema.keyspec_to_entry(ks_path)

            if action == rwdts.QueryAction.CREATE:
                response_info = yield from self._parent.allocate_virtual_network(pathentry.key00.event_id,
                                                                                 request_msg.cloud_account,
                                                                                 request_msg.request_info)
                request_msg.resource_info = response_info
                self.create_record_dts(self._link_reg, None, ks_path.to_xpath(RwResourceMgrYang.get_schema()), request_msg)
            elif action == rwdts.QueryAction.DELETE:
                yield from self._parent.release_virtual_network(pathentry.key00.event_id)
                self.delete_record_dts(self._link_reg, None, ks_path.to_xpath(RwResourceMgrYang.get_schema()))
            elif action == rwdts.QueryAction.READ:
                response_info = yield from self._parent.read_virtual_network_info(pathentry.key00.event_id)
            else:
                raise ValueError("Only read/create/delete actions available. Received action: %s" %(action))

            self._log.debug("Responding with VirtualLinkInfo at xpath %s: %s.",
                            response_xpath, response_info)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK, response_xpath, response_info)


        def on_vdu_request_commit(xact_info):
            """ The transaction has been committed """
            self._log.debug("Received vdu request commit (xact_info: %s)", xact_info)
            return rwdts.MemberRspCode.ACTION_OK

        def monitor_vdu_state(response_xpath, pathentry):
            self._log.info("Initiating VDU state monitoring for xpath: %s ", response_xpath)
            loop_cnt = 180
            for i in range(loop_cnt):
                self._log.debug("VDU state monitoring for xpath: %s. Sleeping for 1 second", response_xpath)
                yield from asyncio.sleep(1, loop = self._loop)
                try:
                    response_info = yield from self._parent.read_virtual_compute_info(pathentry.key00.event_id)
                except Exception as e:
                    self._log.info("VDU state monitoring: Received exception %s in VDU state monitoring for %s. Aborting monitoring",
                                   str(e),response_xpath)
                    response_info = RwResourceMgrYang.VDUEventData_ResourceInfo()
                    response_info.resource_state = 'failed'
                    yield from self._dts.query_update(response_xpath,
                                                      rwdts.XactFlag.ADVISE,
                                                      response_info)
                else:
                    if response_info.resource_state == 'active' or response_info.resource_state == 'failed':
                        self._log.info("VDU state monitoring: VDU reached terminal state. Publishing VDU info: %s at path: %s",
                                       response_info, response_xpath)
                        yield from self._dts.query_update(response_xpath,
                                                          rwdts.XactFlag.ADVISE,
                                                          response_info)
                        return
            else:
                ### End of loop. This is only possible if VDU did not reach active state
                self._log.info("VDU state monitoring: VDU at xpath :{} did not reached active state in {} seconds. Aborting monitoring".format(response_xpath, loop_cnt))
                response_info = RwResourceMgrYang.VDUEventData_ResourceInfo()
                response_info.resource_state = 'failed'
                yield from self._dts.query_update(response_xpath,
                                                  rwdts.XactFlag.ADVISE,
                                                  response_info)
            return

        def allocate_vdu_task(ks_path, event_id, cloud_account, request_msg):
            response_xpath = ks_path.to_xpath(RwResourceMgrYang.get_schema()) + "/resource-info"
            schema = RwResourceMgrYang.VDUEventData().schema()
            pathentry = schema.keyspec_to_entry(ks_path)
            try:
                response_info = yield from self._parent.allocate_virtual_compute(event_id,
                                                                                 cloud_account,
                                                                                 request_msg,)
            except Exception as e:
                self._log.error("Encountered exception : %s while creating virtual compute", str(e))
                response_info = RwResourceMgrYang.VDUEventData_ResourceInfo()
                response_info.resource_state = 'failed'
                yield from self._dts.query_update(response_xpath,
                                                  rwdts.XactFlag.ADVISE,
                                                  response_info)
            else:
                if response_info.resource_state == 'failed' or response_info.resource_state == 'active' :
                    self._log.info("Virtual compute create task completed. Publishing VDU info: %s at path: %s",
                                   response_info, response_xpath)
                    yield from self._dts.query_update(response_xpath,
                                                      rwdts.XactFlag.ADVISE,
                                                      response_info)
                else:
                    asyncio.ensure_future(monitor_vdu_state(response_xpath, pathentry),
                                          loop = self._loop)


        @asyncio.coroutine
        def on_vdu_request_prepare(xact_info, action, ks_path, request_msg):
            self._log.debug("Received vdu on_prepare callback (xact_info: %s, action: %s): %s",
                            xact_info, action, request_msg)
            response_xpath = ks_path.to_xpath(RwResourceMgrYang.get_schema()) + "/resource-info"
            schema = RwResourceMgrYang.VDUEventData().schema()
            pathentry = schema.keyspec_to_entry(ks_path)

            if action == rwdts.QueryAction.CREATE:
                response_info = RwResourceMgrYang.VDUEventData_ResourceInfo()
                response_info.resource_state = 'pending'
                request_msg.resource_info = response_info
                self.create_record_dts(self._vdu_reg,
                                       None,
                                       ks_path.to_xpath(RwResourceMgrYang.get_schema()),
                                       request_msg)
                asyncio.ensure_future(allocate_vdu_task(ks_path,
                                                        pathentry.key00.event_id,
                                                        request_msg.cloud_account,
                                                        request_msg.request_info),
                                      loop = self._loop)
            elif action == rwdts.QueryAction.DELETE:
                response_info = None
                yield from self._parent.release_virtual_compute(pathentry.key00.event_id)
                self.delete_record_dts(self._vdu_reg, None, ks_path.to_xpath(RwResourceMgrYang.get_schema()))
            elif action == rwdts.QueryAction.READ:
                response_info = yield from self._parent.read_virtual_compute_info(pathentry.key00.event_id)
            else:
                raise ValueError("Only create/delete actions available. Received action: %s" %(action))

            self._log.debug("Responding with VDUInfo at xpath %s: %s",
                            response_xpath, response_info)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK, response_xpath, response_info)


        @asyncio.coroutine
        def on_request_ready(registration, status):
            self._log.debug("Got request ready event (registration: %s) (status: %s)",
                            registration, status)

            if registration == self._link_reg:
                self._link_reg_event.set()
            elif registration == self._vdu_reg:
                self._vdu_reg_event.set()
            else:
                self._log.error("Unknown registration ready event: %s", registration)

        link_handlers = rift.tasklets.Group.Handler(on_event=onlink_event,)
        with self._dts.group_create(handler=link_handlers) as link_group:
            self._log.debug("Registering for Link Resource Request using xpath: %s",
                            ResourceMgrEvent.VLINK_REQUEST_XPATH)

            self._link_reg = link_group.register(xpath=ResourceMgrEvent.VLINK_REQUEST_XPATH,
                                            handler=rift.tasklets.DTS.RegistrationHandler(on_ready=on_request_ready,
                                                                                          on_commit=on_link_request_commit,
                                                                                          on_prepare=on_link_request_prepare),
                                            flags=rwdts.Flag.PUBLISHER | rwdts.Flag.DATASTORE,)

        vdu_handlers = rift.tasklets.Group.Handler(on_event=onvdu_event, )
        with self._dts.group_create(handler=vdu_handlers) as vdu_group:

            self._log.debug("Registering for VDU Resource Request using xpath: %s",
                            ResourceMgrEvent.VDU_REQUEST_XPATH)

            self._vdu_reg = vdu_group.register(xpath=ResourceMgrEvent.VDU_REQUEST_XPATH,
                                           handler=rift.tasklets.DTS.RegistrationHandler(on_ready=on_request_ready,
                                                                                         on_commit=on_vdu_request_commit,
                                                                                         on_prepare=on_vdu_request_prepare),
                                           flags=rwdts.Flag.PUBLISHER | rwdts.Flag.DATASTORE,)

