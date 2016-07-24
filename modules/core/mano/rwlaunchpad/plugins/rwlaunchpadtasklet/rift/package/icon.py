
import re
import os.path

from . import package

class IconExtractionError(Exception):
    pass


class PackageIconExtractor(object):
    """ This class extracts icons to a known location for the UI to access """

    DEFAULT_INSTALL_DIR = os.path.join(
            os.environ["RIFT_INSTALL"],
            "usr/share/rw.ui/skyquake/plugins/composer/public/assets/logos"
            )

    ICON_REGEX = "{prefix}/?icons/(?P<icon_name>[^/]+)$"

    def __init__(self, log, install_dir=DEFAULT_INSTALL_DIR):
        self._log = log
        self._install_dir = install_dir

    def _get_rel_dest_path(self, descriptor_type, descriptor_id, icon_name):
        dest_path = os.path.join(self._install_dir, descriptor_type, descriptor_id, icon_name)
        return dest_path

    @classmethod
    def package_icon_files(cls, package):
        icon_file_map = {}

        for file_name in package.files:
            match = re.match(
                    cls.ICON_REGEX.format(prefix=package.prefix),
                    file_name,
                    )
            if match is None:
                continue

            icon_name = match.group("icon_name")

            icon_file_map[icon_name] = file_name

        return icon_file_map

    def get_extracted_icon_path(self, descriptor_type, descriptor_id, icon_name):
        return os.path.join(
                self._get_rel_dest_path(descriptor_type, descriptor_id, icon_name),
                )

    def extract_icons(self, pkg):
        """ Extract any icons in the package to the UI filesystem location

        Arguments:
            pkg - A DescriptorPackage
        """
        descriptor_id = pkg.descriptor_id
        icon_files = PackageIconExtractor.package_icon_files(pkg)

        for icon_name, icon_file in icon_files.items():
            dest_rel_path = self._get_rel_dest_path(pkg.descriptor_type, descriptor_id, icon_name)
            dest_path = os.path.join(self._install_dir, dest_rel_path)

            dest_dir = os.path.dirname(dest_path)
            try:
                os.makedirs(dest_dir, exist_ok=True)
            except OSError as e:
                self._log.error("Failed to create icon directory %s: %s", dest_dir, str(e))
                continue


            self._log.debug("Extracting %s icon to %s", icon_name, dest_path)
            try:
                pkg.extract_file(icon_file, dest_path)
            except package.ExtractError as e:
                self._log.error("Failed to extact icon %s: %s", icon_name, str(e))
                continue

