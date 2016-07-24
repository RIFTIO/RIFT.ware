#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import logging
import io
import json
import os
import sys
import tarfile
import tempfile
import unittest
import xmlrunner
import yaml

import rift.package.archive
import rift.package.package
import rift.package.charm
import rift.package.icon
import rift.package.script
import rift.package.config
import rift.package.store
import rift.package.checksums


import gi
gi.require_version('RwpersonDbYang', '1.0')
gi.require_version('RwYang', '1.0')

from gi.repository import (
        RwpersonDbYang,
        RwYang,
        )


nsd_yaml = b"""nsd:nsd-catalog:
  nsd:nsd:
  - nsd:id: gw_corpA
    nsd:name: gw_corpA
    nsd:description: Gateways to access as corpA to PE1 and PE2
"""

vnfd_yaml = b"""vnfd:vnfd-catalog:
  vnfd:vnfd:
  - vnfd:id: gw_corpA_vnfd
    vnfd:name: gw_corpA_vnfd
    vnfd:description: Gateways to access as corpA to PE1 and PE2
"""

nsd_filename = "gw_corpA__nsd.yaml"
vnfd_filename = "gw_corpA__vnfd.yaml"


def file_hdl_md5(file_hdl):
    return rift.package.checksums.checksum(file_hdl)


class ArchiveTestCase(unittest.TestCase):
    def setUp(self):
        self._log = logging.getLogger()

        self._tar_file_hdl = io.BytesIO()
        self._tar = tarfile.open(fileobj=self._tar_file_hdl, mode="w|gz")

        self._nsd_yaml_hdl = io.BytesIO(nsd_yaml)
        self._vnfd_yaml_hdl = io.BytesIO(vnfd_yaml)

    def tearDown(self):
        self._nsd_yaml_hdl.close()
        self._vnfd_yaml_hdl.close()
        self._tar.close()
        self._tar_file_hdl.close()

    def create_tar_package_archive(self):
        self._tar.close()
        self._tar_file_hdl.flush()
        self._tar_file_hdl.seek(0)
        archive = rift.package.package.TarPackageArchive(
                log=self._log,
                tar_file_hdl=self._tar_file_hdl,
                )

        return archive

    def add_tarinfo(self, name, file_hdl, mode=0o777):
        tarinfo = tarfile.TarInfo(name)
        tarinfo.size = len(file_hdl.read())
        assert tarinfo.size > 0
        file_hdl.seek(0)
        self._tar.addfile(tarinfo, file_hdl)

    def add_tarinfo_dir(self, name):
        tarinfo = tarfile.TarInfo(name)
        tarinfo.type = tarfile.DIRTYPE
        self._tar.addfile(tarinfo)

    def add_nsd_yaml(self):
        self.add_tarinfo(nsd_filename, io.BytesIO(nsd_yaml))

    def add_vnfd_yaml(self):
        self.add_tarinfo(vnfd_filename, io.BytesIO(vnfd_yaml))


class PackageTestCase(ArchiveTestCase):
    def create_nsd_package(self):
        self.add_nsd_yaml()
        archive = self.create_tar_package_archive()
        package = archive.create_package()

        return package

    def create_vnfd_package(self):
        self.add_vnfd_yaml()
        archive = self.create_tar_package_archive()
        package = archive.create_package()

        return package


class TestCreateArchive(ArchiveTestCase):
    def test_create_tar_archive(self):
        self.add_nsd_yaml()
        archive = self.create_tar_package_archive()
        self.assertEquals(set(archive.filenames), {nsd_filename})

    def test_nsd_tar_archive(self):
        #Write the NSD YAML to the tar file
        self.add_nsd_yaml()

        archive = self.create_tar_package_archive()
        with archive.open_file(nsd_filename) as nsd_hdl:
            nsd_bytes = nsd_hdl.read()

        self.assertEquals(nsd_bytes, nsd_yaml)


class TestPackage(PackageTestCase):
    def create_vnfd_package_archive(self, package, hdl):
        # Create an archive from a package
        archive = rift.package.archive.TarPackageArchive.from_package(
                self._log, package, hdl,
                )
        # Closing the archive writes any closing bytes to the file handle
        archive.close()
        hdl.seek(0)

        return archive

    def test_create_nsd_package_from_archive(self):
        package = self.create_nsd_package()
        self.assertTrue(isinstance(package, rift.package.package.NsdPackage))

        json_str = package.json_descriptor
        desc_dict = json.loads(json_str)
        self.assertIn("nsd:nsd-catalog", desc_dict)

    def test_create_vnfd_package_from_archive(self):
        package = self.create_vnfd_package()
        self.assertTrue(isinstance(package, rift.package.package.VnfdPackage))

        json_str = package.json_descriptor
        desc_dict = json.loads(json_str)
        self.assertIn("vnfd:vnfd-catalog", desc_dict)

    def test_create_vnfd_archive_from_package(self):
        package = self.create_vnfd_package()
        hdl = io.BytesIO()
        self.create_vnfd_package_archive(package, hdl)

        # Ensure that the archive created was valid
        with tarfile.open(fileobj=hdl, mode='r|gz'):
            pass

    def test_round_trip_vnfd_package_from_archive(self):
        package = self.create_vnfd_package()
        hdl = io.BytesIO()
        self.create_vnfd_package_archive(package, hdl)

        archive = rift.package.archive.TarPackageArchive(self._log, hdl)
        def md5(file_hdl):
            return rift.package.checksums.checksum(file_hdl)

        # Create the package from the archive and validate file checksums and modes
        new_package = archive.create_package()

        self.assertEqual(package.files, new_package.files)
        self.assertEqual(type(package), type(new_package))

        for filename in package.files:
            pkg_file = package.open(filename)
            new_pkg_file = new_package.open(filename)
            self.assertEqual(md5(pkg_file), md5(new_pkg_file))

    def test_create_nsd_package_from_file(self):
        nsd_file_name = "asdf_nsd.yaml"
        hdl = io.BytesIO(nsd_yaml)
        hdl.name = nsd_file_name

        package = rift.package.package.DescriptorPackage.from_descriptor_file_hdl(
                self._log, hdl
                )
        self.assertTrue(isinstance(package, rift.package.package.NsdPackage))

        with package.open(nsd_file_name) as nsd_hdl:
            nsd_data = nsd_hdl.read()
            self.assertEquals(yaml.load(nsd_data), yaml.load(nsd_yaml))

    def test_create_vnfd_package_from_file(self):
        vnfd_file_name = "asdf_vnfd.yaml"
        hdl = io.BytesIO(vnfd_yaml)
        hdl.name = vnfd_file_name

        package = rift.package.package.DescriptorPackage.from_descriptor_file_hdl(
                self._log, hdl
                )
        self.assertTrue(isinstance(package, rift.package.package.VnfdPackage))

        with package.open(vnfd_file_name) as vnfd_hdl:
            vnfd_data = vnfd_hdl.read()
            self.assertEquals(yaml.load(vnfd_data), yaml.load(vnfd_yaml))


class TestPackageCharmExtractor(PackageTestCase):
    def add_charm_dir(self, charm_name):
        charm_dir = "charms/trusty/{}".format(charm_name)
        charm_file = "{}/actions.yaml".format(charm_dir)
        charm_text = b"THIS IS A FAKE CHARM"
        self.add_tarinfo_dir(charm_dir)
        self.add_tarinfo(charm_file, io.BytesIO(charm_text))

    def test_extract_charm(self):
        charm_name = "charm_a"
        self.add_charm_dir(charm_name)
        package = self.create_vnfd_package()
        with tempfile.TemporaryDirectory() as tmp_dir:
            extractor = rift.package.charm.PackageCharmExtractor(self._log, tmp_dir)
            extractor.extract_charms(package)

            charm_dir = extractor.get_extracted_charm_dir(package.descriptor_id, charm_name)
            self.assertTrue(os.path.exists(charm_dir))
            self.assertTrue(os.path.isdir(charm_dir))


class TestPackageIconExtractor(PackageTestCase):
    def add_icon_file(self, icon_name):
        icon_file = "icons/{}".format(icon_name)
        icon_text = b"png file bytes"
        self.add_tarinfo(icon_file, io.BytesIO(icon_text))

    def test_extract_icon(self):
        icon_name = "icon_a"
        self.add_icon_file(icon_name)
        package = self.create_vnfd_package()
        with tempfile.TemporaryDirectory() as tmp_dir:
            extractor = rift.package.icon.PackageIconExtractor(self._log, tmp_dir)
            extractor.extract_icons(package)

            icon_file = extractor.get_extracted_icon_path(
                    package.descriptor_type, package.descriptor_id, icon_name
                    )
            self.assertTrue(os.path.exists(icon_file))
            self.assertTrue(os.path.isfile(icon_file))


class TestPackageScriptExtractor(PackageTestCase):
    def add_script_file(self, script_name):
        script_file = "scripts/{}".format(script_name)
        script_text = b"""#!/usr/bin/python
        print("hi")
        """
        self.add_tarinfo(script_file, io.BytesIO(script_text), mode=0o666)

    def test_extract_script(self):
        script_name = "add_corporation.py"
        self.add_script_file(script_name)
        package = self.create_vnfd_package()
        with tempfile.TemporaryDirectory() as tmp_dir:
            extractor = rift.package.script.PackageScriptExtractor(self._log, tmp_dir)
            extractor.extract_scripts(package)

            script_dir = extractor.get_extracted_script_path(package.descriptor_id, script_name)
            self.assertTrue(os.path.exists(script_dir))
            self.assertTrue(os.path.isfile(script_dir))


class TestPackageConfigExtractor(PackageTestCase):
    def add_ns_config_file(self, nsd_id):
        config_file = "ns_config/{}.yaml".format(nsd_id)
        config_text = b""" ns_config """
        self.add_tarinfo(config_file, io.BytesIO(config_text), mode=0o666)

        return config_file

    def add_vnf_config_file(self, vnfd_id, member_vnf_index):
        config_file = "vnf_config/{}_{}.yaml".format(vnfd_id, member_vnf_index)
        config_text = b""" vnf_config """
        self.add_tarinfo(config_file, io.BytesIO(config_text), mode=0o666)

        return config_file

    def test_extract_config(self):
        ns_config_file = self.add_ns_config_file("nsd_id")
        vnf_config_file = self.add_vnf_config_file("vnfd_id", 1)
        package = self.create_nsd_package()
        with tempfile.TemporaryDirectory() as tmp_dir:
            extractor = rift.package.config.PackageConfigExtractor(self._log, tmp_dir)
            extractor.extract_configs(package)

            dest_ns_config_file = extractor.get_extracted_config_path(package.descriptor_id, ns_config_file)
            dest_vnf_config_file = extractor.get_extracted_config_path(package.descriptor_id, vnf_config_file)
            self.assertTrue(os.path.isfile(dest_ns_config_file))
            self.assertTrue(os.path.isfile(dest_vnf_config_file))


class TestPackageValidator(PackageTestCase):
    def setUp(self):
        super().setUp()
        self._validator = rift.package.package.PackageChecksumValidator(self._log)

    def create_checksum_file(self, file_md5_map):
        checksum_hdl = io.BytesIO()
        for file_name, md5 in file_md5_map.items():
            checksum_hdl.write("{}  {}\n".format(md5, file_name).encode())

        checksum_hdl.flush()
        checksum_hdl.seek(0)

        self.add_tarinfo("checksums.txt", checksum_hdl)
        self._tar.addfile(tarfile.TarInfo(), checksum_hdl)

    def create_nsd_package_with_checksum(self):
        self.create_checksum_file(
                {nsd_filename: file_hdl_md5(io.BytesIO(nsd_yaml))}
                )
        package = self.create_nsd_package()
        return package

    def test_package_no_checksum(self):
        package = self.create_nsd_package()

        # For now, a missing checksum file will be supported.
        # No files will be validated.
        validated_files = self._validator.validate(package)
        self.assertEquals(validated_files, [])

    def test_package_with_checksum(self):
        package = self.create_nsd_package_with_checksum()
        validated_files = self._validator.validate(package)
        self.assertEquals(validated_files, [nsd_filename])


class TestPackageStore(PackageTestCase):
    def create_store(self, root_dir):
        store = rift.package.store.PackageFilesystemStore(self._log, root_dir)
        return store

    def create_and_store_package(self, store):
        package = self.create_nsd_package()
        store.store_package(package)

        return package

    def test_store_package(self):
        with tempfile.TemporaryDirectory() as root_dir:
            store = self.create_store(root_dir)
            package = self.create_and_store_package(store)
            new_package = store.get_package(package.descriptor_id)
            self.assertEquals(new_package.files, package.files)
            self.assertEquals(type(new_package), type(package))

    def test_store_reload_package(self):
        with tempfile.TemporaryDirectory() as root_dir:
            store = self.create_store(root_dir)
            package = self.create_and_store_package(store)

            new_store = self.create_store(root_dir)
            new_package = new_store.get_package(package.descriptor_id)

            self.assertEquals(new_package.files, package.files)
            self.assertEquals(type(new_package), type(package))

    def test_delete_package(self):
        with tempfile.TemporaryDirectory() as root_dir:
            store = self.create_store(root_dir)
            package = self.create_and_store_package(store)

            store.get_package(package.descriptor_id)
            store.delete_package(package)

            with self.assertRaises(rift.package.store.PackageStoreError):
                store.get_package(package.descriptor_id)

    def test_store_exist_package(self):
        with tempfile.TemporaryDirectory() as root_dir:
            store = self.create_store(root_dir)
            package = self.create_and_store_package(store)

            with self.assertRaises(rift.package.store.PackageStoreError):
                store.store_package(package)


class TestTemporaryPackage(PackageTestCase):
    def test_temp_package(self):
        self._tar_file_hdl = tempfile.NamedTemporaryFile(delete=False)
        self._tar = tarfile.open(fileobj=self._tar_file_hdl, mode="w|gz")

        self.assertTrue(os.path.exists(self._tar_file_hdl.name))

        package = self.create_nsd_package()
        with rift.package.package.TemporaryPackage(self._log, package, self._tar_file_hdl) as temp_pkg:
            self.assertTrue(package is temp_pkg)
            self.assertEquals(package.files, temp_pkg.files)

        self.assertFalse(os.path.exists(self._tar_file_hdl.name))


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
