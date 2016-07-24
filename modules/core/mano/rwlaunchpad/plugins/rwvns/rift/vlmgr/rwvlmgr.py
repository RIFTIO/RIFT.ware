
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import enum
import uuid
import time

import gi
gi.require_version('RwVlrYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
from gi.repository import (
    RwVlrYang,
    VldYang,
    RwDts as rwdts,
    RwResourceMgrYang,
)
import rift.tasklets


class NetworkResourceError(Exception):
    """ Network Resource Error """
    pass


class VlrRecordExistsError(Exception):
    """ VLR record already exists"""
    pass


class VlRecordError(Exception):
    """ VLR record error """
    pass


class VirtualLinkRecordState(enum.Enum):
    """ Virtual Link record state """
    INIT = 1
    INSTANTIATING = 2
    RESOURCE_ALLOC_PENDING = 3
    READY = 4
    TERMINATING = 5
    TERMINATED = 6
    FAILED = 10


class VirtualLinkRecord(object):
    """
        Virtual Link Record object
    """
    def __init__(self, dts, log, loop, vnsm, vlr_msg, req_id=None):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._vnsm = vnsm
        self._vlr_msg = vlr_msg

        self._network_id = None
        self._network_pool = None
        self._assigned_subnet = None
        self._create_time = int(time.time())
        if req_id == None:
            self._request_id = str(uuid.uuid4())
        else:
            self._request_id = req_id

        self._state = VirtualLinkRecordState.INIT

    @property
    def vld_xpath(self):
        """ VLD xpath associated with this VLR record """
        return "C,/vld:vld-catalog/vld:vld[id='{}']".format(self.vld_id)

    @property
    def vld_id(self):
        """ VLD id associated with this VLR record """
        return self._vlr_msg.vld_ref

    @property
    def vlr_id(self):
        """ VLR id associated with this VLR record """
        return self._vlr_msg.id

    @property
    def xpath(self):
        """ path for this VLR """
        return("D,/vlr:vlr-catalog"
               "/vlr:vlr[vlr:id='{}']".format(self.vlr_id))

    @property
    def name(self):
        """ Name of this VLR """
        return self._vlr_msg.name

    @property
    def cloud_account_name(self):
        """ Cloud Account to instantiate the virtual link on """
        return self._vlr_msg.cloud_account

    @property
    def resmgr_path(self):
        """ path for resource-mgr"""
        return ("D,/rw-resource-mgr:resource-mgmt" +
                "/vlink-event/vlink-event-data[event-id='{}']".format(self._request_id))

    @property
    def operational_status(self):
        """ Operational status of this VLR"""
        op_stats_dict = {"INIT": "init",
                         "INSTANTIATING": "vl_alloc_pending",
                         "RESOURCE_ALLOC_PENDING": "vl_alloc_pending",
                         "READY": "running",
                         "FAILED": "failed",
                         "TERMINATING": "vl_terminate_pending",
                         "TERMINATED": "terminated"}

        return op_stats_dict[self._state.name]

    @property
    def msg(self):
        """ VLR message for this VLR """
        msg = RwVlrYang.YangData_Vlr_VlrCatalog_Vlr()
        msg.copy_from(self._vlr_msg)

        if self._network_id is not None:
            msg.network_id = self._network_id

        if self._network_pool is not None:
            msg.network_pool = self._network_pool

        if self._assigned_subnet is not None:
            msg.assigned_subnet = self._assigned_subnet

        msg.operational_status = self.operational_status
        msg.res_id = self._request_id

        return msg

    @property
    def resmgr_msg(self):
        """ VLR message for this VLR """
        msg = RwResourceMgrYang.VirtualLinkEventData()
        msg.event_id = self._request_id
        msg.cloud_account = self.cloud_account_name
        msg.request_info.name = self.name
        msg.request_info.provider_network.from_dict(
                self._vlr_msg.provider_network.as_dict()
                )
        return msg

    @asyncio.coroutine
    def create_network(self, xact):
        """ Create network for this VL """
        self._log.debug("Creating network req-id: %s", self._request_id)
        return (yield from self.request_network(xact, "create"))

    @asyncio.coroutine
    def delete_network(self, xact):
        """ Delete network for this VL """
        self._log.debug("Deleting network - req-id: %s", self._request_id)
        return (yield from self.request_network(xact, "delete"))

    @asyncio.coroutine
    def read_network(self, xact):
        """ Read network for this VL """
        self._log.debug("Reading network - req-id: %s", self._request_id)
        return (yield from self.request_network(xact, "read"))

    @asyncio.coroutine
    def request_network(self, xact, action):
        """Request creation/deletion network for this VL """

        block = xact.block_create()

        if action == "create":
            self._log.debug("Creating network path:%s, msg:%s",
                            self.resmgr_path, self.resmgr_msg)
            block.add_query_create(self.resmgr_path, self.resmgr_msg)
        elif action == "delete":
            self._log.debug("Deleting network path:%s", self.resmgr_path)
            if self.resmgr_msg.request_info.name != "multisite":
                block.add_query_delete(self.resmgr_path)
        elif action == "read":
            self._log.debug("Reading network path:%s", self.resmgr_path)
            block.add_query_read(self.resmgr_path)
        else:
            raise VlRecordError("Invalid action %s received" % action)

        res_iter = yield from block.execute(now=True)

        resp = None

        if action == "create" or action == "read":
            for i in res_iter:
                r = yield from i
                resp = r.result

            if resp is None or not (resp.has_field('resource_info') and
                                    resp.resource_info.has_field('virtual_link_id')):
                raise NetworkResourceError("Did not get a network resource response (resp: %s)",
                                           resp)

            self._log.debug("Got network request response: %s", resp)

        return resp

    @asyncio.coroutine
    def instantiate(self, xact, restart=0):
        """ Instantiate this VL """
        self._state = VirtualLinkRecordState.INSTANTIATING

        self._log.debug("Instantiating VLR path = [%s]", self.xpath)

        try:
            self._state = VirtualLinkRecordState.RESOURCE_ALLOC_PENDING

            if restart == 0:
              network_resp = yield from self.create_network(xact)
            else:
              network_resp = yield from self.read_network(xact)
              if network_resp == None:
                network_resp = yield from self.create_network(xact)

            # Note network_resp.virtual_link_id is CAL assigned network_id.

            self._network_id = network_resp.resource_info.virtual_link_id
            self._network_pool = network_resp.resource_info.pool_name
            self._assigned_subnet = network_resp.resource_info.subnet

            self._state = VirtualLinkRecordState.READY

            yield from self.publish(xact)

        except Exception as e:
            self._log.error("Instantiatiation of  VLR record failed: %s", str(e))
            self._state = VirtualLinkRecordState.FAILED
            yield from self.publish(xact)

    @asyncio.coroutine
    def publish(self, xact):
        """ publish this VLR """
        vlr = self.msg
        self._log.debug("Publishing VLR path = [%s], record = [%s]",
                        self.xpath, self.msg)
        vlr.create_time = self._create_time
        yield from self._vnsm.publish_vlr(xact, self.xpath, self.msg)
        self._log.debug("Published VLR path = [%s], record = [%s]",
                        self.xpath, self.msg)

    @asyncio.coroutine
    def terminate(self, xact):
        """ Terminate this VL """
        if self._state not in [VirtualLinkRecordState.READY, VirtualLinkRecordState.FAILED]:
            self._log.error("Ignoring terminate for VL %s is in %s state",
                            self.vlr_id, self._state)
            return

        if self._state == VirtualLinkRecordState.READY:
            self._log.debug("Terminating VL with id %s", self.vlr_id)
            self._state = VirtualLinkRecordState.TERMINATING
            try:
                yield from self.delete_network(xact)
            except Exception:
                self._log.exception("Caught exception while deleting VL %s", self.vlr_id)
            self._log.debug("Terminated VL with id %s", self.vlr_id)

        yield from self.unpublish(xact)
        self._state = VirtualLinkRecordState.TERMINATED

    @asyncio.coroutine
    def unpublish(self, xact):
        """ Unpublish this VLR """
        self._log.debug("UnPublishing VLR id %s", self.vlr_id)
        yield from self._vnsm.unpublish_vlr(xact, self.xpath)
        self._log.debug("UnPublished VLR id %s", self.vlr_id)


class VlrDtsHandler(object):
    """ Handles DTS interactions for the VLR registration """
    XPATH = "D,/vlr:vlr-catalog/vlr:vlr"

    def __init__(self, dts, log, loop, vnsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._vnsm = vnsm

        self._regh = None

    @property
    def regh(self):
        """ The registration handle assocaited with this Handler"""
        return self._regh

    @asyncio.coroutine
    def register(self):
        """ Register for the VLR path """
        def on_commit(xact_info):
            """ The transaction has been committed """
            self._log.debug("Got vlr commit (xact_info: %s)", xact_info)

            return rwdts.MemberRspCode.ACTION_OK

        @asyncio.coroutine
        def on_event(dts, g_reg, xact, xact_event, scratch_data):
            @asyncio.coroutine
            def instantiate_realloc_vlr(vlr):
                """Re-populate the virtual link information after restart

                Arguments:
                    vlink

                """

                with self._dts.transaction(flags=0) as xact:
                  yield from vlr.instantiate(xact, 1)

            if (xact_event == rwdts.MemberEvent.INSTALL):
              curr_cfg = self.regh.elements
              for cfg in curr_cfg:
                vlr = self._vnsm.create_vlr(cfg)
                self._loop.create_task(instantiate_realloc_vlr(vlr))

            self._log.debug("Got on_event")
            return rwdts.MemberRspCode.ACTION_OK

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            """ prepare for VLR registration"""
            self._log.debug(
                "Got vlr on_prepare callback (xact_info: %s, action: %s): %s",
                xact_info, action, msg
                )

            if action == rwdts.QueryAction.CREATE:
                vlr = self._vnsm.create_vlr(msg)
                with self._dts.transaction(flags=0) as xact:
                    yield from vlr.instantiate(xact)
                self._log.debug("Responding to VL create request path:%s, msg:%s",
                                vlr.xpath, vlr.msg)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath=vlr.xpath, msg=vlr.msg)
                return
            elif action == rwdts.QueryAction.DELETE:
                # Delete an VLR record
                schema = RwVlrYang.YangData_Vlr_VlrCatalog_Vlr.schema()
                path_entry = schema.keyspec_to_entry(ks_path)
                self._log.debug("Terminating VLR id %s", path_entry.key00.id)
                yield from self._vnsm.delete_vlr(path_entry.key00.id, xact_info.xact)
            else:
                err = "%s action on VirtualLinkRecord not supported" % action
                raise NotImplementedError(err)
            xact_info.respond_xpath(rwdts.XactRspCode.ACK)
            return

        self._log.debug("Registering for VLR using xpath: %s",
                        VlrDtsHandler.XPATH)

        reg_handle = rift.tasklets.DTS.RegistrationHandler(
            on_commit=on_commit,
            on_prepare=on_prepare,
            )
        handlers = rift.tasklets.Group.Handler(on_event=on_event,)
        with self._dts.group_create(handler=handlers) as group:
            self._regh = group.register(
                xpath=VlrDtsHandler.XPATH,
                handler=reg_handle,
                flags=rwdts.Flag.PUBLISHER | rwdts.Flag.NO_PREP_READ| rwdts.Flag.DATASTORE,
                )

    @asyncio.coroutine
    def create(self, xact, path, msg):
        """
        Create a VLR record in DTS with path and message
        """
        self._log.debug("Creating VLR xact = %s, %s:%s",
                        xact, path, msg)
        self.regh.create_element(path, msg)
        self._log.debug("Created VLR xact = %s, %s:%s",
                        xact, path, msg)

    @asyncio.coroutine
    def update(self, xact, path, msg):
        """
        Update a VLR record in DTS with path and message
        """
        self._log.debug("Updating VLR xact = %s, %s:%s",
                        xact, path, msg)
        self.regh.update_element(path, msg)
        self._log.debug("Updated VLR xact = %s, %s:%s",
                        xact, path, msg)

    @asyncio.coroutine
    def delete(self, xact, path):
        """
        Delete a VLR record in DTS with path and message
        """
        self._log.debug("Deleting VLR xact = %s, %s", xact, path)
        self.regh.delete_element(path)
        self._log.debug("Deleted VLR xact = %s, %s", xact, path)


class VldDtsHandler(object):
    """ DTS handler for the VLD registration """
    XPATH = "C,/vld:vld-catalog/vld:vld"

    def __init__(self, dts, log, loop, vnsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._vnsm = vnsm

        self._regh = None

    @property
    def regh(self):
        """ The registration handle assocaited with this Handler"""
        return self._regh

    @asyncio.coroutine
    def register(self):
        """ Register the VLD path """
        @asyncio.coroutine
        def on_prepare(xact_info, query_action, ks_path, msg):
            """ prepare callback on vld path """
            self._log.debug(
                "Got on prepare for VLD update (ks_path: %s) (action: %s)",
                ks_path.to_xpath(VldYang.get_schema()), msg)

            schema = VldYang.YangData_Vld_VldCatalog_Vld.schema()
            path_entry = schema.keyspec_to_entry(ks_path)
            vld_id = path_entry.key00.id

            disabled_actions = [rwdts.QueryAction.DELETE, rwdts.QueryAction.UPDATE]
            if query_action not in disabled_actions:
                xact_info.respond_xpath(rwdts.XactRspCode.ACK)
                return

            vlr = self._vnsm.find_vlr_by_vld_id(vld_id)
            if vlr is None:
                self._log.debug(
                    "Did not find an existing VLR record for vld %s. "
                    "Permitting %s vld action", vld_id, query_action)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK)
                return

            raise VlrRecordExistsError(
                "Vlr record(s) exists."
                "Cannot perform %s action on VLD." % query_action)

        handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare)

        yield from self._dts.register(
            VldDtsHandler.XPATH,
            flags=rwdts.Flag.SUBSCRIBER,
            handler=handler
            )
