
import re
import os.path

from . import package


class ScriptExtractionError(Exception):
    pass


class PackageScriptExtractor(object):
    """ This class is reponsible for extracting scripts to the correct directory

    In order to remain compatible with the existing config manager, we extract the scripts
    to a known location (RIFT-13282)
    """
    DEFAULT_INSTALL_DIR = os.path.join(
            os.environ["RIFT_INSTALL"],
            "usr/bin"
            )

    SCRIPT_REGEX = "{prefix}/?scripts/(?P<script_name>[^/]+)$"

    def __init__(self, log, install_dir=DEFAULT_INSTALL_DIR):
        self._log = log
        self._install_dir = install_dir

    def _get_rel_dest_path(self, descriptor_id, script_name):
        dest_path = os.path.join(self._install_dir, script_name)
        return dest_path

    @classmethod
    def package_script_files(cls, package):
        script_file_map = {}

        for file_name in package.files:
            match = re.match(
                    cls.SCRIPT_REGEX.format(prefix=package.prefix),
                    file_name,
                    )
            if match is None:
                continue

            script_name = match.group("script_name")

            script_file_map[script_name] = file_name

        return script_file_map

    def get_extracted_script_path(self, package_id, script_name):
        return os.path.join(
                self._get_rel_dest_path(package_id, script_name),
                )

    def extract_scripts(self, pkg):
        descriptor_id = pkg.descriptor_id
        script_files = PackageScriptExtractor.package_script_files(pkg)

        for script_name, script_file in script_files.items():
            dest_path = self._get_rel_dest_path(descriptor_id, script_name)

            self._log.debug("Extracting %s script to %s", script_name, dest_path)
            try:
                pkg.extract_file(script_file, dest_path)
            except package.ExtractError as e:
                raise ScriptExtractionError("Failed to extract script %s" % script_name) from e
