import re
import os.path

from . import package


class ConfigExtractionError(Exception):
    pass


class PackageConfigExtractor(object):
    """ This class is reponsible for extracting config data to the correct directory

    In order to remain compatible with the existing ConfigManager, we extract the config
    to a known location (RIFT-13282)
    """
    DEFAULT_INSTALL_DIR = os.path.join(
            os.environ["RIFT_ARTIFACTS"],
            "launchpad"
            )

    CONFIG_REGEX = "{prefix}(ns_config|vnf_config)/(?P<config_name>[^/]+.yaml)$"

    def __init__(self, log, install_dir=DEFAULT_INSTALL_DIR):
        self._log = log
        self._install_dir = install_dir

    def _get_rel_dest_path(self, descriptor_id, config_name):
        dest_rel_path = "libs/{}/config/{}".format(descriptor_id, config_name)
        dest_path = os.path.join(self._install_dir, dest_rel_path)
        return dest_path

    @classmethod
    def package_config_files(cls, package):
        config_map = {}
        regex = cls.CONFIG_REGEX.format(prefix=package.prefix)

        for file_name in package.files:
            match = re.match(
                    cls.CONFIG_REGEX.format(prefix=package.prefix), file_name,
                    )
            if match is None:
                continue

            config_name = match.group("config_name")

            config_map[config_name] = file_name

        return config_map

    def get_extracted_config_path(self, package_id, config_name):
        return os.path.join(
                self._get_rel_dest_path(package_id, os.path.basename(config_name)),
                )

    def extract_configs(self, pkg):
        """ Extract any configuration files from the DescriptorPackage

        Arguments:
            pkg - A DescriptorPackage

        Raises:
            ConfigExtractionError - The configuration could not be extracted
        """
        descriptor_id = pkg.descriptor_id

        config_files = PackageConfigExtractor.package_config_files(pkg).items()
        for config_name, config_file in config_files:
            dest_rel_path = self._get_rel_dest_path(descriptor_id, config_name)
            dest_path = os.path.join(self._install_dir, dest_rel_path)

            self._log.debug("Extracting %s config to %s", config_name, dest_path)
            try:
                pkg.extract_file(config_file, dest_path)
            except package.ExtractError as e:
                raise ConfigExtractionError("Failed to extract config %s" % config_name) from e
