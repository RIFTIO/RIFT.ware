
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import io
import os.path
import stat
import time
import uuid

import tornado.web

import rift.package.archive
import rift.package.checksums
import rift.package.package
import rift.package.store
import rift.package.image

from . import state
from . import message
from . import tosca

import gi
gi.require_version('NsdYang', '1.0')
gi.require_version('VnfdYang', '1.0')

from gi.repository import (
        NsdYang,
        VnfdYang,
        )


class ExportStart(message.StatusMessage):
    def __init__(self):
        super().__init__("export-started", "export process started")


class ExportSuccess(message.StatusMessage):
    def __init__(self):
        super().__init__("export-success", "export process successfully completed")


class ExportFailure(message.StatusMessage):
    def __init__(self):
        super().__init__("export-failure", "export process failed")


class ExportError(message.ErrorMessage):
    def __init__(self, msg):
        super().__init__("update-error", msg)


class ExportSingleDescriptorOnlyError(ExportError):
    def __init__(self):
        super().__init__("Only a single descriptor can be exported")


class ArchiveExportError(Exception):
    pass


class DescriptorPackageArchiveExporter(object):
    def __init__(self, log):
        self._log = log

    def _create_archive_from_package(self, archive_hdl, package, open_fn):
        orig_open = package.open
        try:
            package.open = open_fn
            archive = rift.package.archive.TarPackageArchive.from_package(
                    self._log, package, archive_hdl
                    )
            return archive
        finally:
            package.open = orig_open

    def create_archive(self, archive_hdl, package, desc_json_str, serializer):
        """ Create a package archive from an existing package, descriptor messages,
            and a destination serializer.

        In order to stay flexible with the package directory structure and
        descriptor format, attempt to "augment" the onboarded package with the
        updated descriptor in the original format.  If the original package
        contained a checksum file, then recalculate the descriptor checksum.

        Arguments:
            archive_hdl - An open file handle with 'wb' permissions
            package - A DescriptorPackage instance
            desc_json_str - A descriptor (e.g. nsd, vnfd) protobuf message
            serializer - A destination serializer (e.g. VnfdSerializer)

        Returns:
            A TarPackageArchive

        Raises:
            ArchiveExportError - The exported archive failed to create

        """
        new_desc_msg = serializer.from_file_hdl(io.BytesIO(desc_json_str.encode()), ".json")
        _, dest_ext = os.path.splitext(package.descriptor_file)
        new_desc_hdl = io.BytesIO(serializer.to_string(new_desc_msg, dest_ext).encode())
        descriptor_checksum = rift.package.checksums.checksum(new_desc_hdl)

        checksum_file = None
        try:
            checksum_file = rift.package.package.PackageChecksumValidator.get_package_checksum_file(
                    package
                    )

        except FileNotFoundError:
            pass

        # Since we're going to intercept the open function to rewrite the descriptor
        # and checksum, save a handle to use below
        open_fn = package.open

        def create_checksum_file_hdl():
            with open_fn(checksum_file) as checksum_hdl:
                archive_checksums = rift.package.checksums.ArchiveChecksums.from_file_desc(
                        checksum_hdl
                        )

            archive_checksums[package.descriptor_file] = descriptor_checksum

            checksum_hdl = io.BytesIO(archive_checksums.to_string().encode())
            return checksum_hdl

        def open_wrapper(rel_path):
            """ Wraps the package open in order to rewrite the descriptor file and checksum """
            if rel_path == package.descriptor_file:
                return new_desc_hdl

            elif rel_path == checksum_file:
                return create_checksum_file_hdl()

            return open_fn(rel_path)

        archive = self._create_archive_from_package(archive_hdl, package, open_wrapper)

        return archive

    def export_package(self, package, export_dir, file_id, json_desc_str, dest_serializer):
        """ Export package as an archive to the export directory

        Arguments:
            package - A DescriptorPackage instance
            export_dir - The directory to export the package archive to
            file_id - A unique file id to name the archive as (i.e. <file_id>.tar.gz)
            json_desc_str - A descriptor (e.g. nsd, vnfd) json message string
            dest_serializer - A destination serializer (e.g. VnfdSerializer)

        Returns:
            The created archive path

        Raises:
            ArchiveExportError - The exported archive failed to create
        """
        try:
            os.makedirs(export_dir, exist_ok=True)
        except FileExistsError:
            pass

        archive_path = os.path.join(export_dir, file_id + ".tar.gz")
        with open(archive_path, 'wb') as archive_hdl:
            try:
                self.create_archive(
                    archive_hdl, package, json_desc_str, dest_serializer
                    )
            except Exception as e:
                os.remove(archive_path)
                msg = "Failed to create exported archive"
                self._log.error(msg)
                raise ArchiveExportError(msg) from e

        return archive_path


class ExportHandler(tornado.web.RequestHandler):
    def options(self, *args, **kargs):
        pass

    def set_default_headers(self):
        self.set_header('Access-Control-Allow-Origin', '*')
        self.set_header('Access-Control-Allow-Headers',
                        'Content-Type, Cache-Control, Accept, X-Requested-With, Authorization')
        self.set_header('Access-Control-Allow-Methods', 'POST, GET, PUT, DELETE')

    def initialize(self, log, loop, store_map, exporter, catalog_map):
        self.loop = loop
        self.transaction_id = str(uuid.uuid4())
        self.log = message.Logger(
                log,
                self.application.messages[self.transaction_id],
                )
        self.store_map = store_map
        self.exporter = exporter
        self.catalog_map = catalog_map

    def get(self, desc_type):
        if desc_type not in self.catalog_map:
            raise tornado.web.HTTPError(400, "unknown descriptor type: {}".format(desc_type))

        self.log.message(ExportStart())

        # Parse the IDs
        ids_query = self.get_query_argument("ids")
        ids = [id.strip() for id in ids_query.split(',')]
        if len(ids) != 1:
            raise message.MessageException(ExportSingleDescriptorOnlyError)
        desc_id = ids[0]

        # Get the schema for exporting
        schema = self.get_query_argument("schema")

        catalog = self.catalog_map[desc_type]

        if desc_id not in catalog:
            raise tornado.web.HTTPError(400, "unknown descriptor id: {}".format(desc_id))

        desc_msg = catalog[desc_id]

        if schema == 'tosca':
            self.export_tosca(schema, desc_type, desc_id, desc_msg)
        else:
            self.export_rift(schema, desc_type, desc_id, desc_msg)

        self.log.message(ExportSuccess())

        self.write(tornado.escape.json_encode({
            "transaction_id": self.transaction_id,
                }))

    def export_rift(self, schema, desc_type, desc_id, desc_msg):
        convert = rift.package.convert
        schema_serializer_map = {
                "rift": {
                    "vnfd": convert.RwVnfdSerializer,
                    "nsd": convert.RwNsdSerializer,
                    },
                "mano": {
                    "vnfd": convert.VnfdSerializer,
                    "nsd": convert.NsdSerializer,
                    }
                }

        if schema not in schema_serializer_map:
            raise tornado.web.HTTPError(400, "unknown schema: {}".format(schema))

        if desc_type not in schema_serializer_map[schema]:
            raise tornado.web.HTTPError(400, "unknown descriptor type: {}".format(desc_type))

        # Use the rift superset schema as the source
        src_serializer = schema_serializer_map["rift"][desc_type]()

        dest_serializer = schema_serializer_map[schema][desc_type]()

        package_store = self.store_map[desc_type]

        # Attempt to get the package from the package store
        # If that fails, create a temporary package using the descriptor only
        try:
            package = package_store.get_package(desc_id)
        except rift.package.store.PackageNotFoundError:
            self.log.debug("stored package not found.  creating package from descriptor config")

            desc_yaml_str = src_serializer.to_yaml_string(desc_msg)
            with io.BytesIO(desc_yaml_str.encode()) as hdl:
                hdl.name = "{}__{}.yaml".format(desc_msg.id, desc_type)
                package = rift.package.package.DescriptorPackage.from_descriptor_file_hdl(
                    self.log, hdl
                    )

        self.exporter.export_package(
                package=package,
                export_dir=self.application.export_dir,
                file_id=self.transaction_id,
                json_desc_str=src_serializer.to_json_string(desc_msg),
                dest_serializer=dest_serializer,
                )

    def export_tosca(self, schema, desc_type, desc_id, desc_msg):
        convert = rift.package.convert

        if schema != "tosca":
            raise tornado.web.HTTPError(400,
                                        "unknown schema: {}".format(schema))

        if desc_type != "nsd":
            raise tornado.web.HTTPError(
                400,
                "NSD need to passed to generate TOSCA: {}".format(desc_type))

        def get_pkg_from_store(id_, type_):
            package = None
            # Attempt to get the package from the package store
            try:
                package_store = self.store_map[type_]
                package = package_store.get_package(id_)

            except rift.package.store.PackageNotFoundError:
                self.log.debug("stored package not found for {}.".format(id_))
            except rift.package.store.PackageStoreError:
                self.log.debug("stored package error for {}.".format(id_))

            return package

        pkg = tosca.ExportTosca()

        # Add NSD and related descriptors for exporting
        nsd_id = pkg.add_nsd(desc_msg, get_pkg_from_store(desc_id, "nsd"))

        catalog = self.catalog_map["vnfd"]
        for const_vnfd in desc_msg.constituent_vnfd:
            vnfd_id = const_vnfd.vnfd_id_ref
            if vnfd_id in catalog:
                pkg.add_vnfd(nsd_id,
                             catalog[vnfd_id],
                             get_pkg_from_store(vnfd_id, "vnfd"))
            else:
                raise tornado.web.HTTPError(
                    400,
                    "Unknown VNFD descriptor {} for NSD {}".
                    format(vnfd_id, nsd_id))

        # Create the archive.
        pkg.create_archive(self.transaction_id,
                           dest=self.application.export_dir)


class ExportStateHandler(state.StateHandler):
    STARTED = ExportStart
    SUCCESS = ExportSuccess
    FAILURE = ExportFailure


@asyncio.coroutine
def periodic_export_cleanup(log, loop, export_dir, period_secs=10 * 60, min_age_secs=30 * 60):
    """ Periodically cleanup old exported archives (.tar.gz files) in export_dir

    Arguments:
        log - A Logger instance
        loop - A asyncio event loop
        export_dir - The directory to cleanup old archives in
        period_secs - The number of seconds between clean ups
        min_age_secs - The minimum age of a archive to be eligible for cleanup

    """
    log.debug("Starting  periodic export cleaning for export directory: %s", export_dir)

    # Create export dir if not created yet
    if not os.path.exists(export_dir):
        os.makedirs(export_dir)

    while True:
        for file_name in os.listdir(export_dir):
            if not file_name.endswith(".tar.gz"):
                continue

            file_path = os.path.join(export_dir, file_name)

            try:
                file_stat = os.stat(file_path)
            except OSError as e:
                log.warning("Could not stat old exported archive: %s", str(e))
                continue

            file_age = time.time() - file_stat[stat.ST_MTIME]

            if file_age < min_age_secs:
                continue

            log.debug("Cleaning up old exported archive: %s", file_path)

            try:
                os.remove(file_path)
            except OSError as e:
                log.warning("Failed to remove old exported archive: %s", str(e))

        yield from asyncio.sleep(period_secs, loop=loop)
