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

import magic

from rift.mano.yang_translator.common.utils import _
from rift.mano.yang_translator.rwmano.yang_translator import YangTranslator


"""
Test the yang translation from command line as:
#translator
  --template-file=<path to the JSON template or tar.gz>
  --template-type=<type of template e.g. yang>
  --parameters="purpose=test"
  --output_dir=<output directory>
  --validate_only
Takes four user arguments,
1. type of translation (e.g. yang) (required)
2. Path to the file that needs to be translated (required)
3. Input parameters (optional)
4. Write to output files in a dir (optional), else print on screen

In order to use translator to only validate template,
without actual translation, pass --validate-only along with
other required arguments.

"""


class TranslatorShell(object):

    SUPPORTED_TYPES = ['yang']
    COPY_DIRS = ['images']
    SUPPORTED_INPUTS = (TAR, JSON, XML, YAML) = ('tar', 'json', 'xml', 'yaml')

    def _parse_args(self, raw_args=None):
        parser = argparse.ArgumentParser(
            description='RIFT.io YANG translator for descriptors')
        parser.add_argument(
            "-f",
            "--template-file",
            nargs="+",
            required=True,
            action="append",
            help="Template file to translate")
        parser.add_argument(
            "-o",
            "--output-dir",
            default=None,
            help="Directory to output")
        parser.add_argument(
            "-p", "--parameters",
            help="Input parameters")
        parser.add_argument(
            "--archive",
            help="Create a ZIP archive",
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

    def main(self, raw_args=None, log=None):
        args = self._parse_args(raw_args)
        if log is None:
            if args.debug:
                logging.basicConfig(level=logging.DEBUG)
            else:
                logging.basicConfig(level=logging.ERROR)
            log = logging.getLogger("yang-translator")

        log.debug(_("Args passed is {}").format(args))
        self.log = log
        self.in_files = []
        self.ftype = None
        for f in args.template_file:
            path = os.path.abspath(f[0])
            if not os.path.isfile(path):
                msg = _("The path %(path)s is not a valid file.") % {
                    'path': path}
                log.error(msg)
                raise ValueError(msg)
            # Get the file type
            ftype = self._get_file_type(path)
            if self.ftype is None:
                self.ftype = ftype
            elif self.ftype != ftype:
                msg = (_("All input files hould be of same type"))
                log.error(msg)
                raise ValueError(msg)
            self.in_files.append(path)

        self.log.debug(_("Input files are of type {0}").
                       format(self.ftype))

        self.archive = None
        self._translate(output_dir=args.output_dir,
                        archive=args.archive)

    def _translate(self, output_dir=None, archive=False):
        output = None
        self.log.debug(_('Loading the yang template for {0}.').
                       format(self.in_files))
        translator = YangTranslator(self.log, files=self.in_files)
        self.log.debug(_('Translating the yang template for {0}.').
                       format(self.in_files))
        output = translator.translate()
        if output:
            if output_dir:
                translator.write_output(output,
                                        output_dir=output_dir,
                                        archive=archive)
            else:
                for key in output.keys():
                    print(_("TOSCA Template {0}:\n{1}").
                          format(key, output[key]))
        else:
            self.log.error(_("Did not get any translated output!!"))


    def _get_file_type(self, path):
        m = magic.open(magic.MAGIC_MIME)
        m.load()
        typ = m.file(path)
        if typ.startswith('text/plain'):
            # Assume to be yaml
            return self.YAML
        elif typ.startswith('application/x-gzip'):
            return self.TAR
        else:
            msg = _("The file {0} is not a supported type: {1}"). \
                  format(path, typ)
            self.log.error(msg)
            raise ValueError(msg)


def main(args=None, log=None):
    TranslatorShell().main(raw_args=args, log=log)


if __name__ == '__main__':
    main()
