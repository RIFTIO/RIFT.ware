#!/usr/bin/env python
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_multi_vm_vnf_trafgen.py
@author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
@date 03/16/2016
@brief Scriptable load-balancer test with multi-vm VNFs
"""

import json
import logging
import os
import pytest
import shlex
import shutil
import subprocess
import time
import uuid

from gi.repository import (
    RwMcYang,
    NsdYang,
    NsrYang,
    RwNsrYang,
    VnfrYang,
    VldYang,
    RwVnfdYang,
    RwLaunchpadYang,
    RwBaseYang
)

import rift.auto.mano

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

@pytest.fixture(scope='module')
def multi_vm_vnf_nsd_package_file(request, package_gen_script, mvv_descr_dir, package_dir):
    pkg_cmd = "{pkg_scr} --descriptor-type='nsd' --format='xml' --infile='{infile}' --outdir='{outdir}'".format(
            pkg_scr=package_gen_script,
            infile=os.path.join(mvv_descr_dir, 'nsd/xml/multivm_tg_ts_config_nsd.xml'),
            outdir=package_dir)
    pkg_file = os.path.join(package_dir, 'multivm_tg_ts_config_nsd.tar.gz')
    logger.debug("Generating NSD package: %s", pkg_file)
    command = shlex.split(pkg_cmd)
    print("Running the command arguments: %s" % command)
    command = [package_gen_script,
               "--descriptor-type", "nsd",
               "--format", "xml",
               "--infile", "%s" % os.path.join(mvv_descr_dir, 'vnfd/xml/multivm_tg_ts_config_nsd.xml'),
               "--outdir", "%s" % package_dir]
    print("Running new command arguments: %s" % command)
    subprocess.check_call(shlex.split(pkg_cmd))
    return pkg_file

def create_nsr(nsd_id, input_param_list, cloud_account_name):
    """
    Create the NSR record object

    Arguments:
         nsd_id             -  NSD id
         input_param_list - list of input-parameter objects

    Return:
         NSR object
    """
    nsr = RwNsrYang.YangData_Nsr_NsInstanceConfig_Nsr()

    nsr.id = str(uuid.uuid4())
    nsr.name = rift.auto.mano.resource_name(nsr.id)
    nsr.short_name = "nsr_short_name"
    nsr.description = "This is a description"
    nsr.nsd_ref = nsd_id
    nsr.admin_status = "ENABLED"
    nsr.input_parameter.extend(input_param_list)
    nsr.cloud_account = cloud_account_name

    return nsr


def upload_descriptor(logger, descriptor_file, host="127.0.0.1"):
    curl_cmd = 'curl --insecure -F "descriptor=@{file}" http://{host}:4567/api/upload '.format(
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


def wait_onboard_transaction_finished(logger, transaction_id, timeout=10, host="127.0.0.1"):
    logger.info("Waiting for onboard trans_id %s to complete", transaction_id)
    def check_status_onboard_status():
        uri = 'http://%s:4567/api/upload/%s/state' % (host, transaction_id)
        curl_cmd = 'curl --insecure {uri}'.format(
                uri=uri
                )
        return subprocess.check_output(shlex.split(curl_cmd), universal_newlines=True)

    elapsed = 0
    start = time.time()
    while elapsed < timeout:
        reply = check_status_onboard_status()
        state = json.loads(reply)
        if state["status"] == "success":
            break

        if state["status"] != "pending":
            raise DescriptorOnboardError(state)

        time.sleep(1)
        elapsed = time.time() - start

    if state["status"] != "success":
        raise DescriptorOnboardError(state)

    logger.info("Descriptor onboard was successful")


@pytest.mark.setup('multivmvnf')
@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestMultiVmVnfTrafgenApp(object):
    pkg_dir = None
    @classmethod
    def teardown_class(cls):
        """ remove the temporary directory contains the descriptor packages
        """
        logger.debug("Removing the temporary package directory: %s", cls.pkg_dir)
        if not cls.pkg_dir is None:
            shutil.rmtree(cls.pkg_dir)

    def test_onboard_trafgen_vnfd(self, logger, launchpad_host, vnfd_proxy, trafgen_vnfd_package_file):
        TestMultiVmVnfTrafgenApp.pkg_dir = os.path.dirname(trafgen_vnfd_package_file)
        logger.info("Onboarding trafgen vnfd package: %s", trafgen_vnfd_package_file)
        trans_id = upload_descriptor(logger, trafgen_vnfd_package_file, launchpad_host)
        wait_onboard_transaction_finished(logger, trans_id, host=launchpad_host)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 1, "There should only be a single vnfd"
        vnfd = vnfds[0]
        assert vnfd.name == "multivm_trafgen_vnfd"

    def test_onboard_trafsink_vnfd(self, logger, launchpad_host, vnfd_proxy, trafsink_vnfd_package_file):
        TestMultiVmVnfTrafgenApp.pkg_dir = os.path.dirname(trafsink_vnfd_package_file)
        logger.info("Onboarding trafsink vnfd package: %s", trafsink_vnfd_package_file)
        trans_id = upload_descriptor(logger, trafsink_vnfd_package_file, launchpad_host)
        wait_onboard_transaction_finished(logger, trans_id, host=launchpad_host)

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        vnfds = catalog.vnfd
        assert len(vnfds) == 2, "There should be two vnfds"
        assert "multivm_trafsink_vnfd" in [vnfds[0].name, vnfds[1].name]

    def test_onboard_multi_vm_vnf_nsd(self, logger, launchpad_host, nsd_proxy, multi_vm_vnf_nsd_package_file):
        logger.info("Onboarding tg_ts nsd package: %s", multi_vm_vnf_nsd_package_file)
        trans_id = upload_descriptor(logger, multi_vm_vnf_nsd_package_file, launchpad_host)
        wait_onboard_transaction_finished(logger, trans_id, host=launchpad_host)

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsds = catalog.nsd
        assert len(nsds) == 1, "There should only be a single nsd"
        nsd = nsds[0]
        assert nsd.name == "multivm_tg_ts_config_nsd"

    def test_instantiate_multi_vm_vnf_nsr(self, logger, nsd_proxy, nsr_proxy, rwnsr_proxy, base_proxy, cloud_account_name):

        def verify_input_parameters (running_config, config_param):
            """
            Verify the configured parameter set against the running configuration
            """
            for run_input_param in running_config.input_parameter:
                if (input_param.xpath == config_param.xpath and
                    input_param.value == config_param.value):
                    return True

            assert False, ("Verification of configured input parameters: { xpath:%s, value:%s} "
                          "is unsuccessful.\nRunning configuration: %s" % (config_param.xpath,
                                                                           config_param.value,
                                                                           running_nsr_config.input_parameter))

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsd = catalog.nsd[0]

        input_parameters = []
        descr_xpath = "/nsd:nsd-catalog/nsd:nsd[nsd:id='%s']/nsd:description" % nsd.id
        descr_value = "New NSD Description"
        in_param_id = str(uuid.uuid4())

        input_param_1= NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                                                                xpath=descr_xpath,
                                                                value=descr_value)

        input_parameters.append(input_param_1)

        nsr = create_nsr(nsd.id, input_parameters, cloud_account_name)

        logger.info("Instantiating the Network Service")
        rwnsr_proxy.create_config('/ns-instance-config/nsr', nsr)

        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        # Verify the input parameter configuration
        running_config = rwnsr_proxy.get_config("/ns-instance-config/nsr[id='%s']" % nsr.id)
        for input_param in input_parameters:
            verify_input_parameters(running_config, input_param)

        assert len(nsrs) == 1
        assert nsrs[0].ns_instance_config_ref == nsr.id

        xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/operational-status".format(nsr.id)
        rwnsr_proxy.wait_for(xpath, "running", fail_on=['failed'], timeout=360)


@pytest.mark.teardown('multivmvnf')
@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestMultiVmVnfTrafgenAppTeardown(object):
    def test_terminate_nsr(self, nsr_proxy, vnfr_proxy, rwnsr_proxy, logger):
        """
        Terminate the instance and check if the record is deleted.

        Asserts:
        1. NSR record is deleted from instance-config.

        """
        logger.debug("Terminating Multi VM VNF's NSR")

        nsr_path = "/ns-instance-config"
        nsr = rwnsr_proxy.get_config(nsr_path)

        ping_pong = nsr.nsr[0]
        rwnsr_proxy.delete_config("/ns-instance-config/nsr[id='{}']".format(ping_pong.id))
        time.sleep(30)


    def test_delete_records(self, nsd_proxy, vnfd_proxy):
        """Delete the NSD & VNFD records

        Asserts:
            The records are deleted.
        """
        nsds = nsd_proxy.get("/nsd-catalog/nsd", list_obj=True)
        for nsd in nsds.nsd:
            xpath = "/nsd-catalog/nsd[id='{}']".format(nsd.id)
            nsd_proxy.delete_config(xpath)

        vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
        for vnfd_record in vnfds.vnfd:
            xpath = "/vnfd-catalog/vnfd[id='{}']".format(vnfd_record.id)
            vnfd_proxy.delete_config(xpath)

        time.sleep(5)
        nsds = nsd_proxy.get("/nsd-catalog/nsd", list_obj=True)
        assert nsds is None or len(nsds.nsd) == 0

        vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
        assert vnfds is None or len(vnfds.vnfd) == 0
