import io
import os
import re
import shutil
import tarfile

from . import checksums
from . import convert


class ArchiveError(Exception):
    pass


class ExtractError(Exception):
    pass


class PackageError(Exception):
    pass


class PackageValidationError(Exception):
    pass


class PackageFileChecksumError(PackageValidationError):
    def __init__(self, filename):
        self.filename = filename
        super().__init__("Checksum mismatch for {}".format(filename))


class DescriptorPackage(object):
    """ This class provides an base class for a descriptor package representing

    A descriptor package is a package which contains a single descriptor and any
    associated files (logos, charms, scripts, etc).  This package representation
    attempts to be agnostic as to where the package files are being stored
    (in memory, on disk, etc).

    The package provides a simple interface to interact with the files within the
    package and access the contained descriptor.
    """
    DESCRIPTOR_REGEX = r"{prefix}({descriptor_type}/[^/]*|[^/]*{descriptor_type})\.(xml|yml|yaml|json)$"

    def __init__(self, log, open_fn):
        self._log = log
        self._open_fn = open_fn

        self._package_file_mode_map = {}
        self._package_dirs = set()

    @property
    def prefix(self):
        """ Return the leading parent directories shared by all files in the package

        In order to remain flexible as to where tar was invoked to create the package,
        the prefix represents the common parent directory path which all files in the
        package have in common.
        """
        entries = list(self._package_file_mode_map) + list(self._package_dirs)

        if len(entries) > 1:
            prefix = os.path.commonprefix(entries)
            if prefix and not prefix.endswith("/"):
                prefix += "/"
        elif len(entries) == 1:
            entry = entries[0]
            if "/" in entry:
                prefix = os.path.dirname(entry) + "/"
            else:
                prefix = ""
        else:
            prefix = ""

        return prefix

    @property
    def files(self):
        """ Return all files (with the prefix) in the package """
        return list(self._package_file_mode_map)

    @property
    def dirs(self):
        """ Return all directories in the package """
        return list(self._package_dirs)

    @property
    def descriptor_type(self):
        """ A shorthand name for the type of descriptor (e.g. nsd)"""
        raise NotImplementedError("Subclass must implement this property")

    @property
    def serializer(self):
        """ An instance of convert.ProtoMessageSerializer """
        raise NotImplementedError("Subclass must implement this property")

    @property
    def descriptor_file(self):
        """ The descriptor file name (with prefix) """
        regex = self.__class__.DESCRIPTOR_REGEX.format(
                descriptor_type=self.descriptor_type,
                prefix=self.prefix,
                )
        desc_file = None
        for filename in self.files:
            if re.match(regex, filename):
                if desc_file is not None:
                    raise PackageError("Package contains more than one descriptor")
                desc_file = filename

        if desc_file is None:
            raise PackageError("Could not find descriptor file in package")

        return desc_file

    @property
    def descriptor_msg(self):
        """ The proto-GI descriptor message """
        filename = self.descriptor_file
        with self.open(filename) as hdl:
            _, ext = os.path.splitext(filename)
            nsd = self.serializer.from_file_hdl(hdl, ext)
            return nsd

    @property
    def json_descriptor(self):
        """  The JSON serialized descriptor message"""
        nsd = self.descriptor_msg
        return self.serializer.to_json_string(nsd)

    @property
    def descriptor_id(self):
        """  The descriptor id which uniquely identifies this descriptor in the system """
        if not self.descriptor_msg.has_field("id"):
            msg = "Descriptor must have an id field"
            self._log.error(msg)
            raise PackageError(msg)

        return self.descriptor_msg.id

    @classmethod
    def get_descriptor_patterns(cls):
        """ Returns a tuple of descriptor regex and Package Types  """
        package_types = (VnfdPackage, NsdPackage)
        patterns = []

        for pkg_cls in package_types:
            regex = cls.DESCRIPTOR_REGEX.format(
                    descriptor_type=pkg_cls.DESCRIPTOR_TYPE,
                    prefix=".*"
                    )

            patterns.append((regex, pkg_cls))

        return patterns

    @classmethod
    def from_package_files(cls, log, open_fn, files):
        """ Creates a new DescriptorPackage subclass instance from a list of files

        This classmethod detects the Package type from the package contents
        and returns a new Package instance.

        This will NOT subsequently add the files to the package so that must
        be done by the client

        Arguments:
            log - A logger
            open_fn - A function which can take a file name and mode and return
                      a file handle.
            files - A list of files which would be added to the package after
                    intantiation

        Returns:
            A new DescriptorPackage subclass of the correct type for the descriptor

        Raises:
            PackageError - Package type could not be determined from the list of files.
        """
        patterns = cls.get_descriptor_patterns()
        pkg_cls = None
        regexes = set()
        for name in files:
            for regex, cls in patterns:
                regexes.add(regex)
                if re.match(regex, name) is not None:
                    pkg_cls = cls
                    break

        if pkg_cls is None:
            log.error("No file in archive matched known descriptor formats: %s", regexes)
            raise PackageError("Could not determine package type from contents")

        package = pkg_cls(log, open_fn)
        return package

    @classmethod
    def from_descriptor_file_hdl(cls, log, file_hdl):
        """ Creates a new DescriptorPackage from a descriptor file handle

        The descriptor file is added to the package before returning.

        Arguments:
            log - A logger
            file_hdl - A file handle whose name attribute can be recognized as
                       particular descriptor type.

        Returns:
            A new DescriptorPackage subclass of the correct type for the descriptor

        Raises:
            PackageError - Package type could not be determined from the list of files.
            ValueError - file_hdl did not have a name attribute provided
        """

        package_types = (VnfdPackage, NsdPackage)
        filename_patterns = []
        for package_cls in package_types:
            filename_patterns.append(
                    (r".*{}.*".format(package_cls.DESCRIPTOR_TYPE), package_cls)
                    )

        if not hasattr(file_hdl, 'name'):
            raise ValueError("File descriptor must have a name attribute to create a descriptor package")

        # Iterate through the recognized patterns and assign files accordingly
        package_cls = None
        for pattern, cls in filename_patterns:
            if re.match(pattern, file_hdl.name):
                package_cls = cls
                break

        if not package_cls:
            raise PackageError("Could not determine package type from file name: %s" % file_hdl.name)

        _, ext = os.path.splitext(file_hdl.name)
        try:
            package_cls.SERIALIZER.from_file_hdl(file_hdl, ext)
        except convert.SerializationError as e:
            raise PackageError("Could not deserialize descriptor %s" % file_hdl.name) from e

        # Create a new file handle for each open call to prevent independent clients
        # from affecting each other
        file_hdl.seek(0)
        new_hdl = io.BytesIO(file_hdl.read())

        def do_open(file_path):
            assert file_path == file_hdl.name
            hdl = io.BytesIO(new_hdl.getvalue())
            return hdl

        desc_pkg = package_cls(log, do_open)
        desc_pkg.add_file(file_hdl.name)

        return desc_pkg

    def get_file_mode(self, pkg_file):
        """ Returns the file mode for the package file

        Arguments:
            pkg_file - A file name in the package

        Returns:
            The permission mode

        Raises:
            PackageError - The file does not exist in the package
        """
        try:
            return self._package_file_mode_map[pkg_file]
        except KeyError as e:
            msg = "Could not find package_file: %s" % pkg_file
            self._log.error(msg)
            raise PackageError(msg) from e

    def extract_dir(self, src_dir, dest_root_dir):
        """ Extract a specific directory contents to dest_root_dir

        Arguments:
            src_dir - A directory within the package (None means all files/directories)
            dest_root_dir - A directory to extract directory contents to

        Raises:
            ExtractError - Directory contents could not be extracted
        """
        if src_dir is not None and src_dir not in self._package_dirs:
            raise ExtractError("Could not find source dir: %s" % src_dir)

        for filename in self.files:
            if src_dir is not None and not filename.startswith(src_dir):
                continue

            # Copy the contents of the file to the correct path
            dest_file_path = os.path.join(dest_root_dir, filename)
            dest_dir_path = os.path.dirname(dest_file_path)
            if not os.path.exists(dest_dir_path):
                os.makedirs(dest_dir_path)

            with open(dest_file_path, 'wb') as dst_hdl:
                with self.open(filename) as src_hdl:
                    shutil.copyfileobj(src_hdl, dst_hdl, 10 * 1024 * 1024)

                    # Set the file mode to original
                    os.chmod(dest_file_path, self._package_file_mode_map[filename])

    def extract_file(self, src_file, dest_file):
        """ Extract a specific package file to dest_file

        The destination directory will be created if it does not exist.

        Arguments:
            src_file - A file within the package
            dest_file - A file path to extract file contents to

        Raises:
            ExtractError - Directory contents could not be extracted
        """
        if src_file not in self._package_file_mode_map:
            msg = "Could not find source file %s" % src_file
            self._log.error(msg)
            raise ExtractError(msg)

        # Copy the contents of the file to the correct path
        dest_dir_path = os.path.dirname(dest_file)
        if not os.path.isdir(dest_dir_path):
            os.makedirs(dest_dir_path)

        with open(dest_file, 'wb') as dst_hdl:
            with self.open(src_file) as src_hdl:
                shutil.copyfileobj(src_hdl, dst_hdl, 10 * 1024 * 1024)

                # Set the file mode to original
                os.chmod(dest_file, self._package_file_mode_map[src_file])

    def extract(self, dest_root_dir):
        """ Extract all package contents to a destination directory

        Arguments:
            dest_root_dir - The directory to extract package contents to

        Raises:
            NotADirectoryError - dest_root_dir is not a directory
        """
        if not os.path.isdir(dest_root_dir):
            raise NotADirectoryError(dest_root_dir)

        self.extract_dir(None, dest_root_dir)

    def open(self, rel_path):
        """ Open a file contained in the package in read-only, binary mode.

        Arguments:
            rel_path - The file path within the package

        Returns:
            A file-like object opened in read-only mode.

        Raises:
            PackageError - The file could not be opened
        """
        try:
            return self._open_fn(rel_path)
        except Exception as e:
            msg = "Could not open file from package: %s" % rel_path
            self._log.warning(msg)
            raise PackageError(msg) from e

    def add_file(self, rel_path, mode=0o777):
        """ Add a file to the package.

        The file should be specified as a relative path to the package
        root.  The open_fn provided in the constructor must be able to
        take the relative path and open the actual source file from
        wherever the file actually is stored.

        If the file's parent directories do not yet exist, add them to
        the package.

        Arguments:
            rel_path - The file path relative to the top of the package.
            mode - The permission mode the file should be stored with so
                   it can be extracted with the correct permissions.

        Raises:
            PackageError - The file could not be added to the package
        """
        if not rel_path:
            raise PackageError("Empty file name added")

        if rel_path in self._package_file_mode_map:
            raise PackageError("File %s already exists in package" % rel_path)

        # If the file's directory is not in the package add it.
        rel_dir = os.path.dirname(rel_path)
        while rel_dir:
            self._package_dirs.add(rel_dir)
            rel_dir = os.path.dirname(rel_dir)

        self._package_file_mode_map[rel_path] = mode

    def add_dir(self, rel_path):
        """ Add a directory to the package

        Arguments:
            rel_path - The directories relative path.

        Raises:
            PackageError - A file already exists in the package with the same name.
        """
        if rel_path in self._package_file_mode_map:
            raise PackageError("File already exists with the same name: %s", rel_path)

        if rel_path in self._package_dirs:
            self._log.warning("%s directory already exists", rel_path)
            return

        self._package_dirs.add(rel_path)


class NsdPackage(DescriptorPackage):
    DESCRIPTOR_TYPE = "nsd"
    SERIALIZER = convert.RwNsdSerializer()

    @property
    def descriptor_type(self):
        return "nsd"

    @property
    def serializer(self):
        return NsdPackage.SERIALIZER


class VnfdPackage(DescriptorPackage):
    DESCRIPTOR_TYPE = "vnfd"
    SERIALIZER = convert.RwVnfdSerializer()

    @property
    def descriptor_type(self):
        return "vnfd"

    @property
    def serializer(self):
        return VnfdPackage.SERIALIZER


class PackageChecksumValidator(object):
    """  This class uses the checksums.txt file in the package
    and validates that all files in the package match the checksum that exists within
    the file.
    """
    CHECKSUM_FILE = "{prefix}checksums.txt"

    def __init__(self, log):
        self._log = log

    @classmethod
    def get_package_checksum_file(cls, package):
        checksum_file = cls.CHECKSUM_FILE.format(prefix=package.prefix)
        if checksum_file not in package.files:
            raise FileNotFoundError("%s does not exist in archive" % checksum_file)

        return checksum_file

    def validate(self, package):
        """ Validate file checksums match that in the checksums.txt

        Arguments:
            package - The Descriptor Package which possiblity contains checksums.txt

        Returns: A list of files that were validated by the checksums.txt

        Raises:
            PackageValidationError - The package validation failed for some
              generic reason.
            PackageFileChecksumError - A file within the package did not match the
              checksum within checksums.txt
        """
        validated_files = []

        try:
            checksum_file = PackageChecksumValidator.get_package_checksum_file(package)
            with package.open(checksum_file) as checksum_hdl:
                archive_checksums = checksums.ArchiveChecksums.from_file_desc(checksum_hdl)
        except (FileNotFoundError, PackageError) as e:
            self._log.warning("Could not open package checksum file.  Not validating checksums.")
            return validated_files

        for pkg_file in package.files:
            if pkg_file == checksum_file:
                continue

            pkg_file_no_prefix = pkg_file.replace(package.prefix, "", 1)
            if pkg_file_no_prefix not in archive_checksums:
                self._log.warning("File %s not found in checksum file %s",
                                  pkg_file, checksum_file)
                continue

            try:
                with package.open(pkg_file) as pkg_file_hdl:
                    file_checksum = checksums.checksum(pkg_file_hdl)
            except PackageError as e:
                msg = "Could not read package file {} for checksum validation: {}".format(
                      pkg_file, str(e))
                self._log.error(msg)
                raise PackageValidationError(msg) from e

            if archive_checksums[pkg_file_no_prefix] != file_checksum:
                msg = "{} checksum ({}) did match expected checksum ({})".format(
                        pkg_file, file_checksum, archive_checksums[pkg_file_no_prefix]
                        )
                self._log.error(msg)
                raise PackageFileChecksumError(pkg_file)

            validated_files.append(pkg_file)

        return validated_files


class TarPackageArchive(object):
    """  This class represents a package stored within a tar.gz archive file """
    def __init__(self, log, tar_file_hdl, mode="r"):
        self._log = log
        self._tar_filepath = tar_file_hdl
        self._tar_infos = {}

        self._tarfile = tarfile.open(fileobj=tar_file_hdl, mode=mode)

        self._load_archive()

    def __repr__(self):
        return "TarPackageArchive(%s)" % self._tar_filepath

    def _get_members(self):
        return [info for info in self._tarfile.getmembers()]

    def _load_archive(self):
        self._tar_infos = {info.name: info for info in self._get_members() if info.name}

    def __del__(self):
        self.close()

    def close(self):
        """ Close the opened tarfile"""
        if self._tarfile is not None:
            self._tarfile.close()
            self._tarfile = None

    @property
    def filenames(self):
        """ The list of file members within the tar file """
        return [name for name in self._tar_infos if tarfile.TarInfo.isfile(self._tar_infos[name])]

    def open_file(self, rel_file_path):
        """ Opens a file within the archive as read-only, byte mode.

        Arguments:
            rel_file_path - The file path within the archive to open

        Returns:
            A file like object (see tarfile.extractfile())

        Raises:
            ArchiveError - The file could not be opened for some generic reason.
        """
        if rel_file_path not in self._tar_infos:
            raise ArchiveError("Could not find %s in tar file", rel_file_path)

        try:
            return self._tarfile.extractfile(rel_file_path)
        except tarfile.TarError as e:
            msg = "Failed to read file {} from tarfile {}: {}".format(
                  rel_file_path, self._tar_filepath, str(e)
                  )
            self._log.error(msg)
            raise ArchiveError(msg) from e

    def create_package(self):
        """  Creates a Descriptor package from the archive contents """
        package = DescriptorPackage.from_package_files(self._log, self.open_file, self.filenames)
        for pkg_file in self.filenames:
            package.add_file(pkg_file, self._tar_infos[pkg_file].mode)

        return package


class TemporaryPackage(object):
    """  This class is a container for a temporary file-backed package

    This class contains a DescriptorPackage and can be used in place of one.
    Provides a useful context manager which will close and destroy the file
    that is backing the DescriptorPackage on exit.
    """
    def __init__(self, log, package, file_hdl):
        self._log = log
        self._package = package
        self._file_hdl = file_hdl

        if not hasattr(self._file_hdl, "name"):
            raise ValueError("File handle must have a name attribute")

    def __getattr__(self, attr):
        return getattr(self._package, attr)

    def __enter__(self):
        return self._package

    def __exit__(self, type, value, tb):
        self.close()

    def filename(self):
        """ Returns the filepath with is backing the Package """
        return self._file_hdl.name

    def package(self):
        """ The contained DescriptorPackage instance """
        return self._package

    def close(self):
        """ Close and remove the backed file """
        filename = self._file_hdl.name

        try:
            self._file_hdl.close()
        except OSError as e:
            self._log.warning("Failed to close package file: %s", str(e))

        try:
            os.remove(filename)
        except OSError as e:
            self._log.warning("Failed to remove package file: %s", str(e))
