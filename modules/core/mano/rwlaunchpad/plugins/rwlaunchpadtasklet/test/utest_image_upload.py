#!/usr/bin/env python3

import argparse
import logging
import os
import sys
import tempfile
import unittest
import uuid
import xmlrunner

from rift.mano import cloud
from rift.tasklets import rwlaunchpad
from rift.package import checksums
import rw_status

import gi
gi.require_version('RwCal', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwCloudYang', '1.0')
gi.require_version('RwLog', '1.0')
gi.require_version('RwTypes', '1.0')
from gi.repository import (
        RwCal,
        RwCloudYang,
        RwLog,
        RwTypes,
        RwcalYang,
        )

rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    })


class CreateImageMock(object):
    def __init__(self, log):
        self._log = log
        self.image_name = None
        self.image_checksum = None

        self.do_fail = False

        self._image_msgs = []

    def add_existing_image(self, image_msg):
        self._log.debug("Appending existing image msg: %s", image_msg)
        self._image_msgs.append(image_msg)

    @rwstatus
    def do_create_image(self, _, image):
        if self.do_fail:
            self._log.debug("Simulating failure")
            raise ValueError("FAILED")

        if not image.has_field("fileno"):
            raise ValueError("Image must have fileno")

        self.image_name = image.name

        # Create a duplicate file descriptor to allow this code to own
        # its own descritor (and thus close it) and allow the client to
        # own and close its own descriptor.
        new_fileno = os.dup(image.fileno)
        with os.fdopen(new_fileno, 'rb') as hdl:
            self.image_checksum = checksums.checksum(hdl)

        return str(uuid.uuid4())

    @rwstatus
    def do_get_image_list(self, account):
        boxed_image_list = RwcalYang.VimResources()
        for msg in self._image_msgs:
            boxed_image_list.imageinfo_list.append(msg)

        return boxed_image_list


def create_random_image_file():
    with open("/dev/urandom", "rb") as rand_hdl:
        file_hdl = tempfile.NamedTemporaryFile("r+b")
        file_hdl.write(rand_hdl.read(1 * 1024 * 1024))
        file_hdl.flush()
        file_hdl.seek(0)
        return file_hdl


def get_image_checksum(image_hdl):
    image_checksum = checksums.checksum(image_hdl)
    image_hdl.seek(0)
    return image_checksum


class TestAccountImageUpload(unittest.TestCase):
    ACCOUNT_MSG = RwCloudYang.CloudAccount(
        name="mock",
        account_type="mock",
        )

    def setUp(self):
        self._log = logging.getLogger(__file__)
        self._account = cloud.CloudAccount(
                self._log,
                RwLog.Ctx.new(__file__), TestAccountImageUpload.ACCOUNT_MSG
                )
        self._image_uploader = rwlaunchpad.image.ImageUploader(self._log, [self._account])
        self._create_image_mock = CreateImageMock(self._log)

        # Mock the create_image call
        self._account.cal.create_image = self._create_image_mock.do_create_image
        self._account.cal.get_image_list = self._create_image_mock.do_get_image_list

    def create_image_hdl(self, image_name):
        image_hdl = create_random_image_file()

        return image_hdl

    def test_create_image(self):
        image_name = "test.qcow2"
        with self.create_image_hdl(image_name) as hdl:
            image_checksum = get_image_checksum(hdl)
            self._image_uploader.upload_images({image_name: hdl})

        self.assertEqual(self._create_image_mock.image_name, image_name)
        self.assertEqual(self._create_image_mock.image_checksum, image_checksum)

    def test_create_image_failed(self):
        self._create_image_mock.do_fail = True

        image_name = "test.qcow2"
        with self.create_image_hdl(image_name) as hdl:
            with self.assertRaises(rwlaunchpad.image.ImageUploadError):
                self._image_uploader.upload_images({image_name: hdl})

    def test_create_image_name_and_checksum_exists(self):
        image_name = "test.qcow2"
        with self.create_image_hdl(image_name) as hdl:
            image_checksum = get_image_checksum(hdl)

            image_entry = RwcalYang.ImageInfoItem(
                    id="asdf",
                    name=image_name,
                    checksum=image_checksum
                    )
            self._create_image_mock.add_existing_image(image_entry)

            self._image_uploader.upload_images({image_name: hdl})

            # No image should have been uploaded, since the name and checksum
            self.assertEqual(self._create_image_mock.image_checksum, None)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')

    args, unknown = parser.parse_known_args(argv)
    if args.no_runner:
        runner = None

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + unknown + ["-v"], testRunner=runner)

if __name__ == '__main__':
    main()
