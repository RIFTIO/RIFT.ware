
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import os
import tarfile
import tempfile
import threading
import uuid

import tornado
import tornado.escape
import tornado.ioloop
import tornado.web
import tornado.httputil
import requests

# disable unsigned certificate warning
from requests.packages.urllib3.exceptions import InsecureRequestWarning
requests.packages.urllib3.disable_warnings(InsecureRequestWarning)

import gi
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('NsdYang', '1.0')
gi.require_version('VnfdYang', '1.0')

from gi.repository import (
        NsdYang,
        VnfdYang,
        )
import rift.mano.cloud

import rift.package.charm
import rift.package.checksums
import rift.package.config
import rift.package.convert
import rift.package.icon
import rift.package.package
import rift.package.script
import rift.package.store

from . import (
        export,
        extract,
        image,
        message,
        onboard,
        state,
        )

from .message import (
        MessageException,

        # Onboard Error Messages
        OnboardChecksumMismatch,
        OnboardDescriptorError,
        OnboardDescriptorExistsError,
        OnboardDescriptorFormatError,
        OnboardError,
        OnboardExtractionError,
        OnboardImageUploadError,
        OnboardMissingContentBoundary,
        OnboardMissingContentType,
        OnboardMissingTerminalBoundary,
        OnboardUnreadableHeaders,
        OnboardUnreadablePackage,
        OnboardUnsupportedMediaType,

        # Onboard Status Messages
        OnboardDescriptorOnboard,
        OnboardFailure,
        OnboardImageUpload,
        OnboardPackageUpload,
        OnboardPackageValidation,
        OnboardStart,
        OnboardSuccess,


        # Update Error Messages
        UpdateChecksumMismatch,
        UpdateDescriptorError,
        UpdateDescriptorFormatError,
        UpdateError,
        UpdateExtractionError,
        UpdateImageUploadError,
        UpdateMissingContentBoundary,
        UpdateMissingContentType,
        UpdatePackageNotFoundError,
        UpdateUnreadableHeaders,
        UpdateUnreadablePackage,
        UpdateUnsupportedMediaType,

        # Update Status Messages
        UpdateDescriptorUpdate,
        UpdateDescriptorUpdated,
        UpdatePackageUpload,
        UpdateStart,
        UpdateSuccess,
        UpdateFailure,
        )

from .tosca import ExportTosca


class HttpMessageError(Exception):
    def __init__(self, code, msg):
        self.code = code
        self.msg = msg


class RequestHandler(tornado.web.RequestHandler):
    def options(self, *args, **kargs):
        pass

    def set_default_headers(self):
        self.set_header('Access-Control-Allow-Origin', '*')
        self.set_header('Access-Control-Allow-Headers',
                        'Content-Type, Cache-Control, Accept, X-Requested-With, Authorization')
        self.set_header('Access-Control-Allow-Methods', 'POST, GET, PUT, DELETE')


@tornado.web.stream_request_body
class StreamingUploadHandler(RequestHandler):
    def initialize(self, log, loop):
        """Initialize the handler

        Arguments:
            log  - the logger that this handler should use
            loop - the tasklets ioloop

        """
        self.transaction_id = str(uuid.uuid4())

        self.loop = loop
        self.log = self.application.get_logger(self.transaction_id)

        self.package_name = None
        self.package_fp = None
        self.boundary = None

        self.log.debug('created handler (transaction_id = {})'.format(self.transaction_id))

    def msg_missing_content_type(self):
        raise NotImplementedError()

    def msg_unsupported_media_type(self):
        raise NotImplementedError()

    def msg_missing_content_boundary(self):
        raise NotImplementedError()

    def msg_start(self):
        raise NotImplementedError()

    def msg_success(self):
        raise NotImplementedError()

    def msg_failure(self):
        raise NotImplementedError()

    def msg_package_upload(self):
        raise NotImplementedError()

    @tornado.gen.coroutine
    def prepare(self):
        """Prepare the handler for a request

        The prepare function is the first part of a request transaction. It
        creates a temporary file that uploaded data can be written to.

        """
        if self.request.method != "POST":
            return

        self.log.message(self.msg_start())

        try:
            # Retrieve the content type and parameters from the request
            content_type = self.request.headers.get('content-type', None)
            if content_type is None:
                raise HttpMessageError(400, self.msg_missing_content_type())

            content_type, params = tornado.httputil._parse_header(content_type)

            if "multipart/form-data" != content_type.lower():
                raise HttpMessageError(415, self.msg_unsupported_media_type())

            if "boundary" not in params:
                raise HttpMessageError(400, self.msg_missing_content_boundary())

            self.boundary = params["boundary"]
            self.package_fp = tempfile.NamedTemporaryFile(
                    prefix="pkg-",
                    delete=False,
                    )

            self.package_name = self.package_fp.name

            self.log.debug('writing to {}'.format(self.package_name))

        except HttpMessageError as e:
            self.log.message(e.msg)
            self.log.message(self.msg_failure())

            raise tornado.web.HTTPError(e.code, e.msg.name)

        except Exception as e:
            self.log.exception(e)
            self.log.message(self.msg_failure())

    @tornado.gen.coroutine
    def data_received(self, data):
        """Write data to the current file

        Arguments:
            data - a chunk of data to write to file

        """
        self.package_fp.write(data)

    def post(self):
        """Handle a post request

        The function is called after any data associated with the body of the
        request has been received.

        """
        self.package_fp.close()
        self.log.message(self.msg_package_upload())


class UploadHandler(StreamingUploadHandler):
    """
    This handler is used to upload archives that contain VNFDs, NSDs, and PNFDs
    to the launchpad. This is a streaming handler that writes uploaded archives
    to disk without loading them all into memory.
    """

    def msg_missing_content_type(self):
        return OnboardMissingContentType()

    def msg_unsupported_media_type(self):
        return OnboardUnsupportedMediaType()

    def msg_missing_content_boundary(self):
        return OnboardMissingContentBoundary()

    def msg_start(self):
        return OnboardStart()

    def msg_success(self):
        return OnboardSuccess()

    def msg_failure(self):
        return OnboardFailure()

    def msg_package_upload(self):
        return OnboardPackageUpload()

    def post(self):
        """Handle a post request

        The function is called after any data associated with the body of the
        request has been received.

        """
        super().post()

        filesize = os.stat(self.package_name).st_size
        self.log.debug('wrote {} bytes to {}'.format(filesize, self.package_name))

        self.application.onboard(
                self.package_name,
                self.boundary,
                self.transaction_id,
                auth=self.request.headers.get('authorization', None),
                )

        self.set_status(200)
        self.write(tornado.escape.json_encode({
            "transaction_id": self.transaction_id,
                }))


class UpdateHandler(StreamingUploadHandler):
    def msg_missing_content_type(self):
        return UpdateMissingContentType()

    def msg_unsupported_media_type(self):
        return UpdateUnsupportedMediaType()

    def msg_missing_content_boundary(self):
        return UpdateMissingContentBoundary()

    def msg_start(self):
        return UpdateStart()

    def msg_success(self):
        return UpdateSuccess()

    def msg_failure(self):
        return UpdateFailure()

    def msg_package_upload(self):
        return UpdatePackageUpload()

    def post(self):
        """Handle a post request

        The function is called after any data associated with the body of the
        request has been received.

        """
        super().post()

        filesize = os.stat(self.package_name).st_size
        self.log.debug('wrote {} bytes to {}'.format(filesize, self.package_name))

        self.application.update(
                self.package_name,
                self.boundary,
                self.transaction_id,
                auth=self.request.headers.get('authorization', None),
                )

        self.set_status(200)
        self.write(tornado.escape.json_encode({
            "transaction_id": self.transaction_id,
                }))


class UploadStateHandler(state.StateHandler):
    STARTED = OnboardStart
    SUCCESS = OnboardSuccess
    FAILURE = OnboardFailure


class UpdateStateHandler(state.StateHandler):
    STARTED = UpdateStart
    SUCCESS = UpdateSuccess
    FAILURE = UpdateFailure


class UpdatePackage(threading.Thread):
    def __init__(self, log, filename, boundary, pkg_id, auth,
                 onboarder, uploader, package_store_map):
        super().__init__()
        self.log = log
        self.filename = filename
        self.boundary = boundary
        self.pkg_id = pkg_id
        self.auth = auth
        self.onboarder = onboarder
        self.uploader = uploader
        self.package_store_map = package_store_map

        self.io_loop = tornado.ioloop.IOLoop.current()

    def _update_package(self):
        # Extract package could return multiple packages if
        # the package is converted
        for pkg in self.extract_package():
            with pkg as temp_package:
                self.validate_package(temp_package)
                stored_package = self.update_package(temp_package)

            try:
                self.extract_charms(stored_package)
                self.extract_scripts(stored_package)
                self.extract_configs(stored_package)
                self.extract_icons(stored_package)

                self.upload_images(stored_package)
                self.update_descriptors(stored_package)

            except Exception:
                self.delete_stored_package(stored_package)
                raise

    def run(self):
        try:
            self._update_package()
            self.log.message(UpdateSuccess())

        except MessageException as e:
            self.log.message(e.msg)
            self.log.message(UpdateFailure())

        except Exception as e:
            self.log.exception(e)
            if str(e):
                self.log.message(UpdateError(str(e)))
            self.log.message(UpdateFailure())

    def extract_package(self):
        """Extract multipart message from tarball"""
        extractor = extract.UploadPackageExtractor(self.log)
        file_backed_package = extractor.extract_package_from_upload(
                self.filename, self.boundary, self.pkg_id
                )

        return file_backed_package

    def get_package_store(self, package):
        return self.package_store_map[package.descriptor_type]

    def update_package(self, package):
        store = self.get_package_store(package)

        try:
            store.update_package(package)
        except rift.package.store.PackageNotFoundError as e:
            raise MessageException(UpdatePackageNotFoundError(package.descriptor_id)) from e

        stored_package = store.get_package(package.descriptor_id)

        return stored_package

    def delete_stored_package(self, package):
        store = self.get_package_store(package)
        try:
            store.delete_package(package)
        except Exception as e:
            self.log.warning("Failed to delete package from store: %s", str(e))

    def upload_images(self, package):
        image_file_map = rift.package.image.get_package_image_files(package)
        name_hdl_map = {name: package.open(image_file_map[name]) for name in image_file_map}
        try:
            self.uploader.upload_images(name_hdl_map)

        except image.ImageUploadError as e:
            raise MessageException(UpdateImageUploadError()) from e

        finally:
            for hdl in name_hdl_map.values():
                hdl.close()

    def extract_charms(self, package):
        try:
            charm_extractor = rift.package.charm.PackageCharmExtractor(self.log)
            charm_extractor.extract_charms(package)
        except rift.package.charm.CharmExtractionError as e:
            raise MessageException(UpdateExtractionError()) from e

    def extract_scripts(self, package):
        try:
            script_extractor = rift.package.script.PackageScriptExtractor(self.log)
            script_extractor.extract_scripts(package)
        except rift.package.script.ScriptExtractionError as e:
            raise MessageException(UpdateExtractionError()) from e

    def extract_configs(self, package):
        try:
            config_extractor = rift.package.config.PackageConfigExtractor(self.log)
            config_extractor.extract_configs(package)
        except rift.package.config.ConfigExtractionError as e:
            raise MessageException(UpdateExtractionError()) from e

    def extract_icons(self, package):
        try:
            icon_extractor = rift.package.icon.PackageIconExtractor(self.log)
            icon_extractor.extract_icons(package)
        except rift.package.icon.IconExtractionError as e:
            raise MessageException(UpdateExtractionError()) from e

    def validate_package(self, package):
        checksum_validator = rift.package.package.PackageChecksumValidator(self.log)

        try:
            checksum_validator.validate(package)
        except rift.package.package.PackageFileChecksumError as e:
            raise MessageException(UpdateChecksumMismatch(e.filename)) from e
        except rift.package.package.PackageValidationError as e:
            raise MessageException(UpdateUnreadablePackage()) from e

    def update_descriptors(self, package):
        descriptor_msg = package.descriptor_msg

        self.log.message(UpdateDescriptorUpdate())

        try:
            self.onboarder.update(descriptor_msg)
        except onboard.UpdateError as e:
            raise MessageException(UpdateDescriptorError(package.descriptor_file)) from e


class OnboardPackage(threading.Thread):
    def __init__(self, log, filename, boundary, pkg_id, auth,
                 onboarder, uploader, package_store_map):
        super().__init__()
        self.log = log
        self.filename = filename
        self.boundary = boundary
        self.pkg_id = pkg_id
        self.auth = auth
        self.onboarder = onboarder
        self.uploader = uploader
        self.package_store_map = package_store_map

        self.io_loop = tornado.ioloop.IOLoop.current()

    def _onboard_package(self):
        # Extract package could return multiple packages if
        # the package is converted
        for pkg in self.extract_package():
            with pkg as temp_package:
                self.validate_package(temp_package)
                stored_package = self.store_package(temp_package)

                try:
                    self.extract_charms(stored_package)
                    self.extract_scripts(stored_package)
                    self.extract_configs(stored_package)
                    self.extract_icons(stored_package)

                    self.upload_images(stored_package)
                    self.onboard_descriptors(stored_package)

                except Exception:
                    self.delete_stored_package(stored_package)
                    raise

    def run(self):
        try:
            self._onboard_package()
            self.log.message(OnboardSuccess())

        except MessageException as e:
            self.log.message(e.msg)
            self.log.message(OnboardFailure())

        except Exception as e:
            self.log.exception(e)
            if str(e):
                self.log.message(OnboardError(str(e)))
            self.log.message(OnboardFailure())

    def extract_package(self):
        """Extract multipart message from tarball"""
        extractor = extract.UploadPackageExtractor(self.log)
        file_backed_packages = extractor.extract_package_from_upload(
                self.filename, self.boundary, self.pkg_id
                )

        return file_backed_packages

    def get_package_store(self, package):
        return self.package_store_map[package.descriptor_type]

    def store_package(self, package):
        store = self.get_package_store(package)

        try:
            store.store_package(package)
        except rift.package.store.PackageExistsError as e:
            #TODO: The package store needs to stay in sync with configured
            #descriptors (descriptor delete -> stored package deletion).  Until
            #that happens, we should just update the stored package.
            #raise MessageException(OnboardDescriptorExistsError(package.descriptor_id)) from e
            store.update_package(package)

        stored_package = store.get_package(package.descriptor_id)

        return stored_package

    def delete_stored_package(self, package):
        store = self.get_package_store(package)
        try:
            store.delete_package(package)
        except Exception as e:
            self.log.warning("Failed to delete package from store: %s", str(e))

    def upload_images(self, package):
        image_names = rift.package.image.get_package_image_files(package)
        image_file_map = rift.package.image.get_package_image_files(package)
        name_hdl_map = {name: package.open(image_file_map[name]) for name in image_file_map}
        try:
            self.uploader.upload_images(name_hdl_map)

        except image.ImageUploadError as e:
            raise MessageException(OnboardImageUploadError()) from e

        finally:
            for hdl in name_hdl_map.values():
                hdl.close()

    def extract_charms(self, package):
        try:
            charm_extractor = rift.package.charm.PackageCharmExtractor(self.log)
            charm_extractor.extract_charms(package)
        except rift.package.charm.CharmExtractionError as e:
            raise MessageException(OnboardExtractionError()) from e

    def extract_scripts(self, package):
        try:
            script_extractor = rift.package.script.PackageScriptExtractor(self.log)
            script_extractor.extract_scripts(package)
        except rift.package.script.ScriptExtractionError as e:
            raise MessageException(OnboardExtractionError()) from e

    def extract_configs(self, package):
        try:
            config_extractor = rift.package.config.PackageConfigExtractor(self.log)
            config_extractor.extract_configs(package)
        except rift.package.config.ConfigExtractionError as e:
            raise MessageException(OnboardExtractionError()) from e

    def extract_icons(self, package):
        try:
            icon_extractor = rift.package.icon.PackageIconExtractor(self.log)
            icon_extractor.extract_icons(package)
        except rift.package.icon.IconExtractionError as e:
            raise MessageException(OnboardExtractionError()) from e

    def validate_package(self, package):
        checksum_validator = rift.package.package.PackageChecksumValidator(self.log)

        try:
            checksum_validator.validate(package)
        except rift.package.package.PackageFileChecksumError as e:
            raise MessageException(OnboardChecksumMismatch(e.filename)) from e
        except rift.package.package.PackageValidationError as e:
            raise MessageException(OnboardUnreadablePackage()) from e

    def onboard_descriptors(self, package):
        descriptor_msg = package.descriptor_msg

        self.log.message(OnboardDescriptorOnboard())

        try:
            self.onboarder.onboard(descriptor_msg)
        except onboard.OnboardError as e:
            raise MessageException(OnboardDescriptorError(package.descriptor_file)) from e


class UploaderApplication(tornado.web.Application):
    def __init__(self, tasklet):
        self.tasklet = tasklet
        self.accounts = []
        self.messages = collections.defaultdict(list)
        self.export_dir = os.path.join(os.environ['RIFT_ARTIFACTS'], 'launchpad/exports')

        manifest = tasklet.tasklet_info.get_pb_manifest()
        self.use_ssl = manifest.bootstrap_phase.rwsecurity.use_ssl
        self.ssl_cert = manifest.bootstrap_phase.rwsecurity.cert
        self.ssl_key = manifest.bootstrap_phase.rwsecurity.key

        self.uploader = image.ImageUploader(self.log, self.accounts)
        self.onboarder = onboard.DescriptorOnboarder(
                self.log, "127.0.0.1", 8008, self.use_ssl, self.ssl_cert, self.ssl_key
                )
        self.package_store_map = {
                "vnfd": rift.package.store.VnfdPackageFilesystemStore(self.log),
                "nsd": rift.package.store.NsdPackageFilesystemStore(self.log),
                }

        self.exporter = export.DescriptorPackageArchiveExporter(self.log)
        self.loop.create_task(export.periodic_export_cleanup(self.log, self.loop, self.export_dir))

        attrs = dict(log=self.log, loop=self.loop)

        export_attrs = attrs.copy()
        export_attrs.update({
            "store_map": self.package_store_map,
            "exporter": self.exporter,
            "catalog_map": {
                "vnfd": self.vnfd_catalog,
                "nsd": self.nsd_catalog
                }
            })

        super(UploaderApplication, self).__init__([
            (r"/api/update", UpdateHandler, attrs),
            (r"/api/upload", UploadHandler, attrs),

            (r"/api/upload/([^/]+)/state", UploadStateHandler, attrs),
            (r"/api/update/([^/]+)/state", UpdateStateHandler, attrs),
            (r"/api/export/([^/]+)/state", export.ExportStateHandler, attrs),

            (r"/api/export/(nsd|vnfd)$", export.ExportHandler, export_attrs),
            (r"/api/export/([^/]+.tar.gz)", tornado.web.StaticFileHandler, {
                "path": self.export_dir,
                }),
            (r"/api/export/([^/]+.zip)", tornado.web.StaticFileHandler, {
                "path": self.export_dir,
                }),
            ])

    @property
    def log(self):
        return self.tasklet.log

    @property
    def loop(self):
        return self.tasklet.loop

    def get_logger(self, transaction_id):
        return message.Logger(self.log, self.messages[transaction_id])

    def onboard(self, package, boundary, transaction_id, auth=None):
        log = message.Logger(self.log, self.messages[transaction_id])

        pkg_id = str(uuid.uuid1())
        OnboardPackage(
                log,
                package,
                boundary,
                pkg_id,
                auth,
                self.onboarder,
                self.uploader,
                self.package_store_map,
                ).start()

    def update(self, package, boundary, transaction_id, auth=None):
        log = message.Logger(self.log, self.messages[transaction_id])

        pkg_id = str(uuid.uuid1())
        UpdatePackage(
                log,
                package,
                boundary,
                pkg_id,
                auth,
                self.onboarder,
                self.uploader,
                self.package_store_map,
                ).start()

    @property
    def vnfd_catalog(self):
        return self.tasklet.vnfd_catalog

    @property
    def nsd_catalog(self):
        return self.tasklet.nsd_catalog
