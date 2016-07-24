import os
import shutil

from . import package


class PackageStoreError(Exception):
    pass


class PackageExistsError(PackageStoreError):
    pass


class PackageNotFoundError(PackageStoreError):
    pass


class PackageFilesystemStore(object):
    """ This class is able to store/retreive/delete DescriptorPackages on disk

    To insulate components from having to deal with accessing the filesystem directly
    to deal with onboarded packages, this class provides a convenient interface for
    storing, retreiving, deleting packages stored on disk.  The interfaces deal directly
    with DescriptorPackages so clients are abstracted from the actual location on disk.
    """

    def __init__(self, log, root_dir):
        self._log = log
        self._root_dir = root_dir
        self._package_dirs = {}

        self.refresh()

    def _get_package_dir(self, package_id):
        return os.path.join(self._root_dir, package_id)

    def _get_package_files(self, package_id):
        package_files = {}

        package_dir = self._get_package_dir(package_id)

        for dirpath, dirnames, filenames in os.walk(package_dir):
            for name in filenames:
                file_path = os.path.join(dirpath, name)
                file_rel_path = os.path.relpath(file_path, package_dir)
                package_files[file_rel_path] = file_path

        return package_files

    def refresh(self):
        """ Refresh the package index from disk  """
        if not os.path.exists(self._root_dir):
            self._package_dirs = {}
            return

        package_dirs = {}
        for package_id_dir in os.listdir(self._root_dir):
            try:
                package_dir_path = os.path.join(self._root_dir, package_id_dir)
                if not os.path.isdir(package_dir_path):
                    self._log.warning("Unknown file in package store: %s", package_dir_path)
                    continue

                files = os.listdir(package_dir_path)
                if len(files) == 0:
                    self._log.warning("Package directory %s is empty", package_dir_path)
                    continue

                package_id = os.path.basename(package_id_dir)
                package_dirs[package_id] = package_id_dir

            except OSError as e:
                self._log.warning("Failed to read packages from %s: %s",
                                  package_dir_path, str(e))

        self._package_dirs = package_dirs


    def get_package(self, package_id):
        """ Get a DescriptorPackage on disk from the package descriptor id

        Arguments:
            package_id - The DescriptorPackage.descriptor_id

        Returns:
            A DescriptorPackage instance of the correct type

        Raises:
            PackageStoreError- The package could not be retrieved
        """
        if package_id not in self._package_dirs:
            msg = "Package %s not found in %s" % (package_id, self._root_dir)
            raise PackageStoreError(msg)

        package_files = self._get_package_files(package_id)
        package_dir = self._get_package_dir(package_id)

        def do_open(pkg_file):
            pkg_path = os.path.join(package_dir, pkg_file)
            return open(pkg_path, "rb")

        pkg = package.DescriptorPackage.from_package_files(self._log, do_open, package_files)
        for pkg_file in package_files:
            pkg.add_file(pkg_file)

        return pkg

    def store_package(self, pkg):
        """ Store a DescriptorPackage to disk

        Arguments:
            pkg - A DescriptorPackage

        Raises:
            PackageStoreError - The package could not be stored
        """
        if pkg.descriptor_id in self._package_dirs:
            raise PackageExistsError("Package %s already exists", pkg.descriptor_id)

        package_dir = self._get_package_dir(pkg.descriptor_id)

        try:
            os.makedirs(package_dir, exist_ok=True)
        except OSError as e:
            raise PackageStoreError("Failed to create package dir: %s", package_dir) from e

        try:
            self._log.debug("Storing package in dir %s", package_dir)
            pkg.extract(package_dir)
            self._log.debug("Package stored in dir %s", package_dir)
        except pkg.PackageError as e:
            raise PackageStoreError("Failed to extract package to package store") from e

        self._package_dirs[pkg.descriptor_id] = package_dir

    def delete_package(self, pkg):
        """ Delete a stored DescriptorPackage

        Arguments:
            pkg - A DescriptorPackage

        Raises:
            PackageNotFoundError - The package could not be found
            PackageStoreError - The package could not be deleted
        """

        if pkg.descriptor_id not in self._package_dirs:
            raise PackageNotFoundError("Package %s does not exists", pkg.descriptor_id)

        package_dir = self._get_package_dir(pkg.descriptor_id)
        try:
            if os.path.exists(package_dir):
                self._log.debug("Removing stored package directory: %s", package_dir)
                shutil.rmtree(package_dir)
        except OSError as e:
            raise PackageStoreError("Failed to remove stored package directory: %s", package_dir) from e

        del self._package_dirs[pkg.descriptor_id]

    def update_package(self, pkg):
        """ Update a stored DescriptorPackage

        Arguments:
            pkg - A DescriptorPackage

        Raises:
            PackageNotFoundError - The package could not be found
            PackageStoreError - The package could not be deleted
        """
        self.delete_package(pkg)
        self.store_package(pkg)


class NsdPackageFilesystemStore(PackageFilesystemStore):
    DEFAULT_ROOT_DIR = os.path.join(
            os.environ["RIFT_ARTIFACTS"],
            "launchpad", "packages", "nsd"
            )

    def __init__(self, log, root_dir=DEFAULT_ROOT_DIR):
        super().__init__(log, root_dir)


class VnfdPackageFilesystemStore(PackageFilesystemStore):
    DEFAULT_ROOT_DIR = os.path.join(
            os.environ["RIFT_ARTIFACTS"],
            "launchpad", "packages", "vnfd"
            )

    def __init__(self, log, root_dir=DEFAULT_ROOT_DIR):
        super().__init__(log, root_dir)

