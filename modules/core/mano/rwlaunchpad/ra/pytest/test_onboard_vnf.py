#!/usr/bin/env python
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_onboard_vnf.py
@author Varun Prasad (varun.prasad@riftio.com)
@brief Onboard descriptors
"""

import json
import logging
import os
import pytest
import shlex
import requests
import shutil
import subprocess
import time
import uuid

import rift.auto.mano
import rift.auto.session

import gi
gi.require_version('RwMcYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwBaseYang', '1.0')

from gi.repository import (
    RwcalYang,
    RwMcYang,
    NsdYang,
    RwNsrYang,
    RwVnfrYang,
    NsrYang,
    VnfrYang,
    VldYang,
    RwVnfdYang,
    RwLaunchpadYang,
    RwBaseYang
)

logging.basicConfig(level=logging.DEBUG)


@pytest.fixture(scope='module')
def launchpad_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwLaunchpadYang)

@pytest.fixture(scope='module')
def vnfd_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwVnfdYang)

@pytest.fixture(scope='module')
def rwvnfr_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwVnfrYang)

@pytest.fixture(scope='module')
def vld_proxy(request, launchpad_session):
    return launchpad_session.proxy(VldYang)


@pytest.fixture(scope='module')
def nsd_proxy(request, launchpad_session):
    return launchpad_session.proxy(NsdYang)


@pytest.fixture(scope='module')
def rwnsr_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwNsrYang)

@pytest.fixture(scope='module')
def base_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwBaseYang)


@pytest.fixture(scope="module")
def endpoint():
    return "upload"

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


def upload_descriptor(
        logger,
        descriptor_file,
        scheme,
        cert,
        host="127.0.0.1",
        endpoint="upload"):
    curl_cmd = ('curl --cert {cert} --key {key} -F "descriptor=@{file}" -k '
                '{scheme}://{host}:4567/api/{endpoint}'.format(
            cert=cert[0],
            key=cert[1],
            scheme=scheme,
            endpoint=endpoint,
            file=descriptor_file,
            host=host,
            ))

    logger.debug("Uploading descriptor %s using cmd: %s", descriptor_file, curl_cmd)
    stdout = subprocess.check_output(shlex.split(curl_cmd), universal_newlines=True)

    json_out = json.loads(stdout)
    transaction_id = json_out["transaction_id"]

    return transaction_id


class DescriptorOnboardError(Exception):
    pass


def wait_onboard_transaction_finished(
        logger,
        transaction_id,
        scheme,
        cert,
        timeout=600,
        host="127.0.0.1",
        endpoint="upload"):

    logger.info("Waiting for onboard trans_id %s to complete", transaction_id)
    uri = '%s://%s:4567/api/%s/%s/state' % (scheme, host, endpoint, transaction_id)

    elapsed = 0
    start = time.time()
    while elapsed < timeout:
        reply = requests.get(uri, cert=cert, verify=False)
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


def onboard_descriptor(host, file_name, logger, endpoint, scheme, cert):
    """On-board/update the descriptor.

    Args:
        host (str): Launchpad IP
        file_name (str): Full file path.
        logger: Logger instance
        endpoint (str): endpoint to be used for the upload operation.

    """
    logger.info("Onboarding package: %s", file_name)
    trans_id = upload_descriptor(
            logger,
            file_name,
            scheme,
            cert,
            host=host,
            endpoint=endpoint)
    wait_onboard_transaction_finished(
        logger,
        trans_id,
        scheme,
        cert,
        host=host,
        endpoint=endpoint)

def terminate_nsr(rwvnfr_proxy, rwnsr_proxy, logger):
    """
    Terminate the instance and check if the record is deleted.

    Asserts:
    1. NSR record is deleted from instance-config.

    """
    logger.debug("Terminating NSRs")

    nsr_path = "/ns-instance-config"
    nsr = rwnsr_proxy.get_config(nsr_path)
    nsrs = nsr.nsr

    xpaths = []
    for nsr in nsrs:
        xpath = "/ns-instance-config/nsr[id='{}']".format(nsr.id)
        rwnsr_proxy.delete_config(xpath)
        xpaths.append(xpath)

    time.sleep(30)
    for xpath in xpaths:
        nsr = rwnsr_proxy.get_config(xpath)
        assert nsr is None

    # Get the ns-instance-config
    ns_instance_config = rwnsr_proxy.get_config("/ns-instance-config")

    # Termination tests
    vnfr = "/vnfr-catalog/vnfr"
    vnfrs = rwvnfr_proxy.get(vnfr, list_obj=True)
    assert vnfrs is None or len(vnfrs.vnfr) == 0

    # nsr = "/ns-instance-opdata/nsr"
    # nsrs = rwnsr_proxy.get(nsr, list_obj=True)
    # assert len(nsrs.nsr) == 0



@pytest.mark.setup('nsr')
@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestNsrStart(object):
    """A brief overview of the steps performed.
    1. Generate & on-board new descriptors
    2. Start the NSR 
    """

    def test_upload_descriptors(
            self,
            logger,
            vnfd_proxy,
            nsd_proxy,
            launchpad_session,
            scheme,
            cert,
            descriptors
        ):
        """Generates & On-boards the descriptors.
        """
        endpoint = "upload"

        for file_name in descriptors:
            onboard_descriptor(
                    launchpad_session.host,
                    file_name,
                    logger,
                    endpoint,
                    scheme,
                    cert)

        descriptor_vnfds, descriptor_nsd = descriptors[:-1], descriptors[-1]

        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        actual_vnfds = catalog.vnfd
        assert len(actual_vnfds) == len(descriptor_vnfds), \
                "There should {} vnfds".format(len(descriptor_vnfds))

        catalog = nsd_proxy.get_config('/nsd-catalog')
        actual_nsds = catalog.nsd
        assert len(actual_nsds) == 1, "There should only be a single nsd"

    @pytest.mark.feature("upload-image")
    def test_upload_images(self, descriptor_images, cloud_host, cloud_user, cloud_tenants):

        openstack = rift.auto.mano.OpenstackManoSetup(
                cloud_host,
                cloud_user,
                [(tenant, "private") for tenant in cloud_tenants])

        for image_location in descriptor_images:
            image = RwcalYang.ImageInfoItem.from_dict({
                    'name': os.path.basename(image_location),
                    'location': image_location,
                    'disk_format': 'qcow2',
                    'container_format': 'bare'})
            openstack.create_image(image)


    def test_set_scaling_params(self, nsd_proxy):
        nsds = nsd_proxy.get('/nsd-catalog')
        nsd = nsds.nsd[0]
        for scaling_group in nsd.scaling_group_descriptor:
            scaling_group.max_instance_count = 2

        nsd_proxy.replace_config('/nsd-catalog/nsd[id="{}"]'.format(
            nsd.id), nsd)


    def test_instantiate_nsr(self, logger, nsd_proxy, rwnsr_proxy, base_proxy, cloud_account_name):

        def verify_input_parameters(running_config, config_param):
            """
            Verify the configured parameter set against the running configuration
            """
            for run_input_param in running_config.input_parameter:
                if (run_input_param.xpath == config_param.xpath and
                    run_input_param.value == config_param.value):
                    return True

            assert False, ("Verification of configured input parameters: { xpath:%s, value:%s} "
                          "is unsuccessful.\nRunning configuration: %s" % (config_param.xpath,
                                                                           config_param.value,
                                                                           running_config.input_parameter))

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsd = catalog.nsd[0]

        input_parameters = []
        descr_xpath = "/nsd:nsd-catalog/nsd:nsd[nsd:id='%s']/nsd:description" % nsd.id
        descr_value = "New NSD Description"
        in_param_id = str(uuid.uuid4())

        input_param_1 = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                                                                xpath=descr_xpath,
                                                                value=descr_value)

        input_parameters.append(input_param_1)

        nsr = create_nsr(nsd.id, input_parameters, cloud_account_name)

        logger.info("Instantiating the Network Service")
        rwnsr_proxy.create_config('/ns-instance-config/nsr', nsr)

        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata/nsr[ns-instance-config-ref="{}"]'.format(nsr.id))
        assert nsr_opdata is not None

        # Verify the input parameter configuration
        running_config = rwnsr_proxy.get_config("/ns-instance-config/nsr[id='%s']" % nsr.id)
        for input_param in input_parameters:
            verify_input_parameters(running_config, input_param)

    def test_wait_for_nsr_started(self, rwnsr_proxy):
        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        for nsr in nsrs:
            xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/operational-status".format(nsr.ns_instance_config_ref)
            rwnsr_proxy.wait_for(xpath, "running", fail_on=['failed'], timeout=240)

    def test_wait_for_nsr_configured(self, rwnsr_proxy):
        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        for nsr in nsrs:
            xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/config-status".format(
                    nsr.ns_instance_config_ref)
            rwnsr_proxy.wait_for(xpath, "configured", fail_on=['failed'], timeout=300)


@pytest.mark.teardown('nsr')
@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestNsrTeardown(object):
    def test_terminate_nsr(self, rwvnfr_proxy, rwnsr_proxy, logger):
        """
        Terminate the instance and check if the record is deleted.

        Asserts:
        1. NSR record is deleted from instance-config.

        """
        logger.debug("Terminating NSR")
        terminate_nsr(rwvnfr_proxy, rwnsr_proxy, logger)

    def test_delete_records(self, nsd_proxy, vnfd_proxy):
        """Delete the NSD & VNFD records

        Asserts:
            The records are deleted.
        """
        nsds = nsd_proxy.get("/nsd-catalog/nsd", list_obj=True)
        for nsd in nsds.nsd:
            xpath = "/nsd-catalog/nsd[id='{}']".format(nsd.id)
            nsd_proxy.delete_config(xpath)

        nsds = nsd_proxy.get("/nsd-catalog/nsd", list_obj=True)
        assert nsds is None or len(nsds.nsd) == 0

        vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
        for vnfd_record in vnfds.vnfd:
            xpath = "/vnfd-catalog/vnfd[id='{}']".format(vnfd_record.id)
            vnfd_proxy.delete_config(xpath)

        vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
        assert vnfds is None or len(vnfds.vnfd) == 0
