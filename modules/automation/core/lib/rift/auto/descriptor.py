
# STANDARD_RIFT_IO_COPYRIGHT
'''
@file descriptor.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 2016-01-24
@brief Helper methods for interacting with descriptors
'''

import json
import logging
import os
import requests
import requests_toolbelt
import shlex
import subprocess
import time
import uuid

import rift.rwlib.util
from gi.repository import (
    RwNsrYang,
    RwPkgMgmtYang,
    RwStagingMgmtYang,
)

import rift.auto.mano

logger = logging.getLogger(__name__)

class DescriptorOnboardError(Exception):
    '''Raised when there is an error during the onboarding process'''
    pass


def create_nsr(
        cloud_account_name,
        nsr_name,
        nsd,
        input_param_list=[],
        short_name="test short name",
        description="test description",
        admin_status="ENABLED",
        nsr_id=None):
    ''' A helper method for creating an NSR

    Arguments:
        cloud_account_name - name of the cloud account being used by NSR
        nsr_name - name of the nsr being created
        nsd - The NSD this NSR is an instance of
        short_name - short name of the nsr being created
        description - description of the nsr being created
        admin_status - flag indicating NSR admin status (enabled/disabled)
        nsr_id - unique identifier of this NSR
    '''
    nsr = RwNsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
    nsr.cloud_account = cloud_account_name
    nsr.name = rift.auto.mano.resource_name(nsr_name)
    nsr.nsd.from_dict(nsd.as_dict())
    nsr.short_name = short_name
    nsr.description = description
    nsr.admin_status = admin_status
    nsr.input_parameter.extend(input_param_list)

    if nsr_id:
        nsr.id = nsr_id
    else:
        nsr.id = str(uuid.uuid4())

    return nsr


def terminate_nsr(rwvnfr_proxy, rwnsr_proxy, logger, wait_after_kill=True):
    """
    Terminate the instance and checks if the records are deleted.

    Checks if VNFR and NSR records are deleted.

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

    if wait_after_kill:
        time.sleep(30)
    else:
        time.sleep(5)

    for xpath in xpaths:
        nsr = rwnsr_proxy.get_config(xpath)
        assert nsr is None

    # Get the ns-instance-config
    ns_instance_config = rwnsr_proxy.get_config("/ns-instance-config")

    vnfr_del = nsr_del = False
    start_time = time.time()
    while time.time() - start_time < 300:
        try:
            if vnfr_del and nsr_del:
                break
            vnfr = "/vnfr-catalog/vnfr"
            vnfrs = rwvnfr_proxy.get(vnfr, list_obj=True)
            if vnfrs is not None and len(vnfrs.vnfr) != 0:
                vnfr_del = False
                raise AssertionError
            vnfr_del = True
            nsr = "/ns-instance-opdata/nsr"
            nsrs = rwnsr_proxy.get(nsr, list_obj=True)
            if nsrs is not None and len(nsrs.nsr) != 0:
                nsr_del = False
                raise AssertionError
            nsr_del = True
        except AssertionError:
            time.sleep(10)
            
    if not vnfr_del:
        raise AssertionError("vnfr termination failed")

    if not nsr_del:
        raise AssertionError("nsr termination failed")


def get_package_type(descriptor):
    ''' Try to determine the type of the descriptor provided from its package name

    Arugments:
        descriptor - descriptor for which the type is being determined

    Returns:
        type of descriptor as an uppercase string ['NSD', 'VNFD']

    Raises:
        DescriptorOnboardError - Raised if descriptor type cannot be determined
    '''
    for keyword in ['nsd','vnfd']:
        if (keyword in descriptor
            or keyword.upper() in descriptor
            or keyword.capitalize() in descriptor):
            return keyword.upper()
    raise DescriptorOnboardError("Failed to determine descriptor package type")


def onboard(session, descriptor, package_type=None):
    ''' Onboard provided descriptor to the host of the management session

    Arguments:
        session - management session
        descriptor - descriptor to be onboarded

    Raises:
        DescriptorOnboardError - Raised if onboarding process fails
    '''
    logger.info('Onboarding descriptor: %s' % descriptor)
    try: # Try to onboard descriptor
        if not package_type:
            package_type = get_package_type(descriptor)
        staging_area = create_staging_area(session, descriptor, package_type)
        descriptor_uri = stage_descriptor(session, staging_area, descriptor)
        transaction_id = upload_staged_descriptor(session, descriptor_uri, package_type)
        wait_for_upload_finished(session, transaction_id)
    except Exception:
        raise DescriptorOnboardError('Failed to onboard descriptor')
    logger.info("Descriptor onboarded successfully")


def create_staging_area(session, descriptor, package_type=None):
    ''' Create a staging area for the provided descriptor

    Arguments:
        session - management session
        descriptor - descriptor to be onboarded

    Returns:
        A GI object describing staging area created
        or None
    '''
    if package_type is None:
        package_type = get_package_type(descriptor)
    rpc_input = RwStagingMgmtYang.YangInput_RwStagingMgmt_CreateStagingArea.from_dict({
        "package_type": package_type
    })
    return session.proxy(RwStagingMgmtYang).rpc(rpc_input)


def stage_descriptor(session, staging_area, descriptor):
    ''' Stage a descriptor in a previously created staging area

    Arguments:
        session - management session
        staging_area - staging area in which to stage descriptor
        descriptor - descriptor to be staged

    Returns:
        URI to staged descriptor resources
    '''
    form = requests_toolbelt.MultipartEncoder(
        fields={
            'file':(
                os.path.basename(descriptor),
                open(descriptor, 'rb'),
                'application/octet-stream'
    )})

    if rift.rwlib.util.certs.USE_SSL:
        _, cert, key = rift.rwlib.util.certs.get_bootstrap_cert_and_key()
        reply = requests.post(
                        "https://{host}:{port}/{endpoint}".format(
                                host=session.host,
                                port=staging_area.port,
                                endpoint=staging_area.endpoint),
                        data=form.to_string(),
                        cert=(cert,key),
                        verify=False,
                        headers={"Content-Type": "multipart/form-data"}
        )
        resource = json.loads(reply.text)
        descriptor_uri = "https://{}:{}{}".format(session.host, staging_area.port, resource['path'])
    else:
        reply = requests.post(
                        "http://{host}:{port}/{endpoint}".format(
                                host=session.host,
                                port=staging_area.port,
                                endpoint=staging_area.endpoint),
                        data=form.to_string(),
                        cert=cert,  # cert is a tuple
                        verify=False,
                        headers={"Content-Type": "multipart/form-data"}
        )
        resource = json.loads(reply.text)
        descriptor_uri = "http://{}:{}{}".format(session.host, staging_area.port, resource['path'])

    return descriptor_uri


def upload_staged_descriptor(session, descriptor_uri, package_type):
    ''' Upload a descriptor previously added to a staging area

    Arguments:
        session - management session
        descriptor_uri - uri to staged descriptor resources
        package_type - type of staged descriptor resources : one of ['NSD', 'VNFD']

    Returns:
        transaction id of upload transaction
    '''
    rpc_input = RwPkgMgmtYang.YangInput_RwPkgMgmt_PackageCreate.from_dict({
            "package_type": package_type,
        "external_url": descriptor_uri
    })
    response = session.proxy(RwPkgMgmtYang).rpc(rpc_input)
    return response.transaction_id


def get_upload_state(session, transaction_id):
    ''' Get the state of a previously started upload transaction

    Arguments:
        session - management session
        transaction_id - transaction id of upload transaction

    Returns:
        dictionary describing upload transaction state
    '''
    if rift.rwlib.util.certs.USE_SSL:
        _, cert, key = rift.rwlib.util.certs.get_bootstrap_cert_and_key()
        reply = requests.get(
            'https://%s:4567/api/upload/%s/state' % (session.host, transaction_id),
            cert=(cert, key),
            verify=False)
    else:
        reply = requests.get(
            'http://%s:4567/api/upload/%s/state' % (session.host, transaction_id))
    return reply.json()


def wait_for_upload_finished(session, transaction_id, timeout=600, retry_interval=10):
    ''' Wait for an upload transaction to finish

    Arugments:
        session - management session (instance of rift.auto.session.Session)
        transaction_id - id of the upload transaction to wait for
        timeout - time in seconds allowed for transaction to finish
        retry_interval - time in seconds between checks for transaction finished

    Raises:
        DescriptorOnboardError - Raised if descriptor enters an unexpected state
                Or if the timeout elapses without the descriptor being uploaded
    '''

    start_time = time.time()
    while True:
        state = get_upload_state(session, transaction_id)
        if state["status"] == "success": break
        if state["status"] != "pending":
            logger.error('Upload transaction entered unexpected state')
            raise DescriptorOnboardError(state)
        time_elapsed = time.time() - start_time
        time_remaining = timeout - time_elapsed
        if time_remaining <= 0:
            logger.error('Timed out waiting for upload [%s] transaction to finish', transaction_id)
            raise DescriptorOnboardError(state)
        time.sleep(min(time_remaining, retry_interval))
    if state["status"] != "success": raise DescriptorOnboardError(state)
