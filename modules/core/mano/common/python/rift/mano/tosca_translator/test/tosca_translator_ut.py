#!/usr/bin/env python3

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

'''
Unittest for TOSCA tranlator to RIFT.io YANG model
'''

import argparse
import logging
import os
import shutil
import sys
import tarfile
import tempfile
import xmlrunner

import unittest

import rift.mano.examples.ping_pong_nsd as ping_pong_nsd

from rift.mano.tosca_translator.common.utils import _
import rift.mano.tosca_translator.shell as shell

from rift.mano.utils.compare_desc import CompareDescShell

from rift.package import convert

from toscaparser.common.exception import TOSCAException


_TRUE_VALUES = ('True', 'true', '1', 'yes')


class PingPongDescriptors(object):
    def __init__(self):
        ping_vnfd, pong_vnfd, nsd = \
                ping_pong_nsd.generate_ping_pong_descriptors(
                    pingcount=1,
                    external_vlr_count=1,
                    internal_vlr_count=0,
                    num_vnf_vms=1,
                    ping_md5sum='1234567890abcdefg',
                    pong_md5sum='1234567890abcdefg',
                    mano_ut=False,
                    use_scale_group=True,
                    use_mon_params=True,
                )
        self.ping_pong_nsd = nsd.descriptor.nsd[0]
        self.ping_vnfd = ping_vnfd.descriptor.vnfd[0]
        self.pong_vnfd = pong_vnfd.descriptor.vnfd[0]

class TestToscaTranslator(unittest.TestCase):

    tosca_helloworld = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "data/tosca_helloworld.yaml")
    template_file = '--template-file=' + tosca_helloworld
    template_validation = "--validate-only"
    debug="--debug"
    failure_msg = _('The program raised an exception unexpectedly.')

    log_level = logging.WARN
    log = None

    exp_descs = None

    @classmethod
    def setUpClass(cls):
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  " \
                "(%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        stderr_handler.setFormatter(fmt)
        logging.basicConfig(level=cls.log_level)
        cls.log = logging.getLogger('tosca-translator-ut')
        cls.log.addHandler(stderr_handler)
        cls.exp_descs = PingPongDescriptors()

    def test_missing_arg(self):
       self.assertRaises(SystemExit, shell.main, '')

    def test_invalid_file_arg(self):
        self.assertRaises(SystemExit, shell.main, 'translate me')

    def test_invalid_file_value(self):
        self.assertRaises(SystemExit,
                          shell.main,
                          ('--template-file=template.txt'))

    def test_invalid_type_value(self):
        self.assertRaises(SystemExit, shell.main,
                          (self.template_file, '--template-type=xyz'))

    def test_invalid_parameters(self):
        self.assertRaises(ValueError, shell.main,
                          (self.template_file,
                           '--parameters=key'))

    def test_valid_template(self):
        try:
            shell.main([self.template_file])
        except Exception as e:
            self.log.exception(e)
            self.fail(self.failure_msg)

    def test_validate_only(self):
        try:
            shell.main([self.template_file,
                        self.template_validation])
        except Exception as e:
            self.log.exception(e)
            self.fail(self.failure_msg)

        template = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "data/tosca_helloworld_invalid.yaml")
        invalid_template = '--template-file=' + template
        self.assertRaises(TOSCAException, shell.main,
                          [invalid_template,
                           self.template_validation])

    def compare_dict(self, gen_d, exp_d):
        gen = "--generated="+str(gen_d)
        exp = "--expected="+str(exp_d)
        CompareDescShell.compare_dicts(gen, exp, log=self.log)

    def check_output(self, out_dir, archive=False):
        prev_dir = os.getcwd()
        os.chdir(out_dir)
        # Check the archives or directories are present
        dirs = os.listdir(out_dir)
        # The desc dirs are using uuid, so cannot match name
        # Check there are 3 dirs or files
        self.assertTrue(len(dirs) >= 3)

        try:
            count = 0
            for a in dirs:
                desc = None
                if archive:
                    if os.path.isfile(a):
                        self.log.debug("Checking archive: {}".format(a))
                        with tarfile.open(a, 'r') as t:
                            for m in t.getnames():
                                if m.endswith('.yaml')  or m.endswith('.yml'):
                                    # Descriptor file
                                    t.extract(m)
                                    self.log.debug("Extracted file: {}".format(m))
                                    desc = m
                                    break
                    else:
                        continue

                else:
                    if os.path.isdir(a):
                        self.log.debug("Checking directory: {}".format(a))
                        for m in os.listdir(a):
                            if m.endswith('.yaml')  or m.endswith('.yml'):
                                desc = os.path.join(a, m)
                                break

                if desc:
                    self.log.debug("Checking descriptor: {}".format(desc))
                    with open(desc, 'r') as d:
                        rest, ext = os.path.splitext(desc)
                        if '_vnfd.y' in desc:
                            vnfd = convert.VnfdSerializer().from_file_hdl(d, ext)
                            gen_desc = vnfd.as_dict()
                            if 'ping_vnfd.y' in desc:
                                exp_desc = self.exp_descs.ping_vnfd.as_dict()
                            elif 'pong_vnfd.y' in desc:
                                exp_desc = self.exp_descs.pong_vnfd.as_dict()
                            else:
                                raise Exception("Unknown VNFD descriptor: {}".
                                                format(desc))
                        elif '_nsd.y' in desc:
                            nsd = convert.NsdSerializer().from_file_hdl(d, ext)
                            gen_desc = nsd.as_dict()
                            exp_desc = self.exp_descs.ping_pong_nsd.as_dict()
                        else:
                            raise Exception("Unknown file: {}".format(desc))

                        # Compare the descriptors
                        self.compare_dict(gen_desc, exp_desc)

                        # Increment the count of descriptiors found
                        count += 1

            if count != 3:
                raise Exception("Did not find expected number of descriptors: {}".
                                format(count))
        except Exception as e:
            self.log.exception(e)
            raise e

        finally:
            os.chdir(prev_dir)

    def test_output_dir(self):
        test_base_dir = os.path.join(os.path.dirname(
            os.path.abspath(__file__)), 'data')
        template_file = os.path.join(test_base_dir,
                            "ping_pong_csar/Definitions/ping_pong_nsd.yaml")
        template = '--template-file='+template_file
        temp_dir = tempfile.mkdtemp()
        output_dir = "--output-dir=" + temp_dir
        try:
            shell.main([template, output_dir], log=self.log)

        except Exception as e:
            self.log.exception(e)
            self.fail("Exception in test_output_dir: {}".format(e))

        else:
            self.check_output(temp_dir)

        finally:
            if self.log_level != logging.DEBUG:
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir)
            else:
                self.log.warn("Generated desc in {}".format(temp_dir))

    def test_input_csar(self):
        test_base_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            'data')
        template_file = os.path.join(test_base_dir, "ping_pong_csar.zip")
        template = '--template-file='+template_file
        temp_dir = tempfile.mkdtemp()
        output_dir = "--output-dir=" + temp_dir

        try:
            shell.main([template, output_dir, '--archive'], log=self.log)

        except Exception as e:
            self.log.exception(e)
            self.fail("Exception in test_output_dir: {}".format(e))

        else:
            self.check_output(temp_dir, archive=True)

        finally:
            if self.log_level != logging.DEBUG:
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir)
            else:
                self.log.warn("Generated desc in {}".format(temp_dir))

    def test_input_csar_no_gi(self):
        test_base_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            'data')
        template_file = os.path.join(test_base_dir, "ping_pong_csar.zip")
        template = '--template-file='+template_file
        temp_dir = tempfile.mkdtemp()
        output_dir = "--output-dir=" + temp_dir
        no_gi = '--no-gi'

        try:
            shell.main([template, output_dir, no_gi, '--archive'], log=self.log)

        except Exception as e:
            self.log.exception(e)
            self.fail("Exception in input_csar_no_gi: {}".format(e))

        else:
            self.check_output(temp_dir, archive=True)

        finally:
            if self.log_level != logging.DEBUG:
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir)
            else:
                self.log.warn("Generated desc in {}".format(temp_dir))

def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None

    TestToscaTranslator.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner, argv=[sys.argv[0]] + unittest_args)

if __name__ == '__main__':
    main()
