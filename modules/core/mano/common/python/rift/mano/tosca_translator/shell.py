#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

# Copyright 2016 RIFT.io Inc


import argparse
import logging
import logging.config
import os
import shutil
import stat
import subprocess
import tempfile
import zipfile

import magic

import yaml

from rift.mano.tosca_translator.common.utils import _
from rift.mano.tosca_translator.common.utils import ChecksumUtils
from rift.mano.tosca_translator.rwmano.syntax.mano_template import ManoTemplate
from rift.mano.tosca_translator.rwmano.tosca_translator import TOSCATranslator

from toscaparser.tosca_template import ToscaTemplate


"""
Test the tosca translation from command line as:
#translator
  --template-file=<path to the YAML template or CSAR>
  --template-type=<type of template e.g. tosca>
  --parameters="purpose=test"
  --output_dir=<output directory>
  --archive
  --validate_only
Takes following user arguments,
. Path to the file that needs to be translated (required)
. Input parameters (optional)
. Write to output files in a dir (optional), else print on screen
. Create archive or not

In order to use translator to only validate template,
without actual translation, pass --validate-only along with
other required arguments.

"""


class ToscaShellError(Exception):
    pass


class ToscaEntryFileError(ToscaShellError):
    pass


class ToscaNoEntryDefinitionError(ToscaShellError):
    pass


class ToscaEntryFileNotFoundError(ToscaShellError):
    pass


class ToscaCreateArchiveError(ToscaShellError):
    pass


class TranslatorShell(object):

    SUPPORTED_TYPES = ['tosca']
    COPY_DIRS = ['images']
    SUPPORTED_INPUTS = (YAML, ZIP) = ('yaml', 'zip')

    def __init__(self, log=None):
        self.log = log

    def main(self, raw_args=None):
        args = self._parse_args(raw_args)

        if self.log is None:
            if args.debug:
                logging.basicConfig(level=logging.DEBUG)
            else:
                logging.basicConfig(level=logging.ERROR)
            self.log = logging.getLogger("tosca-translator")

        self.template_file = args.template_file

        parsed_params = {}
        if args.parameters:
            parsed_params = self._parse_parameters(args.parameters)

        self.archive = False
        if args.archive:
            self.archive = True

        self.tmpdir = None

        if args.validate_only:
            a_file = os.path.isfile(args.template_file)
            tpl = ToscaTemplate(self.template_file, parsed_params, a_file)
            self.log.debug(_('Template = {}').format(tpl.__dict__))
            msg = (_('The input {} successfully passed ' \
                     'validation.').format(self.template_file))
            print(msg)
        else:
            self.use_gi = not args.no_gi
            tpl = self._translate("tosca", parsed_params)
            if tpl:
                return self._write_output(tpl, args.output_dir)

    def translate(self,
                  template_file,
                  output_dir=None,
                  use_gi=True,
                  archive=False,):
        self.template_file = template_file

        # Check the input file
        path = os.path.abspath(template_file)
        self.in_file = path
        a_file = os.path.isfile(path)
        if not a_file:
            msg = _("The path {0} is not a valid file.").format(template_file)
            self.log.error(msg)
            raise ValueError(msg)

        # Get the file type
        self.ftype = self._get_file_type()
        self.log.debug(_("Input file {0} is of type {1}").
                       format(path, self.ftype))

        self.archive = archive

        self.tmpdir = None

        self.use_gi = use_gi

        tpl = self._translate("tosca", {})
        if tpl:
            return self._write_output(tpl, output_dir)

    def _parse_args(self, raw_args=None):
        parser = argparse.ArgumentParser(
            description='RIFT TOSCA translator for descriptors')

        parser.add_argument(
            "-f",
            "--template-file",
            required=True,
            help="Template file to translate")

        parser.add_argument(
            "-o",
            "--output-dir",
            help="Directory to output")

        parser.add_argument(
            "-p", "--parameters",
            help="Input parameters")

        parser.add_argument(
            "-a", "--archive",
            action="store_true",
            help="Archive the translated files")

        parser.add_argument(
            "--no-gi",
            help="Do not use the YANG GI to generate descriptors",
            action="store_true")

        parser.add_argument(
            "--validate-only",
            help="Validate template, no translation",
            action="store_true")

        parser.add_argument(
            "--debug",
            help="Enable debug logging",
            action="store_true")

        if raw_args:
            args = parser.parse_args(raw_args)
        else:
            args = parser.parse_args()
        return args

    def _parse_parameters(self, parameter_list):
        parsed_inputs = {}
        if parameter_list:
            # Parameters are semi-colon separated
            inputs = parameter_list.replace('"', '').split(';')
            # Each parameter should be an assignment
            for param in inputs:
                keyvalue = param.split('=')
                # Validate the parameter has both a name and value
                msg = _("'%(param)s' is not a well-formed parameter.") % {
                    'param': param}
                if keyvalue.__len__() is 2:
                    # Assure parameter name is not zero-length or whitespace
                    stripped_name = keyvalue[0].strip()
                    if not stripped_name:
                        self.log.error(msg)
                        raise ValueError(msg)
                    # Add the valid parameter to the dictionary
                    parsed_inputs[keyvalue[0]] = keyvalue[1]
                else:
                    self.log.error(msg)
                    raise ValueError(msg)
        return parsed_inputs

    def get_entry_file(self):
        # Extract the archive and get the entry file
        if self.ftype == self.YAML:
            return self.in_file

        self.prefix = ''
        if self.ftype == self.ZIP:
            self.tmpdir = tempfile.mkdtemp()
            prevdir = os.getcwd()
            try:
                with zipfile.ZipFile(self.in_file) as zf:
                    self.prefix = os.path.commonprefix(zf.namelist())
                    self.log.debug(_("Zipfile prefix is {0}").
                                   format(self.prefix))
                    zf.extractall(self.tmpdir)

                    # Set the execute bits on scripts as zipfile
                    # does not restore the permissions bits
                    os.chdir(self.tmpdir)
                    for fname in zf.namelist():
                        if (fname.startswith('scripts/') and
                            os.path.isfile(fname)):
                            # Assume this is a script file
                            # Give all permissions to owner and read+execute
                            # for group and others
                            os.chmod(fname,
                                     stat.S_IRWXU|stat.S_IRGRP|stat.S_IXGRP|stat.S_IROTH|stat.S_IXOTH)

                # TODO (pjoseph): Use the below code instead of extract all
                # once unzip is installed on launchpad VMs
                # zfile = os.path.abspath(self.in_file)
                # os.chdir(self.tmpdir)
                # zip_cmd = "unzip {}".format(zfile)
                # subprocess.check_call(zip_cmd,
                #                       #stdout=subprocess.PIPE,
                #                       #stderr=subprocess.PIPE,
                #                       shell=True,)

            except Exception as e:
                msg = _("Exception extracting input file {0}: {1}"). \
                      format(self.in_file, e)
                self.log.error(msg)
                self.log.exception(e)
                os.chdir(prevdir)
                shutil.rmtree(self.tmpdir)
                self.tmpdir = None
                raise ToscaEntryFileError(msg)

        os.chdir(self.tmpdir)

        try:
            # Goto the TOSAC Metadata file
            prefix_dir = os.path.join(self.tmpdir, self.prefix)
            meta_file = os.path.join(prefix_dir, 'TOSCA-Metadata',
                                     'TOSCA.meta')
            self.log.debug(_("Checking metadata file {0}").format(meta_file))
            if not os.path.exists(meta_file):
                self.log.error(_("Not able to find metadata file in archive"))
                return

            # Open the metadata file and get the entry file
            with open(meta_file, 'r') as f:
                meta = yaml.load(f)

                if 'Entry-Definitions' in meta:
                    entry_file = os.path.join(prefix_dir,
                                              meta['Entry-Definitions'])
                    if os.path.exists(entry_file):
                        self.log.debug(_("TOSCA entry file is {0}").
                                       format(entry_file))
                        return entry_file

                    else:
                        msg = _("Unable to get the entry file: {0}"). \
                              format(entry_file)
                        self.log.error(msg)
                        raise ToscaEntryFileNotFoundError(msg)

                else:
                    msg = _("Did not find entry definition " \
                            "in metadata: {0}").format(meta)
                    self.log.error(msg)
                    raise ToscaNoEntryDefinitionError(msg)

        except Exception as e:
            msg = _('Exception parsing metadata file {0}: {1}'). \
                  format(meta_file, e)
            self.log.error(msg)
            self.log.exception(e)
            raise ToscaEntryFileError(msg)

        finally:
            os.chdir(prevdir)

    def _translate(self, sourcetype, parsed_params):
        output = None

        # Check the input file
        path = os.path.abspath(self.template_file)
        self.in_file = path
        a_file = os.path.isfile(path)
        if not a_file:
            msg = _("The path {} is not a valid file."). \
                  format(self.template_file)
            self.log.error(msg)
            raise ValueError(msg)

        # Get the file type
        self.ftype = self._get_file_type()
        self.log.debug(_("Input file {0} is of type {1}").
                       format(path, self.ftype))

        if sourcetype == "tosca":
            entry_file = self.get_entry_file()
            if entry_file:
                self.log.debug(_('Loading the tosca template.'))
                tosca = ToscaTemplate(entry_file, parsed_params, True)
                self.log.debug(_('TOSCA Template: {}').format(tosca.__dict__))
                translator = TOSCATranslator(self.log, tosca, parsed_params,
                                         use_gi=self.use_gi)
                self.log.debug(_('Translating the tosca template.'))
                output = translator.translate()
        return output

    def _copy_supporting_files(self, output_dir, files):
        # Copy supporting files, if present in archive
        if self.tmpdir:
            # The files are refered relative to the definitions directory
            arc_dir = os.path.join(self.tmpdir,
                                   self.prefix,
                                   'Definitions')
            prevdir = os.getcwd()
            try:
                os.chdir(arc_dir)
                for fn in files:
                    fname = fn['name']
                    fpath = os.path.abspath(fname)
                    ty = fn['type']
                    if ty == 'image':
                        dest = os.path.join(output_dir, 'images')
                    elif ty == 'script':
                        dest = os.path.join(output_dir, 'scripts')
                    else:
                        self.log.warn(_("Unknown file type {0} for {1}").
                                      format(ty, fname))
                        continue

                    self.log.debug(_("File type {0} copy from {1} to {2}").
                                   format(ty, fpath, dest))
                    if os.path.exists(fpath):
                        # Copy the files to the appropriate dir
                        self.log.debug(_("Copy file(s) {0} to {1}").
                                         format(fpath, dest))
                        if os.path.isdir(fpath):
                            # Copy directory structure like charm dir
                            shutil.copytree(fpath, dest)
                        else:
                            # Copy a single file
                            os.makedirs(dest, exist_ok=True)
                            shutil.copy2(fpath, dest)

                    else:
                        self.log.warn(_("Could not find file {0} at {1}").
                                      format(fname, fpath))

            except Exception as e:
                self.log.error(_("Exception copying files {0}: {1}").
                               format(arc_dir, e))
                self.log.exception(e)

            finally:
                os.chdir(prevdir)

    def _create_checksum_file(self, output_dir):
        # Create checkum for all files
        flist = {}
        for root, dirs, files in os.walk(output_dir):
            rel_dir = root.replace(output_dir, '').lstrip('/')

            for f in files:
                fpath = os.path.join(root, f)
                # TODO (pjoseph): To be fixed when we can
                # retrieve image files from Launchpad
                if os.path.getsize(fpath) != 0:
                    flist[os.path.join(rel_dir, f)] = \
                                                ChecksumUtils.get_md5(fpath)
                    self.log.debug(_("Files in output_dir: {}").format(flist))

                chksumfile = os.path.join(output_dir, 'checksums.txt')
                with open(chksumfile, 'w') as c:
                    for key in sorted(flist.keys()):
                        c.write("{}  {}\n".format(flist[key], key))

    def _create_archive(self, desc_id, output_dir):
        """Create a tar.gz archive for the descriptor"""
        aname = desc_id + '.tar.gz'
        apath = os.path.join(output_dir, aname)
        self.log.debug(_("Generating archive: {}").format(apath))

        prevdir = os.getcwd()
        os.chdir(output_dir)

        # Generate the archive
        tar_cmd = "tar zcvf {} {}".format(apath, desc_id)
        self.log.debug(_("Generate archive: {}").format(tar_cmd))

        try:
            subprocess.check_call(tar_cmd,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE,
                                  shell=True)
            return apath

        except subprocess.CalledProcessError as e:
            msg = _("Error creating archive with {}: {}"). \
                           format(tar_cmd, e)
            self.log.error(msg)
            raise ToscaCreateArchiveError(msg)

        finally:
            os.chdir(prevdir)

    def _write_output(self, output, output_dir=None):
        out_files = []

        if output_dir:
            output_dir = os.path.abspath(output_dir)

        if output:
            # Do the VNFDs first and then NSDs as later when
            # loading in launchpad, VNFDs need to be loaded first
            for key in [ManoTemplate.VNFD, ManoTemplate.NSD]:
                for desc in output[key]:
                    if output_dir:
                        desc_id = desc[ManoTemplate.ID]
                        # Create separate directories for each descriptors
                        # Use the descriptor id to avoid name clash
                        subdir = os.path.join(output_dir, desc_id)
                        os.makedirs(subdir)

                        output_file = os.path.join(subdir,
                                            desc[ManoTemplate.NAME]+'.yml')
                        self.log.debug(_("Writing file {0}").
                                       format(output_file))
                        with open(output_file, 'w+') as f:
                            f.write(desc[ManoTemplate.YANG])

                        if ManoTemplate.FILES in desc:
                            self._copy_supporting_files(subdir,
                                                desc[ManoTemplate.FILES])

                        if self.archive:
                            # Create checksum file
                            self._create_checksum_file(subdir)
                            out_files.append(self._create_archive(desc_id,
                                                                  output_dir))
                            # Remove the desc directory
                            shutil.rmtree(subdir)
                    else:
                        print(_("Descriptor {0}:\n{1}").
                              format(desc[ManoTemplate.NAME],
                                     desc[ManoTemplate.YANG]))

            if output_dir and self.archive:
                # Return the list of archive files
                return out_files

    def _get_file_type(self):
        m = magic.open(magic.MAGIC_MIME)
        m.load()
        typ = m.file(self.in_file)
        if typ.startswith('text/plain'):
            # Assume to be yaml
            return self.YAML
        elif typ.startswith('application/zip'):
            return self.ZIP
        else:
            msg = _("The file {0} is not a supported type: {1}"). \
                  format(self.in_file, typ)
            self.log.error(msg)
            raise ValueError(msg)


def main(args=None, log=None):
    TranslatorShell(log=log).main(raw_args=args)


if __name__ == '__main__':
    main()
