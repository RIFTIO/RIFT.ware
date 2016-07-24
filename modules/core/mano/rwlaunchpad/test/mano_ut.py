#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import asyncio
import os
import sys
import unittest
import uuid
import xmlrunner
import argparse
import logging
import time
import types

import gi
gi.require_version('RwCloudYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwNsmYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
gi.require_version('NsrYang', '1.0')
gi.require_version('RwlogMgmtYang', '1.0')

from gi.repository import (
    RwCloudYang as rwcloudyang,
    RwDts as rwdts,
    RwLaunchpadYang as launchpadyang,
    RwNsmYang as rwnsmyang,
    RwNsrYang as rwnsryang,
    NsrYang as nsryang,
    RwResourceMgrYang as rmgryang,
    RwcalYang as rwcalyang,
    RwConfigAgentYang as rwcfg_agent,
    RwlogMgmtYang
)

from gi.repository.RwTypes import RwStatus
import rift.mano.examples.ping_pong_nsd as ping_pong_nsd
import rift.tasklets
import rift.test.dts
import rw_peas


openstack_info = {
        'username': 'pluto',
        'password': 'mypasswd',
        'auth_url': 'http://10.66.4.27:5000/v3/',
        'project_name': 'demo',
        'mgmt_network': 'private',
        }


if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async

import rift.tasklets.rwconmantasklet.juju_intf
rift.tasklets.rwconmantasklet.juju_intf.ApiEnvironment = rift.tasklets.rwconmantasklet.juju_intf.ApiEnvironmentMock

class XPaths(object):
    @staticmethod
    def nsd(k=None):
        return ("C,/nsd:nsd-catalog/nsd:nsd" +
                ("[nsd:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def vld(k=None):
        return ("C,/vld:vld-catalog/vld:vld" +
                ("[vld:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def vnfd(k=None):
        return ("C,/vnfd:vnfd-catalog/vnfd:vnfd" +
                ("[vnfd:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def vnfr(k=None):
        return ("D,/vnfr:vnfr-catalog/vnfr:vnfr" +
                ("[vnfr:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def vlr(k=None):
        return ("D,/vlr:vlr-catalog/vlr:vlr" +
                ("[vlr:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsd_ref_count(k=None):
        return ("D,/nsr:ns-instance-opdata/rw-nsr:nsd-ref-count" +
                ("[rw-nsr:nsd-id-ref='{}']".format(k) if k is not None else ""))

    @staticmethod
    def vnfd_ref_count(k=None):
        return ("D,/vnfr:vnfr-catalog/rw-vnfr:vnfd-ref-count" +
                ("[rw-nsr:nsd-id-ref='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsr_config(k=None):
        return ("C,/nsr:ns-instance-config/nsr:nsr" +
                ("[nsr:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsr_opdata(k=None):
        return ("D,/nsr:ns-instance-opdata/nsr:nsr" +
                ("[nsr:ns-instance-config-ref='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsr_config_status(k=None):
        return ("D,/nsr:ns-instance-opdata/nsr:nsr" +
                ("[nsr:ns-instance-config-ref='{}']/config_status".format(k) if k is not None else ""))

    @staticmethod
    def cm_state(k=None):
        if k is None:
            return ("D,/rw-conman:cm-state/rw-conman:cm-nsr")
        else:
            return ("D,/rw-conman:cm-state/rw-conman:cm-nsr" +
                    ("[rw-conman:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsr_scale_group_instance(nsr_id=None, group_name=None, index=None):
        return (("D,/nsr:ns-instance-opdata/nsr:nsr") +
                ("[nsr:ns-instance-config-ref='{}']".format(nsr_id) if nsr_id is not None else "") +
                ("/nsr:scaling-group-record") +
                ("[nsr:scaling-group-name-ref='{}']".format(group_name) if group_name is not None else "") +
                ("/nsr:instance") +
                ("[nsr:scaling-group-index-ref='{}']".format(index) if index is not None else ""))

    @staticmethod
    def nsr_scale_group_instance_config(nsr_id=None, group_name=None, index=None):
        return (("C,/nsr:ns-instance-config/nsr:nsr") +
                ("[nsr:id='{}']".format(nsr_id) if nsr_id is not None else "") +
                ("/nsr:scaling-group") +
                ("[nsr:scaling-group-name-ref='{}']".format(group_name) if group_name is not None else "") +
                ("/nsr:instance") +
                ("[nsr:index='{}']".format(index) if index is not None else ""))


class ManoQuerier(object):
    def __init__(self, log, dts):
        self.log = log
        self.dts = dts

    @asyncio.coroutine
    def _read_query(self, xpath, do_trace=False):
        self.log.debug("Running XPATH read query: %s (trace: %s)", xpath, do_trace)
        flags = rwdts.XactFlag.MERGE
        flags += rwdts.XactFlag.TRACE if do_trace else 0
        res_iter = yield from self.dts.query_read(
                xpath, flags=flags
                )

        results = []
        for i in res_iter:
            result = yield from i
            if result is not None:
                results.append(result.result)

        return results

    @asyncio.coroutine
    def get_cm_state(self, nsr_id=None):
        return (yield from self._read_query(XPaths.cm_state(nsr_id), False))

    @asyncio.coroutine
    def get_nsr_opdatas(self, nsr_id=None):
        return (yield from self._read_query(XPaths.nsr_opdata(nsr_id), False))

    @asyncio.coroutine
    def get_nsr_scale_group_instance_opdata(self, nsr_id=None, group_name=None, index=None):
        return (yield from self._read_query(XPaths.nsr_scale_group_instance(nsr_id, group_name, index), False))
        #return (yield from self._read_query(XPaths.nsr_scale_group_instance(nsr_id, group_name), True))

    @asyncio.coroutine
    def get_nsr_configs(self, nsr_id=None):
        return (yield from self._read_query(XPaths.nsr_config(nsr_id)))

    @asyncio.coroutine
    def get_nsr_config_status(self, nsr_id=None):
        return (yield from self._read_query(XPaths.nsr_config_status(nsr_id)))

    @asyncio.coroutine
    def get_vnfrs(self, vnfr_id=None):
        return (yield from self._read_query(XPaths.vnfr(vnfr_id)))

    @asyncio.coroutine
    def get_vlrs(self, vlr_id=None):
        return (yield from self._read_query(XPaths.vlr(vlr_id)))

    @asyncio.coroutine
    def get_nsd_ref_counts(self, nsd_id=None):
        return (yield from self._read_query(XPaths.nsd_ref_count(nsd_id)))

    @asyncio.coroutine
    def get_vnfd_ref_counts(self, vnfd_id=None):
        return (yield from self._read_query(XPaths.vnfd_ref_count(vnfd_id)))

    @asyncio.coroutine
    def delete_nsr(self, nsr_id):
        with self.dts.transaction() as xact:
            yield from self.dts.query_delete(
                    XPaths.nsr_config(nsr_id),
                    0
                    #rwdts.XactFlag.TRACE,
                    #rwdts.Flag.ADVISE,
                    )

    @asyncio.coroutine
    def delete_nsd(self, nsd_id):
        nsd_xpath = XPaths.nsd(nsd_id)
        self.log.debug("Attempting to delete NSD with path = %s", nsd_xpath)
        with self.dts.transaction() as xact:
            yield from self.dts.query_delete(
                    nsd_xpath,
                    rwdts.XactFlag.ADVISE,
                    )

    @asyncio.coroutine
    def delete_vnfd(self, vnfd_id):
        vnfd_xpath = XPaths.vnfd(vnfd_id)
        self.log.debug("Attempting to delete VNFD with path = %s", vnfd_xpath)
        with self.dts.transaction() as xact:
            yield from self.dts.query_delete(
                    vnfd_xpath,
                    rwdts.XactFlag.ADVISE,
                    )

    @asyncio.coroutine
    def update_nsd(self, nsd_id, nsd_msg):
        nsd_xpath = XPaths.nsd(nsd_id)
        self.log.debug("Attempting to update NSD with path = %s", nsd_xpath)
        with self.dts.transaction() as xact:
            yield from self.dts.query_update(
                    nsd_xpath,
                    rwdts.XactFlag.ADVISE,
                    nsd_msg,
                    )

    @asyncio.coroutine
    def update_vnfd(self, vnfd_id, vnfd_msg):
        vnfd_xpath = XPaths.vnfd(vnfd_id)
        self.log.debug("Attempting to delete VNFD with path = %s", vnfd_xpath)
        with self.dts.transaction() as xact:
            yield from self.dts.query_update(
                    vnfd_xpath,
                    rwdts.XactFlag.ADVISE,
                    vnfd_msg,
                    )


class ManoTestCase(rift.test.dts.AbstractDTSTest):
    @asyncio.coroutine
    def verify_nsr_state(self, nsr_id, state):
        nsrs = yield from self.querier.get_nsr_opdatas(nsr_id)
        self.assertEqual(1, len(nsrs))
        nsr = nsrs[0]

        self.log.debug("Got nsr = %s", nsr)
        self.assertEqual(state, nsr.operational_status)

    @asyncio.coroutine
    def verify_vlr_state(self, vlr_id, state):
        vlrs = yield from self.querier.get_vlrs(vlr_id)
        self.assertEqual(1, len(vlrs))
        vlr = vlrs[0]

        self.assertEqual(state, vlr.operational_status)

    def verify_vdu_state(self, vdu, state):
        self.assertEqual(state, vdu.operational_status)

    @asyncio.coroutine
    def verify_vnf_state(self, vnfr_id, state):
        vnfrs = yield from self.querier.get_vnfrs(vnfr_id)
        self.assertEqual(1, len(vnfrs))
        vnfr = vnfrs[0]

        self.assertEqual(state, vnfr.operational_status)

    @asyncio.coroutine
    def terminate_nsr(self, nsr_id):
        self.log.debug("Terminating nsr id: %s", nsr_id)
        yield from self.querier.delete_nsr(nsr_id)

    @asyncio.coroutine
    def verify_nsr_deleted(self, nsr_id):
        nsr_opdatas = yield from self.querier.get_nsr_opdatas(nsr_id)
        self.assertEqual(0, len(nsr_opdatas))

        nsr_configs = yield from self.querier.get_nsr_configs(nsr_id)
        self.assertEqual(0, len(nsr_configs))

    @asyncio.coroutine
    def verify_num_vlrs(self, num_vlrs):
        vlrs = yield from self.querier.get_vlrs()
        self.assertEqual(num_vlrs, len(vlrs))

    @asyncio.coroutine
    def get_nsr_vlrs(self, nsr_id):
        nsrs = yield from self.querier.get_nsr_opdatas(nsr_id)
        return [v.vlr_ref for v in nsrs[0].vlr]

    @asyncio.coroutine
    def get_nsr_vnfs(self, nsr_id):
        nsrs = yield from self.querier.get_nsr_opdatas(nsr_id)
        return nsrs[0].constituent_vnfr_ref

    @asyncio.coroutine
    def get_vnf_vlrs(self, vnfr_id):
        vnfrs = yield from self.querier.get_vnfrs(vnfr_id)
        return [i.vlr_ref for i in vnfrs[0].internal_vlr]

    @asyncio.coroutine
    def verify_num_nsr_vlrs(self, nsr_id, num_vlrs):
        vlrs = yield from self.get_nsr_vlrs(nsr_id)
        self.assertEqual(num_vlrs, len(vlrs))

    @asyncio.coroutine
    def verify_num_nsr_vnfrs(self, nsr_id, num_vnfs):
        vnfs = yield from self.get_nsr_vnfs(nsr_id)
        self.assertEqual(num_vnfs, len(vnfs))

    @asyncio.coroutine
    def verify_num_vnfr_vlrs(self, vnfr_id, num_vlrs):
        vlrs = yield from self.get_vnf_vlrs(vnfr_id)
        self.assertEqual(num_vlrs, len(vlrs))

    @asyncio.coroutine
    def get_vnf_vdus(self, vnfr_id):
        vnfrs = yield from self.querier.get_vnfrs(vnfr_id)
        return [i for i in vnfrs[0].vdur]

    @asyncio.coroutine
    def verify_num_vnfr_vdus(self, vnfr_id, num_vdus):
        vdus = yield from self.get_vnf_vdus(vnfr_id)
        self.assertEqual(num_vdus, len(vdus))

    @asyncio.coroutine
    def verify_num_vnfrs(self, num_vnfrs):
        vnfrs = yield from self.querier.get_vnfrs()
        self.assertEqual(num_vnfrs, len(vnfrs))

    @asyncio.coroutine
    def verify_nsd_ref_count(self, nsd_id, num_ref):
        nsd_ref_counts = yield from self.querier.get_nsd_ref_counts(nsd_id)
        self.assertEqual(num_ref, nsd_ref_counts[0].instance_ref_count)

class DescriptorPublisher(object):
    def __init__(self, log, loop, dts):
        self.log = log
        self.loop = loop
        self.dts = dts

        self._registrations = []

    @asyncio.coroutine
    def publish(self, w_path, path, desc):
        ready_event = asyncio.Event(loop=self.loop)

        @asyncio.coroutine
        def on_ready(regh, status):
            self.log.debug("Create element: %s, obj-type:%s obj:%s",
                           path, type(desc), desc)
            with self.dts.transaction() as xact:
                regh.create_element(path, desc, xact.xact)
            self.log.debug("Created element: %s, obj:%s", path, desc)
            ready_event.set()

        handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready
                )

        self.log.debug("Registering path: %s, obj:%s", w_path, desc)
        reg = yield from self.dts.register(
                w_path,
                handler,
                flags=rwdts.Flag.PUBLISHER | rwdts.Flag.NO_PREP_READ
                )
        self._registrations.append(reg)
        self.log.debug("Registered path : %s", w_path)
        yield from ready_event.wait()

        return reg

    def unpublish_all(self):
        self.log.debug("Deregistering all published descriptors")
        for reg in self._registrations:
            reg.deregister()


class PingPongNsrConfigPublisher(object):
    XPATH = "C,/nsr:ns-instance-config"

    def __init__(self, log, loop, dts, ping_pong, cloud_account_name):
        self.dts = dts
        self.log = log
        self.loop = loop
        self.ref = None

        self.nsr_config = rwnsryang.YangData_Nsr_NsInstanceConfig()

        nsr = rwnsryang.YangData_Nsr_NsInstanceConfig_Nsr()
        nsr.id = str(uuid.uuid4())
        nsr.name = "ns1.{}".format(nsr.id)
        nsr.nsd_ref = ping_pong.nsd_id
        nsr.cloud_account = cloud_account_name

        inputs = nsryang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter()
        inputs.xpath = "/nsd:nsd-catalog/nsd:nsd[nsd:id={}]/nsd:name".format(ping_pong.nsd_id)
        inputs.value = "inigo montoya"

        
        fast_cpu = {'metadata_key': 'FASTCPU', 'metadata_value': 'True'}
        self.create_nsd_placement_group_map(nsr,
                                            group_name      = 'Orcus',
                                            cloud_type      = 'openstack',
                                            construct_type  = 'host_aggregate',
                                            construct_value = [fast_cpu])
        
        fast_storage = {'metadata_key': 'FASTSSD', 'metadata_value': 'True'}
        self.create_nsd_placement_group_map(nsr,
                                            group_name      = 'Quaoar',
                                            cloud_type      = 'openstack',
                                            construct_type  = 'host_aggregate',
                                            construct_value = [fast_storage])

        fast_cpu = {'metadata_key': 'BLUE_HW', 'metadata_value': 'True'}
        self.create_vnfd_placement_group_map(nsr,
                                             group_name      = 'Eris',
                                             vnfd_id         = ping_pong.ping_vnfd_id,
                                             cloud_type      = 'openstack',
                                             construct_type  = 'host_aggregate',
                                             construct_value = [fast_cpu])
        
        fast_storage = {'metadata_key': 'YELLOW_HW', 'metadata_value': 'True'}
        self.create_vnfd_placement_group_map(nsr,
                                             group_name      = 'Weywot',
                                             vnfd_id         = ping_pong.pong_vnfd_id,
                                             cloud_type      = 'openstack',
                                             construct_type  = 'host_aggregate',
                                             construct_value = [fast_storage])


        nsr.input_parameter.append(inputs)

        self._nsr = nsr
        self.nsr_config.nsr.append(nsr)

        self._ready_event = asyncio.Event(loop=self.loop)
        asyncio.ensure_future(self.register(), loop=loop)

    @asyncio.coroutine
    def register(self):
        @asyncio.coroutine
        def on_ready(regh, status):
            self._ready_event.set()

        self.log.debug("Registering path: %s", PingPongNsrConfigPublisher.XPATH)
        self.reg = yield from self.dts.register(
                PingPongNsrConfigPublisher.XPATH,
                flags=rwdts.Flag.PUBLISHER,
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    ),
                )

    @asyncio.coroutine
    def publish(self):
        yield from self._ready_event.wait()
        with self.dts.transaction() as xact:
            self.reg.create_element(
                    PingPongNsrConfigPublisher.XPATH,
                    self.nsr_config,
                    xact=xact.xact,
                    )

        return self._nsr.id

    @asyncio.coroutine
    def create_scale_group_instance(self, group_name, index):
        index = 1
        scaling_group = self.nsr_config.nsr[0].scaling_group.add()
        scaling_group.from_dict({
            "scaling_group_name_ref": group_name,
            "instance": [{"index": index}],
            })
        with self.dts.transaction() as xact:
            self.reg.update_element(
                    PingPongNsrConfigPublisher.XPATH,
                    self.nsr_config,
                    xact=xact.xact,
                    )

        return index
    
    def create_nsd_placement_group_map(self,
                                       nsr,
                                       group_name,
                                       cloud_type,
                                       construct_type,
                                       construct_value):
        placement_group  = nsr.nsd_placement_group_maps.add()
        placement_group.from_dict({
            "placement_group_ref" : group_name,
            "cloud_type"          : cloud_type,
            construct_type        : construct_value,
            })
        

    def create_vnfd_placement_group_map(self,
                                        nsr,
                                        group_name,
                                        vnfd_id,
                                        cloud_type,
                                        construct_type,
                                        construct_value):
        placement_group  = nsr.vnfd_placement_group_maps.add()
        placement_group.from_dict({
            "placement_group_ref"  : group_name,
            "vnfd_id_ref"          : vnfd_id,
            "cloud_type"           : cloud_type,
            construct_type         : construct_value,
            })
        
    
    @asyncio.coroutine
    def delete_scale_group_instance(self, group_name, index):
        self.log.debug("Deleting scale group %s instance %s", group_name, index)
        #del self.nsr_config.nsr[0].scaling_group[0].instance[0]
        xpath = XPaths.nsr_scale_group_instance_config(self.nsr_config.nsr[0].id, group_name, index)
        yield from self.dts.query_delete(xpath, flags=rwdts.XactFlag.ADVISE)
        #with self.dts.transaction() as xact:
        #    self.reg.update_element(
        #            PingPongNsrConfigPublisher.XPATH,
        #            self.nsr_config,
        #            flags=rwdts.XactFlag.REPLACE,
        #            xact=xact.xact,
        #            )

    def deregister(self):
        if self.reg is not None:
            self.reg.deregister()


class PingPongDescriptorPublisher(object):
    def __init__(self, log, loop, dts, num_external_vlrs=1, num_internal_vlrs=1, num_ping_vms=1):
        self.log = log
        self.loop = loop
        self.dts = dts

        self.querier = ManoQuerier(self.log, self.dts)
        self.publisher = DescriptorPublisher(self.log, self.loop, self.dts)
        self.ping_vnfd, self.pong_vnfd, self.ping_pong_nsd = \
                ping_pong_nsd.generate_ping_pong_descriptors(
                        pingcount=1,
                        external_vlr_count=num_external_vlrs,
                        internal_vlr_count=num_internal_vlrs,
                        num_vnf_vms=2,
                        mano_ut=True,
                        use_scale_group=True,
                        use_mon_params=False,
                        )

        self.config_dir = os.path.join(os.getenv('RIFT_ARTIFACTS'),
                                       "launchpad/libs",
                                       self.ping_pong_nsd.id,
                                       "config")

    @property
    def nsd_id(self):
        return self.ping_pong_nsd.id

    @property
    def ping_vnfd_id(self):
        return self.ping_vnfd.id

    @property
    def pong_vnfd_id(self):
        return self.pong_vnfd.id

    @asyncio.coroutine
    def publish_desciptors(self):
        # Publish ping_vnfd
        xpath = XPaths.vnfd(self.ping_vnfd_id)
        xpath_wild = XPaths.vnfd()
        for obj in self.ping_vnfd.descriptor.vnfd:
            self.log.debug("Publishing ping_vnfd path: %s - %s, type:%s, obj:%s",
                           xpath, xpath_wild, type(obj), obj)
            yield from self.publisher.publish(xpath_wild, xpath, obj)

        # Publish pong_vnfd
        xpath = XPaths.vnfd(self.pong_vnfd_id)
        xpath_wild = XPaths.vnfd()
        for obj in self.pong_vnfd.descriptor.vnfd:
            self.log.debug("Publishing pong_vnfd path: %s, wild_path: %s, obj:%s",
                           xpath, xpath_wild, obj)
            yield from self.publisher.publish(xpath_wild, xpath, obj)

        # Publish ping_pong_nsd
        xpath = XPaths.nsd(self.nsd_id)
        xpath_wild = XPaths.nsd()
        for obj in self.ping_pong_nsd.descriptor.nsd:
            self.log.debug("Publishing ping_pong nsd path: %s, wild_path: %s, obj:%s",
                           xpath, xpath_wild, obj)
            yield from self.publisher.publish(xpath_wild, xpath, obj)

        self.log.debug("DONE - publish_desciptors")

    def unpublish_descriptors(self):
        self.publisher.unpublish_all()

    @asyncio.coroutine
    def delete_nsd(self):
        yield from self.querier.delete_nsd(self.ping_pong_nsd.id)

    @asyncio.coroutine
    def delete_ping_vnfd(self):
        yield from self.querier.delete_vnfd(self.ping_vnfd.id)

    @asyncio.coroutine
    def update_nsd(self):
        yield from self.querier.update_nsd(
                self.ping_pong_nsd.id,
                self.ping_pong_nsd.descriptor.nsd[0]
                )

    @asyncio.coroutine
    def update_ping_vnfd(self):
        yield from self.querier.update_vnfd(
                self.ping_vnfd.id,
                self.ping_vnfd.descriptor.vnfd[0]
                )




class ManoTestCase(rift.test.dts.AbstractDTSTest):
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
        vns_dir = os.environ.get('VNS_DIR')
        vnfm_dir = os.environ.get('VNFM_DIR')
        nsm_dir = os.environ.get('NSM_DIR')
        rm_dir = os.environ.get('RM_DIR')

        rwmain.add_tasklet(vns_dir, 'rwvnstasklet')
        rwmain.add_tasklet(vnfm_dir, 'rwvnfmtasklet')
        rwmain.add_tasklet(nsm_dir, 'rwnsmtasklet')
        rwmain.add_tasklet(rm_dir, 'rwresmgrtasklet')
        rwmain.add_tasklet(rm_dir, 'rwconmantasklet')

    @classmethod
    def configure_schema(cls):
        return rwnsmyang.get_schema()

    @classmethod
    def configure_timeout(cls):
        return 240

    @staticmethod
    def get_cal_account(account_type, account_name):
        """
        Creates an object for class RwcalYang.Clo
        """
        account = rwcloudyang.CloudAccount()
        if account_type == 'mock':
            account.name          = account_name
            account.account_type  = "mock"
            account.mock.username = "mock_user"
        elif ((account_type == 'openstack_static') or (account_type == 'openstack_dynamic')):
            account.name = account_name
            account.account_type = 'openstack'
            account.openstack.key = openstack_info['username']
            account.openstack.secret       = openstack_info['password']
            account.openstack.auth_url     = openstack_info['auth_url']
            account.openstack.tenant       = openstack_info['project_name']
            account.openstack.mgmt_network = openstack_info['mgmt_network']
        return account

    @asyncio.coroutine
    def configure_cloud_account(self, dts, cloud_type, cloud_name="cloud1"):
        account = self.get_cal_account(cloud_type, cloud_name)
        account_xpath = "C,/rw-cloud:cloud/rw-cloud:account[rw-cloud:name='{}']".format(cloud_name)
        self.log.info("Configuring cloud-account: %s", account)
        yield from dts.query_create(account_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    account)

    @asyncio.coroutine
    def configure_juju_account(self, dts):
        # This is fake account for juju "mock - monkey patch"
        account = rwcfg_agent.ConfigAgentAccount()
        account.name = 'juju'
        account.account_type = 'juju'
        account.juju.ip_address = '1.1.1.1'
        account.juju.port = 17070
        account.juju.user = 'user-admin'
        account.juju.secret = 'nfvjuju'

        configagent_account_xpath = "C,/rw-config-agent:config-agent/rw-config-agent:account[rw-config-agent:name='juju']"
        self.log.info("Configuring juju-account: %s", account)
        yield from dts.query_create(configagent_account_xpath,
                                    rwdts.XactFlag.ADVISE | rwdts.XactFlag.TRACE,
                                    account)

    @asyncio.coroutine
    def wait_tasklets(self):
        yield from asyncio.sleep(5, loop=self.loop)

    def configure_test(self, loop, test_id):
        self.log.debug("STARTING - %s", self.id())
        self.tinfo = self.new_tinfo(self.id())
        self.dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop)
        self.ping_pong = PingPongDescriptorPublisher(self.log, self.loop, self.dts)
        self.querier = ManoQuerier(self.log, self.dts)
        self.nsr_publisher = PingPongNsrConfigPublisher(
                self.log,
                loop,
                self.dts,
                self.ping_pong,
                "mock_account",
                )

    def test_create_nsr_record(self):
        
        @asyncio.coroutine
        def verify_cm_state(termination=False, nsrid=None):
            self.log.debug("Verifying cm_state path = %s", XPaths.cm_state(nsrid))
            #print("###>>> Verifying cm_state path:", XPaths.cm_state(nsrid))

            loop_count = 10
            loop_sleep = 10
            while loop_count:
                yield from asyncio.sleep(loop_sleep, loop=self.loop)
                loop_count -= 1
                cm_nsr = None
                cm_nsr_i = yield from self.querier.get_cm_state(nsr_id=nsrid)
                if (cm_nsr_i is not None and len(cm_nsr_i) != 0):
                    self.assertEqual(1, len(cm_nsr_i))
                    cm_nsr = cm_nsr_i[0].as_dict()
                    #print("###>>> cm_nsr=", cm_nsr)
                if termination:
                    if len(cm_nsr_i) == 0:
                        print("\n###>>> cm-state NSR deleted OK <<<###\n")
                        return
                elif (cm_nsr is not None and
                    'state' in cm_nsr and
                    (cm_nsr['state'] == 'ready')):
                    self.log.debug("Got cm_nsr record %s", cm_nsr)
                    print("\n###>>> cm-state NSR 'ready' OK <<<###\n")
                    return
                    
                # if (len(cm_nsr_i) == 1 and cm_nsr_i[0].state == 'ready'):
                #     self.log.debug("Got cm_nsr record %s", cm_nsr)
                # else:
                #     yield from asyncio.sleep(10, loop=self.loop)

            print("###>>> Failed cm-state, termination:", termination)
            self.assertEqual(1, loop_count)

        @asyncio.coroutine
        def verify_nsr_opdata(termination=False):
            self.log.debug("Verifying nsr opdata path = %s", XPaths.nsr_opdata())

            while True:
                nsrs = yield from self.querier.get_nsr_opdatas()
                if termination:
                    if len(nsrs) != 0:
                        for i in range(10):
                            nsrs = yield from self.querier.get_nsr_opdatas()
                            if len(nsrs) == 0:
                                self.log.debug("No active NSR records found. NSR termination successful")
                                return
                        else:
                            self.assertEqual(0, len(nsrs))
                            self.log.error("Active NSR records found. NSR termination failed")

                    else:
                        self.log.debug("No active NSR records found. NSR termination successful")
                        self.assertEqual(0, len(nsrs))
                        return

                nsr = nsrs[0]
                self.log.debug("Got nsr record %s", nsr)
                if nsr.operational_status == 'running':
                    self.log.debug("!!! Rcvd NSR with running status !!!")
                    self.assertEqual("configuring", nsr.config_status)
                    break

                self.log.debug("Rcvd NSR with %s status", nsr.operational_status)
                self.log.debug("Sleeping for 10 seconds")
                yield from asyncio.sleep(10, loop=self.loop)

        @asyncio.coroutine
        def verify_nsr_config(termination=False):
            self.log.debug("Verifying nsr config path = %s", XPaths.nsr_config())

            nsr_configs = yield from self.querier.get_nsr_configs()
            self.assertEqual(1, len(nsr_configs))

            nsr_config = nsr_configs[0]
            self.assertEqual(
                    "/nsd:nsd-catalog/nsd:nsd[nsd:id={}]/nsd:name".format(self.ping_pong.nsd_id),
                    nsr_config.input_parameter[0].xpath,
                    )

        @asyncio.coroutine
        def verify_nsr_config_status(termination=False, nsrid=None):
            if termination is False and nsrid is not None:
                self.log.debug("Verifying nsr config status path = %s", XPaths.nsr_opdata(nsrid))

                loop_count = 6
                loop_sleep = 10
                while loop_count:
                    loop_count -= 1
                    yield from asyncio.sleep(loop_sleep, loop=self.loop)
                    nsr_opdata_l = yield from self.querier.get_nsr_opdatas(nsrid)
                    self.assertEqual(1, len(nsr_opdata_l))
                    nsr_opdata = nsr_opdata_l[0].as_dict()
                    if ("configured" == nsr_opdata['config_status']):
                        print("\n###>>> NSR Config Status 'configured' OK <<<###\n")
                        return
                self.assertEqual("configured", nsr_opdata['config_status'])
                
        @asyncio.coroutine
        def verify_vnfr_record(termination=False):
            self.log.debug("Verifying vnfr record path = %s, Termination=%d",
                           XPaths.vnfr(), termination)
            if termination:
                for i in range(10):
                    vnfrs = yield from self.querier.get_vnfrs()
                    if len(vnfrs) == 0:
                        return True

                    for vnfr in vnfrs:
                        self.log.debug("VNFR still exists = %s", vnfr)

                    yield from asyncio.sleep(.5, loop=self.loop)


                assert len(vnfrs) == 0

            while True:
                vnfrs = yield from self.querier.get_vnfrs()
                if len(vnfrs) != 0 and termination is False:
                    vnfr = vnfrs[0]
                    self.log.debug("Rcvd VNFR with %s status", vnfr.operational_status)
                    if vnfr.operational_status == 'running':
                        self.log.debug("!!! Rcvd VNFR with running status !!!")
                        return True

                    elif vnfr.operational_status == "failed":
                        self.log.debug("!!! Rcvd VNFR with failed status !!!")
                        return False

                self.log.debug("Sleeping for 10 seconds")
                yield from asyncio.sleep(10, loop=self.loop)

        @asyncio.coroutine
        def verify_vlr_record(termination=False):
            vlr_xpath = XPaths.vlr()
            self.log.debug("Verifying vlr record path = %s, termination: %s",
                           vlr_xpath, termination)
            res_iter = yield from self.dts.query_read(vlr_xpath)

            for i in res_iter:
                result = yield from i
                if termination:
                    self.assertIsNone(result)

                self.log.debug("Got vlr record %s", result)

        @asyncio.coroutine
        def verify_nsd_ref_count(termination):
            self.log.debug("Verifying nsd ref count= %s", XPaths.nsd_ref_count())
            res_iter = yield from self.dts.query_read(XPaths.nsd_ref_count())

            for i in res_iter:
                result = yield from i
                self.log.debug("Got nsd ref count record %s", result)

        @asyncio.coroutine
        def verify_vnfd_ref_count(termination):
            self.log.debug("Verifying vnfd ref count= %s", XPaths.vnfd_ref_count())
            res_iter = yield from self.dts.query_read(XPaths.vnfd_ref_count())

            for i in res_iter:
                result = yield from i
                self.log.debug("Got vnfd ref count record %s", result)

        @asyncio.coroutine
        def verify_scale_group_reaches_state(nsr_id, scale_group, index, state, timeout=1000):
            start_time = time.time()
            instance_state = None
            while (time.time() - start_time) < timeout:
                results = yield from self.querier.get_nsr_opdatas(nsr_id=nsr_id)
                if len(results) == 1:
                    result = results[0]
                    if len(result.scaling_group_record) == 0:
                        continue

                    if len(result.scaling_group_record[0].instance) == 0:
                        continue

                    instance = result.scaling_group_record[0].instance[0]
                    self.assertEqual(instance.scaling_group_index_ref, index)

                    instance_state = instance.op_status
                    if instance_state == state:
                        self.log.debug("Scale group instance reached %s state", state)
                        return

                yield from asyncio.sleep(1, loop=self.loop)

            self.assertEqual(state, instance_state)

        @asyncio.coroutine
        def verify_results(termination=False, nsrid=None):
            yield from verify_vnfr_record(termination)
            #yield from verify_vlr_record(termination)
            yield from verify_nsr_opdata(termination)
            yield from verify_nsr_config(termination)
            yield from verify_nsd_ref_count(termination)
            yield from verify_vnfd_ref_count(termination)

            # Config Manager
            yield from verify_cm_state(termination, nsrid)
            yield from verify_nsr_config_status(termination, nsrid)

        @asyncio.coroutine
        def verify_scale_instance(index):
            self.log.debug("Verifying scale record path = %s, Termination=%d",
                           XPaths.vnfr(), termination)
            if termination:
                for i in range(5):
                    vnfrs = yield from self.querier.get_vnfrs()
                    if len(vnfrs) == 0:
                        return True

                    for vnfr in vnfrs:
                        self.log.debug("VNFR still exists = %s", vnfr)


                assert len(vnfrs) == 0

            while True:
                vnfrs = yield from self.querier.get_vnfrs()
                if len(vnfrs) != 0 and termination is False:
                    vnfr = vnfrs[0]
                    self.log.debug("Rcvd VNFR with %s status", vnfr.operational_status)
                    if vnfr.operational_status == 'running':
                        self.log.debug("!!! Rcvd VNFR with running status !!!")
                        return True

                    elif vnfr.operational_status == "failed":
                        self.log.debug("!!! Rcvd VNFR with failed status !!!")
                        return False

                self.log.debug("Sleeping for 10 seconds")
                yield from asyncio.sleep(10, loop=self.loop)

        @asyncio.coroutine
        def terminate_ns(nsr_id):
            xpath = XPaths.nsr_config(nsr_id)
            self.log.debug("Terminating network service with path %s", xpath)
            yield from self.dts.query_delete(xpath, flags=rwdts.XactFlag.ADVISE)
            self.log.debug("Terminated network service with path %s", xpath)

        @asyncio.coroutine
        def run_test():
            yield from self.wait_tasklets()


            cloud_type = "mock"
            yield from self.configure_cloud_account(self.dts, cloud_type, "mock_account")

            # yield from self.configure_juju_account(self.dts)
            yield from self.ping_pong.publish_desciptors()

            # Attempt deleting VNFD not in use
            yield from self.ping_pong.update_ping_vnfd()

            # Attempt deleting NSD not in use
            yield from self.ping_pong.update_nsd()

            # Attempt deleting VNFD not in use
            yield from self.ping_pong.delete_ping_vnfd()

            # Attempt deleting NSD not in use
            yield from self.ping_pong.delete_nsd()

            yield from self.ping_pong.publish_desciptors()

            nsr_id = yield from self.nsr_publisher.publish()

            yield from verify_results(nsrid=nsr_id)

            # yield from self.nsr_publisher.create_scale_group_instance("ping_group", 1)

            # yield from verify_scale_group_reaches_state(nsr_id, "ping_group", 1, "running")

            # yield from self.nsr_publisher.delete_scale_group_instance("ping_group", 1)

            yield from asyncio.sleep(10, loop=self.loop)

            # Attempt deleting VNFD in use
            yield from self.ping_pong.delete_ping_vnfd()

            # Attempt deleting NSD in use
            yield from self.ping_pong.delete_nsd()

            yield from terminate_ns(nsr_id)

            yield from asyncio.sleep(25, loop=self.loop)
            self.log.debug("Verifying termination results")
            yield from verify_results(termination=True, nsrid=nsr_id)
            self.log.debug("Verified termination results")

            self.log.debug("Attempting to delete VNFD for real")
            yield from self.ping_pong.delete_ping_vnfd()

            self.log.debug("Attempting to delete NSD for real")
            yield from self.ping_pong.delete_nsd()

        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()


def main():
    top_dir = __file__[:__file__.find('/modules/core/')]
    build_dir = os.path.join(top_dir, '.build/modules/core/rwvx/src/core_rwvx-build')
    launchpad_build_dir = os.path.join(top_dir, '.build/modules/core/mc/core_mc-build/rwlaunchpad')

    if 'VNS_DIR' not in os.environ:
        os.environ['VNS_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwvns')

    if 'VNFM_DIR' not in os.environ:
        os.environ['VNFM_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwvnfm')

    if 'NSM_DIR' not in os.environ:
        os.environ['NSM_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwnsm')

    if 'RM_DIR' not in os.environ:
        os.environ['RM_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwresmgrtasklet')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None

    ManoTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner, argv=[sys.argv[0]] + unittest_args)

if __name__ == '__main__':
    main()

# vim: sw=4
