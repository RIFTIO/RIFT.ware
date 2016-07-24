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
import lxml.etree as et

from rift.restconf import (
    ConfdRestTranslator,
    convert_xml_to_collection,
    create_xpath_from_url,
    XmlToJsonTranslator,
    load_schema_root,
    load_multiple_schema_root,
    convert_get_request_to_xml,
    NetconfOperation,
)

from rift.rwlib.testing import (
    are_xml_doms_equal,
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

class TestRestGet(unittest.TestCase):
    def compare_doms(self, dom1, dom2):
        are_equal, errors = are_xml_doms_equal(dom1, dom2)

        if not are_equal:
            for e in errors:
                print(e)

        # also compare strings directly
        #self.assertEqual(dom1, dom2)
        self.assertTrue(are_equal)        

    def test_get_cross_namespace(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/car/honda%20motors/extras"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <brand>honda motors</brand>
    <vehicle-augment-a:extras xmlns:vehicle-augment-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a"/>
  </vehicle-a:car>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_00(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_01(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/a"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:a/>
  </vehicle-a:top-container-deep>
</data>
        ''')
        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_02(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep/>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_03(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/some-key"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep>
      <k>some-key</k>
    </vehicle-a:inner-list-deep>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_04(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/some-key/inner-container-deep"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep>
      <k>some-key</k>
      <vehicle-a:inner-container-deep/>
    </vehicle-a:inner-list-deep>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_05(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/some-key/inner-container-deep/bottom-list-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep>
      <k>some-key</k>
      <vehicle-a:inner-container-deep>
        <vehicle-a:bottom-list-shallow/>
      </vehicle-a:inner-container-deep>
    </vehicle-a:inner-list-deep>
  </vehicle-a:top-container-deep>
</data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_06(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/some-key/inner-container-deep/bottom-list-shallow/some-key"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep>
      <k>some-key</k>
      <vehicle-a:inner-container-deep>
        <vehicle-a:bottom-list-shallow>
          <k>some-key</k>
        </vehicle-a:bottom-list-shallow>
      </vehicle-a:inner-container-deep>
    </vehicle-a:inner-list-deep>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_07(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/some-key/inner-container-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep>
      <k>some-key</k>
      <vehicle-a:inner-container-shallow/>
    </vehicle-a:inner-list-deep>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_08(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/some-key/inner-container-shallow/a"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-deep>
      <k>some-key</k>
      <vehicle-a:inner-container-shallow>
        <vehicle-a:a/>
      </vehicle-a:inner-container-shallow>
    </vehicle-a:inner-list-deep>
  </vehicle-a:top-container-deep>
</data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_09(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-shallow/>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_10(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-shallow/some-key"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-list-shallow>
      <k>some-key</k>
    </vehicle-a:inner-list-shallow>
  </vehicle-a:top-container-deep>
</data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_11(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_12(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-shallow/a"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:a/>
  </vehicle-a:top-container-shallow>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_13(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_14(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
  </vehicle-a:top-list-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_15(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-container-deep"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-container-deep/>
  </vehicle-a:top-list-deep>
</data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_16(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-container-deep/bottom-list-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-container-deep>
      <vehicle-a:bottom-list-shallow/>
    </vehicle-a:inner-container-deep>
  </vehicle-a:top-list-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_17(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-container-deep/bottom-list-shallow/some-key"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-container-deep>
      <vehicle-a:bottom-list-shallow>
        <k>some-key</k>
      </vehicle-a:bottom-list-shallow>
    </vehicle-a:inner-container-deep>
  </vehicle-a:top-list-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_18(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-container-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-container-shallow/>
  </vehicle-a:top-list-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_19(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-container-shallow/a"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-container-shallow>
      <vehicle-a:a/>
    </vehicle-a:inner-container-shallow>
  </vehicle-a:top-list-deep>
</data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_20(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-shallow"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_21(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-shallow/some-key"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
  </vehicle-a:top-list-shallow>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_22(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-list/a"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-list>
      <vehicle-a:a/>
    </vehicle-a:inner-list>
  </vehicle-a:top-list-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_23(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-list-deep/some-key/inner-list/inner-container"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-list-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>some-key</k>
    <vehicle-a:inner-list>
      <vehicle-a:inner-container/>
    </vehicle-a:inner-list>
  </vehicle-a:top-list-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_24(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-container"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <vehicle-a:inner-container/>
  </vehicle-a:top-container-deep>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_25(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/multi-key/firstkey,secondkey/treasure"
        expected_xml = _collapse_string('''
<data>
  <vehicle-a:multi-key xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <foo>firstkey</foo>
    <bar>secondkey</bar>
    <vehicle-a:treasure/>
  </vehicle-a:multi-key>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_26(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/vehicle-augment-a:clash"
        expected_xml = _collapse_string('''
<data>
  <vehicle-augment-a:clash xmlns:vehicle-augment-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a"/>
</data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)


    def test_conversion_CONFD_URL_to_XML_27(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/vehicle-augment-a:clash/bar"
        expected_xml = _collapse_string('''
<data>
  <vehicle-augment-a:clash xmlns:vehicle-augment-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">
    <vehicle-augment-a:bar/>
  </vehicle-augment-a:clash>
</data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)

        self.compare_doms(actual_xml, expected_xml)

########################################
# BEGIN BOILERPLATE JENKINS INTEGRATION

def main():                                                                      
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])      
    unittest.main(testRunner=runner)   

if __name__ == '__main__':
    main()

