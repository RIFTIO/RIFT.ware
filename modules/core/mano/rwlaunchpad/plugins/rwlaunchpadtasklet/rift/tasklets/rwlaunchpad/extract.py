import collections
import io
import os
import shutil
import tarfile
import tempfile
import tornado.httputil

import rift.package.package
import rift.package.convert

from .convert_pkg import ConvertPackage


class ExtractError(Exception):
    pass


class UnreadableHeadersError(ExtractError):
    pass


class MissingTerminalBoundary(ExtractError):
    pass


class UnreadableDescriptorError(ExtractError):
    pass


class UnreadablePackageError(ExtractError):
    pass


def boundary_search(fd, boundary):
    """
    Use the Boyer-Moore-Horpool algorithm to find matches to a message boundary
    in a file-like object.

    Arguments:
        fd       - a file-like object to search
        boundary - a string defining a message boundary

    Returns:
        An array of indices corresponding to matches in the file

    """
    # It is easier to work with a search pattern that is reversed with this
    # algorithm
    needle = ''.join(reversed(boundary)).encode()

    # Create a lookup to efficiently determine how far we can skip through the
    # file based upon the characters in the pattern.
    lookup = dict()
    for i, c in enumerate(needle[1:], start=1):
        if c not in lookup:
            lookup[c] = i

    blength = len(boundary)
    indices = list()

    # A buffer that same length as the pattern is used to read characters from
    # the file. Note that characters are added from the left to make for a
    # straight forward comparison with the reversed orientation of the needle.
    buffer = collections.deque(maxlen=blength)
    buffer.extendleft(fd.read(blength))

    # Iterate through the file and construct an array of the indices where
    # matches to the boundary occur.
    index = 0
    while True:
        tail = buffer[0]

        # If the "tail" of the buffer matches the first character in the
        # needle, perform a character by character check.
        if tail == needle[0]:
            for x, y in zip(buffer, needle):
                if x != y:
                    break

            else:
                # Success! Record the index of the of beginning of the match
                indices.append(index)

        # Determine how far to skip based upon the "tail" character
        skip = lookup.get(tail, blength)
        chunk = fd.read(skip)

        if chunk == b'':
            break

        # Push the chunk into the buffer and update the index
        buffer.extendleft(chunk)
        index += skip

    return indices


def extract_package(log, fd, boundary, pkgfile):
    """Extract tarball from multipart message on disk

    Arguments:
        fd       - A file object that the package can be read from
        boundary - a string defining the boundary of different parts in the
                    multipart message.

    """
    log.debug("extracting archive from data")

    # The boundary parameter in the message is defined as the boundary
    # parameter in the message headers prepended by two dashes, i.e.
    # "--{boundary}".
    boundary = "--" + boundary

    # Find the indices of the message boundaries
    boundaries = boundary_search(fd, boundary)
    if not boundaries:
        log.error("Could not find boundaries in uploaded package")
        raise UnreadablePackageError()

    # check that the message has a terminal boundary
    fd.seek(boundaries[-1])
    terminal = fd.read(len(boundary) + 2)
    if terminal != boundary.encode() + b'--':
        raise MissingTerminalBoundary()

    log.debug("search for part containing archive")
    # find the part of the message that contains the descriptor
    for alpha, bravo in zip(boundaries[:-1], boundaries[1:]):
        # Move to the beginning of the message part (and trim the
        # boundary)
        fd.seek(alpha)
        fd.readline()

        # Extract the headers from the beginning of the message
        headers = tornado.httputil.HTTPHeaders()
        while fd.tell() < bravo:
            line = fd.readline()
            if not line.strip():
                break

            headers.parse_line(line.decode('utf-8'))

        else:
            raise UnreadableHeadersError()

        # extract the content disposition and options or move on to the next
        # part of the message.
        try:
            content_disposition = headers['content-disposition']
            disposition, options = tornado.httputil._parse_header(content_disposition)
        except KeyError:
            continue

        # If this is not form-data, it is not what we are looking for
        if disposition != 'form-data':
            continue

        # If there is no descriptor in the options, this data does not
        # represent a descriptor archive.
        if options.get('name', '') != 'descriptor':
            continue

        file_uploaded = options.get('filename', '')

        # Write the archive section to disk
        with open(pkgfile + ".partial", 'wb') as tp:
            log.debug("writing archive ({}) to filesystem".format(pkgfile))

            remaining = bravo - fd.tell()
            while remaining > 0:
                length = min(remaining, 1024)
                tp.write(fd.read(length))
                remaining -= length

            tp.flush()

        # If the data contains a end-of-feed and carriage-return
        # characters, this can cause gzip to issue warning or errors. Here,
        # we check the last to bytes of the data and remove them if they
        # corresponding to '\r\n'.
        with open(pkgfile + ".partial", "rb+") as tp:
            tp.seek(-2, 2)
            if tp.read(2) == b"\r\n":
                tp.seek(-2, 2)
                tp.truncate()

        # The pkgfile sometimes has b'--' characters at the end of file.
        # Here we check the last two bytes of the data and remove them if they
        # corresponding to b'--'.
        with open(pkgfile + ".partial", "rb+") as tp:
            tp.seek(-2, 2)
            if tp.read(2) == b"--":
                log.debug("Found and removed terminal boundary --")
                tp.seek(-2, 2)
                tp.truncate()

        log.debug("finished writing archive")

        # Strip the "upload" suffix from the basename
        shutil.move(pkgfile + ".partial", pkgfile)

        return file_uploaded

    log.error("Did not find descriptor field in package upload")
    raise UnreadablePackageError()


class UploadPackageExtractor(object):
    DEFAULT_EXTRACT_DIR = tempfile.gettempdir()

    def __init__(self, log, extract_dir=DEFAULT_EXTRACT_DIR):
        self._log = log
        self._extract_dir = extract_dir

    def _extract_upload(self, pkg_id, uploaded_file, boundary):
        pkgfile = os.path.join(self._extract_dir, pkg_id + ".extract")
        try:
            self._log.debug("Extracting uploaded file to %s", pkgfile)
            with open(uploaded_file, "r+b") as uploaded_fd:
                uploaded_filename = extract_package(self._log, uploaded_fd, boundary, pkgfile)
                if uploaded_filename is None:
                    msg = "Unable to extract package"
                    self._log.warning(msg)
                    raise ExtractError(msg)
        finally:
            self._log.debug("Removing uploaded pkg file: %s", uploaded_file)
            try:
                os.remove(uploaded_file)
            except OSError as e:
                self._log.warning("Failed to remove uploaded file: %s", str(e))

        return uploaded_filename, pkgfile

    def _create_package_from_upload(self, uploaded_file, extracted_pkgfile):
        def create_package_from_descriptor_file(desc_hdl):
            # Uploaded package was a plain descriptor file
            bytes_hdl = io.BytesIO(desc_hdl.read())
            bytes_hdl.name = uploaded_file
            try:
                package = rift.package.package.DescriptorPackage.from_descriptor_file_hdl(
                        self._log, bytes_hdl
                        )
            except rift.package.package.PackageError as e:
                msg = "Could not create descriptor package from descriptor: %s" % str(e)
                self._log.error(msg)
                raise UnreadableDescriptorError(msg) from e

            return package

        def create_package_from_tar_file(tar_hdl):
            # Uploaded package was in a .tar.gz format
            tar_archive = rift.package.package.TarPackageArchive(
                    self._log, tar_hdl,
                    )
            try:
                package = tar_archive.create_package()
            except rift.package.package.PackageError as e:
                msg = "Could not create package from tar archive: %s" % str(e)
                self._log.error(msg)
                raise UnreadablePackageError(msg) from e

            return package

        tmp_pkgs = []
        try:
            # This file handle will be passed to TemporaryPackage to be closed
            # and the underlying file removed.
            upload_hdl = open(extracted_pkgfile, "r+b")

            # Process the package archive
            if tarfile.is_tarfile(extracted_pkgfile):
                package = create_package_from_tar_file(upload_hdl)
                tmp_pkgs.append(rift.package.package.TemporaryPackage(self._log,
                                                                      package,
                                                                      upload_hdl))

            # Check if this is just a descriptor file
            elif rift.package.convert.ProtoMessageSerializer.is_supported_file(uploaded_file):
                package = create_package_from_descriptor_file(upload_hdl)
                tmp_pkgs.append(rift.package.package.TemporaryPackage(self._log,
                                                                      package,
                                                                      upload_hdl))

            else:
                # See if the pacakage can be converted
                files = ConvertPackage(self._log,
                                       uploaded_file,
                                       extracted_pkgfile).convert(delete=True)

                if files is None or not len(files):
                    # Not converted successfully
                    msg = "Uploaded file was neither a tar.gz or descriptor file"
                    self._log.error(msg)
                    raise UnreadablePackageError(msg)

                # Close the open file handle as this file is not used anymore
                upload_hdl.close()

                for f in files:
                    self._log.debug("Upload converted file: {}".format(f))
                    upload_hdl = open(f, "r+b")
                    package = create_package_from_tar_file(upload_hdl)
                    tmp_pkgs.append(rift.package.package.TemporaryPackage(self._log,
                                                                          package,
                                                                          upload_hdl))

        except Exception as e:
            # Cleanup any TemporaryPackage instances created
            for t in tmp_pkgs:
                t.close()

            # Close the handle if not already closed 
            try:
                upload_hdl.close()
            except OSError as e:
                self._log.warning("Failed to close file handle: %s", str(e))

            try:
                self._log.debug("Removing extracted package file: %s", extracted_pkgfile)
                os.remove(extracted_pkgfile)
            except OSError as e:
                self._log.warning("Failed to remove extracted package dir: %s", str(e))

            raise e

        return tmp_pkgs

    def extract_package_from_upload(self, uploaded_file, boundary, pkg_id):
        """ Returns DescriptorPackage instances from a upload stream

        Arguments:
            uploaded_file - A multipart upload message with a "descriptor" option
                            provided with the upload filename.
            boundary - The multipart boundary
            pkg_id - A unique upload package id

        Returns:
            A list of TemporyPackage instances

        Raises:
        """
        try:
            os.makedirs(self._extract_dir, exist_ok=True)
        except FileExistsError:
            pass

        self._log.info("extracting uploaded descriptor file/package from HTTP upload")
        uploaded_filename, extracted_pkgfile = self._extract_upload(pkg_id, uploaded_file, boundary)

        self._log.info("creating package from uploaded descriptor file/package")
        temp_packages = self._create_package_from_upload(uploaded_filename, extracted_pkgfile)

        return temp_packages

