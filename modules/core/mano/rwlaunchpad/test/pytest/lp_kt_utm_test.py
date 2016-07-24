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
import gi

gi.require_version('RwlogMgmtYang', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwCloudYang', '1.0')
gi.require_version('RwNsmYang', '1.0')
gi.require_version('RwIwpYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
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


RW_KT_UTM_PKG_INSTALL_DIR = os.path.join(
    os.environ["RIFT_INSTALL"],
    "usr/rift/mano/vnfds/kt_utm"
    )

RW_KT_UTM_NSD_PKG_INSTALL_DIR = os.path.join(
    os.environ["RIFT_INSTALL"],
    "usr/rift/mano/nsds/utm_only"
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
def kt_utm_vnfd_package_file():
    ktutm_pkg_file = os.path.join(
            RW_KT_UTM_PKG_INSTALL_DIR,
            "kt_utm_vnfd.tar.gz",
            )
    if not os.path.exists(ktutm_pkg_file):
        raise_package_error()

    return ktutm_pkg_file

@pytest.fixture(scope='session')
def utm_only_nsd_package_file():
      ktutm_nsd_pkg_file = os.path.join(
              RW_KT_UTM_NSD_PKG_INSTALL_DIR,
              "utm_only_nsd.tar.gz",
              )
      if not os.path.exists(ktutm_nsd_pkg_file):
          raise_package_error()
  
      return ktutm_nsd_pkg_file

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

def create_nsr_from_nsd_id(nsd_id):
      nsr = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
      nsr.id = str(uuid.uuid4())
      nsr.name = "UTM-only"
      nsr.short_name = "UTM-only"
      nsr.description = "1 VNFs with 5 VLs"
      nsr.nsd_ref = nsd_id
      nsr.admin_status = "ENABLED"
  
      return nsr

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
        cloud_account = RwCloudYang.CloudAccountConfig()
        # cloud_account.name = "cloudsim_proxy"
        # cloud_account.account_type = "cloudsim_proxy"
        cloud_account.name = "openstack"
        cloud_account.account_type = "openstack"
        cloud_account.openstack.key = 'pluto'
        cloud_account.openstack.secret = 'mypasswd'
        cloud_account.openstack.auth_url = 'http://10.66.4.13:5000/v3/'
        cloud_account.openstack.tenant = 'demo'
        cloud_account.openstack.mgmt_network = 'private'

        cloud_proxy.merge_config("/rw-cloud:cloud-account", cloud_account)

    def test_configure_pools(self, resource_mgr_proxy):
        pools = RwResourceMgrYang.ResourcePools.from_dict({
            "pools": [{ "name": "vm_pool_a",
                        "resource_type": "compute",
                        "pool_type" : "dynamic"},
                      {"name": "network_pool_a",
                       "resource_type": "network",
                       "pool_type" : "dynamic",}]})

        resource_mgr_proxy.merge_config('/rw-resource-mgr:resource-mgr-config/rw-resource-mgr:resource-pools', pools)

    def test_configure_resource_orchestrator(self, so_proxy):
        cfg = RwConmanYang.RoEndpoint.from_dict({'ro_ip_address': '127.0.0.1',
                                                'ro_port'      :  2022,
                                                'ro_username'  : 'admin',
                                                'ro_password'  : 'admin'})
        so_proxy.merge_config('/rw-conman:cm-config', cfg)

    def test_configure_service_orchestrator(self, nsm_proxy):
        cfg = RwNsmYang.SoEndpoint.from_dict({'cm_ip_address': '127.0.0.1',
                                              'cm_port'      :  2022,
                                              'cm_username'  : 'admin',
                                              'cm_password'  : 'admin'})
        nsm_proxy.merge_config('/rw-nsm:ro-config/rw-nsm:cm-endpoint', cfg)

    
    def test_onboard_ktutm_vnfd(self, logger, vnfd_proxy, kt_utm_vnfd_package_file):
        logger.info("Onboarding kt_utm_vnfd package: %s", kt_utm_vnfd_package_file)
        trans_id = upload_descriptor(logger, kt_utm_vnfd_package_file)
        wait_unboard_transaction_finished(logger, trans_id)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 1, "There should only be a single vnfd"
        vnfd = vnfds[0]
        assert vnfd.name == "kt_utm_vnfd"

    def test_onboard_utm_only_nsd(self, logger, nsd_proxy, utm_only_nsd_package_file):
          logger.info("Onboarding utm_onlynsd package: %s", utm_only_nsd_package_file)
          trans_id = upload_descriptor(logger, utm_only_nsd_package_file)
          wait_unboard_transaction_finished(logger, trans_id)
  
          catalog = nsd_proxy.get_config('/nsd-catalog')
          nsds = catalog.nsd
          assert len(nsds) == 1, "There should only be a single nsd"
          nsd = nsds[0]
  
    def test_instantiate_utm_only_nsr(self, logger, nsd_proxy, nsr_proxy, rwnsr_proxy, base_proxy):
          catalog = nsd_proxy.get_config('/nsd-catalog')
          nsd = catalog.nsd[0]
  
          nsr = create_nsr_from_nsd_id(nsd.id)
          nsr_proxy.merge_config('/ns-instance-config', nsr)
  
          nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
          nsrs = nsr_opdata.nsr
          assert len(nsrs) == 1
          assert nsrs[0].ns_instance_config_ref == nsr.id
