
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
'''
@file descriptor.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 2016-01-24
@brief Helper methods for interacting with descriptors
'''

import json
import logging
import requests
import shlex
import subprocess
import time
import uuid

from gi.repository import NsrYang

import rift.auto.mano

logger = logging.getLogger(__name__)

class DescriptorOnboardError(Exception):
    '''Raised when there is an error during the onboarding process'''
    pass

def upload(host, descriptor):
    ''' Begin onboarding a descriptor package by uploading it

    This method begins an asynchronous onboard of a descriptor, waiting
    for the onboard process to finish is left up to the user.

    Arguments:
        host - address of the host participating in the upload process
        descriptor - path to the descriptor package

    Returns:
        transaction_id associated with this package's onboarding process
    '''
    cmd = 'curl -F "descriptor=@{descriptor}" http://{host}:4567/api/upload'.format(
        descriptor=descriptor,
        host=host,
        )
    logger.debug("Uploading descriptor %s using cmd: %s", descriptor, cmd)

    output = subprocess.check_output(shlex.split(cmd), universal_newlines=True)
    result = json.loads(output)
    return result["transaction_id"]


def wait_for_onboard_finished(host, transaction_id, timeout=600, retry_interval=10):
    ''' Wait for an onboard transaction to finish

    This method can be used to wait for an onboard process that has been started
    using upload descriptor to finish.

    Arugments:
        host - address of host participating in the onboard process
        transaction_id - id of the onboard transaction to wait for
        timeout - time in seconds allowed for transaction to finish
        retry_interval - time in seconds between checks for transaction finished

    Raises:
        DescriptorOnboardError - Raised if descriptor enters an unexpected state
                Or if the timeout elapses without the descriptor being onboarded
    '''
    logger.info("Waiting for onboard trans_id %s to complete", transaction_id)
    uri = 'http://%s:4567/api/upload/%s/state' % (host, transaction_id)   

    start_time = time.time()
    while True:
        reply = requests.get(uri)
        state = reply.json()

        if state["status"] == "success": break
        if state["status"] != "pending": raise DescriptorOnboardError(state)

        time_elapsed = time.time() - start_time
        time_remaining = timeout - time_elapsed
        if time_remaining <= 0:
            logger.error('Timed out waiting for onboard transaction to finish')
            raise DescriptorOnboardError(state)
        time.sleep(min(time_remaining, retry_interval))

    if state["status"] != "success": raise DescriptorOnboardError(state)
    logger.info("Descriptor onboard was successful")


def onboard(host, descriptor, timeout=600, retry_interval=10):
    ''' Onboard a descriptor

    This method performs a sychronous onboard of the descriptor, by invoking
    upload followed by wait_for_onboard_finished on the returned
    transaction_id.

    Arguments:
        host - address of the host participating in the onboard process
        descriptor - path to the descriptor package
        timeout - time allowed for the onboard process to finish
        retry - time between checks for the onboard process to finish

    Raises:
        DescriptorOnboardError - Raised if descriptor enters an unexpected state
                Or if the timeout elapses without the descriptor being onboarded
    '''
    logger.info("Onboarding descriptor package %s to host %s", descriptor, host)
    transaction_id = upload(host, descriptor)
    wait_for_onboard_finished(host, transaction_id, timeout, retry_interval)


def generate_nsr(
        name,
        nsd_ref,
        short_name="test short name",
        description="test description",
        admin_status="ENABLED",
        nsr_id=None):
    ''' A helper method for generating an NSR

    Arguments:
        name - name of the nsr being generated
        nsd_ref - reference to the nsd this NSR is an instance of
        short_name - short name of the nsr being generated
        description - description of the nsr being generated
        admin_status - flag indicating NSR admin status (enabled/disabled)
        nsr_id - unique identifier of this NSR
    '''

    nsr = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
    nsr.name = rift.auto.mano.resource_name(name)
    nsr.nsd_ref = nsd_ref
    nsr.short_name = short_name
    nsr.description = description
    nsr.admin_status = "ENABLED"

    if nsr_id:
        nsr.id = nsr_id
    else:
        nsr.id = str(uuid.uuid4())

    return nsr

