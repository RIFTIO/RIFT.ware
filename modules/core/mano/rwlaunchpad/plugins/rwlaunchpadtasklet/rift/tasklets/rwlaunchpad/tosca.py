# Copyright 2016 RIFT.io Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import logging
import os
import shutil
import subprocess
import tempfile
import uuid
import zipfile

from rift.mano.tosca_translator.shell import TranslatorShell
from rift.mano.yang_translator.rwmano.yang_translator import YangTranslator


class ToscaPackageError(Exception):
    pass


class ToscaPackageReadError(Exception):
    pass


class InvalidToscaPackageError(ToscaPackageError):
    pass


class ToscaTranslateError(ToscaPackageError):
    pass


class YangTranslateError(Exception):
    pass


class ToscaArchiveCreateError(YangTranslateError):
    pass


class YangTranslateNsdError(YangTranslateError):
    pass


class ExportTosca(object):
    def __init__(self, log=None):
        if log is None:
            self.log = logging.getLogger("rw-mano-log")
        else:
            self.log = log
        self.nsds = {}
        self.csars = list()

    def add_image(self, nsd_id, image, chksum=None):
        if image.name not in self.images:
            self.images[image.name] = image

    def add_vld(self, nsd_id, vld, pkg=None):
        if not 'vlds' in self.nsds[nsd_id]:
            self.nsds[nsd_id]['vlds'] = []
        self.nsds[nsd_id]['vlds'].append(vld)
        if pkg:
            self.nsds[nsd_id]['pkgs'].append(pkg)

    def add_vnfd(self, nsd_id, vnfd, pkg=None):
        if not 'vnfds' in self.nsds[nsd_id]:
            self.nsds[nsd_id]['vnfds'] = []
        self.nsds[nsd_id]['vnfds'].append(vnfd)
        if pkg:
            self.nsds[nsd_id]['pkgs'].append(pkg)

    def add_nsd(self, nsd, pkg=None):
        nsd_id = str(uuid.uuid4())
        self.nsds[nsd_id] = {'nsd': nsd}
        self.nsds[nsd_id]['pkgs'] = []
        if pkg:
            self.nsds[nsd_id]['pkgs'].append(pkg)
        return nsd_id

    def create_csar(self, nsd_id, dest=None):
        if dest is None:
            dest = tempfile.mkdtemp()

        # Convert YANG to dict
        yangs = {}
        yangs['vnfd'] = []
        for vnfd in self.nsds[nsd_id]['vnfds']:
            yangs['vnfd'].append(vnfd.as_dict())
            self.log.debug("Translate VNFD: {}".format(vnfd.as_dict()))
        yangs['nsd'] = []
        yangs['nsd'].append(self.nsds[nsd_id]['nsd'].as_dict())
        self.log.debug("Translate NSD : {}".format(yangs['nsd']))

        # Translate YANG model to TOSCA template
        translator = YangTranslator(self.log,
                                    yangs=yangs,
                                    packages=self.nsds[nsd_id]['pkgs'])
        output = translator.translate()
        self.csars.extend(translator.write_output(output,
                                                  output_dir=dest,
                                                  archive=True))
        self.log.debug("Created CSAR archive {}".format(self.csars[-1]))

    def create_archive(self, archive_name, dest=None):
        if not len(self.nsds):
            self.log.error("Did not find any NSDs to export")
            return

        if dest is None:
            dest = tempfile.mkdtemp()

        prevdir = os.getcwd()

        if not os.path.exists(dest):
            os.makedirs(dest)

        try:
            # Convert each NSD to a TOSCA template
            for nsd_id in self.nsds:
                # Not passing the dest dir to prevent clash in case
                # multiple export of the same desc happens
                self.create_csar(nsd_id)

        except Exception as e:
            msg = "Exception converting NSD {}: {}".format(nsd_id, e)
            self.log.exception(e)
            raise YangTranslateNsdError(msg)

        os.chdir(dest)

        try:
            if archive_name.endswith(".zip"):
                archive_name = archive_name[:-4]

            archive_path = os.path.join(dest, archive_name)

            # Construct a zip of the csar archives
            zip_name = '{}.zip'.format(archive_path)

            if len(self.csars) == 1:
                # Only 1 TOSCA template, just rename csar if required
                if self.csars[0] != zip_name:
                    mv_cmd = "mv {} {}".format(self.csars[0], zip_name)
                    subprocess.check_call(mv_cmd, shell=True, stdout=subprocess.DEVNULL)
                    # Remove the temporary directory created
                    shutil.rmtree(os.path.dirname(self.csars[0]))

            else:
                with zipfile.ZipFile('{}.partial'.format(zip_name), 'w') as zf:
                    for csar in self.csars:
                        # Move file to the current dest dir
                        if os.path.dirname(csar) != dest:
                            file_mv = "mv {} {}".format(csar, dest)
                            subprocess.check_call(file_mv,
                                                  shell=True,
                                                  stdout=subprocess.DEVNULL)
                            # Remove the temporary directory created
                            shutil.rmtree(os.path.dirname(csar))

                        csar_f = os.basename(csar)
                        # Now add to the archive
                        zf.write(csar_f)
                        # Remove the csar file
                        os.remove(csar_f)

                    # Rename archive to final name
                    mv_cmd = "mv {0}.partial {0}".format(zip_name)
                    subprocess.check_call(mv_cmd, shell=True, stdout=subprocess.DEVNULL)

            return zip_name

        except Exception as e:
            msg = "Creating CSAR archive failed: {0}".format(e)
            self.log.exception(e)
            raise YangTranslateError(msg)

        finally:
            os.chdir(prevdir)

class ImportTosca(object):

    def __init__(self, log, in_file, out_dir=None):
        if log is None:
            self.log = logging.getLogger("rw-mano-log")
        else:
            self.log = log
        self.log = log
        self.in_file = in_file
        self.out_dir = out_dir

    def translate(self):
        # Check if the input file is a zip file
        if not zipfile.is_zipfile(self.in_file):
            err_msg = "{} is not a zip file.".format(self.in_file)
            self.log.error(err_msg)
            raise InvalidToscaPackageError(err_msg)

        try:
            # Store the current working directory
            prevdir = os.getcwd()

            # See if we need to create a output directory
            out_dir = self.out_dir
            if out_dir is None:
                out_dir = tempfile.mkdtemp()

            # Call the TOSCA translator
            self.log.debug("Calling tosca-translator for {}".
                           format(self.in_file))
            return TranslatorShell(self.log).translate(self.in_file,
                                                       out_dir,
                                                       archive=True)

        except Exception as e:
            self.log.exception(e)
            raise ToscaTranslateError("Error translating TOSCA package {}: {}".
                                      format(self.in_file, e))

        finally:
                os.chdir(prevdir)

    @staticmethod
    def is_tosca_package(in_file):
        if zipfile.is_zipfile(in_file):
            return True
        else:
            return False


