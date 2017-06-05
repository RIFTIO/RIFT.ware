"""
# STANDARD_RIFT_IO_COPYRIGHT #

   Copyright 2016-2017 RIFT.IO Inc

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

@file upload_image_cal.py
@date 06/02/2017
@brief Uploads images to a VIM account
"""
import sys
import os
import traceback
import re
import argparse
import hashlib
import time
import logging
import functools

from gi import require_version
require_version('RwcalYang', '1.0')
require_version('RwTypes', '1.0')
require_version('RwCal', '1.0')
from gi.repository import RwcalYang
from gi.repository import RwTypes
import rw_peas
import rwlogger

logger = logging.getLogger()


class ImageUploadError(Exception):
    """Thrown if upload of image failed"""
    pass


def parse_args():
    parser = argparse.ArgumentParser(description="Uploads images to a VIM account")

    parser.add_argument('--file', required=True, nargs='+', help='image paths to be uploaded')
    parser.add_argument('--disk-format', default='qcow2',
                        choices=['ami', 'ari', 'aki', 'vhd', 'vmdk', 'raw', 'qcow2', 'vdi', 'iso'],
                        help="VM image's disk format")
    parser.add_argument('--container-format', default='bare', choices=['ami', 'ari', 'aki', 'bare', 'ovf'],
                        help="VM image's container format")

    # Make the VIM types exclusive and mandatory
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--openstack', help='openstack auth details user:pass@openstack-host')
    group.add_argument('--openvim', help='openvim auth details user:pass@openvim-host')
    # group.add_argument('--cloudsim', help='cloudsim host cloudsim-host')

    parser.add_argument('--tenant', help='Needed if --openstack and --openvim is passed')
    parser.add_argument('--upload-timeout', default=300, type=int, help='timeout in seconds for each image upload')

    # Default log level is WARN. Make it INFO or DEBUG based on this option
    parser.add_argument('--log-level', action="store", choices=['info', 'debug'], help='log level to use')
    parser.add_argument('--keystone-api', default='v3', help='log level to use')
    
    return parser.parse_args()


def cloud_account_cal_interface(args):
    """Returns RwcalYang.CloudAccount object and cal cloud interface"""
    if args.openstack or args.openvim:
        auth = args.openstack or args.openvim
        match = re.search(r'([\w.-]+):([\w.-]+)@([\w.-]+)', auth)
        if not match:
            logger.error('Error- auth provided not in proper format: {}'.format(auth))
            sys.exit(1)
        user = match.group(1)
        passwd = match.group(2)
        host = match.group(3)

    if args.openstack:
        auth_url = 'http://{}:5000/{}/'.format(host, args.keystone_api)
        account = RwcalYang.CloudAccount.from_dict({
                    'name': 'rift.auto.openstack',
                    'account_type': 'openstack',
                    'openstack': {
                      'key':  user,
                      'secret': passwd,
                      'auth_url': auth_url,
                      'tenant': args.tenant}})
        plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')

    if args.openvim:
        account = RwcalYang.CloudAccount.from_dict({
                    'name': 'rift.auto.openvim',
                    'account_type': 'openvim',
                    'openvim': {
                      'host':  host,
                      'port': 9080,
                      'tenant_name': args.tenant,
                      'image_management': {
                        'username': user,
                        'password': passwd}}})
        plugin = rw_peas.PeasPlugin('rwcal_openmano_vimconnector', 'RwCal-1.0')

    # if args.cloudsim:
    #     account = RwcalYang.CloudAccount.from_dict({
    #                 'name': 'rift.auto.cloudsim',
    #                 'account_type': 'cloudsim'})
    #     plugin = rw_peas.PeasPlugin('rwcal_cloudsim', 'RwCal-1.0')

    engine, info, extension = plugin()
    cal = plugin.get_interface("Cloud")
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")
    rc = cal.init(rwloggerctx)
    assert rc == RwTypes.RwStatus.SUCCESS
    return account, cal


def create_image(cal, account, image, use_existing=True):
    """Creates the image in cloud account.

    Returns:
        image_id: UUID of image being created or UUID of existing image if image already exists in the cloud.

    Raises:
        AssertionError: If image creation fails.
    """
    def md5sum(path):
        with open(path, mode='rb') as fd:
            md5 = hashlib.md5()
            for buf in iter(functools.partial(fd.read, 4096), b''):
                md5.update(buf)
        return md5.hexdigest()

    if use_existing:
        if not image.checksum:
            image.checksum = md5sum(image.location)
        rc, imagelist = cal.get_image_list(account)
        existing_images = {image.checksum:image.id for image in imagelist.imageinfo_list}

        if image.checksum and image.checksum in existing_images:
            logger.info("Using existing image [{}] with matching checksum [{}]".format(existing_images[image.checksum],
                                                                                       image.checksum))
            return existing_images[image.checksum]

    rc, image_id = cal.create_image(account, image)
    assert rc == RwTypes.RwStatus.SUCCESS

    return image_id


def wait_for_image_state(cal, account, image_id, state='active', timeout=300):
    """Waits for image upload state to become active.
    
    Returns:
        RwcalYang.ImageInfoItem object
    
    Raises:
        TimeoutError: If the image state doeesn't reach 'active' state within timeout period.
        ImageUploadError: If image creation fails.
    """
    start_time = time.time()
    state = state.lower()
    current_state = 'unknown'
    while current_state != state:
        rc, image_info = cal.get_image(account, image_id)
        current_state = image_info.state.lower()
        if current_state in ['failed']:
           raise ImageUploadError('Image [{}] entered {} state while waiting for state [{}]'.format(image_id, current_state, state))
        time_elapsed = time.time() - start_time
        time_remaining = timeout - time_elapsed
        if time_remaining <= 0:
            logger.error('Image still in state [{}] after [{}] seconds'.format(current_state, timeout))
            raise TimeoutError('Image [{}] failed to reach state [{}] within timeout [{}]'.format(image_id, state, timeout))
        if current_state != state:
            time.sleep(5)

    return image_info

if __name__ == '__main__':
    if not os.environ.get('RIFT_ROOT'):
        logger.error('It has to be invoked in a rift-shell')
        sys.exit(1)

    args = parse_args()
    if args.openstack or args.openvim:
        if not args.tenant:
            logger.error('--tenant is mandatory for --openstack / --openvim')
            sys.exit(1)

    log_level = {'info': logging.INFO, 'debug': logging.DEBUG}
    if args.log_level:
        logger.setLevel(log_level[args.log_level])
        
    # Get RwcalYang.CloudAccount object and cal interface
    account, cal = cloud_account_cal_interface(args)
    logger.info('cloud account details - {}'.format(account))

    # Go through each image path and upload it to VIM
    for file in args.file:
        if not os.path.exists(file):
            logger.error("'{}' - Provided image file path does not exist".format(file))
            sys.exit(1)

        # Create RwcalYang.ImageInfoItem object
        image = RwcalYang.ImageInfoItem.from_dict({
                        'name': os.path.basename(file),
                        'location': file,
                        'disk_format': args.disk_format,
                        'container_format': args.container_format})

        # Upload the image to VIM and get the image_id
        logger.info('Creating image: {}'.format(image.as_dict()))
        image_id = create_image(cal, account, image)

        # Wait for image upload to finish
        wait_for_image_state(cal, account, image_id, timeout=args.upload_timeout)
        logger.info('Image Uploaded successfully. Image id - {}'.format(image_id))

    sys.exit(0)
