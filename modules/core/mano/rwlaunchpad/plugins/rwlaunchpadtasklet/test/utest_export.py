#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import argparse
import asyncio
import logging
import io
import os
import sys
import tarfile
import tempfile
import time
import unittest
import uuid
import xmlrunner

import rift.package.archive
import rift.package.charm
import rift.package.checksums
import rift.package.config
import rift.package.convert
import rift.package.icon
import rift.package.package
import rift.package.script
import rift.package.store

from rift.tasklets.rwlaunchpad import export

import gi
gi.require_version('RwVnfdYang', '1.0')
from gi.repository import (
        RwVnfdYang,
        VnfdYang,
        )

import utest_package


class TestExport(utest_package.PackageTestCase):
    def setUp(self):
        super().setUp()
        self._exporter = export.DescriptorPackageArchiveExporter(self._log)
        self._rw_vnfd_serializer = rift.package.convert.RwVnfdSerializer()
        self._vnfd_serializer = rift.package.convert.VnfdSerializer()

    def test_create_archive(self):
        rw_vnfd_msg = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd(
                id="new_id", name="new_name", description="new_description"
                )
        json_desc_str = self._rw_vnfd_serializer.to_json_string(rw_vnfd_msg)

        vnfd_package = self.create_vnfd_package()
        with io.BytesIO() as archive_hdl:
            archive = self._exporter.create_archive(
                    archive_hdl, vnfd_package, json_desc_str, self._rw_vnfd_serializer
                    )

            archive_hdl.seek(0)

            # Create a new read-only archive from the archive handle and a package from that archive
            archive = rift.package.archive.TarPackageArchive(self._log, archive_hdl)
            package = archive.create_package()

            # Ensure that the descriptor in the package has been overwritten
            self.assertEqual(package.descriptor_msg, rw_vnfd_msg)

    def test_export_package(self):
        rw_vnfd_msg = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd(
                id="new_id", name="new_name", description="new_description",
                meta="THIS FIELD IS NOT IN REGULAR VNFD"
                )
        vnfd_msg = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd()
        vnfd_msg.from_dict(rw_vnfd_msg.as_dict(), ignore_missing_keys=True)

        self.assertNotEqual(rw_vnfd_msg, vnfd_msg)

        json_desc_str = self._rw_vnfd_serializer.to_json_string(rw_vnfd_msg)

        with tempfile.TemporaryDirectory() as tmp_dir:
            vnfd_package = self.create_vnfd_package()
            pkg_id = str(uuid.uuid4())
            exported_path = self._exporter.export_package(
                    vnfd_package, tmp_dir, pkg_id, json_desc_str, self._vnfd_serializer
                    )

            self.assertTrue(os.path.isfile(exported_path))
            self.assertTrue(tarfile.is_tarfile(exported_path))

            with open(exported_path, "rb") as archive_hdl:
                archive = rift.package.archive.TarPackageArchive(self._log, archive_hdl)
                package = archive.create_package()

                self.assertEqual(package.descriptor_msg, vnfd_msg)

    def test_export_cleanup(self):
        loop = asyncio.get_event_loop()
        with tempfile.TemporaryDirectory() as tmp_dir:
            archive_files = [tempfile.mkstemp(dir=tmp_dir, suffix=".tar.gz")[1] for _ in range(2)]

            # Set the mtime on only one of the files to test the min_age_secs argument
            times = (time.time(), time.time() - 10)
            os.utime(archive_files[0], times)

            task = loop.create_task(
                    export.periodic_export_cleanup(
                        self._log, loop, tmp_dir, period_secs=.01, min_age_secs=5
                        )
                    )
            loop.run_until_complete(asyncio.sleep(.05, loop=loop))

            if task.done() and task.exception() is not None:
                raise task.exception()

            self.assertFalse(task.done())

            self.assertFalse(os.path.exists(archive_files[0]))
            self.assertTrue(os.path.exists(archive_files[1]))

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
