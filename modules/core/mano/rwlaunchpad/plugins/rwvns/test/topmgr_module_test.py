#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import asyncio
import logging
import os
import sys
import types
import unittest
import uuid
import random

import xmlrunner

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwMain', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwTypes', '1.0')
import gi.repository.CF as cf
import gi.repository.RwDts as rwdts
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.IetfL2TopologyYang as l2Tl
import gi.repository.RwTopologyYang as RwTl
import gi.repository.RwLaunchpadYang as launchpadyang
from gi.repository import RwsdnYang
from gi.repository.RwTypes import RwStatus

from create_stackedl2topology import MyL2Topology
from create_stackedProvNettopology import MyProvTopology
from create_stackedVMNettopology import MyVMTopology
from create_stackedSfctopology import MySfcTopology

import rw_peas
import rift.tasklets
import rift.test.dts

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class TopMgrTestCase(rift.test.dts.AbstractDTSTest):

    @classmethod
    def configure_suite(cls, rwmain):
        vns_mgr_dir = os.environ.get('VNS_MGR_DIR')

        cls.rwmain.add_tasklet(vns_mgr_dir, 'rwvnstasklet')

    @classmethod
    def configure_schema(cls):
        return RwTl.get_schema()
        
    @asyncio.coroutine
    def wait_tasklets(self):
        yield from asyncio.sleep(1, loop=self.loop)

    @classmethod
    def configure_timeout(cls):
        return 360


    @asyncio.coroutine
    def configure_l2_network(self, dts):
        nwtop = RwTl.YangData_IetfNetwork()
        l2top = MyL2Topology(nwtop, self.log)
        l2top.setup_all()
        nw_xpath = "C,/nd:network"
        self.log.info("Configuring l2 network: %s",nwtop)
        yield from dts.query_create(nw_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    nwtop)

    @asyncio.coroutine
    def configure_prov_network(self, dts):
        nwtop = RwTl.YangData_IetfNetwork()
        l2top = MyL2Topology(nwtop, self.log)
        l2top.setup_all()

        provtop = MyProvTopology(nwtop, l2top, self.log)
        provtop.setup_all()
        nw_xpath = "C,/nd:network"
        self.log.info("Configuring provider network: %s",nwtop)
        yield from dts.query_create(nw_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    nwtop)

    @asyncio.coroutine
    def configure_vm_network(self, dts):
        nwtop = RwTl.YangData_IetfNetwork()
        l2top = MyL2Topology(nwtop, self.log)
        l2top.setup_all()

        provtop = MyProvTopology(nwtop, l2top, self.log)
        provtop.setup_all()

        vmtop = MyVMTopology(nwtop, l2top, provtop, self.log)
        vmtop.setup_all()
        nw_xpath = "C,/nd:network"
        self.log.info("Configuring VM network: %s",nwtop)
        yield from dts.query_create(nw_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    nwtop)

    @asyncio.coroutine
    def configure_sfc_network(self, dts):
        nwtop = RwTl.YangData_IetfNetwork()
        l2top = MyL2Topology(nwtop, self.log)
        l2top.setup_all()

        provtop = MyProvTopology(nwtop, l2top, self.log)
        provtop.setup_all()

        vmtop = MyVMTopology(nwtop, l2top, provtop, self.log)
        vmtop.setup_all()

        sfctop = MySfcTopology(nwtop, l2top, provtop, vmtop, self.log)
        sfctop.setup_all()

        nw_xpath = "C,/nd:network"
        self.log.info("Configuring SFC network: %s",nwtop)
        yield from dts.query_create(nw_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    nwtop)


    #@unittest.skip("Skipping test_network_config")                            
    def test_network_config(self):
        self.log.debug("STARTING - test_network_config")
        tinfo = self.new_tinfo('static_network')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

        @asyncio.coroutine
        def run_test():
            networks = []
            computes = []

            yield from asyncio.sleep(120, loop=self.loop)
            yield from self.configure_l2_network(dts)
            yield from self.configure_prov_network(dts)
            yield from self.configure_vm_network(dts)
            yield from self.configure_sfc_network(dts)

        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_network_config")

def main():
    top_dir = __file__[:__file__.find('/modules/core/')]
    build_dir = os.path.join(top_dir, '.build/modules/core/rwvx/src/core_rwvx-build')
    mc_build_dir = os.path.join(top_dir, '.build/modules/core/mc/core_mc-build')
    launchpad_build_dir = os.path.join(mc_build_dir, 'rwlaunchpad')

    if 'VNS_MGR_DIR' not in os.environ:
        os.environ['VNS_MGR_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwvns')

    if 'MESSAGE_BROKER_DIR' not in os.environ:
        os.environ['MESSAGE_BROKER_DIR'] = os.path.join(build_dir, 'rwmsg/plugins/rwmsgbroker-c')

    if 'ROUTER_DIR' not in os.environ:
        os.environ['ROUTER_DIR'] = os.path.join(build_dir, 'rwdts/plugins/rwdtsrouter-c')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()

