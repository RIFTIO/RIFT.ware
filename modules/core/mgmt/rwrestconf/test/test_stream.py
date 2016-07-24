#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 7/10/2015
# 

import argparse
import json
import logging
import os
import sys
import unittest
import xmlrunner

from rift.restconf import (
    SubscriptionParser,
    load_multiple_schema_root,
)

def _collapse_string(string):
    return ''.join([line.strip() for line in string.splitlines()])

def _ordered(obj):
    '''sort json dictionaries for comparison'''
    if isinstance(obj, dict):
        return sorted((k, _ordered(v)) for k, v in obj.items())
    if isinstance(obj, list):
        return sorted(_ordered(x) for x in obj)
    else:
        return obj

class TestStream(unittest.TestCase):

    def test_parse_message(self):
        logger = logging.getLogger(__name__)
        self.maxDiff = None # expected string is too large

        message = '''
        {
            "path":"/car/audi",
            "fields":{
                "bogus":"bogus",
                "bogus":"bogus"
            },
            "channel":"test_rest"
        }
        '''

        expected_xpath = 'D,/vehicle-a:car[vehicle-a:brand="audi"]'

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        subscription_parser = SubscriptionParser(logger, schema)

        actual_xpath = subscription_parser.parse(message)

        self.assertEquals(actual_xpath, expected_xpath)

########################################
# BEGIN BOILERPLATE JENKINS INTEGRATION

def main():                                                                      
    logging.basicConfig(level=logging.DEBUG)
    

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])      
    unittest.main(testRunner=runner)   

if __name__ == '__main__':
    main()

