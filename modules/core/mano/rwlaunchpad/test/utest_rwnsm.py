#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import logging
import os
import sys
import unittest
import uuid
import xmlrunner

from gi.repository import (
        NsdYang,
        NsrYang,
        )

logger = logging.getLogger('test-rwnsmtasklet')

import rift.tasklets.rwnsmtasklet.rwnsmtasklet as rwnsmtasklet
import rift.tasklets.rwnsmtasklet.xpath as rwxpath

class TestGiXpath(unittest.TestCase):
    def setUp(self):
        rwxpath.reset_cache()

    def test_nsd_elements(self):
        """
        Test that a particular element in a list is corerctly retrieved. In
        this case, we are trying to retrieve an NSD from the NSD catalog.

        """
        # Create the initial NSD catalog
        nsd_catalog = NsdYang.YangData_Nsd_NsdCatalog()

        # Create an NSD, set its 'id', and add it to the catalog
        nsd_id = str(uuid.uuid4())
        nsd_catalog.nsd.append(
                NsdYang.YangData_Nsd_NsdCatalog_Nsd(
                    id=nsd_id,
                    )
                )

        # Retrieve the NSD using and xpath expression
        xpath = '/nsd:nsd-catalog/nsd:nsd[nsd:id={}]'.format(nsd_id)
        nsd = rwxpath.getxattr(nsd_catalog, xpath)

        self.assertEqual(nsd_id, nsd.id)

        # Modified the name of the NSD using an xpath expression
        rwxpath.setxattr(nsd_catalog, xpath + "/nsd:name", "test-name")

        name = rwxpath.getxattr(nsd_catalog, xpath + "/nsd:name")
        self.assertEqual("test-name", name)

    def test_nsd_scalar_fields(self):
        """
        Test that setxattr correctly sets the value specified by an xpath.

        """
        # Define a simple NSD
        nsd = NsdYang.YangData_Nsd_NsdCatalog_Nsd()

        # Check that the unset fields are in fact set to None
        self.assertEqual(None, rwxpath.getxattr(nsd, "/nsd:nsd-catalog/nsd:nsd/nsd:name"))
        self.assertEqual(None, rwxpath.getxattr(nsd, "/nsd:nsd-catalog/nsd:nsd/nsd:short-name"))

        # Set the values of the 'name' and 'short-name' fields
        rwxpath.setxattr(nsd, "/nsd:nsd-catalog/nsd:nsd/nsd:name", "test-name")
        rwxpath.setxattr(nsd, "/nsd:nsd-catalog/nsd:nsd/nsd:short-name", "test-short-name")

        # Check that the 'name' and 'short-name' fields are correctly set
        self.assertEqual(nsd.name, rwxpath.getxattr(nsd, "/nsd:nsd-catalog/nsd:nsd/nsd:name"))
        self.assertEqual(nsd.short_name, rwxpath.getxattr(nsd, "/nsd:nsd-catalog/nsd:nsd/nsd:short-name"))


class TestInputParameterSubstitution(unittest.TestCase):
    def setUp(self):
        self.substitute_input_parameters = rwnsmtasklet.InputParameterSubstitution(logger)

    def test_null_arguments(self):
        """
        If None is passed to the substitutor for either the NSD or the NSR
        config, no exception should be raised.

        """
        nsd = NsdYang.YangData_Nsd_NsdCatalog_Nsd()
        nsr_config = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr()

        self.substitute_input_parameters(None, None)
        self.substitute_input_parameters(nsd, None)
        self.substitute_input_parameters(None, nsr_config)

    def test_illegal_input_parameter(self):
        """
        In the NSD there is a list of the parameters that are allowed to be
        sbustituted by input parameters. This test checks that when an input
        parameter is provided in the NSR config that is not in the NSD, it is
        not applied.

        """
        # Define the original NSD
        nsd = NsdYang.YangData_Nsd_NsdCatalog_Nsd()
        nsd.name = "robert"
        nsd.short_name = "bob"

        # Define which parameters may be modified
        nsd.input_parameter_xpath.append(
                NsdYang.YangData_Nsd_NsdCatalog_Nsd_InputParameterXpath(
                    xpath="/nsd:nsd-catalog/nsd:nsd/nsd:name",
                    label="NSD Name",
                    )
                )

        # Define the input parameters that are intended to be modified
        nsr_config = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
        nsr_config.input_parameter.extend([
            NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                xpath="/nsd:nsd-catalog/nsd:nsd/nsd:name",
                value="alice",
                ),
            NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                xpath="/nsd:nsd-catalog/nsd:nsd/nsd:short-name",
                value="alice",
                ),
            ])

        self.substitute_input_parameters(nsd, nsr_config)

        # Verify that only the parameter in the input_parameter_xpath list is
        # modified after the input parameters have been applied.
        self.assertEqual("alice", nsd.name)
        self.assertEqual("bob", nsd.short_name)

    def test_substitution(self):
        """
        Test that substitution of input parameters occurs as expected.

        """
        # Define the original NSD
        nsd = NsdYang.YangData_Nsd_NsdCatalog_Nsd()
        nsd.name = "robert"
        nsd.short_name = "bob"

        # Define which parameters may be modified
        nsd.input_parameter_xpath.extend([
                NsdYang.YangData_Nsd_NsdCatalog_Nsd_InputParameterXpath(
                    xpath="/nsd:nsd-catalog/nsd:nsd/nsd:name",
                    label="NSD Name",
                    ),
                NsdYang.YangData_Nsd_NsdCatalog_Nsd_InputParameterXpath(
                    xpath="/nsd:nsd-catalog/nsd:nsd/nsd:short-name",
                    label="NSD Short Name",
                    ),
                ])

        # Define the input parameters that are intended to be modified
        nsr_config = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr()
        nsr_config.input_parameter.extend([
            NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                xpath="/nsd:nsd-catalog/nsd:nsd/nsd:name",
                value="robert",
                ),
            NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_InputParameter(
                xpath="/nsd:nsd-catalog/nsd:nsd/nsd:short-name",
                value="bob",
                ),
            ])

        self.substitute_input_parameters(nsd, nsr_config)

        # Verify that both the 'name' and 'short-name' fields are correctly
        # replaced.
        self.assertEqual("robert", nsd.name)
        self.assertEqual("bob", nsd.short_name)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')

    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.FATAL)

    # Make the test logger very quiet
    logger.addHandler(logging.NullHandler())

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
