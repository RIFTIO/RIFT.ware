#!/usr/bin/env python3

############################################################################
# Copyright 2016 RIFT.io Inc                                               #
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

import argparse
import logging
import os
import shutil
import sys
import tarfile
import tempfile
import unittest
import xmlrunner

import rift.mano.examples.ping_pong_nsd as ping_pong_nsd

from rift.mano.utils.compare_desc import CompareDescShell

from rift.tasklets.rwlaunchpad.tosca import ExportTosca
from rift.tasklets.rwlaunchpad.tosca import ImportTosca

from rift.package.package import TarPackageArchive

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
                    use_placement_group=False,
                    use_ns_init_conf=False,
                )
        self.ping_pong_nsd = nsd.descriptor.nsd[0]
        self.ping_vnfd = ping_vnfd.descriptor.vnfd[0]
        self.pong_vnfd = pong_vnfd.descriptor.vnfd[0]


class ToscaTestCase(unittest.TestCase):
    """ Unittest for YANG to TOSCA and back translations

    This generates the Ping Pong descrptors using the script
    in examles and then converts it to TOSCA and back to YANG.
    """
    default_timeout = 0
    top_dir = __file__[:__file__.find('/modules/core/')]
    log_level = logging.WARN
    log = None

    @classmethod
    def setUpClass(cls):
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        stderr_handler.setFormatter(fmt)
        logging.basicConfig(level=cls.log_level)
        cls.log = logging.getLogger('tosca-ut')
        cls.log.addHandler(stderr_handler)

    def setUp(self):
        """Run before each test method to initialize test environment."""

        super(ToscaTestCase, self).setUp()
        self.output_dir = tempfile.mkdtemp()

    def compare_dict(self, gen_d, exp_d):
        gen = "--generated="+str(gen_d)
        exp = "--expected="+str(exp_d)
        CompareDescShell.compare_dicts(gen, exp, log=self.log)

    def yang_to_tosca(self, descs):
        """Convert YANG model to TOSCA model"""
        pkg = ExportTosca(self.log)
        nsd_id = pkg.add_nsd(descs.ping_pong_nsd)
        pkg.add_vnfd(nsd_id, descs.ping_vnfd)
        pkg.add_vnfd(nsd_id, descs.pong_vnfd)

        return pkg.create_archive('ping_pong_nsd', self.output_dir)

    def tosca_to_yang(self, tosca_file):
        """Convert TOSCA model to YANG model"""
        if ImportTosca.is_tosca_package(tosca_file):
            # This could be a tosca package, try processing
            tosca = ImportTosca(self.log, tosca_file, out_dir=self.output_dir)
            files = tosca.translate()
            if files is None or len(files) < 3:
                raise ValueError("Could not process as a "
                                 "TOSCA package {}: {}".format(tosca_file, files))
            else:
                 self.log.info("Tosca package was translated successfully")
                 return files
        else:
            raise ValueError("Not a valid TOSCA archive: {}".
                             format(tosca_file))

    def compare_descs(self, descs, yang_files):
        """Compare the sescriptors generated with original"""
        for yang_file in yang_files:
            if tarfile.is_tarfile(yang_file):
                with open(yang_file, "r+b") as tar:
                    archive = TarPackageArchive(self.log, tar)
                    pkg = archive.create_package()
                    desc_type = pkg.descriptor_type
                    if desc_type == 'nsd':
                        nsd_yang = pkg.descriptor_msg.as_dict()
                        self.compare_dict(nsd_yang,
                                          descs.ping_pong_nsd.as_dict())
                    elif desc_type == 'vnfd':
                        vnfd_yang = pkg.descriptor_msg.as_dict()
                        if 'ping_vnfd' == vnfd_yang['name']:
                            self.compare_dict(vnfd_yang,
                                              descs.ping_vnfd.as_dict())
                        elif 'pong_vnfd' == vnfd_yang['name']:
                            self.compare_dict(vnfd_yang,
                                              descs.pong_vnfd.as_dict())
                        else:
                            raise Exception("Unknown descriptor type {} found: {}".
                                            format(desc_type, pkg.files))
            else:
                raise Exception("Did not find a valid tar file for yang model: {}".
                                format(yang_file))

    def test_output(self):
        try:
            # Generate the Ping Pong descriptors
            descs = PingPongDescriptors()

            # Translate the descriptors to TOSCA
            tosca_file = self.yang_to_tosca(descs)

            # Now translate back to YANG
            yang_files = self.tosca_to_yang(tosca_file)

            # Compare the generated YANG to original
            self.compare_descs(descs, yang_files)

            # Removing temp dir only on success to allow debug in case of failures
            if self.output_dir is not None:
                shutil.rmtree(self.output_dir)
                self.output_dir = None

        except Exception as e:
            self.log.exception(e)
            self.fail("Exception {}".format(e))



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None
    else:
        runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    ToscaTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner, argv=[sys.argv[0]] + unittest_args)

if __name__ == '__main__':
    main()
