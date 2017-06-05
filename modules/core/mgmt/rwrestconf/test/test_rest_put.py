#!/usr/bin/env python3

# RIFT_IO_STANDARD_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 7/10/2015
# RIFT_IO_STANDARD_COPYRIGHT_HEADER(END)

import argparse
import json
import logging
import os
import sys
import unittest
import xmlrunner

from rift.restconf import (
    ConfdRestTranslator,
    convert_xml_to_collection,
    create_xpath_from_url,
    XmlToJsonTranslator,
    load_schema_root,
    load_multiple_schema_root,
    ConfdRestTranslatorV2,
)

from gi.repository import (
        VehicleAYang,
        RwYang,
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

class TestRestPut(unittest.TestCase):
    def compare_doms(self, dom1, dom2):
        self.assertEqual(dom1, dom2)


    def test_json_body_0(self):
        self.maxDiff = None
        url = "/api/config/top-container-shallow"
        body = _collapse_string('''
{
  "a" : "some leaf 0"
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:top-container-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">some leaf 0</a></vehicle-a:top-container-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_1(self):
        self.maxDiff = None
        url = "/api/config/top-list-shallow/"
        body = _collapse_string('''
{
  "top-list-shallow"  :
  [
    {
      "k" : "some key 1"
    },
    {
      "k" : "some key 2"
    }

  ]
}

        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some key 1</k></top-list-shallow><top-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some key 2</k></top-list-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)


    def test_json_body_2(self):
        self.maxDiff = None
        url = "/api/config/multi-key"
        body = _collapse_string('''
{
  "multi-key" :
  [
    {
      "foo" : "key part 1",
      "bar" : "key part 2",
      "treasure" : "some string"
    },
    {
      "foo" : "other key part 1",
      "bar" : "other key part 2",
      "treasure" : "some other string"
    }
  ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><multi-key xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><foo xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key part 1</foo><bar xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key part 2</bar><treasure xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some string</treasure></multi-key><multi-key xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><foo xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">other key part 1</foo><bar xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">other key part 2</bar><treasure xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some other string</treasure></multi-key></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_3(self):
        self.maxDiff = None
        url = "/api/config/top-list-deep"
        body = _collapse_string('''
{
  "top-list-deep" :
  [
    {
      "k" : "some key",
      "inner-list" :
      [
        {
	  "k" : "some other key",
	  "a" : "some string",
	  "inner-container" : {
	    "a" : "some other string"
	  }
	}
      ],
      "inner-container-shallow" :
      {
        "a" : "yet a third string"
      },
      "inner-container-deep" :
      {
        "bottom-list-shallow" :
	[
	  {
	  "k" : "yet a third key"
	  }
	]
      }
    }
  ]
}        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some key</k><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some other key</k><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some string</a><inner-container xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some other string</a></inner-container></inner-list><inner-container-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">yet a third string</a></inner-container-shallow><inner-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><bottom-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">yet a third key</k></bottom-list-shallow></inner-container-deep></top-list-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_4(self):
        self.maxDiff = None
        url = "/api/config/top-container-deep"
        body = _collapse_string('''
{
    "a":"some kind of string",
    "inner-list-shallow":[
        {
            "k":"some key thing"
        },
        {
            "k":"some other key thing"
        }
    ],
    "inner-container":{
        "a":"another string",
        "inner-list":[
            {
                "k":"another key thing"
            }
        ]
    },
    "inner-list-deep":[
        {
            "k":"inner key",
            "inner-container-shallow":{
                "a":"an inner string"
            },
            "inner-container-deep":{
                "bottom-list-shallow":[
                    {
                        "k":"bottom key"
                    }
                ]
            }
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">some kind of string</a><inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some key thing</k></inner-list-shallow><inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some other key thing</k></inner-list-shallow><inner-container xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">another string</a><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">another key thing</k></inner-list></inner-container><inner-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">inner key</k><inner-container-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">an inner string</a></inner-container-shallow><inner-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><bottom-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">bottom key</k></bottom-list-shallow></inner-container-deep></inner-list-deep></vehicle-a:top-container-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_5(self):
        self.maxDiff = None
        url = "/api/config/top-list-deep/key1/inner-list/key2"
        body = _collapse_string('''
{
    "inner-list":[
        {
            "k":"key2"
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k>key1</k><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key2</k></inner-list></top-list-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_6(self):
        self.maxDiff = None
        url = "/api/config/top-container-deep/inner-list-shallow"
        body = _collapse_string('''
{
    "inner-list-shallow":[
        {
            "k":"some key thing"
        },
        {
            "k":"some other key thing"
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some key thing</k></inner-list-shallow><inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">some other key thing</k></inner-list-shallow></top-container-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_7(self):
        self.maxDiff = None
        url = "/api/config/top-list-deep/key1/inner-list/key2"
        body = _collapse_string('''
{
    "inner-list":[
        {
            "k":"key2"
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k>key1</k><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key2</k></inner-list></top-list-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_8(self):
        self.maxDiff = None
        url = "/api/config/top-container-deep/inner-container"
        body = _collapse_string('''
{
    "a":"another string",
    "inner-list":[
        {
            "k":"another key thing"
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><vehicle-a:inner-container xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">another string</a><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">another key thing</k></inner-list></vehicle-a:inner-container></top-container-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_9(self):
        self.maxDiff = None
        url = "/api/config/top-list-deep/some%20key/inner-container-shallow"
        body = _collapse_string('''
{
    "a" : "yet a third string"
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k>some key</k><vehicle-a:inner-container-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">yet a third string</a></vehicle-a:inner-container-shallow></top-list-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_10(self):
        self.maxDiff = None
        url = "/api/config/misc"
        body = _collapse_string('''
{
        "empty-leaf" : [null]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:misc xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><empty-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"></empty-leaf></vehicle-a:misc></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_11(self):
        self.maxDiff = None
        url = "/api/config/misc"
        body = _collapse_string('''
{
    "bool-leaf":true,
    "empty-leaf":[null],
    "enum-leaf":"a",
    "int-leaf":42,
    "list-a":[
        {
            "id":0,
            "foo":"asdf"
        }
    ],
    "list-b":[
        {
            "id":0
        }
    ],
    "numbers":[
        {
            "int8-leaf":0,
            "int16-leaf":0,
            "int32-leaf":0,
            "int64-leaf":0,
            "uint8-leaf":0,
            "uint16-leaf":0,
            "uint32-leaf":0,
            "uint64-leaf":0,
            "decimal-leaf":0
        },
        {
            "int8-leaf":"1",
            "int16-leaf":"0",
            "int32-leaf":"0",
            "int64-leaf":"0",
            "uint8-leaf":"0",
            "uint16-leaf":"0",
            "uint32-leaf":"0",
            "uint64-leaf":"0",
            "decimal-leaf":"0"
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:misc xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><bool-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">true</bool-leaf><empty-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"></empty-leaf><enum-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">a</enum-leaf><int-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">42</int-leaf><list-a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><id xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</id><foo xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">asdf</foo></list-a><list-b xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><id xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</id></list-b><numbers xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><int8-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int8-leaf><int16-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int16-leaf><int32-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int32-leaf><int64-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int64-leaf><uint8-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint8-leaf><uint16-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint16-leaf><uint32-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint32-leaf><uint64-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint64-leaf><decimal-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</decimal-leaf></numbers><numbers xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><int8-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">1</int8-leaf><int16-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int16-leaf><int32-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int32-leaf><int64-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</int64-leaf><uint8-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint8-leaf><uint16-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint16-leaf><uint32-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint32-leaf><uint64-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</uint64-leaf><decimal-leaf xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</decimal-leaf></numbers></vehicle-a:misc></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_json_body_serialized_list_element(self):
        self.maxDiff = None
        url = "/api/config/misc/list-a/0"
        model = RwYang.model_create_libncx()
        model.load_schema_ypbc(VehicleAYang.get_schema())
        list_a_msg = VehicleAYang.YangData_VehicleA_Misc_ListA(id=0, foo="asdf")
        list_a_json = list_a_msg.to_json(model)
        body = _collapse_string(list_a_json)

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><misc xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><list-a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><id xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">0</id><foo xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">asdf</foo></list-a></misc></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body, "json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_xml_body_0(self):
        self.maxDiff = None
        url = "/api/config/top-container-shallow"
        body = _collapse_string('''
<top-container-shallow>
  <a>some leaf 0</a>
</top-container-shallow>
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><a>some leaf 0</a></top-container-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"xml"))
        
        self.assertEqual(actual_xml, expected_xml)

    def test_xml_body_1(self):
        self.maxDiff = None
        url = "/api/config/top-list-shallow/some%20key%201"
        body = _collapse_string('''
<top-list-shallow xmlns="http://riftio.com/ns/example">
   <k xmlns="http://riftio.com/ns/example">some key 1</k>
</top-list-shallow>
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-shallow xmlns="http://riftio.com/ns/example" xmlns="" xc:operation="replace"><k xmlns="http://riftio.com/ns/example">some key 1</k></top-list-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"xml"))

        self.assertEqual(actual_xml, expected_xml)

    def test_xml_body_2(self):
        self.maxDiff = None
        url = "/api/config/multi-key/key%20part%201,key%20part%202"
        body = _collapse_string('''
<multi-key>
  <foo>key part 1</foo>
  <bar>key part 2</bar>
  <treasure>some string</treasure>
</multi-key>
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><multi-key xmlns="" xc:operation="replace"><foo>key part 1</foo><bar>key part 2</bar><treasure>some string</treasure></multi-key></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"xml"))

        self.assertEqual(actual_xml, expected_xml)

    def test_xml_body_3(self):
        self.maxDiff = None
        url = "/api/config/top-list-deep"
        body = _collapse_string('''
<top-list-deep>
  <k>some key<k>
  <inner-list>
    <k>some other key</k?
    <a>some string</a>
    <inner-container>
      <a>some other string</a>
    </inner-container>      
  </inner-list>
  <inner-container-shallow>
    <a>yet a third string</a>
  </inner-container-shallow>    
  <inner-container-deep>
    <bottom-list-shallow>
      <k>yet a third key</a>
    </bottom-list-shallow>
  </inner-container-deep>    
</top-list-deep>
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k>some key<k><inner-list><k>some other key</k?<a>some string</a><inner-container><a>some other string</a></inner-container></inner-list><inner-container-shallow><a>yet a third string</a></inner-container-shallow><inner-container-deep><bottom-list-shallow><k>yet a third key</a></bottom-list-shallow></inner-container-deep></top-list-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"xml"))

        self.assertEqual(actual_xml, expected_xml)


    def test_xml_body_4(self):
        self.maxDiff = None
        url = "/api/config/top-list-deep/key1/inner-list/key2"
        body = _collapse_string('''
<inner-list>
  <k>key2</k>
</inner-list>
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k>key1</k><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k>key2</k></inner-list></top-list-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("PUT", url, (body,"xml"))

        self.assertEqual(actual_xml, expected_xml)

class TestConfdRestTranslatorV2(unittest.TestCase):
    def test_POST_top_container_shallow(self):
        self.maxDiff = None
        url = "/v2/api/config"
        body = _collapse_string('''
{
  "top-container-shallow" : {
    "a" : "value"
  }
}
        ''')
        expected_xml= _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">value</a></top-container-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("POST", url, (body,"json"))
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_top_container_shallow_leaf(self):
        self.maxDiff = None
        url = "/v2/api/config/top-container-shallow/a"
        body = _collapse_string('''
{
    "a" : "new-value"
}
        ''')
        expected_xml= _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><vehicle-a:a xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">new-value</vehicle-a:a></top-container-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_top_container_shallow(self):
        self.maxDiff = None
        url = "/v2/api/config/top-container-shallow"
        body = _collapse_string('''
{
  "top-container-shallow" : {
    "a" : "new-value"
  }
}
        ''')
        expected_xml= _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:top-container-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">new-value</a></vehicle-a:top-container-shallow></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        self.assertEqual(actual_xml, expected_xml)

    def test_POST_top_list_shallow(self):
        self.maxDiff = None
        url = "/v2/api/running"
        body = _collapse_string('''
{
  "top-list-shallow" : {
    "k" : "key1"
  }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("POST", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key1</k></top-list-shallow></config>
        ''')
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_top_list_shallow(self):
        self.maxDiff = None
        url = "/v2/api/running/top-list-shallow"
        body = _collapse_string('''
{
  "top-list-shallow" : {
    "k" : "key1"
  }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:top-list-shallow xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key1</k></vehicle-a:top-list-shallow></config>
        ''')
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_top_list_shallow_with_key(self):
        self.maxDiff = None
        url = "/v2/api/running/top-list-shallow/key1"
        body = _collapse_string('''
{
  "top-list-shallow" : {
    "k" : "key1"
  }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key1</k></top-list-shallow></config>
        ''')
        self.assertEqual(actual_xml, expected_xml)

    def test_POST_top_container_deep_list(self):
        self.maxDiff = None
        url = "/v2/api/running/top-container-deep"
        body = _collapse_string('''
{
  "inner-list-shallow" : {
    "k" : "key"
  }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("POST", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><vehicle-a:top-container-deep xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key</k></inner-list-shallow></vehicle-a:top-container-deep></config>        
        ''')
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_top_container_deep_list(self):
        self.maxDiff = None
        url = "/v2/api/running/top-container-deep/inner-list-shallow/key"
        body = _collapse_string('''
{
  "inner-list-shallow" : {
    "k" : "key"
  }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key</k></inner-list-shallow></top-container-deep></config>
        ''')
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_top_list_deep_inner(self):
        self.maxDiff = None
        url = "/v2/api/running/top-list-deep/key_top/inner-list/key_inner"
        body = _collapse_string('''
{
  "inner-list" : {
      "k" : "key_inner",
      "a" : "a_value",
      "inner-container" : {
        "a" : "a_value"
      }
  }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">
  <top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>key_top</k>
    <inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">
      <k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">key_inner</k>
      <a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">a_value</a>
      <inner-container xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
        <a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">a_value</a>
      </inner-container>
    </inner-list>
  </top-list-deep>
</config>        
        ''')
        self.assertEqual(actual_xml, expected_xml)

    def test_PUT_deep_list_container(self):
        self.maxDiff = None
        url = "/v2/api/running/top-list-deep/key_top/inner-list/key_inner/inner-container"
        body = _collapse_string('''
{
      "inner-container" : {
        "a" : "a_value"
      }
}
        ''')
        schema = load_multiple_schema_root(["vehicle-a"])
        converter = ConfdRestTranslatorV2(schema)
        actual_xml = converter.convert("PUT", url, (body,"json"))
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">
  <top-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <k>key_top</k>
    <inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
      <k>key_inner</k>
      <vehicle-a:inner-container xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace">
        <a xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">a_value</a>
      </vehicle-a:inner-container>
    </inner-list>
  </top-list-deep>
</config>        
        ''')
        self.assertEqual(actual_xml, expected_xml)

########################################
# BEGIN BOILERPLATE JENKINS INTEGRATION

def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')
    args, unittest_args = parser.parse_known_args()
    if args.no_runner:
        runner = None

    unittest.main(testRunner=runner, argv=[sys.argv[0]] + unittest_args)

if __name__ == '__main__':
    main()

