import re
import os.path

from . import package


class CharmExtractionError(Exception):
    pass


class PackageCharmExtractor(object):
    """ This class is reponsible for extracting charms to the correct directory

    In order to remain compatible with the existing Jujuclient, we extract the charms
    to a known location (RIFT-13282)
    """
    DEFAULT_INSTALL_DIR = os.path.join(
            os.environ["RIFT_ARTIFACTS"],
            "launchpad"
            )

    CHARM_REGEX = "{prefix}charms/(trusty/)?(?P<charm_name>[^/]+)$"

    def __init__(self, log, install_dir=DEFAULT_INSTALL_DIR):
        self._log = log
        self._install_dir = install_dir

    def _get_rel_dest_path(self, descriptor_id, charm_name):
        dest_rel_path = "libs/{}/charms/trusty/{}".format(descriptor_id, charm_name)
        dest_path = os.path.join(self._install_dir, dest_rel_path)
        return dest_path

    @classmethod
    def charm_dir_map(cls, package):
        charm_map = {}
        regex = cls.CHARM_REGEX.format(prefix=package.prefix)

        for dir_name in package.dirs:
            match = re.match(
                    cls.CHARM_REGEX.format(prefix=package.prefix), dir_name,
                    )
            if match is None:
                continue

            charm_name = match.group("charm_name")
            if charm_name == "trusty":
                continue

            charm_map[charm_name] = dir_name

        return charm_map

    def get_extracted_charm_dir(self, package_id, charm_name):
        return os.path.join(
                self._get_rel_dest_path(package_id, charm_name),
                )

    def extract_charms(self, pkg):
        """ Extract charms contained within the DescriptorPackage
        to the known charm directory.

        Arguments:
            pkg - The descriptor package that MAY contain charm directories

        Raises:
            CharmExtractionError - Charms in the package failed to get extracted
        """
        descriptor_id = pkg.descriptor_id
        charm_dir_map = PackageCharmExtractor.charm_dir_map(pkg)

        for charm_name, charm_dir in charm_dir_map.items():
            dest_rel_path = self._get_rel_dest_path(descriptor_id, charm_name)
            dest_path = os.path.join(self._install_dir, dest_rel_path)

            self._log.debug("Extracting %s charm to %s", charm_name, dest_path)
            try:
                pkg.extract_dir(charm_dir, dest_path)
            except package.ExtractError as e:
                raise CharmExtractionError("Failed to extract charm %s" % charm_name) from e
