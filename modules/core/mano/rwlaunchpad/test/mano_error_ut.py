#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import argparse
import asyncio
import logging
import os
import sys
import time
import unittest
import uuid

import xmlrunner

import gi.repository.RwDts as rwdts
import gi.repository.RwNsmYang as rwnsmyang
import gi.repository.RwResourceMgrYang as RwResourceMgrYang
import gi.repository.RwLaunchpadYang as launchpadyang
import rift.tasklets
import rift.test.dts

import mano_ut


if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class OutOfResourceError(Exception):
    pass


class ComputeResourceRequestMockEventHandler(object):
    def __init__(self):
        self._pool_name = "vm_pool"
        self._vdu_id = str(uuid.uuid4())
        self._vdu_info = {
                "vdu_id": self._vdu_id,
                "state": "active",
                "management_ip": "1.1.1.1",
                "public_ip": "1.1.1.1",
                "connection_points": [],
                }

        self._resource_state = "active"

        self._event_id = None
        self._request_info = None

    def allocate(self, event_id, request_info):
        self._event_id = event_id
        self._request_info = request_info

        self._vdu_info.update({
            "name": self._request_info.name,
            "flavor_id": self._request_info.flavor_id,
            "image_id": self._request_info.image_id,
            })

        for cp in request_info.connection_points:
            info_cp = dict(
                name=cp.name,
                virtual_link_id=cp.virtual_link_id,
                vdu_id=self._vdu_id,
                state="active",
                ip_address="1.2.3.4",
                )
            info_cp = self._vdu_info["connection_points"].append(info_cp)

    @property
    def event_id(self):
        return self._event_id

    @property
    def resource_state(self):
        return self._resource_state

    def set_active(self):
        self._resource_state = "active"

    def set_failed(self):
        self._resource_state = "failed"

    def set_pending(self):
        self._resource_state = "pending"

    @property
    def response_msg(self):
        resource_info = dict(
                pool_name=self._pool_name,
                resource_state=self.resource_state,
                )
        resource_info.update(self._vdu_info)

        response = RwResourceMgrYang.VDUEventData.from_dict(dict(
            event_id=self._event_id,
            request_info=self._request_info.as_dict(),
            resource_info=resource_info,
            ))

        return response.resource_info


class NetworkResourceRequestMockEventHandler(object):
    def __init__(self):
        self._pool_name = "network_pool"
        self._link_id = str(uuid.uuid4())
        self._link_info = {
                "virtual_link_id": self._link_id,
                "state": "active",
                }

        self._resource_state = "active"

        self._event_id = None
        self._request_info = None

    def allocate(self, event_id, request_info):
        self._event_id = event_id
        self._request_info = request_info

        self._link_info.update({
            "name": self._request_info.name,
            "subnet": self._request_info.subnet,
            })

    @property
    def event_id(self):
        return self._event_id

    @property
    def resource_state(self):
        return self._resource_state

    def set_active(self):
        self._resource_state = "active"

    def set_failed(self):
        self._resource_state = "failed"

    def set_pending(self):
        self._resource_state = "pending"

    @property
    def response_msg(self):
        resource_info = dict(
                pool_name=self._pool_name,
                resource_state=self.resource_state,
                )
        resource_info.update(self._link_info)

        response = RwResourceMgrYang.VirtualLinkEventData.from_dict(dict(
            event_id=self._event_id,
            request_info=self._request_info.as_dict(),
            resource_info=resource_info,
            ))

        return response.resource_info


class ResourceMgrMock(object):
    VDU_REQUEST_XPATH = "D,/rw-resource-mgr:resource-mgmt/vdu-event/vdu-event-data"
    VLINK_REQUEST_XPATH = "D,/rw-resource-mgr:resource-mgmt/vlink-event/vlink-event-data"

    def __init__(self, dts, log, loop):
        self._log = log
        self._dts = dts
        self._loop = loop
        self._vdu_reg = None
        self._link_reg = None

        self._vdu_reg_event = asyncio.Event(loop=self._loop)
        self._link_reg_event = asyncio.Event(loop=self._loop)

        self._available_compute_handlers = []
        self._available_network_handlers = []

        self._used_compute_handlers = {}
        self._used_network_handlers = {}

        self._compute_allocate_requests = 0
        self._network_allocate_requests = 0

        self._registered = False

    def _allocate_virtual_compute(self, event_id, request_info):
        self._compute_allocate_requests += 1

        if not self._available_compute_handlers:
            raise OutOfResourceError("No more compute handlers")

        handler = self._available_compute_handlers.pop()
        handler.allocate(event_id, request_info)
        self._used_compute_handlers[event_id] = handler

        return handler.response_msg

    def _allocate_virtual_network(self, event_id, request_info):
        self._network_allocate_requests += 1

        if not self._available_network_handlers:
            raise OutOfResourceError("No more network handlers")

        handler = self._available_network_handlers.pop()
        handler.allocate(event_id, request_info)
        self._used_network_handlers[event_id] = handler

        return handler.response_msg

    def _release_virtual_network(self, event_id):
        del self._used_network_handlers[event_id]

    def _release_virtual_compute(self, event_id):
        del self._used_compute_handlers[event_id]

    def _read_virtual_network(self, event_id):
        return self._used_network_handlers[event_id].response_msg

    def _read_virtual_compute(self, event_id):
        return self._used_compute_handlers[event_id].response_msg

    @asyncio.coroutine
    def on_link_request_prepare(self, xact_info, action, ks_path, request_msg):
        if not self._registered:
            self._log.error("Got a prepare callback when not registered!")
            xact_info.respond_xpath(rwdts.XactRspCode.NA)
            return

        self._log.debug("Received virtual-link on_prepare callback (self: %s, xact_info: %s, action: %s): %s",
                        self, xact_info, action, request_msg)

        response_info = None
        response_xpath = ks_path.to_xpath(RwResourceMgrYang.get_schema()) + "/resource-info"

        schema = RwResourceMgrYang.VirtualLinkEventData().schema()
        pathentry = schema.keyspec_to_entry(ks_path)

        if action == rwdts.QueryAction.CREATE:
            response_info = self._allocate_virtual_network(
                    pathentry.key00.event_id,
                    request_msg.request_info,
                    )

        elif action == rwdts.QueryAction.DELETE:
            self._release_virtual_network(pathentry.key00.event_id)

        elif action == rwdts.QueryAction.READ:
            response_info = self._read_virtual_network(
                    pathentry.key00.event_id
                    )
        else:
            raise ValueError("Only read/create/delete actions available. Received action: %s" %(action))

        self._log.debug("Responding with VirtualLinkInfo at xpath %s: %s.",
                        response_xpath, response_info)

        xact_info.respond_xpath(rwdts.XactRspCode.ACK, response_xpath, response_info)

    @asyncio.coroutine
    def on_vdu_request_prepare(self, xact_info, action, ks_path, request_msg):
        if not self._registered:
            self._log.error("Got a prepare callback when not registered!")
            xact_info.respond_xpath(rwdts.XactRspCode.NA)
            return

        @asyncio.coroutine
        def monitor_vdu_state(response_xpath, pathentry):
            self._log.info("Initiating VDU state monitoring for xpath: %s ", response_xpath)
            loop_cnt = 120
            while loop_cnt > 0:
                self._log.debug("VDU state monitoring: Sleeping for 1 second ")
                yield from asyncio.sleep(1, loop = self._loop)
                try:
                    response_info = self._read_virtual_compute(
                            pathentry.key00.event_id
                            )
                except Exception as e:
                    self._log.error(
                            "VDU state monitoring: Received exception %s "
                            "in VDU state monitoring for %s. Aborting monitoring",
                            str(e), response_xpath
                            )
                    raise

                if response_info.resource_state == 'active' or response_info.resource_state == 'failed':
                    self._log.info(
                            "VDU state monitoring: VDU reached terminal state."
                            "Publishing VDU info: %s at path: %s",
                            response_info, response_xpath
                            )
                    yield from self._dts.query_update(response_xpath,
                                                      rwdts.XactFlag.ADVISE,
                                                      response_info)
                    return
                else:
                    loop_cnt -= 1

            ### End of while loop. This is only possible if VDU did not reach active state
            self._log.info("VDU state monitoring: VDU at xpath :%s did not reached active state in 120 seconds. Aborting monitoring",
                           response_xpath)
            response_info = RwResourceMgrYang.VDUEventData_ResourceInfo()
            response_info.resource_state = 'failed'
            yield from self._dts.query_update(response_xpath,
                                              rwdts.XactFlag.ADVISE,
                                              response_info)
            return

        self._log.debug("Received vdu on_prepare callback (xact_info: %s, action: %s): %s",
                        xact_info, action, request_msg)

        response_info = None
        response_xpath = ks_path.to_xpath(RwResourceMgrYang.get_schema()) + "/resource-info"

        schema = RwResourceMgrYang.VDUEventData().schema()
        pathentry = schema.keyspec_to_entry(ks_path)

        if action == rwdts.QueryAction.CREATE:
            response_info = self._allocate_virtual_compute(
                    pathentry.key00.event_id,
                    request_msg.request_info,
                    )
            if response_info.resource_state == 'pending':
                asyncio.ensure_future(monitor_vdu_state(response_xpath, pathentry),
                                      loop = self._loop)

        elif action == rwdts.QueryAction.DELETE:
            self._release_virtual_compute(
                    pathentry.key00.event_id
                    )

        elif action == rwdts.QueryAction.READ:
            response_info = self._read_virtual_compute(
                    pathentry.key00.event_id
                    )
        else:
            raise ValueError("Only create/delete actions available. Received action: %s" %(action))

        self._log.debug("Responding with VDUInfo at xpath %s: %s",
                        response_xpath, response_info)

        xact_info.respond_xpath(rwdts.XactRspCode.ACK, response_xpath, response_info)

    @asyncio.coroutine
    def register(self):
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


        with self._dts.group_create() as group:
            self._log.debug("Registering for Link Resource Request using xpath: %s",
                            ResourceMgrMock.VLINK_REQUEST_XPATH)

            self._link_reg = group.register(
                    xpath=ResourceMgrMock.VLINK_REQUEST_XPATH,
                    handler=rift.tasklets.DTS.RegistrationHandler(on_ready=on_request_ready,
                                                                  on_prepare=self.on_link_request_prepare),
                    flags=rwdts.Flag.PUBLISHER)

            self._log.debug("Registering for VDU Resource Request using xpath: %s",
                            ResourceMgrMock.VDU_REQUEST_XPATH)

            self._vdu_reg = group.register(
                    xpath=ResourceMgrMock.VDU_REQUEST_XPATH,
                    handler=rift.tasklets.DTS.RegistrationHandler(on_ready=on_request_ready,
                                                                  on_prepare=self.on_vdu_request_prepare),
                    flags=rwdts.Flag.PUBLISHER)

        self._registered = True

    def unregister(self):
        self._link_reg.deregister()
        self._vdu_reg.deregister()
        self._registered = False

    @asyncio.coroutine
    def wait_ready(self, timeout=5):
        self._log.debug("Waiting for all request registrations to become ready.")
        yield from asyncio.wait([self._link_reg_event.wait(), self._vdu_reg_event.wait()],
                                timeout=timeout, loop=self._loop)

    def create_compute_mock_event_handler(self):
        handler = ComputeResourceRequestMockEventHandler()
        self._available_compute_handlers.append(handler)

        return handler

    def create_network_mock_event_handler(self):
        handler = NetworkResourceRequestMockEventHandler()
        self._available_network_handlers.append(handler)

        return handler

    @property
    def num_compute_requests(self):
        return self._compute_allocate_requests

    @property
    def num_network_requests(self):
        return self._network_allocate_requests

    @property
    def num_allocated_compute_resources(self):
        return len(self._used_compute_handlers)

    @property
    def num_allocated_network_resources(self):
        return len(self._used_network_handlers)


@unittest.skip('failing and needs rework')
class ManoErrorTestCase(rift.test.dts.AbstractDTSTest):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """

    @classmethod
    def configure_suite(cls, rwmain):
        launchpad_build_dir = os.path.join(
                cls.top_dir,
                '.build/modules/core/mc/core_mc-build/rwlaunchpad'
                )

        rwmain.add_tasklet(
                os.path.join(launchpad_build_dir, 'plugins/rwvns'),
                'rwvnstasklet'
                )

        rwmain.add_tasklet(
                os.path.join(launchpad_build_dir, 'plugins/rwvnfm'),
                'rwvnfmtasklet'
                )

        rwmain.add_tasklet(
                os.path.join(launchpad_build_dir, 'plugins/rwnsm'),
                'rwnsmtasklet'
                )

        cls.waited_for_tasklets = False

    @asyncio.coroutine
    def register_mock_res_mgr(self):
        self.res_mgr = ResourceMgrMock(
                self.dts,
                self.log,
                self.loop,
                )
        yield from self.res_mgr.register()

        self.log.info("Waiting for resource manager to be ready")
        yield from self.res_mgr.wait_ready()

    def unregister_mock_res_mgr(self):
        self.res_mgr.unregister()

    @classmethod
    def configure_schema(cls):
        return rwnsmyang.get_schema()

    @classmethod
    def configure_timeout(cls):
        return 240

    @asyncio.coroutine
    def wait_tasklets(self):
        if not ManoErrorTestCase.waited_for_tasklets:
            yield from asyncio.sleep(5, loop=self.loop)
            ManoErrorTestCase.waited_for_tasklets = True

    @asyncio.coroutine
    def publish_desciptors(self, num_external_vlrs=1, num_internal_vlrs=1, num_ping_vms=1):
        yield from self.ping_pong.publish_desciptors(
                num_external_vlrs,
                num_internal_vlrs,
                num_ping_vms
                )

    def unpublish_descriptors(self):
        self.ping_pong.unpublish_descriptors()

    @asyncio.coroutine
    def wait_until_nsr_active_or_failed(self, nsr_id, timeout_secs=20):
        start_time = time.time()
        while (time.time() - start_time) < timeout_secs:
            nsrs = yield from self.querier.get_nsr_opdatas(nsr_id)
            self.assertEqual(1, len(nsrs))
            if nsrs[0].operational_status in ['running', 'failed']:
                return

            self.log.debug("Rcvd NSR with %s status", nsrs[0].operational_status)
            yield from asyncio.sleep(2, loop=self.loop)

        self.assertIn(nsrs[0].operational_status, ['running', 'failed'])

    def verify_number_compute_requests(self, num_requests):
        self.assertEqual(num_requests, self.res_mgr.num_compute_requests)

    def verify_number_network_requests(self, num_requests):
        self.assertEqual(num_requests, self.res_mgr.num_network_requests)

    def verify_number_allocated_compute(self, num_allocated):
        self.assertEqual(num_allocated, self.res_mgr.num_allocated_compute_resources)

    def verify_number_allocated_network(self, num_allocated):
        self.assertEqual(num_allocated, self.res_mgr.num_allocated_network_resources)

    def allocate_network_handlers(self, num_networks):
        return [self.res_mgr.create_network_mock_event_handler() for _ in range(num_networks)]

    def allocate_compute_handlers(self, num_computes):
        return [self.res_mgr.create_compute_mock_event_handler() for _ in range(num_computes)]

    @asyncio.coroutine
    def create_mock_launchpad_tasklet(self):
        yield from mano_ut.create_mock_launchpad_tasklet(self.log, self.dts)

    def configure_test(self, loop, test_id):
        self.log.debug("STARTING - %s", self.id())
        self.tinfo = self.new_tinfo(self.id())
        self.dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop)
        self.ping_pong = mano_ut.PingPongDescriptorPublisher(self.log, self.loop, self.dts)
        self.querier = mano_ut.ManoQuerier(self.log, self.dts)

        # Add a task to wait for tasklets to come up
        asyncio.ensure_future(self.wait_tasklets(), loop=self.loop)

    @rift.test.dts.async_test
    def test_fail_first_nsm_vlr(self):
        yield from self.publish_desciptors(num_external_vlrs=2)
        yield from self.register_mock_res_mgr()

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(1)
        yield from self.verify_num_nsr_vlrs(nsr_id, 2)
        yield from self.verify_num_vnfrs(0)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "failed")

        self.verify_number_network_requests(1)
        self.verify_number_compute_requests(0)
        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()

    @rift.test.dts.async_test
    def test_fail_second_nsm_vlr(self):
        yield from self.publish_desciptors(num_external_vlrs=2)
        yield from self.register_mock_res_mgr()
        self.allocate_network_handlers(1)

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(2)
        yield from self.verify_num_nsr_vlrs(nsr_id, 2)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "running")
        yield from self.verify_vlr_state(nsr_vlrs[1], "failed")

        self.verify_number_network_requests(2)
        self.verify_number_compute_requests(0)
        self.verify_number_allocated_network(1)
        self.verify_number_allocated_compute(0)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()

    @rift.test.dts.async_test
    def test_fail_first_vnf_first_vlr(self):
        yield from self.publish_desciptors(num_internal_vlrs=2)
        yield from self.register_mock_res_mgr()
        self.allocate_network_handlers(1)

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(2)
        yield from self.verify_num_nsr_vlrs(nsr_id, 1)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "running")

        yield from self.verify_num_nsr_vnfrs(nsr_id, 2)

        # Verify only a single vnfr was instantiated and is failed
        yield from self.verify_num_vnfrs(1)
        nsr_vnfs = yield from self.get_nsr_vnfs(nsr_id)
        yield from self.verify_vnf_state(nsr_vnfs[0], "failed")

        yield from self.verify_num_vnfr_vlrs(nsr_vnfs[0], 2)
        vnf_vlrs = yield from self.get_vnf_vlrs(nsr_vnfs[0])
        yield from self.verify_vlr_state(vnf_vlrs[0], "failed")

        self.verify_number_network_requests(2)
        self.verify_number_compute_requests(0)
        self.verify_number_allocated_network(1)
        self.verify_number_allocated_compute(0)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()

    @rift.test.dts.async_test
    def test_fail_first_vnf_second_vlr(self):
        yield from self.publish_desciptors(num_internal_vlrs=2)
        yield from self.register_mock_res_mgr()
        self.allocate_network_handlers(2)

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(3)
        yield from self.verify_num_nsr_vlrs(nsr_id, 1)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "running")

        yield from self.verify_num_nsr_vnfrs(nsr_id, 2)

        # Verify only a single vnfr was instantiated and is failed
        yield from self.verify_num_vnfrs(1)
        nsr_vnfs = yield from self.get_nsr_vnfs(nsr_id)
        yield from self.verify_vnf_state(nsr_vnfs[0], "failed")

        yield from self.verify_num_vnfr_vlrs(nsr_vnfs[0], 2)
        vnf_vlrs = yield from self.get_vnf_vlrs(nsr_vnfs[0])
        yield from self.verify_vlr_state(vnf_vlrs[0], "running")
        yield from self.verify_vlr_state(vnf_vlrs[1], "failed")

        self.verify_number_network_requests(3)
        self.verify_number_compute_requests(0)
        self.verify_number_allocated_network(2)
        self.verify_number_allocated_compute(0)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()

    @rift.test.dts.async_test
    def test_fail_first_vnf_first_vdu(self):
        yield from self.publish_desciptors(num_internal_vlrs=2, num_ping_vms=2)
        yield from self.register_mock_res_mgr()
        yield from self.create_mock_launchpad_tasklet()
        self.allocate_network_handlers(3)

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(3)
        yield from self.verify_num_nsr_vlrs(nsr_id, 1)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "running")

        yield from self.verify_num_nsr_vnfrs(nsr_id, 2)

        # Verify only a single vnfr was instantiated and is failed
        yield from self.verify_num_vnfrs(1)
        nsr_vnfs = yield from self.get_nsr_vnfs(nsr_id)
        yield from self.verify_vnf_state(nsr_vnfs[0], "failed")

        yield from self.verify_num_vnfr_vlrs(nsr_vnfs[0], 2)
        vnf_vlrs = yield from self.get_vnf_vlrs(nsr_vnfs[0])
        yield from self.verify_vlr_state(vnf_vlrs[0], "running")
        yield from self.verify_vlr_state(vnf_vlrs[1], "running")

        yield from self.verify_num_vnfr_vdus(nsr_vnfs[0], 2)
        vdus = yield from self.get_vnf_vdus(nsr_vnfs[0])
        self.verify_vdu_state(vdus[0], "failed")

        self.verify_number_network_requests(3)
        self.verify_number_compute_requests(1)
        self.verify_number_allocated_network(3)
        self.verify_number_allocated_compute(0)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()

    @rift.test.dts.async_test
    def test_fail_first_vnf_second_vdu(self):
        yield from self.publish_desciptors(num_internal_vlrs=2, num_ping_vms=2)
        yield from self.register_mock_res_mgr()
        yield from self.create_mock_launchpad_tasklet()
        self.allocate_network_handlers(3)
        self.allocate_compute_handlers(1)

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(3)
        yield from self.verify_num_nsr_vlrs(nsr_id, 1)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "running")

        yield from self.verify_num_nsr_vnfrs(nsr_id, 2)

        # Verify only a single vnfr was instantiated and is failed
        yield from self.verify_num_vnfrs(1)
        nsr_vnfs = yield from self.get_nsr_vnfs(nsr_id)
        yield from self.verify_vnf_state(nsr_vnfs[0], "failed")

        yield from self.verify_num_vnfr_vlrs(nsr_vnfs[0], 2)
        vnf_vlrs = yield from self.get_vnf_vlrs(nsr_vnfs[0])
        yield from self.verify_vlr_state(vnf_vlrs[0], "running")
        yield from self.verify_vlr_state(vnf_vlrs[1], "running")

        yield from self.verify_num_vnfr_vdus(nsr_vnfs[0], 2)

        vdus = yield from self.get_vnf_vdus(nsr_vnfs[0])
        self.verify_vdu_state(vdus[0], "running")
        self.verify_vdu_state(vdus[1], "failed")

        self.verify_number_network_requests(3)
        self.verify_number_compute_requests(2)
        self.verify_number_allocated_network(3)
        self.verify_number_allocated_compute(1)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()

    @rift.test.dts.async_test
    def test_fail_second_vnf_second_vdu(self):
        yield from self.publish_desciptors(num_internal_vlrs=2, num_ping_vms=2)
        yield from self.register_mock_res_mgr()
        yield from self.create_mock_launchpad_tasklet()
        self.allocate_network_handlers(5)
        self.allocate_compute_handlers(3)

        nsr_id = yield from self.ping_pong.create_nsr()
        yield from self.wait_until_nsr_active_or_failed(nsr_id)

        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 1)
        yield from self.verify_nsr_state(nsr_id, "failed")
        yield from self.verify_num_vlrs(5)
        yield from self.verify_num_nsr_vlrs(nsr_id, 1)

        nsr_vlrs = yield from self.get_nsr_vlrs(nsr_id)
        yield from self.verify_vlr_state(nsr_vlrs[0], "running")

        yield from self.verify_num_nsr_vnfrs(nsr_id, 2)

        # Verify only a single vnfr was instantiated and is failed
        yield from self.verify_num_vnfrs(2)
        nsr_vnfs = yield from self.get_nsr_vnfs(nsr_id)
        yield from self.verify_vnf_state(nsr_vnfs[0], "running")
        yield from self.verify_vnf_state(nsr_vnfs[1], "failed")

        yield from self.verify_num_vnfr_vlrs(nsr_vnfs[0], 2)

        vnf_vlrs = yield from self.get_vnf_vlrs(nsr_vnfs[0])
        yield from self.verify_vlr_state(vnf_vlrs[0], "running")
        yield from self.verify_vlr_state(vnf_vlrs[1], "running")

        vnf_vlrs = yield from self.get_vnf_vlrs(nsr_vnfs[1])
        yield from self.verify_vlr_state(vnf_vlrs[0], "running")
        yield from self.verify_vlr_state(vnf_vlrs[1], "running")

        yield from self.verify_num_vnfr_vdus(nsr_vnfs[0], 2)
        yield from self.verify_num_vnfr_vdus(nsr_vnfs[1], 2)

        vdus = yield from self.get_vnf_vdus(nsr_vnfs[0])
        self.verify_vdu_state(vdus[0], "running")
        self.verify_vdu_state(vdus[1], "running")

        vdus = yield from self.get_vnf_vdus(nsr_vnfs[1])
        self.verify_vdu_state(vdus[0], "running")
        self.verify_vdu_state(vdus[1], "failed")

        self.verify_number_network_requests(5)
        self.verify_number_compute_requests(4)
        self.verify_number_allocated_network(5)
        self.verify_number_allocated_compute(3)

        yield from self.terminate_nsr(nsr_id)

        yield from self.verify_nsr_deleted(nsr_id)
        yield from self.verify_nsd_ref_count(self.ping_pong.nsd_id, 0)
        yield from self.verify_num_vlrs(0)

        self.verify_number_allocated_network(0)
        self.verify_number_allocated_compute(0)

        self.unregister_mock_res_mgr()
        self.unpublish_descriptors()


def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args, _ = parser.parse_known_args()

    ManoErrorTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()

# vim: sw
