import re
import os

import gi
gi.require_version('RwcalYang', '1.0')
from gi.repository import (
        RwcalYang as rwcal,
        )

import rift.mano.cloud
import rift.package.checksums
import rift.package.image


class ImageUploadError(Exception):
    pass


class ImageUploader(object):
    """ This class is responsible for uploading package images to cloud accounts """
    def __init__(self, log, accounts):
        """ Create an instance of ImageUploader

        Arguments:
            log - A logger
            accounts - A list which contains CloudAccount instances
        """
        self._log = log
        self._accounts = accounts

    def _get_account_images(self):
        account_images = {}
        for account in self._accounts:
            self._log.debug("getting image list for account {}".format(account.name))
            account_images[account] = []
            try:
                account_images[account] = account.get_image_list()
            except rift.mano.cloud.CloudAccountCalError as e:
                self._log.warning("could not get image list for account {}".format(account.name))
                continue

        return account_images

    def upload_images(self, image_name_hdl_map):
        """ Upload images to all CloudAccounts

        Arguments:
            image_name_hdl_map - A map of image name to an opened image handle

        Raises:
            ImageUploadError - A image failed to be uploaded to a cloud account
        """
        if len(image_name_hdl_map) == 0:
            return

        self._log.debug("uploading %s images to cloud accounts: %s",
            len(image_name_hdl_map),
            self._accounts
            )

        account_images = self._get_account_images()
        for image_name, image_hdl in image_name_hdl_map.items():
            self._log.debug('uploading image: {}'.format(image_name))

            image_checksum = rift.package.checksums.checksum(image_hdl)
            image_hdl.seek(0)

            image = rwcal.ImageInfoItem()
            image.name = image_name

            try:
                image.fileno = image_hdl.fileno()
                image.location = image_hdl.name
            except Exception as e:
                msg = "Image handle must a fileno() and name attribute"
                self._log.error(msg)
                raise ImageUploadError(msg)

            for account in self._accounts:
                # Find images on the cloud account which have the same name
                matching_images = [i for i in account_images[account] if i.name == image.name]
                matching_checksum = [i for i in matching_images if i.checksum == image_checksum]
                if len(matching_checksum) > 0:
                    self._log.debug("found matching image with checksum, not uploading to {}".format(account.name))
                    continue

                self._log.debug("uploading to account {}: {}".format(account.name, image))
                try:
                    # If we are using file descriptor to upload the image, make sure we
                    # set the position to the beginning of the file for each upload.
                    if image.has_field("fileno"):
                        os.lseek(image.fileno, 0, os.SEEK_SET)

                    image.id = account.create_image(image)
                except rift.mano.cloud.CloudAccountCalError as e:
                    msg = "error when uploading image {} to cloud account: {}".format(image_name, str(e))
                    self._log.error(msg)
                    raise ImageUploadError(msg) from e

                self._log.debug('uploaded image to account{}: {}'.format(account.name, image_name))
