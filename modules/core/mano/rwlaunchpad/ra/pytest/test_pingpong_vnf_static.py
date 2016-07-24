#!/usr/bin/env python
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file lp_test.py
@author Austin Cormier (Austin.Cormier@riftio.com)
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 11/03/2015
@brief Launchpad System Test
"""

import json
import logging
import os
import pytest
import shlex
import requests
import subprocess
import time
import uuid
import rift.auto.session

import gi
gi.require_version('RwMcYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
from gi.repository import RwMcYang, NsdYang, NsrYang, RwNsrYang, VldYang, RwVnfdYang, RwLaunchpadYang, RwBaseYang

logging.basicConfig(level=logging.DEBUG)


@pytest.fixture(scope='module')
def mc_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwMcYang)


@pytest.fixture(scope='module')
def launchpad_session(request, mc_proxy, mgmt_domain_name, session_type):
    launchpad_host = mc_proxy.get("/mgmt-domain/domain[name='%s']/launchpad/ip_address" % mgmt_domain_name)

    if session_type == 'netconf':
        launchpad_session = rift.auto.session.NetconfSession(host=launchpad_host)
    elif session_type == 'restconf':
        launchpad_session = rift.auto.session.RestconfSession(host=launchpad_host)

    launchpad_session.connect()
    rift.vcs.vcs.wait_until_system_started(launchpad_session)
    return launchpad_session


@pytest.fixture(scope='module')
def launchpad_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwLaunchpadYang)


@pytest.fixture(scope='module')
def vnfd_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwVnfdYang)


@pytest.fixture(scope='module')
def vld_proxy(request, launchpad_session):
    return launchpad_session.proxy(VldYang)


@pytest.fixture(scope='module')
def nsd_proxy(request, launchpad_session):
    return launchpad_session.proxy(NsdYang)


@pytest.fixture(scope='module')
def nsr_proxy(request, launchpad_session):
    return launchpad_session.proxy(NsrYang)


@pytest.fixture(scope='module')
def rwnsr_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwNsrYang)


@pytest.fixture(scope='module')
def base_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwBaseYang)


def create_nsr_from_nsd_id(nsd_id):
    nsr = RwNsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
    nsr.id = str(uuid.uuid4())
    nsr.name = "nsr_name"
    nsr.short_name = "nsr_short_name"
    nsr.description = "This is a description"
    nsr.nsd_ref = nsd_id
    nsr.admin_status = "ENABLED"
    nsr.cloud_account = "cloud_account_name"

    return nsr


def upload_descriptor(logger, descriptor_file, host="127.0.0.1"):
    curl_cmd = 'curl -F "descriptor=@{file}" http://{host}:4567/api/upload'.format(
            file=descriptor_file,
            host=host,
            )
    logger.debug("Uploading descriptor %s using cmd: %s", descriptor_file, curl_cmd)
    stdout = subprocess.check_output(shlex.split(curl_cmd), universal_newlines=True)

    json_out = json.loads(stdout)
    transaction_id = json_out["transaction_id"]

    return transaction_id


class DescriptorOnboardError(Exception):
    pass


def wait_onboard_transaction_finished(logger, transaction_id, timeout=600, host="127.0.0.1"):
    logger.info("Waiting for onboard trans_id %s to complete", transaction_id)
    uri = 'http://%s:4567/api/upload/%s/state' % (host, transaction_id)
    elapsed = 0
    start = time.time()
    while elapsed < timeout:
        reply = requests.get(uri)
        state = reply.json()
        if state["status"] == "success":
            break

        if state["status"] != "pending":
            raise DescriptorOnboardError(state)

        time.sleep(1)
        elapsed = time.time() - start

    if state["status"] != "success":
        raise DescriptorOnboardError(state)

    logger.info("Descriptor onboard was successful")

    

@pytest.mark.setup('pingpong')
@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestPingPongStart(object):
    def test_configure_pools(self, mc_proxy, vm_pool_name, network_pool_name):
        vm_pool = mc_proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        available_ids = [vm.id for vm in vm_pool.available]

        assert len(available_ids) >= 2

        network_pool = mc_proxy.get("/network-pool/pool[name='%s']" % network_pool_name)
        available_ids = [network.id for network in network_pool.available]
        assert len(available_ids) >= 3

        vm_pool_config = RwMcYang.VmPool.from_dict({
                'name':vm_pool_name,
                'assigned':[
                    {'id':available_ids[0]},
                    {'id':available_ids[1]},
                ]})

        mc_proxy.merge_config(
                "/vm-pool/pool[name='%s']" % vm_pool_name,
                vm_pool_config)

        network_pool_config = RwMcYang.NetworkPool.from_dict({
                'name':network_pool_name,
                'assigned':[
                    {'id':available_ids[0]},
                    {'id':available_ids[1]},
                    {'id':available_ids[2]},
                ]})
        mc_proxy.merge_config(
                "/network-pool/pool[name='%s']" % network_pool_name,
                network_pool_config)

    def test_restart_launchpad(self, mc_proxy, mgmt_domain_name, launchpad_session, launchpad_scraper):
        mc_proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=10,
                fail_on=['crashed'])

        stop_launchpad_input = RwMcYang.StopLaunchpadInput(mgmt_domain=mgmt_domain_name)
        stop_launchpad_output = mc_proxy.rpc(stop_launchpad_input)

        mc_proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'stopped',
                timeout=60,
                fail_on=['crashed'])

        start_launchpad_input = RwMcYang.StartLaunchpadInput(mgmt_domain=mgmt_domain_name)
        start_launchpad_output = mc_proxy.rpc(start_launchpad_input)
        mc_proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=200,
                fail_on=['crashed'])

        rift.vcs.vcs.wait_until_system_started(launchpad_session)
        launchpad_scraper.reset()

    def test_onboard_ping_vnfd(self, logger, mc_proxy, mgmt_domain_name, vnfd_proxy, ping_vnfd_package_file):
        launchpad_host = mc_proxy.get("/mgmt-domain/domain[name='%s']/launchpad/ip_address" % mgmt_domain_name)
        logger.info("Onboarding ping_vnfd package: %s", ping_vnfd_package_file)
        trans_id = upload_descriptor(logger, ping_vnfd_package_file, launchpad_host)
        wait_onboard_transaction_finished(logger, trans_id, host=launchpad_host)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 1, "There should only be a single vnfd"
        vnfd = vnfds[0]
        assert vnfd.name == "rw_ping_vnfd"

    def test_onboard_pong_vnfd(self, logger, mc_proxy, mgmt_domain_name, vnfd_proxy, pong_vnfd_package_file):
        launchpad_host = mc_proxy.get("/mgmt-domain/domain[name='%s']/launchpad/ip_address" % mgmt_domain_name)
        logger.info("Onboarding pong_vnfd package: %s", pong_vnfd_package_file)
        trans_id = upload_descriptor(logger, pong_vnfd_package_file, launchpad_host)
        wait_onboard_transaction_finished(logger, trans_id, host=launchpad_host)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 2, "There should be two vnfds"
        assert "rw_pong_vnfd" in [vnfds[0].name, vnfds[1].name]

    def test_onboard_ping_pong_nsd(self, logger, mc_proxy, mgmt_domain_name, nsd_proxy, ping_pong_nsd_package_file):
        launchpad_host = mc_proxy.get("/mgmt-domain/domain[name='%s']/launchpad/ip_address" % mgmt_domain_name)
        logger.info("Onboarding ping_pong_nsd package: %s", ping_pong_nsd_package_file)
        trans_id = upload_descriptor(logger, ping_pong_nsd_package_file, launchpad_host)
        wait_onboard_transaction_finished(logger, trans_id, host=launchpad_host)

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsds = catalog.nsd
        assert len(nsds) == 1, "There should only be a single nsd"
        nsd = nsds[0]
        assert nsd.name == "rw_ping_pong_nsd"

    def test_instantiate_ping_pong_nsr(self, logger, nsd_proxy, nsr_proxy, rwnsr_proxy, base_proxy):
        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsd = catalog.nsd[0]

        nsr = create_nsr_from_nsd_id(nsd.id)
        rwnsr_proxy.merge_config('/ns-instance-config', nsr)

        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr
        assert len(nsrs) == 1
        assert nsrs[0].ns_instance_config_ref == nsr.id

        logger.info("Waiting up to 120 seconds for ping and pong components to show "
                 "up in show vcs info")

        start_time = time.time()
        while (time.time() - start_time) < 120:
            vcs_info = base_proxy.get('/vcs/info')
            components = vcs_info.components.component_info

            def find_component_by_name(name):
                for component in components:
                    if name in component.component_name:
                        return component

                logger.warning("Did not find %s component name in show vcs info",
                            name)

                return None

            ping_vm_component = find_component_by_name(
                    "rw_ping_vnfd:rwping_vm"
                    )
            if ping_vm_component is None:
                continue

            pong_vm_component = find_component_by_name(
                    "rw_pong_vnfd:rwpong_vm"
                    )
            if pong_vm_component is None:
                continue

            ping_proc_component = find_component_by_name(
                    "rw_ping_vnfd:rwping_proc"
                    )
            if ping_proc_component is None:
                continue

            pong_proc_component = find_component_by_name(
                    "rw_pong_vnfd:rwpong_proc"
                    )
            if pong_proc_component is None:
                continue

            ping_tasklet_component = find_component_by_name(
                    "rw_ping_vnfd:rwping_tasklet"
                    )
            if ping_tasklet_component is None:
                continue

            pong_tasklet_component = find_component_by_name(
                    "rw_pong_vnfd:rwpong_tasklet"
                    )
            if pong_tasklet_component is None:
                continue

            logger.info("TEST SUCCESSFUL: All ping and pong components were found in show vcs info")
            break

        else:
            assert False, "Did not find all ping and pong component in time"
