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
import shutil
import subprocess
import tempfile
import time
import uuid

import rift.auto.mano
import rift.auto.session
import rift.mano.examples.ping_pong_nsd as ping_pong

import gi
gi.require_version('RwMcYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwBaseYang', '1.0')

from gi.repository import (
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

class DescriptorOnboardError(Exception):
    pass

def create_nsr(nsd_id, input_param_list, cloud_account_name):
    """
    Create the NSR record object

    Arguments:
        nsd_id              - NSD id
        input_param_list    - list of input-parameter objects
        cloud_account_name  - name of cloud account

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
    curl_cmd = 'curl --insecure -F "descriptor=@{file}" https://{host}:4567/api/upload'.format(
            file=descriptor_file,
            host=host,
    )

    logger.debug("Uploading descriptor %s using cmd: %s", descriptor_file, curl_cmd)
    stdout = subprocess.check_output(shlex.split(curl_cmd), universal_newlines=True)

    json_out = json.loads(stdout)
    transaction_id = json_out["transaction_id"]

    return transaction_id

def wait_onboard_transaction_finished(logger, transaction_id, timeout=30, host="127.0.0.1"):

    def check_status_onboard_status():
        uri = 'https://%s:4567/api/upload/%s/state' % (host, transaction_id)
        curl_cmd = 'curl --insecure {uri}'.format(uri=uri)
        return subprocess.check_output(shlex.split(curl_cmd), universal_newlines=True)

    logger.info("Waiting for onboard transaction [%s] to complete", transaction_id)

    elapsed = 0
    start = time.time()
    while elapsed < timeout:

        reply = check_status_onboard_status()
        state = json.loads(reply)
        if state["status"] == "success":
            break

        if state["status"] == "failure":
            raise DescriptorOnboardError(state["errors"])

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
            host=host)
    wait_onboard_transaction_finished(
        logger,
        trans_id,
        host=host)


def terminate_nsrs(rwvnfr_proxy, rwnsr_proxy, logger):
    """
    Terminate the instance and check if the record is deleted.

    Asserts:
    1. NSR record is deleted from instance-config.

    """
    logger.debug("Terminating Ping Pong NSRs")

    nsr_path = "/ns-instance-config"
    nsr = rwnsr_proxy.get_config(nsr_path)
    nsrs = nsr.nsr

    xpaths = []
    for ping_pong in nsrs:
        xpath = "/ns-instance-config/nsr[id='{}']".format(ping_pong.id)
        rwnsr_proxy.delete_config(xpath)
        xpaths.append(xpath)

    time.sleep(60)
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


def generate_tar_files(tmpdir, ping_vnfd, pong_vnfd, ping_pong_nsd):
    """Converts the descriptor to files and package them into zip files
    that can be uploaded to LP instance.

    Args:
        tmpdir (string): Full path where the zipped files should be
        ping_vnfd (VirtualNetworkFunction): Ping VNFD data
        pong_vnfd (VirtualNetworkFunction): Pong VNFD data
        ping_pong_nsd (NetworkService): PingPong NSD data

    Returns:
        Tuple: file path for ping vnfd, pong vnfd and ping_pong_nsd
    """
    rift_build = os.environ['RIFT_BUILD']
    MANO_DIR = os.path.join(
            rift_build,
            "modules/core/mano/src/core_mano-build/examples/ping_pong_ns")
    ping_img = os.path.join(MANO_DIR, "ping_vnfd_with_image/images/Fedora-x86_64-20-20131211.1-sda-ping.qcow2")
    pong_img = os.path.join(MANO_DIR, "pong_vnfd_with_image/images/Fedora-x86_64-20-20131211.1-sda-pong.qcow2")

    """ grab cached copies of these files if not found. They may not exist
        because our git submodule dependency mgmt
        will not populate these because they live in .build, not .install
    """
    if not os.path.exists(ping_img):
        ping_img = os.path.join(
                    os.environ['RIFT_ROOT'],
                    'images/Fedora-x86_64-20-20131211.1-sda-ping.qcow2')
        pong_img = os.path.join(
                    os.environ['RIFT_ROOT'],
                    'images/Fedora-x86_64-20-20131211.1-sda-pong.qcow2')

    for descriptor in [ping_vnfd, pong_vnfd, ping_pong_nsd]:
        descriptor.write_to_file(output_format='xml', outdir=tmpdir.name)

    ping_img_path = os.path.join(tmpdir.name, "{}/images/".format(ping_vnfd.name))
    pong_img_path = os.path.join(tmpdir.name, "{}/images/".format(pong_vnfd.name))
    os.makedirs(ping_img_path)
    os.makedirs(pong_img_path)

    shutil.copy(ping_img, ping_img_path)
    shutil.copy(pong_img, pong_img_path)

    for dir_name in [ping_vnfd.name, pong_vnfd.name, ping_pong_nsd.name]:
        subprocess.call([
                "sh",
                "{}/bin/generate_descriptor_pkg.sh".format(os.environ['RIFT_ROOT']),
                tmpdir.name,
                dir_name])

    return (os.path.join(tmpdir.name, "{}.tar.gz".format(ping_vnfd.name)),
            os.path.join(tmpdir.name, "{}.tar.gz".format(pong_vnfd.name)),
            os.path.join(tmpdir.name, "{}.tar.gz".format(ping_pong_nsd.name)))


@pytest.mark.setup('pingpong')
@pytest.mark.depends('launchpad')
@pytest.mark.usefixtures('cloud_account')
@pytest.mark.incremental
class TestPingPongStart(object):
    """A brief overview of the steps performed.
    1. Generate & on-board new descriptors
    2. Start & stop the ping pong NSR
    3. Update the exiting descriptor files.
    4. Start the ping pong NSR.

    """


    def test_onboard_descriptors(
            self,
            logger,
            vnfd_proxy,
            nsd_proxy,
            launchpad_session,
            scheme,
            cert,
            ping_pong_records):
        """Generates & On-boards the descriptors.
        """
        temp_dirs = []
        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        endpoint = "upload"

        """
        This upload routine can get called multiples times for upload API,
        depending on the combinations of 'cloud_account' & 'endpoint'
        fixtures. Since the records are cached at module level, we might end up
        uploading the same uuids multiple times, thus causing errors. So a
        simple work-around will be to skip the records when they are uploaded
        for the second time.
        """
        def onboard_ping_pong_vnfds(ping_vnfd_file, pong_vnfd_file):
            # On-board VNFDs
            for file_name in [ping_vnfd_file, pong_vnfd_file]:
                onboard_descriptor(
                        launchpad_session.host,
                        file_name,
                        logger,
                        endpoint,
                        scheme,
                        cert)

            catalog = vnfd_proxy.get_config('/vnfd-catalog')
            vnfds = catalog.vnfd
            assert len(vnfds) == 2, "There should two vnfds"
            assert "ping_vnfd" in [vnfds[0].name, vnfds[1].name]
            assert "pong_vnfd" in [vnfds[0].name, vnfds[1].name]


        def delete_vnfds():
            vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
            for vnfd_record in vnfds.vnfd:
                xpath = "/vnfd-catalog/vnfd[id='{}']".format(vnfd_record.id)
                vnfd_proxy.delete_config(xpath)

            time.sleep(5)
            vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
            assert vnfds is None or len(vnfds.vnfd) == 0


        if catalog is not None and len(catalog.vnfd) == 2 and endpoint == "upload":
            return

        if endpoint == "update":
            for vnfd_record in [ping_vnfd, pong_vnfd]:
                vnfd_record.descriptor.vnfd[0].description += "_update"
            ping_pong_nsd.descriptor.nsd[0].description += "_update"

        tmpdir2 = tempfile.TemporaryDirectory()
        temp_dirs.append(tmpdir2)
        ping_pong.generate_ping_pong_descriptors(pingcount=1,
                                                  write_to_file=True,
                                                  out_dir=tmpdir2.name,
                                                  ping_fmt='json',
                                                  pong_fmt='xml',
                                                  )

        # On-board VNFDs without image
        ping_vnfd_file = os.path.join(tmpdir2.name, 'ping_vnfd/vnfd/ping_vnfd.json')
        pong_vnfd_file = os.path.join(tmpdir2.name, 'pong_vnfd/vnfd/pong_vnfd.xml')
        onboard_ping_pong_vnfds(ping_vnfd_file, pong_vnfd_file)

        delete_vnfds()

        tmpdir = tempfile.TemporaryDirectory()
        temp_dirs.append(tmpdir)

        ping_vnfd, pong_vnfd, ping_pong_nsd = ping_pong_records
        ping_vnfd_file, pong_vnfd_file, pingpong_nsd_file = \
            generate_tar_files(tmpdir, ping_vnfd, pong_vnfd, ping_pong_nsd)

        # On-board VNFDs with image
        onboard_ping_pong_vnfds(ping_vnfd_file, pong_vnfd_file)

        # On-board NSD
        onboard_descriptor(
                launchpad_session.host,
                pingpong_nsd_file,
                logger,
                endpoint,
                scheme,
                cert)

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsds = catalog.nsd
        assert len(nsds) == 1, "There should only be a single nsd"
        assert nsds[0].name == "ping_pong_nsd"

        # Temp directory cleanup
#         for temp_dir in temp_dirs:
#             temp_dir.cleanup()

    def test_instantiate_ping_pong_nsr(self, logger, nsd_proxy, rwnsr_proxy, base_proxy, cloud_account):

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
        descr_xpath = "/nsd:nsd-catalog/nsd:nsd[nsd:id='%s']/nsd:vendor" % nsd.id
        descr_value = "automation"
        in_param_id = str(uuid.uuid4())

        input_param_1 = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                                                                xpath=descr_xpath,
                                                                value=descr_value)

        input_parameters.append(input_param_1)

        nsr = create_nsr(nsd.id, input_parameters, cloud_account.name)

        logger.info("Instantiating the Network Service")
        rwnsr_proxy.create_config('/ns-instance-config/nsr', nsr)

        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata/nsr[ns-instance-config-ref="{}"]'.format(nsr.id))
        assert nsr_opdata is not None

        # Verify the input parameter configuration
        running_config = rwnsr_proxy.get_config("/ns-instance-config/nsr[id='%s']" % nsr.id)
        for input_param in input_parameters:
            verify_input_parameters(running_config, input_param)

    def test_wait_for_pingpong_started(self, rwnsr_proxy):
        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        for nsr in nsrs:
            xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/operational-status".format(
                    nsr.ns_instance_config_ref)
            rwnsr_proxy.wait_for(xpath, "running", fail_on=['failed'], timeout=180)

    def test_wait_for_pingpong_configured(self, rwnsr_proxy):
        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        for nsr in nsrs:
            xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/config-status".format(
                    nsr.ns_instance_config_ref)
            rwnsr_proxy.wait_for(xpath, "configured", fail_on=['failed'], timeout=450)


@pytest.mark.feature("update-api")
@pytest.mark.depends('pingpong')
@pytest.mark.usefixtures('cloud_account')
@pytest.mark.incremental
class TestUpdateNsr(object):
    def test_stop_nsr(self, rwvnfr_proxy, rwnsr_proxy, logger):
        """Terminate the currently running NSR instance before updating the descriptor files"""
        terminate_nsrs(rwvnfr_proxy, rwnsr_proxy, logger)

    def test_onboard_descriptors(
            self,
            logger,
            vnfd_proxy,
            nsd_proxy,
            launchpad_session,
            scheme,
            cert,
            ping_pong_records):
        """Generates & On-boards the descriptors.
        """
        temp_dirs = []
        catalog = vnfd_proxy.get_config('/vnfd-catalog')
        endpoint = "update"
        ping_vnfd, pong_vnfd, ping_pong_nsd = ping_pong_records                

        """
        This upload routine can get called multiples times for upload API,
        depending on the combinations of 'cloud_account' & 'endpoint'
        fixtures. Since the records are cached at module level, we might end up
        uploading the same uuids multiple times, thus causing errors. So a
        simple work-around will be to skip the records when they are uploaded
        for the second time.
        """
        def onboard_ping_pong_vnfds(ping_vnfd_file, pong_vnfd_file):
            # On-board VNFDs
            for file_name in [ping_vnfd_file, pong_vnfd_file]:
                onboard_descriptor(
                        launchpad_session.host,
                        file_name,
                        logger,
                        endpoint,
                        scheme,
                        cert)

            catalog = vnfd_proxy.get_config('/vnfd-catalog')
            vnfds = catalog.vnfd

            assert len(vnfds) == 2, "There should two vnfds"
            assert "ping_vnfd" in [vnfds[0].name, vnfds[1].name]
            assert "pong_vnfd" in [vnfds[0].name, vnfds[1].name]

        def delete_nsds():
            nsds = nsd_proxy.get("/nsd-catalog/nsd", list_obj=True)
            for nsd_record in nsds.nsd:
                xpath = "/nsd-catalog/nsd[id='{}']".format(nsd_record.id)
                nsd_proxy.delete_config(xpath)

            time.sleep(5)
            nsds = nsd_proxy.get("/nsd-catalog/nsd", list_obj=True)
            assert nsds is None or len(nsds.nsd) == 0
        delete_nsds()

        def delete_vnfds():
            vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
            for vnfd_record in vnfds.vnfd:
                xpath = "/vnfd-catalog/vnfd[id='{}']".format(vnfd_record.id)
                vnfd_proxy.delete_config(xpath)

            time.sleep(5)
            vnfds = vnfd_proxy.get("/vnfd-catalog/vnfd", list_obj=True)
            assert vnfds is None or len(vnfds.vnfd) == 0

        delete_vnfds()

        if catalog is not None and len(catalog.vnfd) == 2 and endpoint == "upload":
            return

        ping_vnfd, pong_vnfd, ping_pong_nsd = ping_pong_records

        if endpoint == "update":
            for vnfd_record in [ping_vnfd, pong_vnfd]:
                vnfd_record.descriptor.vnfd[0].description += "_update"
            ping_pong_nsd.descriptor.nsd[0].description += "_update"

        tmpdir2 = tempfile.TemporaryDirectory()
        temp_dirs.append(tmpdir2)
        ping_pong.generate_ping_pong_descriptors(pingcount=1,
                                                  write_to_file=True,
                                                  out_dir=tmpdir2.name,
                                                  ping_fmt='json',
                                                  pong_fmt='xml',
                                                  )

        # On-board VNFDs without image
        ping_vnfd_file = os.path.join(tmpdir2.name, 'ping_vnfd/vnfd/ping_vnfd.json')
        pong_vnfd_file = os.path.join(tmpdir2.name, 'pong_vnfd/vnfd/pong_vnfd.xml')
        onboard_ping_pong_vnfds(ping_vnfd_file, pong_vnfd_file)
        delete_vnfds()

        tmpdir = tempfile.TemporaryDirectory()
        temp_dirs.append(tmpdir)

        ping_vnfd_file, pong_vnfd_file, pingpong_nsd_file = \
            generate_tar_files(tmpdir, ping_vnfd, pong_vnfd, ping_pong_nsd)

        # On-board VNFDs with image
        onboard_ping_pong_vnfds(ping_vnfd_file, pong_vnfd_file)


        # On-board NSD
        onboard_descriptor(
                launchpad_session.host,
                pingpong_nsd_file,
                logger,
                endpoint,
                scheme,
                cert)

        catalog = nsd_proxy.get_config('/nsd-catalog')
        nsds = catalog.nsd
        assert len(nsds) == 1, "There should only be a single nsd"
        assert nsds[0].name == "ping_pong_nsd"

        # Temp directory cleanup
#         for temp_dir in temp_dirs:
#             temp_dir.cleanup()

    def test_instantiate_ping_pong_nsr(self, logger, nsd_proxy, rwnsr_proxy, base_proxy, cloud_account):
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
        descr_xpath = "/nsd:nsd-catalog/nsd:nsd[nsd:id='%s']/nsd:vendor" % nsd.id
        descr_value = "automation"
        in_param_id = str(uuid.uuid4())

        input_param_1 = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                                                                xpath=descr_xpath,
                                                                value=descr_value)

        input_parameters.append(input_param_1)

        nsr = create_nsr(nsd.id, input_parameters, cloud_account.name)

        logger.info("Instantiating the Network Service")
        rwnsr_proxy.create_config('/ns-instance-config/nsr', nsr)

        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata/nsr[ns-instance-config-ref="{}"]'.format(nsr.id))
        assert nsr_opdata is not None

        # Verify the input parameter configuration
        running_config = rwnsr_proxy.get_config("/ns-instance-config/nsr[id='%s']" % nsr.id)
        for input_param in input_parameters:
            verify_input_parameters(running_config, input_param)

    def test_wait_for_pingpong_started(self, rwnsr_proxy):
        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        for nsr in nsrs:
            xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/operational-status".format(
                    nsr.ns_instance_config_ref)
            rwnsr_proxy.wait_for(xpath, "running", fail_on=['failed'], timeout=180)

    def test_wait_for_pingpong_configured(self, rwnsr_proxy):
        nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        for nsr in nsrs:
            xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/config-status".format(
                    nsr.ns_instance_config_ref)
            rwnsr_proxy.wait_for(xpath, "configured", fail_on=['failed'], timeout=450)


@pytest.mark.teardown('pingpong')
@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestPingPongTeardown(object):
    def test_terminate_nsrs(self, rwvnfr_proxy, rwnsr_proxy, logger):
        """
        Terminate the instance and check if the record is deleted.

        Asserts:
        1. NSR record is deleted from instance-config.

        """
        logger.debug("Terminating Ping Pong NSR")
        terminate_nsrs(rwvnfr_proxy, rwnsr_proxy, logger)

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
