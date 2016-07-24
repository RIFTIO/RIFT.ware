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
import gi.repository.NsrYang as NsrYang
import gi.repository.RwNsrYang as RwNsrYang
import gi.repository.RwTypes as RwTypes
import gi.repository.ProtobufC as ProtobufC
import gi.repository.RwResourceMgrYang as RwResourceMgrYang
import gi.repository.RwLaunchpadYang as launchpadyang
import rift.tasklets
import rift.test.dts

import mano_ut


if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class NsrDtsHandler(object):
    """ The network service DTS handler """
    NSR_XPATH = "C,/nsr:ns-instance-config/nsr:nsr"
    SCALE_INSTANCE_XPATH = "C,/nsr:ns-instance-config/nsr:nsr/nsr:scaling-group/nsr:instance"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._nsr_regh = None
        self._scale_regh = None

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    def get_scale_group_instances(self, nsr_id, group_name):
        def nsr_id_from_keyspec(ks):
            nsr_path_entry = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr.schema().keyspec_to_entry(ks)
            nsr_id = nsr_path_entry.key00.id
            return nsr_id

        def group_name_from_keyspec(ks):
            group_path_entry = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup.schema().keyspec_to_entry(ks)
            group_name = group_path_entry.key00.scaling_group_name_ref
            return group_name


        xact_ids = set()
        for instance_cfg, keyspec in self._scale_regh.get_xact_elements(include_keyspec=True):
            elem_nsr_id = nsr_id_from_keyspec(keyspec)
            if elem_nsr_id != nsr_id:
                continue

            elem_group_name = group_name_from_keyspec(keyspec)
            if elem_group_name != group_name:
                continue

            xact_ids.add(instance_cfg.id)

        return xact_ids

    @asyncio.coroutine
    def register(self):
        """ Register for Nsr create/update/delete/read requests from dts """

        def nsr_id_from_keyspec(ks):
            nsr_path_entry = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr.schema().keyspec_to_entry(ks)
            nsr_id = nsr_path_entry.key00.id
            return nsr_id

        def group_name_from_keyspec(ks):
            group_path_entry = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup.schema().keyspec_to_entry(ks)
            group_name = group_path_entry.key00.scaling_group_name_ref
            return group_name

        def is_instance_in_reg_elements(nsr_id, group_name, instance_id):
            """ Return boolean indicating if scaling group instance was already commited previously.

            By looking at the existing elements in this registration handle (elements not part
            of this current xact), we can tell if the instance was configured previously without
            keeping any application state.
            """
            for instance_cfg, keyspec in self._nsr_regh.get_xact_elements(include_keyspec=True):
                elem_nsr_id = nsr_id_from_keyspec(keyspec)
                elem_group_name = group_name_from_keyspec(keyspec)

                if elem_nsr_id != nsr_id or group_name != elem_group_name:
                    continue

                if instance_cfg.id == instance_id:
                    return True

            return False

        def get_scale_group_instance_delta(nsr_id, group_name, xact):

            #1. Find all elements in the  transaction add to the "added"
            #2. Find matching elements in current elements, remove from "added".
            #3. Find elements only in current, add to "deleted"

            xact_ids = set()
            for instance_cfg, keyspec in self._scale_regh.get_xact_elements(xact, include_keyspec=True):
                elem_nsr_id = nsr_id_from_keyspec(keyspec)
                if elem_nsr_id != nsr_id:
                    continue

                elem_group_name = group_name_from_keyspec(keyspec)
                if elem_group_name != group_name:
                    continue

                xact_ids.add(instance_cfg.id)

            current_ids = set()
            for instance_cfg, keyspec in self._scale_regh.get_xact_elements(include_keyspec=True):
                elem_nsr_id = nsr_id_from_keyspec(keyspec)
                if elem_nsr_id != nsr_id:
                    continue

                elem_group_name = group_name_from_keyspec(keyspec)
                if elem_group_name != group_name:
                    continue

                current_ids.add(instance_cfg.id)

            delta = {
                    "added": xact_ids - current_ids,
                    "deleted": current_ids - xact_ids
                    }
            return delta

        def get_add_delete_update_cfgs(dts_member_reg, xact, key_name):
            # Unforunately, it is currently difficult to figure out what has exactly
            # changed in this xact without Pbdelta support (RIFT-4916)
            # As a workaround, we can fetch the pre and post xact elements and
            # perform a comparison to figure out adds/deletes/updates
            xact_cfgs = list(dts_member_reg.get_xact_elements(xact))
            curr_cfgs = list(dts_member_reg.elements)

            xact_key_map = {getattr(cfg, key_name): cfg for cfg in xact_cfgs}
            curr_key_map = {getattr(cfg, key_name): cfg for cfg in curr_cfgs}

            # Find Adds
            added_keys = set(xact_key_map) - set(curr_key_map)
            added_cfgs = [xact_key_map[key] for key in added_keys]

            # Find Deletes
            deleted_keys = set(curr_key_map) - set(xact_key_map)
            deleted_cfgs = [curr_key_map[key] for key in deleted_keys]

            # Find Updates
            updated_keys = set(curr_key_map) & set(xact_key_map)
            updated_cfgs = [xact_key_map[key] for key in updated_keys if xact_key_map[key] != curr_key_map[key]]

            return added_cfgs, deleted_cfgs, updated_cfgs

        def on_apply(dts, acg, xact, action, scratch):
            """Apply the  configuration"""
            def handle_create_nsr(msg):
                # Handle create nsr requests """
                # Do some validations
                if not msg.has_field("nsd_ref"):
                    err = "NSD reference not provided"
                    self._log.error(err)
                    raise NetworkServiceRecordError(err)

                self._log.info("Creating NetworkServiceRecord %s  from nsd_id  %s",
                               msg.id, msg.nsd_ref)

                #nsr = self.nsm.create_nsr(msg)
                return nsr

            def handle_delete_nsr(msg):
                @asyncio.coroutine
                def delete_instantiation(ns_id):
                    """ Delete instantiation """
                    pass
                    #with self._dts.transaction() as xact:
                        #yield from self._nsm.terminate_ns(ns_id, xact)

                # Handle delete NSR requests
                self._log.info("Delete req for  NSR Id: %s received", msg.id)
                # Terminate the NSR instance
                #nsr = self._nsm.get_ns_by_nsr_id(msg.id)

                #nsr.set_state(NetworkServiceRecordState.TERMINATE_RCVD)
                #event_descr = "Terminate rcvd for NS Id:%s" % msg.id
                #nsr.record_event("terminate-rcvd", event_descr)

                #self._loop.create_task(delete_instantiation(msg.id))

            @asyncio.coroutine
            def begin_instantiation(nsr):
                # Begin instantiation
                pass
                #self._log.info("Beginning NS instantiation: %s", nsr.id)
                #yield from self._nsm.instantiate_ns(nsr.id, xact)

            self._log.debug("Got nsr apply (xact: %s) (action: %s)(scr: %s)",
                            xact, action, scratch)

            if action == rwdts.AppconfAction.INSTALL and xact.id is None:
                self._log.debug("No xact handle.  Skipping apply config")
                xact = None

            (added_msgs, deleted_msgs, updated_msgs) = get_add_delete_update_cfgs(self._nsr_regh, xact, "id")

            for msg in added_msgs:
                self._log.info("Create NSR received in on_apply to instantiate NS:%s", msg.id)
                #if msg.id not in self._nsm.nsrs:
                #    self._log.info("Create NSR received in on_apply to instantiate NS:%s", msg.id)
                #    nsr = handle_create_nsr(msg)
                #    self._loop.create_task(begin_instantiation(nsr))

            for msg in deleted_msgs:
                self._log.info("Delete NSR received in on_apply to terminate NS:%s", msg.id)
                try:
                    handle_delete_nsr(msg)
                except Exception:
                    self._log.exception("Failed to terminate NS:%s", msg.id)

            for msg in updated_msgs:
                self._log.info("Update NSR received in on_apply to change scaling groups in NS:%s", msg.id)

                for group in msg.scaling_group:
                    instance_delta = get_scale_group_instance_delta(msg.id, group.scaling_group_name_ref, xact)
                    self._log.debug("Got NSR:%s scale group instance delta: %s", msg.id, instance_delta)

                    #for instance_id in instance_delta["added"]:
                    #    self._nsm.scale_nsr_out(msg.id, group.scaling_group_name_ref, instance_id, xact)

                    #for instance_id in instance_delta["deleted"]:
                    #    self._nsm.scale_nsr_in(msg.id, group.scaling_group_name_ref, instance_id)


            return RwTypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ Prepare calllback from DTS for NSR """

            xpath = ks_path.to_xpath(NsrYang.get_schema())
            action = xact_info.query_action
            self._log.debug(
                    "Got Nsr prepare callback (xact: %s) (action: %s) (info: %s), %s:%s)",
                    xact, action, xact_info, xpath, msg
                    )

            fref = ProtobufC.FieldReference.alloc()
            fref.goto_whole_message(msg.to_pbcm())

            if action in [rwdts.QueryAction.CREATE, rwdts.QueryAction.UPDATE]:
                pass
                # Ensure the Cloud account has been specified if this is an NSR create
                #if msg.id not in self._nsm.nsrs:
                #    if not msg.has_field("cloud_account"):
                #        raise NsrInstantiationFailed("Cloud account not specified in NSR")

                # We do not allow scaling actions to occur if the NS is not in running state
                #elif msg.has_field("scaling_group"):
                #    nsr = self._nsm.nsrs[msg.id]
                #    if nsr.state != NetworkServiceRecordState.RUNNING:
                #        raise ScalingOperationError("Unable to perform scaling action when NS is not in running state")

                #    if len(msg.scaling_group) > 1:
                #        raise ScalingOperationError("Only a single scaling group can be configured at a time")

                #    for group_msg in msg.scaling_group:
                #        num_new_group_instances = len(group_msg.instance)
                #        if num_new_group_instances > 1:
                #            raise ScalingOperationError("Only a single scaling instance can be created at a time")

                #        elif num_new_group_instances == 1:
                #            scale_group = nsr.scaling_groups[group_msg.scaling_group_name_ref]
                #            if len(scale_group.instances) == scale_group.max_instance_count:
                #                raise ScalingOperationError("Max instances for %s reached" % scale_group)


            acg.handle.prepare_complete_ok(xact_info.handle)


        self._log.debug("Registering for NSR config using xpath: %s",
                        NsrDtsHandler.NSR_XPATH)

        acg_hdl = rift.tasklets.AppConfGroup.Handler(on_apply=on_apply)
        with self._dts.appconf_group_create(handler=acg_hdl) as acg:
            self._nsr_regh = acg.register(xpath=NsrDtsHandler.NSR_XPATH,
                                      flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY | rwdts.Flag.CACHE,
                                      on_prepare=on_prepare)

            self._scale_regh = acg.register(
                                      xpath=NsrDtsHandler.SCALE_INSTANCE_XPATH,
                                      flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY | rwdts.Flag.CACHE,
                                      )


class XPaths(object):
    @staticmethod
    def nsr_config(nsr_id=None):
        return ("C,/nsr:ns-instance-config/nsr:nsr" +
                ("[nsr:id='{}']".format(nsr_id) if nsr_id is not None else ""))

    def scaling_group_instance(nsr_id, group_name, instance_id):
        return ("C,/nsr:ns-instance-config/nsr:nsr" +
                "[nsr:id='{}']".format(nsr_id) +
                "/nsr:scaling-group" +
                "[nsr:scaling-group-name-ref='{}']".format(group_name) +
                "/nsr:instance" +
                "[nsr:id='{}']".format(instance_id)
                )


class NsrHandlerTestCase(rift.test.dts.AbstractDTSTest):
    """
    DTS GI interface unittests
    """
    @classmethod
    def configure_schema(cls):
        return NsrYang.get_schema()

    @classmethod
    def configure_timeout(cls):
        return 240

    def configure_test(self, loop, test_id):
        self.log.debug("STARTING - %s", self.id())
        self.tinfo = self.new_tinfo(self.id())
        self.dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop)
        self.handler = NsrDtsHandler(self.dts, self.log, self.loop, None)

        self.tinfo_c = self.new_tinfo(self.id() + "_client")
        self.dts_c = rift.tasklets.DTS(self.tinfo_c, self.schema, self.loop)

    @rift.test.dts.async_test
    def test_add_delete_ns(self):

        nsr1_uuid = "nsr1_uuid" # str(uuid.uuid4())
        nsr2_uuid = "nsr2_uuid" # str(uuid.uuid4())

        assert nsr1_uuid != nsr2_uuid

        yield from self.handler.register()
        yield from asyncio.sleep(.5, loop=self.loop)

        self.log.debug("Creating NSR")
        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_update(
                XPaths.nsr_config(nsr1_uuid),
                NsrYang.YangData_Nsr_NsInstanceConfig_Nsr(id=nsr1_uuid, name="fu"),
                flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                )
            yield from block.execute(now=True)

        yield from asyncio.sleep(.5, loop=self.loop)

        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_update(
                    XPaths.scaling_group_instance(nsr1_uuid, "group", 1234),
                    NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup_Instance(id=1234),
                    flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                    )
            yield from block.execute(now=True)

        yield from asyncio.sleep(.5, loop=self.loop)

        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_delete(
                    XPaths.scaling_group_instance(nsr1_uuid, "group", 1234),
                    flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                    )
            yield from block.execute(now=True)

        yield from asyncio.sleep(.5, loop=self.loop)

        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_create(
                    XPaths.scaling_group_instance(nsr1_uuid, "group", 12345),
                    NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup_Instance(id=12345),
                    flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                    )
            yield from block.execute(now=True)

        yield from asyncio.sleep(.5, loop=self.loop)

        group_ids = self.handler.get_scale_group_instances(nsr2_uuid, "group")
        self.log.debug("Got group ids in nsr2 after adding 12345 to nsr1: %s", group_ids)
        group_ids = self.handler.get_scale_group_instances(nsr1_uuid, "group")
        self.log.debug("Got group ids in nsr1 after adding 12345 to nsr1: %s", group_ids)
        assert group_ids == {12345}

        self.log.debug("\n\nADD A COMPLETELY DIFFERENT NSR\n")
        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_update(
                XPaths.nsr_config(nsr2_uuid),
                NsrYang.YangData_Nsr_NsInstanceConfig_Nsr(id=nsr2_uuid, name="fu2"),
                flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                )
            yield from block.execute(now=True)

        yield from asyncio.sleep(.5, loop=self.loop)
 
        group_ids = self.handler.get_scale_group_instances(nsr2_uuid, "group")
        self.log.debug("Got group ids in nsr2 after adding new nsr: %s", group_ids)
        group_ids = self.handler.get_scale_group_instances(nsr1_uuid, "group")
        self.log.debug("Got group ids in nsr1 after adding new nsr: %s", group_ids)
        assert group_ids == {12345}

        self.log.debug("\n\nDELETE A COMPLETELY DIFFERENT NSR\n")
        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_delete(
                XPaths.nsr_config(nsr2_uuid),
                flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                )
            yield from block.execute(now=True)

        yield from asyncio.sleep(.5, loop=self.loop)

        group_ids = self.handler.get_scale_group_instances(nsr2_uuid, "group")
        self.log.debug("Got group ids in nsr2 after deleting nsr2: %s", group_ids)
        group_ids = self.handler.get_scale_group_instances(nsr1_uuid, "group")
        self.log.debug("Got group ids in nsr1 after deleting nsr2: %s", group_ids)
        assert group_ids == {12345}

        with self.dts_c.transaction() as xact:
            block = xact.block_create()
            block.add_query_delete(
                    XPaths.scaling_group_instance(nsr1_uuid, "group", 12345),
                    flags=rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                    )
            yield from block.execute(now=True)

        yield from asyncio.sleep(2, loop=self.loop)

def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None

    NsrHandlerTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner, argv=[sys.argv[0]] + unittest_args)

if __name__ == '__main__':
    main()
