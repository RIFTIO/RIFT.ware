#!/usr/bin/env python3

import argparse
import dictdiffer
import logging
import os
import sys
import unittest
import xmlrunner
import yaml

import rift.openmano.rift2openmano as rift2openmano
import rift.openmano.openmano_client as openmano_client

logger = logging.getLogger()

THIS_DIR = os.path.dirname(os.path.realpath(__file__))

def delete_list_dict_keys(source_list, lst_keys):
    for l in source_list:
        if isinstance(l, dict):
            delete_keys_from_dict(l, lst_keys)
        elif isinstance(l, list):
            delete_list_dict_keys(l, lst_keys)

def delete_keys_from_dict(source_dict, lst_keys):
    for k in lst_keys:
        try:
            del source_dict[k]
        except KeyError:
            pass
        for v in source_dict.values():
            if isinstance(v, dict):
                delete_keys_from_dict(v, lst_keys)
            if isinstance(v, list):
                delete_list_dict_keys(v, lst_keys)


class Rift2OpenmanoTest(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.maxDiff = None

    def load_openmano_vnf(self, openmano_vnf_path):
        with open(openmano_vnf_path, 'rb') as hdl:
            openmano_vnf = yaml.load(hdl)

        return openmano_vnf

    def load_openmano_ns(self, openmano_ns_path):
        with open(openmano_ns_path, 'rb') as hdl:
            openmano_ns = yaml.load(hdl)

        return openmano_ns

    def rift_vnf(self, rift_vnf_path):
        with open(rift_vnf_path, 'r') as xml_hdl:
            rift_vnf = rift2openmano.RiftVNFD.from_xml_file_hdl(xml_hdl)
            return rift_vnf

    def rift2openmano_vnf(self, rift_vnf_path):
        rift_vnf = self.rift_vnf(rift_vnf_path)
        openmano_vnfd = rift2openmano.rift2openmano_vnfd(rift_vnf)

        return openmano_vnfd

    def rift2openmano_ns(self, rift_ns_path, rift_vnf_paths):
        rift_vnf_hdls = [open(path, 'r') for path in rift_vnf_paths]
        vnf_dict = rift2openmano.create_vnfd_from_xml_files(rift_vnf_hdls)

        with open(rift_ns_path, 'r') as xml_hdl:
            rift_ns = rift2openmano.RiftNSD.from_xml_file_hdl(xml_hdl)

        openmano_nsd = rift2openmano.rift2openmano_nsd(rift_ns, vnf_dict)
        logger.debug(
                "Converted ns: %s",
                yaml.safe_dump(openmano_nsd, indent=4, default_flow_style=False))

        return openmano_nsd

    def generate_vnf_dict_diffs(self, source_dict, dest_dict):
        delete_keys_from_dict(source_dict, ["description"])
        delete_keys_from_dict(dest_dict, ["description"])

        diff = dictdiffer.diff(source_dict, dest_dict)
        return list(diff)

    def generate_ns_dict_diffs(self, source_dict, dest_dict):
        delete_keys_from_dict(dest_dict, ["graph"])
        diff = dictdiffer.diff(source_dict, dest_dict)
        return list(diff)


class Mwc16GenTest(Rift2OpenmanoTest):
    OPENMANO_TIDGEN_VNF_PATH = os.path.join(
            THIS_DIR, "openmano_descriptors/tidgen4pLarge.yaml"
            )
    RIFT_TIDGEN_VNF_PATH = os.path.join(
            THIS_DIR, "rift_descriptors/tidgen4pLarge.xml"
            )

    OPENMANO_MWC16_NS_PATH = os.path.join(
            THIS_DIR, "openmano_descriptors/mwc16-gen.yaml"
            )
    RIFT_MWC16_NS_PATH = os.path.join(
            THIS_DIR, "rift_descriptors/mwc16-gen.xml"
            )

    def test_tidgen_vnf(self):
        converted_vnf = self.rift2openmano_vnf(Mwc16GenTest.RIFT_TIDGEN_VNF_PATH)
        dest_vnf = self.load_openmano_vnf(Mwc16GenTest.OPENMANO_TIDGEN_VNF_PATH)

        diffs = self.generate_vnf_dict_diffs(converted_vnf, dest_vnf)
        self.assertEqual([], diffs)

    def test_mwc16_gen_ns(self):
        converted_ns = self.rift2openmano_ns(
                Mwc16GenTest.RIFT_MWC16_NS_PATH, [Mwc16GenTest.RIFT_TIDGEN_VNF_PATH]
                )

        dest_ns = self.load_openmano_ns(Mwc16GenTest.OPENMANO_MWC16_NS_PATH)

        diffs = self.generate_ns_dict_diffs(converted_ns, dest_ns)
        self.assertEqual([], diffs)


def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None

    logger.setLevel(logging.DEBUG if args.verbose else logging.WARN)

    unittest.main(testRunner=runner, argv=[sys.argv[0]]+unittest_args)

if __name__ == '__main__':
    main()
