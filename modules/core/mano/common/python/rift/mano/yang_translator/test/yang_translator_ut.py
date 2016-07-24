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


import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import xmlrunner

import unittest

import rift.mano.examples.ping_pong_nsd as ping_pong_nsd

import rift.mano.tosca_translator.shell as tshell

import rift.mano.utils.compare_desc as cmpdesc

from rift.mano.tosca_translator.common.utils import ChecksumUtils

from rift.mano.yang_translator.common.utils import _
import rift.mano.yang_translator.shell as shell


_TRUE_VALUES = ('True', 'true', '1', 'yes')


class PingPongDescriptors(object):

    def __init__(self, output_dir, log):
        ping_vnfd, pong_vnfd, nsd = \
                ping_pong_nsd.generate_ping_pong_descriptors(
                    fmt='yaml',
                    write_to_file=True,
                    out_dir=output_dir,
                    pingcount=1,
                    external_vlr_count=1,
                    internal_vlr_count=0,
                    num_vnf_vms=1,
                    ping_md5sum='1234567890abcdefg',
                    pong_md5sum='1234567890abcdefg',
                    mano_ut=False,
                    use_scale_group=True,
                    use_mon_params=True,
                    use_placement_group = False,
                )

        # Create the tar files in output dir
        def create_archive(desc):
            # Create checksum file
            cur_dir = os.path.join(output_dir, desc)
            flist = {}
            for root, dirs, files in os.walk(cur_dir):
                rel_dir = root.replace(cur_dir+'/', '')
                for f in files:
                    fpath = os.path.join(root, f)
                    flist[os.path.join(rel_dir, f)] = \
                                        ChecksumUtils.get_md5(fpath)
            log.debug(_("Files in {}: {}").format(cur_dir, flist))

            chksumfile = os.path.join(cur_dir, 'checksums.txt')
            with open(chksumfile, 'w') as c:
                for key in sorted(flist.keys()):
                    c.write("{}  {}\n".format(flist[key], key))

            # Create the tar archive
            tar_cmd = "tar zcvf {0}.tar.gz {0}"
            subprocess.check_call(tar_cmd.format(desc),
                                  shell=True,
                                  stdout=subprocess.DEVNULL)

        prevdir = os.getcwd()
        os.chdir(output_dir)
        for d in os.listdir(output_dir):
            create_archive(d)
        os.chdir(prevdir)

class TestYangTranslator(unittest.TestCase):

    yang_helloworld = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "data/yang_helloworld.json")
    template_file = '--template-file=' + yang_helloworld
    template_validation = "--validate-only"
    debug="--debug"
    failure_msg = _('The program raised an exception unexpectedly.')

    default_timeout = 0
    log_level = logging.WARN
    log = None

    @classmethod
    def setUpClass(cls):
        fmt = logging.Formatter(
            '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        stderr_handler.setFormatter(fmt)
        logging.basicConfig(level=cls.log_level)
        cls.log = logging.getLogger('yang-translator-ut')
        cls.log.addHandler(stderr_handler)

        cls.desc_dir = tempfile.mkdtemp()
        PingPongDescriptors(cls.desc_dir, cls.log)
        cls.log.debug("Yang comaprison descs in {}".format(cls.desc_dir))

    @classmethod
    def tearDownClass(cls):
        '''Clean up temporary directory'''
        # Remove directory if not debug level
        if cls.log_level != logging.DEBUG:
            shutil.rmtree(cls.desc_dir)
        else:
            cls.log.warn("Descriptor directory: {}".format(cls.desc_dir))

    def test_missing_arg(self):
        self.assertRaises(SystemExit, shell.main, '')

    def test_invalid_file_arg(self):
        self.assertRaises(SystemExit, shell.main, 'translate me')

    def test_invalid_file_value(self):
        self.assertRaises(SystemExit,
                          shell.main,
                          ('--template-file=template.txt'))

    def test_invalid_type_value(self):
        self.assertRaises(SystemExit,
                          shell.main,
                          (self.template_file,
                           '--template-type=xyz'))

    def compare_tosca(self, gen_desc, exp_desc):
        gen = "--generated="+gen_desc
        exp = "--expected="+exp_desc
        cmpdesc.main([gen, exp])

    def test_output(self):
        test_base_dir = os.path.join(os.path.dirname(
            os.path.abspath(__file__)), 'data')
        temp_dir = tempfile.mkdtemp()
        args = []
        for f in os.listdir(self.desc_dir):
            fpath = os.path.join(self.desc_dir, f)
            if os.path.isfile(fpath):
                template = '--template-file='+fpath
                args.append(template)
        output_dir = "--output-dir=" + temp_dir
        args.append(output_dir)
        self.log.debug("Args passed: {}".format(args))

        try:
            shell.main(args, log=self.log)

            # Check the dirs are present
            out_dir = os.path.join(temp_dir, 'ping_pong_nsd')
            self.assertTrue(os.path.isdir(out_dir))
            dirs = os.listdir(out_dir)
            expected_dirs = ['TOSCA-Metadata', 'Definitions']
            self.assertTrue(set(expected_dirs) <= set(dirs))

            # Compare the descriptors
            gen_desc = os.path.join(out_dir, 'Definitions', 'ping_pong_nsd.yaml')
            exp_desc = os.path.join(test_base_dir,
                                    'ping_pong_tosca.yaml')
            self.compare_tosca(gen_desc, exp_desc)

            # Convert back to yang and compare
            template = '--template-file='+gen_desc
            yang_out_dir = os.path.join(temp_dir, 'ping_pong_yang')
            output_dir = "--output-dir=" + yang_out_dir
            tshell.main([template, output_dir], log=self.log)

            # Check the dirs are present
            dirs = os.listdir(yang_out_dir)
            self.assertTrue(len(dirs) >= 3)

        except Exception as e:
            self.log.exception(e)
            self.fail(_("Exception {}").format(e))

        finally:
            if temp_dir:
                shutil.rmtree(temp_dir)


def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None

    TestYangTranslator.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner, argv=[sys.argv[0]] + unittest_args)

if __name__ == '__main__':
    main()
