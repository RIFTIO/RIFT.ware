############################################################################
# Copyright 2016 RIFT.IO Inc                                               #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
############################################################################


import os
import shutil
import tempfile

from .tosca import ImportTosca


class ConvertPackageError(Exception):
    pass


class ConvertPackage(object):
    """Convert a package to our YANG model

    Currently only TOSCA to our model is supported
    """

    def __init__(self, log, filename, pkgfile):
        self._log = log
        self._filename = filename
        self._pkgfile = pkgfile
        self._tempdir = None

    def convert(self, delete=False):
        """Convert package to our YANG model

        Arguments:
          delete: If the pkgfile is to be deleted after converting

        Returns:
          List of descriptor packages. If the package is not a
          suported format, None is returned

        Note:
          This will create a temporary directory and the converted
          files will be in that. The converted files and directory
          need to be deleted after use.
        """

        # Create a temporary directory to store the converted packages
        tempdir = tempfile.mkdtemp()

        out_files = []
        converted = False
        # Check if this is a tosca archive
        if ImportTosca.is_tosca_package(self._pkgfile):
            self._log.debug("Uploaded file {} is a TOSCA archive".
                            format(self._filename))
            try:
                tosca = ImportTosca(self._log, self._pkgfile, out_dir=tempdir)
                out_files = tosca.translate()
                converted = True

            except Exception as e:
                self._log.error("Exception converting package from TOSCA {}: {}".
                                format(self._filename, e))

                # Remove the tempdir
                try:
                    shutil.rmtree(tempdir)
                except OSError as e:
                    self._log.warning("Unable to remove temporary directory {}: {}".
                                      format(tempdir, e))

                raise

        # Delete the input file, if converted
        if converted:
            self._tempdir = tempdir
            try:
                os.remove(self._pkgfile)
            except OSError as e:
                self._log.warning("Failed to remove package file: %s", str(e))
        else:
            # Remove the temp dir
            shutil.rmtree(tempdir, ignore_errors=True)

            #Return the input file
            out_files.append(self._pkgfile)


        # Return the converted files
        self._log.debug("Converted package files: {}".format(out_files))
        return out_files

