#!/usr/bin/env python
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file lp_test.py
@author Austin Cormier (Austin.Cormier@riftio.com)
@date 10/15/2015
@brief Launchpad Module Test
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
import datetime

import gi
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwCloudYang', '1.0')
gi.require_version('RwIwpYang', '1.0')
gi.require_version('RwlogMgmtYang', '1.0')
gi.require_version('RwNsmYang', '1.0')
gi.require_version('RwNsmYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwConmanYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')

from gi.repository import (
        NsdYang,
        NsrYang,
        RwBaseYang,
        RwCloudYang,
        RwIwpYang,
        RwlogMgmtYang,
        RwNsmYang,
        RwNsrYang,
        RwResourceMgrYang,
        RwConmanYang,
        RwVnfdYang,
        VldYang,
        )

logging.basicConfig(level=logging.DEBUG)


RW_PING_PONG_PKG_INSTALL_DIR = os.path.join(
    os.environ["RIFT_ROOT"],
    "images"
    )

class PackageError(Exception):
    pass


def raise_package_error():
    raise PackageError("Could not find ns packages")


@pytest.fixture(scope='module')
def iwp_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwIwpYang)


@pytest.fixture(scope='module')
def rwlog_mgmt_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwlogMgmtYang)


@pytest.fixture(scope='module')
def resource_mgr_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwResourceMgrYang)


@pytest.fixture(scope='module')
def cloud_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwCloudYang)


@pytest.fixture(scope='module')
def vnfd_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwVnfdYang)


@pytest.fixture(scope='module')
def vld_proxy(request, mgmt_session):
    return mgmt_session.proxy(VldYang)


@pytest.fixture(scope='module')
def nsd_proxy(request, mgmt_session):
    return mgmt_session.proxy(NsdYang)


@pytest.fixture(scope='module')
def nsr_proxy(request, mgmt_session):
    return mgmt_session.proxy(NsrYang)


@pytest.fixture(scope='module')
def rwnsr_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwNsrYang)


@pytest.fixture(scope='module')
def base_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwBaseYang)


@pytest.fixture(scope='module')
def so_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwConmanYang)


@pytest.fixture(scope='module')
def nsm_proxy(request, mgmt_session):
    return mgmt_session.proxy(RwNsmYang)


@pytest.fixture(scope='session')
def ping_vnfd_package_file():
    ping_pkg_file = os.path.join(
            RW_PING_PONG_PKG_INSTALL_DIR,
            "ping_vnfd_with_image.tar.gz",
            )
    if not os.path.exists(ping_pkg_file):
        raise_package_error()

    return ping_pkg_file


@pytest.fixture(scope='session')
def pong_vnfd_package_file():
    pong_pkg_file = os.path.join(
            RW_PING_PONG_PKG_INSTALL_DIR,
            "pong_vnfd_with_image.tar.gz",
            )
    if not os.path.exists(pong_pkg_file):
        raise_package_error()

    return pong_pkg_file


@pytest.fixture(scope='session')
def ping_pong_nsd_package_file():
    ping_pong_pkg_file = os.path.join(
            RW_PING_PONG_PKG_INSTALL_DIR,
            "ping_pong_nsd.tar.gz",
            )
    if not os.path.exists(ping_pong_pkg_file):
        raise_package_error()

    return ping_pong_pkg_file


def create_nsr_from_nsd_id(nsd_id):
    nsr = RwNsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
    nsr.id = str(uuid.uuid4())
    nsr.name = "pingpong_{}".format(datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
    nsr.short_name = "nsr_short_name"
    nsr.description = "This is a description"
    nsr.nsd_ref = nsd_id
    nsr.admin_status = "ENABLED"
    nsr.cloud_account = "openstack"

    param = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter()
    param.xpath = '/nsd:nsd-catalog/nsd:nsd/nsd:vendor'
    param.value = "rift-o-matic"

    nsr.input_parameter.append(param)

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


def wait_unboard_transaction_finished(logger, transaction_id, timeout_secs=600, host="127.0.0.1"):
    logger.info("Waiting for onboard trans_id %s to complete",
                transaction_id)
    start_time = time.time()
    while (time.time() - start_time) < timeout_secs:
        r = requests.get(
                'http://{host}:4567/api/upload/{t_id}/state'.format(
                    host=host, t_id=transaction_id
                    )
                )
        state = r.json()
        if state["status"] == "pending":
            time.sleep(1)
            continue

        elif state["status"] == "success":
            logger.info("Descriptor onboard was successful")
            return

        else:
            raise DescriptorOnboardError(state)

    if state["status"] != "success":
        raise DescriptorOnboardError(state)


@pytest.mark.incremental
class TestLaunchpadStartStop(object):
    def test_configure_logging(self, rwlog_mgmt_proxy):
        logging = RwlogMgmtYang.Logging.from_dict({
                "console": {
                    "on": True,
                    "filter": {
                        "category": [{
                            "name": "rw-generic",
                            "severity": "error"
                            }],
                        }
                    }
                })
        rwlog_mgmt_proxy.merge_config("/rwlog-mgmt:logging", logging)

    def test_configure_cloud_account(self, cloud_proxy, logger):
        cloud_account = RwCloudYang.CloudAccount()
        # cloud_account.name = "cloudsim_proxy"
        # cloud_account.account_type = "cloudsim_proxy"
        cloud_account.name = "openstack"
        cloud_account.account_type = "openstack"
        cloud_account.openstack.key = 'pluto'
        cloud_account.openstack.secret = 'mypasswd'
        cloud_account.openstack.auth_url = 'http://10.96.4.2:5000/v3/'
        cloud_account.openstack.tenant = 'mano1'
        cloud_account.openstack.mgmt_network = 'private1'

        cloud_proxy.merge_config("/rw-cloud:cloud/account", cloud_account)

    def test_onboard_ping_vnfd(self, logger, vnfd_proxy, ping_vnfd_package_file):
        logger.info("Onboarding ping_vnfd package: %s", ping_vnfd_package_file)
        trans_id = upload_descriptor(logger, ping_vnfd_package_file)
        wait_unboard_transaction_finished(logger, trans_id)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 1, "There should only be a single vnfd"
        vnfd = vnfds[0]
        assert vnfd.name == "ping_vnfd"

    def test_onboard_pong_vnfd(self, logger, vnfd_proxy, pong_vnfd_package_file):
        logger.info("Onboarding pong_vnfd package: %s", pong_vnfd_package_file)
        trans_id = upload_descriptor(logger, pong_vnfd_package_file)
        wait_unboard_transaction_finished(logger, trans_id)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 2, "There should be two vnfds"
        assert "pong_vnfd" in [vnfds[0].name, vnfds[1].name]

    def test_onboard_ping_pong_nsd(self, logger, nsd_proxy, ping_pong_nsd_package_file):
        logger.info("Onboarding ping_pong_nsd package: %s", ping_pong_nsd_package_file)
        trans_id = upload_descriptor(logger, ping_pong_nsd_package_file)
        wait_unboard_transaction_finished(logger, trans_id)

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsds = catalog.nsd
        assert len(nsds) == 1, "There should only be a single nsd"
        nsd = nsds[0]
        assert nsd.name == "ping_pong_nsd"

    def test_instantiate_ping_pong_nsr(self, logger, nsd_proxy, nsr_proxy, rwnsr_proxy, base_proxy):
        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsd = catalog.nsd[0]

        nsr = create_nsr_from_nsd_id(nsd.id)
        rwnsr_proxy.merge_config('/ns-instance-config', nsr)

        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr
        assert len(nsrs) == 1
        assert nsrs[0].ns_instance_config_ref == nsr.id

        # logger.info("Waiting up to 30 seconds for ping and pong components to show "
        #          "up in show tasklet info")

        # start_time = time.time()
        # while (time.time() - start_time) < 30:
        #     vcs_info = base_proxy.get('/vcs/info')
        #     components = vcs_info.components.component_info

        #     def find_component_by_name(name):
        #         for component in components:
        #             if name in component.component_name:
        #                 return component

        #         logger.warning("Did not find %s component name in show tasklet info",
        #                     name)

        #         return None

        #     """
        #     ping_cluster_component = find_component_by_name(
        #             "rw_ping_vnfd:rwping_cluster"
        #             )
        #     if ping_cluster_component is None:
        #         continue

        #     pong_cluster_component = find_component_by_name(
        #             "rw_pong_vnfd:rwpong_cluster"
        #             )
        #     if pong_cluster_component is None:
        #         continue
        #     """

        #     ping_vm_component = find_component_by_name(
        #             "rw_ping_vnfd:rwping_vm"
        #             )
        #     if ping_vm_component is None:
        #         continue

        #     pong_vm_component = find_component_by_name(
        #             "rw_pong_vnfd:rwpong_vm"
        #             )
        #     if pong_vm_component is None:
        #         continue

        #     ping_proc_component = find_component_by_name(
        #             "rw_ping_vnfd:rwping_proc"
        #             )
        #     if ping_proc_component is None:
        #         continue

        #     pong_proc_component = find_component_by_name(
        #             "rw_pong_vnfd:rwpong_proc"
        #             )
        #     if pong_proc_component is None:
        #         continue

        #     ping_tasklet_component = find_component_by_name(
        #             "rw_ping_vnfd:rwping_tasklet"
        #             )
        #     if ping_tasklet_component is None:
        #         continue

        #     pong_tasklet_component = find_component_by_name(
        #             "rw_pong_vnfd:rwpong_tasklet"
        #             )
        #     if pong_tasklet_component is None:
        #         continue

        #     logger.info("TEST SUCCESSFUL: All ping and pong components were found in show tasklet info")
        #     break

        # else:
        #     assert False, "Did not find all ping and pong component in time"

    #def test_terminate_ping_pong_ns(self, logger, nsd_proxy, nsr_proxy, rwnsr_proxy, base_proxy):
    #    nsr_configs = nsr_proxy.get_config('/ns-instance-config')
    #    nsr = nsr_configs.nsr[0]
    #    nsr_id = nsr.id

    #    nsr_configs = nsr_proxy.delete_config("/ns-instance-config/nsr[id='{}']".format(nsr_id))
