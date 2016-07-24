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
import gi

from rift.restconf import (
    ConfdRestTranslator,
    naive_xml_to_json,
    convert_rpc_to_json_output,
    convert_rpc_to_xml_output,
    convert_xml_to_collection,
    create_dts_xpath_from_url,
    create_xpath_from_url,
    XmlToJsonTranslator,
    load_schema_root,
    load_multiple_schema_root,
    convert_get_request_to_xml,
)

gi.require_version('VehicleAYang', '1.0')
gi.require_version('RwYang', '1.0')

from gi.repository import (
    RwYang,
    VehicleAYang,
    RwKeyspec,
    RwRestconfYang,
)

from rift.rwlib.testing import (
    are_xml_doms_equal,
)


logger = logging.getLogger(__name__)

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

class TestRest(unittest.TestCase):

    def compare_doms(self, dom1, dom2):
        are_equal, errors = are_xml_doms_equal(dom1, dom2)

        if not are_equal:
            for e in errors:
                print(e)

        # also compare strings directly
        self.assertEqual(dom1, dom2)
        self.assertTrue(are_equal)        

    def test_conversion_PUT_JSON_to_XML_1(self):
        self.maxDiff = None

        url = '/api/config/car'
        json = _collapse_string('''
{
    "car":[
        {
            "brand":"subaru",
            "models":{
                "name-m":"WRX",
                "year":2015,
                "capacity":5,
                "is-cool":"True"
            }
        }
    ]
}
       ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><brand xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">subaru</brand><models xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><name-m xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">WRX</name-m><year xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">2015</year><capacity xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">5</capacity><is-cool xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">true</is-cool></models></car></config>
        ''')

        root = load_schema_root("vehicle-a")

        converter = ConfdRestTranslator(root)

        actual_xml = converter.convert("PUT", url, (json,"application/data+json"))

        self.compare_doms(actual_xml, expected_xml)

    def test_conversion_POST_JSON_to_XML_1(self):
        self.maxDiff = None

        url = '/api/config/car'
        json = _collapse_string('''
{
    "car":[
        {
            "brand":"subaru",
            "models":{
                "name-m":"WRX",
                "year":2015,
                "capacity":5,
                "is-cool":"True"
            }
        }
    ]
}
        ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><brand xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">subaru</brand><models xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><name-m xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">WRX</name-m><year xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">2015</year><capacity xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">5</capacity><is-cool xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">true</is-cool></models></car></config>
        ''')

        root = load_schema_root("vehicle-a")

        converter = ConfdRestTranslator(root)

        actual_xml = converter.convert("POST", url, (json,"application/data+json"))

        self.assertEqual(actual_xml, expected_xml)


    def test_conversion_CONFD_URL_to_XML_1(self):
        self.maxDiff = None # expected_xml is too large
        # trailing '/' is on purpose to test that edge case
        url = "/api/operational/car/subaru/models/" 
        expected_xml = _collapse_string('''
<data><vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><brand>subaru</brand><vehicle-a:models><name-m></name-m></vehicle-a:models></vehicle-a:car></data>
        ''')

        schema = load_schema_root("vehicle-a")
        
        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_2(self):
        self.maxDiff = None # expected_xml is too large
        url = "/api/operational/whatever/inner-whatever/list-whatever"
        expected_xml = _collapse_string('''
<data><vehicle-a:whatever xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><vehicle-a:inner-whatever><vehicle-a:list-whatever/></vehicle-a:inner-whatever></vehicle-a:whatever></data>
        ''')

        schema = load_schema_root("vehicle-a")
        
        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_3(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/car/subaru/extras?select=speakers;engine(*)"
        expected_xml = _collapse_string('''
<data><vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><brand>subaru</brand><vehicle-augment-a:extras xmlns:vehicle-augment-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a"/></vehicle-a:car></data>

        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_4(self):
        self.maxDiff = None # expected_xml is too large
        url = "/api/operational/whatever/inner-whatever?deep"
        expected_xml = _collapse_string('''
<data><vehicle-a:whatever xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><vehicle-a:inner-whatever/></vehicle-a:whatever></data>
        ''')

        schema = load_schema_root("vehicle-a")

        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)


    def test_conversion_CONFD_URL_to_XML_5(self):
        self.maxDiff = None # expected_xml is too large
        url = "/api/operational/whatever/inner-whatever"
        expected_xml = _collapse_string('''
<data><vehicle-a:whatever xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><vehicle-a:inner-whatever/></vehicle-a:whatever></data>
        ''')

        schema = load_schema_root("vehicle-a")
        
        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_6(self):
        self.maxDiff = None # expected_xml is too large
        url = "/api/operational/car?select=brand"
        expected_xml = _collapse_string('''
<data><vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/></data>
        ''')

        schema = load_schema_root("vehicle-a")
        
        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_get_car_deep(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/car?deep"
        expected_xml = _collapse_string('''
<data><vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/></data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_get_car(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/car"
        expected_xml = _collapse_string('''
<data><vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"/></data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url)        

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_get_list_key(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/car/honda"
        expected_xml = _collapse_string('''
<data><vehicle-a:car xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><brand>honda</brand></vehicle-a:car></data>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual_xml = convert_get_request_to_xml(schema, url) 

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_delete_list_key_simple(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/car/honda"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">
  <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="delete">
    <brand>honda</brand>
  </car>
</config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("DELETE", url, None)

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_delete_list_in_container(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/top-container-deep/inner-list-shallow/asdf?deep"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">
  <top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
    <inner-list-shallow xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="delete">
      <k>asdf</k>
    </inner-list-shallow>
  </top-container-deep>
</config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("DELETE", url, None)

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_URL_to_XML_get_bogus(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/bogus"

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        caught_exception = False
        try:
            actual_xml = convert_get_request_to_xml(schema, url) 
        except ValueError:
            caught_exception = True

        self.assertTrue(caught_exception)

    def test_conversion_CONFD_URL_to_XML_delete_list_key(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/operational/top-container-deep/inner-container/inner-list/some-key"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-container xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-list xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="delete"><k>some-key</k></inner-list></inner-container></top-container-deep></config>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("DELETE", url, None)

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_CONFD_XML_to_JSON_convert_error_1(self):
        self.maxDiff = None

        xml = _collapse_string('''
<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:2bdd6476-92d4-11e5-86ca-fa163e622f12" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
  <rpc-error>
  <error-type>application</error-type>
  <error-tag>data-missing</error-tag>
  <error-severity>error</error-severity>
  <error-path xmlns:rw-mc="http://riftio.com/ns/riftware-1.0/rw-mc" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
    /nc:rpc/nc:edit-config/nc:config/rw-mc:cloud/rw-mc:account[rw-mc:name=\'cloudname\']
  </error-path>
  <error-info>
    <bad-element>account</bad-element>
  </error-info>
</rpc-error>
</rpc-reply>
        ''')

        expected_json = _collapse_string('''
{"rpc-reply" : {"rpc-error" : {"error-type" : "application","error-tag" : "data-missing","error-severity" : "error","error-path" : "/nc:rpc/nc:edit-config/nc:config/rw-mc:cloud/rw-mc:account[rw-mc:name='cloudname']","error-info" : {"bad-element" : "account"}}}}
        ''')

        actual_json = naive_xml_to_json(bytes(xml,"utf-8"))

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_CONFD_XML_to_JSON_convert_error_2(self):
        self.maxDiff = None

        xml = _collapse_string('''
<?xml version="1.0" encoding="UTF-8"?>
<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:b5b5fd30-8eeb-11e5-8001-fa163e05ddfa" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0"><rpc-error>
<error-type>application</error-type>
<error-tag>data-missing</error-tag>
<error-severity>error</error-severity>
<error-app-tag>instance-required</error-app-tag>
<error-path xmlns:rw-mc="http://riftio.com/ns/riftware-1.0/rw-mc">
    /rpc/edit-config/config/rw-mc:network-pool/rw-mc:pool[rw-mc:name='n1']/rw-mc:cloud-account
</error-path>
<error-message xml:lang="en">illegal reference /network-pool/pool[name='n1']/cloud-account</error-message>
<error-info xmlns:tailf="http://tail-f.com/ns/netconf/params/1.1"  xmlns:rw-mc="http://riftio.com/ns/riftware-1.0/rw-mc">
  <tailf:bad-keyref>
    <tailf:bad-element>/rw-mc:network-pool/rw-mc:pool[rw-mc:name='n1']/rw-mc:cloud-account</tailf:bad-element>
    <tailf:missing-element>/rw-mc:cloud/rw-mc:account[rw-mc:name='c1']</tailf:missing-element>
  </tailf:bad-keyref>
</error-info>
</rpc-error>
</rpc-reply>
        ''')

        expected_json = _collapse_string('''
{"rpc-reply" : {"rpc-error" : {"error-type" : "application","error-tag" : "data-missing","error-severity" : "error","error-app-tag" : "instance-required","error-path" : "/rpc/edit-config/config/rw-mc:network-pool/rw-mc:pool[rw-mc:name='n1']/rw-mc:cloud-account","error-message" : "illegal reference /network-pool/pool[name='n1']/cloud-account","error-info" : {"bad-keyref" : {"bad-element" : "/rw-mc:network-pool/rw-mc:pool[rw-mc:name='n1']/rw-mc:cloud-account","missing-element" : "/rw-mc:cloud/rw-mc:account[rw-mc:name='c1']"}}}}}
        ''')

        actual_json = naive_xml_to_json(bytes(xml,"utf-8"))

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)


    def test_conversion_CONFD_XML_to_JSON_convert_error_3(self):
        self.maxDiff = None

        xml = _collapse_string('''
<?xml version="1.0" encoding="UTF-8"?>
<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:ab56ec5e-92e3-11e5-83c5-fa163e622f12" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
    <ok/>
</rpc-reply>
        ''')

        expected_json = _collapse_string('''
        {"rpc-reply" : {"ok" : ""}}
        ''')

        actual_json = naive_xml_to_json(bytes(xml,"utf-8"))

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_CONFD_XML_to_JSON_convert_error_4(self):
        self.maxDiff = None

        xml = _collapse_string('''
 <?xml version="1.0" encoding="UTF-8"?>

<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:c7c92ac8-921b-11e5-8155-fa163e05ddfa" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0"><rpc-error>

<error-type>application</error-type>

<error-tag>data-missing</error-tag>

<error-severity>error</error-severity>

<error-app-tag>instance-required</error-app-tag>

<error-path xmlns:rw-mc="http://riftio.com/ns/riftware-1.0/rw-mc">

    /rpc/edit-config/config/rw-mc:mgmt-domain/rw-mc:domain[rw-mc:name='m1']/rw-mc:pools/rw-mc:network[rw-mc:name='n1']/rw-mc:name

  </error-path><error-message xml:lang="en">illegal reference /mgmt-domain/domain[name='m1']/pools/network[name='n1']/name</error-message><error-info xmlns:tailf="http://tail-f.com/ns/netconf/params/1.1"  xmlns:rw-mc="http://riftio.com/ns/riftware-1.0/rw-mc">

<tailf:bad-keyref>

<tailf:bad-element>/rw-mc:mgmt-domain/rw-mc:domain[rw-mc:name='m1']/rw-mc:pools/rw-mc:network[rw-mc:name='n1']/rw-mc:name</tailf:bad-element>

<tailf:missing-element>/rw-mc:network-pool/rw-mc:pool[rw-mc:name='n1']</tailf:missing-element>

</tailf:bad-keyref>

</error-info>

</rpc-error>

</rpc-reply>

        ''')

        expected_json = _collapse_string('''
{"rpc-reply" : {"rpc-error" : {"error-type" : "application","error-tag" : "data-missing","error-severity" : "error","error-app-tag" : "instance-required","error-path" : "/rpc/edit-config/config/rw-mc:mgmt-domain/rw-mc:domain[rw-mc:name='m1']/rw-mc:pools/rw-mc:network[rw-mc:name='n1']/rw-mc:name","error-message" : "illegal reference /mgmt-domain/domain[name='m1']/pools/network[name='n1']/name","error-info" : {"bad-keyref" : {"bad-element" : "/rw-mc:mgmt-domain/rw-mc:domain[rw-mc:name='m1']/rw-mc:pools/rw-mc:network[rw-mc:name='n1']/rw-mc:name","missing-element" : "/rw-mc:network-pool/rw-mc:pool[rw-mc:name='n1']"}}}}}
        ''')

        actual_json = naive_xml_to_json(bytes(xml,"utf-8"))

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    # POST /api/config/top-container-deep/inner-list-deep
    def test_conversion_post_container_list_1(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-deep/asdf"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-list-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><k>asdf</k><inner-container-shallow><a>fdsa</a></inner-container-shallow></inner-list-deep></top-container-deep></config>
        ''')

        body = _collapse_string('''
<inner-list-deep>
  <k>asdf</k>
  <inner-container-shallow>
    <a>fdsa</a>
  </inner-container-shallow>
</inner-list-deep>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("POST", url, (body,"xml"))

        self.assertEquals(actual_xml, expected_xml)

    # POST /api/config/top-container-deep/inner-list-deep
    def test_conversion_post_container_list_2(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><a>asdf</a><inner-list-deep><k>asdf</k><inner-container-shallow><a>fdsa</a></inner-container-shallow></inner-list-deep></top-container-deep></config>
        ''')

        body = _collapse_string('''
<top-container-deep>
<a>asdf</a>
<inner-list-deep>
  <k>asdf</k>
    <inner-container-shallow>
    <a>fdsa</a>
  </inner-container-shallow>
</inner-list-deep>
</top-container-deep>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("POST", url, (body,"xml"))

        self.assertEquals(actual_xml, expected_xml)

    # POST /api/config/top-container-deep/inner-list-deep
    def test_conversion_post_container_list_3(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-container"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-container xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><a>fdsa</a></inner-container></top-container-deep></config>
        ''')

        body = _collapse_string('''
<inner-container>
  <a>fdsa</a>
</inner-container>
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("POST", url, (body,"xml"))

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_rpc_to_xml_output(self):
        self.maxDiff = None
        xml = _collapse_string('''
<?xml version="1.0" encoding="UTF-8"?>
<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:0ba69bd8-938c-11e5-a933-fa163e622f12" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
    <out xmlns="http://riftio.com/ns/riftware-1.0/rw-restconf">true</out>
</rpc-reply>
        ''')

        expected_xml = _collapse_string('''
<output><out xmlns="http://riftio.com/ns/riftware-1.0/rw-restconf" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">true</out></output>
        ''')

        actual_xml = convert_rpc_to_xml_output(xml)

        self.assertEqual(actual_xml, expected_xml)

    def test_conversion_rpc_to_xml_output_1(self):
        self.maxDiff = None
        xml = _collapse_string('''
<?xml version="1.0" encoding="UTF-8"?>\n<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:0fc67048-93c4-11e5-bfc9-fa163e622f12" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0"><out1 xmlns="http://riftio.com/ns/riftware-1.0/rw-restconf">hello</out1>\n<out2 xmlns="http://riftio.com/ns/riftware-1.0/rw-restconf">world</out2>\n</rpc-reply>
        ''')

        expected_xml = _collapse_string('''
<output><out1 xmlns="http://riftio.com/ns/riftware-1.0/rw-restconf" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">hello</out1><out2 xmlns="http://riftio.com/ns/riftware-1.0/rw-restconf" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">world</out2></output>
        ''')

        actual_xml = convert_rpc_to_xml_output(xml)

        self.assertEqual(actual_xml, expected_xml)

    def test_conversion_rpc_to_json_output(self):
        self.maxDiff = None
        url = "/api/operations/ping"
        xml = _collapse_string('''
<?xml version="1.0" encoding="UTF-8"?>
<rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="urn:uuid:0ba69bd8-938c-11e5-a933-fa163e622f12" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
    <out xmlns='http://riftio.com/ns/riftware-1.0/rw-restconf'>true</out>
</rpc-reply>
        ''')

        expected_json = _collapse_string('''
{"output": {"out": "true"}}
        ''')

        intermediate_json = naive_xml_to_json(bytes(xml,"utf-8"))

        actual_json = convert_rpc_to_json_output(intermediate_json)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEqual(actual, expected)

    def test_conversion_XML_to_JSON_1(self):
        self.maxDiff = None
        url = "/api/operational/car/toyota/models"
        xml = _collapse_string('''
        <data>
          <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
            <models>
              <name-m>Camry</name-m>	  
              <year>2015</year>	  
              <capacity>5</capacity>	  
            </models>
          </car>
        </data>
        ''')

        expected_json = _collapse_string('''
{"vehicle-a:models":[{"year" : 2015,"name-m" : "Camry","capacity" : 5}]}
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(False, url, xpath, xml)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_XML_to_JSON_2(self):
        self.maxDiff = None
        url = "/api/operational/car/toyota/models"
        xml = _collapse_string('''
        <data>
          <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
            <models>
              <name-m>Camry</name-m>	  
              <year>2015</year>	  
              <capacity>5</capacity>	  
            </models>
            <models>
              <name-m>Tacoma</name-m>	  
              <year>2017</year>	  
              <capacity>7</capacity>	  
            </models>
          </car>
        </data>
        ''')

        expected_json = _collapse_string('''
{"collection":{"vehicle-a:models":[{"name-m" : "Camry","capacity" : 5,"year" : 2015},{"name-m" : "Tacoma","capacity" : 7,"year" : 2017}]}}
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(True, url, xpath, xml)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_post(self):
        self.maxDiff = None # expected_xml is too large

        url = "/api/config/top-container-deep/inner-list-shallow"
        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><top-container-deep xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><inner-list-shallow xmlns="http://riftio.com/ns/example" xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="create"><k>asdf</k></inner-list-shallow></top-container-deep></config>
        ''')

        body = _collapse_string('''
<inner-list-shallow xmlns="http://riftio.com/ns/example">
        <k>asdf</k>
</inner-list-shallow>
        ''')

        schema = load_multiple_schema_root(["vehicle-a"])

        converter = ConfdRestTranslator(schema)

        actual_xml = converter.convert("POST", url, (body,"xml"))

        self.assertEquals(actual_xml, expected_xml)

    def test_conversion_XML_to_JSON_null_string(self):
        self.maxDiff = None
        url = "/api/operational/car/toyota/models"
        xml = _collapse_string('''
        <data>
          <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
            <models>
              <name-m/>
            </models>
          </car>
        </data>
        ''')

        expected_json = _collapse_string('''
        {"vehicle-a:models":[{"name-m" : ""}]}
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(False, url, xpath, xml)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_POST_JSON_to_XML_rpc_negative(self):
        self.maxDiff = None

        url = '/api/operations/in-list-no-out'
        json = _collapse_string('''
{
  "input" : 
    {
"extra":"asdf"
    }
}
        ''')


        root = load_schema_root("vehicle-a")

        converter = ConfdRestTranslator(root)

        caught = False
        try:
            actual_xml = converter.convert("POST", url, (json,"application/data+json"))
        except ValueError:
            caught = True
        self.assertEquals(True, caught)

    def test_conversion_POST_JSON_to_XML_rpc_with_list_input(self):
        self.maxDiff = None

        url = '/api/operations/in-list-no-out'
        json = _collapse_string('''
{
  "input" : 
    {
        "in" :
          [
        {"k" : "asdf"},
        {"k" : "fdsa"}
          ]
    }
}
        ''')

        expected_xml = _collapse_string('''
<vehicle-a:in-list-no-out xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><in xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">asdf</k></in><in xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><k xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">fdsa</k></in></vehicle-a:in-list-no-out>

       ''')

        root = load_schema_root("vehicle-a")

        converter = ConfdRestTranslator(root)

        actual_xml = converter.convert("POST", url, (json,"application/data+json"))

        self.assertEqual(actual_xml, expected_xml)

    def test_conversion_XML_to_JSON_notification(self):
        yang_model = RwYang.Model.create_libncx()
        yang_model.load_module("vehicle-a")
        schema = yang_model.get_root_node()

        converter = XmlToJsonTranslator(schema, logger)

        expected_json = """
{"notification" : {"eventTime" : "2016-03-02T01:50:15.774039-05:00","vehicle-a:notif" : {"cont":{"cont-leaf" : "true","dict" : [{"value" : "bar","name" : "foo"},{"value" : "bar1","name" : "foo1"}]},"top-leaf" : 12}}}
"""
        notif_pb = VehicleAYang.YangNotif_VehicleA().create_notif()
        cont_pb = notif_pb.create_cont()
        dict_pb = cont_pb.create_dict()
        dict_pb.name = "foo"
        dict_pb.value = "bar"
        cont_pb.dict.append(dict_pb)
        dict_pb = cont_pb.create_dict()
        dict_pb.name = "foo1"
        dict_pb.value = "bar1"
        cont_pb.dict.append(dict_pb)
        cont_pb.cont_leaf = True
        notif_pb.cont = cont_pb
        notif_pb.top_leaf = 12

        content_xml = notif_pb.to_xml_v2(yang_model)
        notif_xml = """<notification xmlns="urn:ietf:params:xml:ns:netconf:notification:1.0"><eventTime>2016-03-02T01:50:15.774039-05:00</eventTime>"""
        notif_xml += content_xml
        notif_xml += "</notification>"

        json_str = converter.convert_notification(notif_xml)

        actual = _ordered(json.loads(json_str))
        expected = _ordered(json.loads(expected_json))
        self.assertEqual(actual, expected)
        

    def test_conversion_POST_JSON_to_XML_asdf(self):
        self.maxDiff = None

        url = '/api/operations/exec-ns-config-primitive'
        json = _collapse_string('''
{
    "input":{
        "vehicle-a:name":"create-user",
        "vehicle-a:nsr_id_ref":"1181c5ce-9c8b-4e1d-9247-2a38cdf128bb",
        "vehicle-a:vnf-primitive-group":[
            {
                "member-vnf-index-ref":"1",
                "vnfr-id-ref":"76d116e8-cf66-11e5-841e-6cb3113b406f",
                "primitive":[
                    {
                        "name":"number",
                        "value":"123"
                    },
                    {
                        "name":"password",
                        "value":"letmein"
                    }
                ]
            }
        ]
    }
}
        ''')

        expected_xml = _collapse_string('''
<vehicle-a:exec-ns-config-primitive xmlns:vehicle-a="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><name xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">create-user</name><nsr_id_ref xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">1181c5ce-9c8b-4e1d-9247-2a38cdf128bb</nsr_id_ref><vnf-primitive-group xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><member-vnf-index-ref xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">1</member-vnf-index-ref><vnfr-id-ref xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">76d116e8-cf66-11e5-841e-6cb3113b406f</vnfr-id-ref><primitive xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><name xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">number</name><value xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">123</value></primitive><primitive xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><name xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">password</name><value xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">letmein</value></primitive></vnf-primitive-group></vehicle-a:exec-ns-config-primitive>
        ''')

        root = load_schema_root("vehicle-a")

        converter = ConfdRestTranslator(root)

        actual_xml = converter.convert("POST", url, (json,"application/data+json"))

        self.assertEqual(actual_xml, expected_xml)


    def test_conversion_url_to_dts_xpath(self):
        self.maxDiff = None
        url = "/api/operational/car/toyota/models"
        expected = ""

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        actual = create_dts_xpath_from_url(url, schema)

        xpath = "D,/vehicle-a:car[vehicle-a:key_name = 'toyota']"
        value = RwKeyspec.path_from_xpath(VehicleAYang.get_schema(), actual, RwKeyspec.RwXpathType.KEYSPEC, None);



    def test_conversion_url_to_dts_xpath_2(self):
        self.maxDiff = None
        url = "/api/running/rwrestconf-configuration/" # trailing / on purpose
        expected = ""

        schema = load_multiple_schema_root(["rw-restconf"])

        actual = create_dts_xpath_from_url(url, schema)

        value = RwKeyspec.path_from_xpath(RwRestconfYang.get_schema(), actual, RwKeyspec.RwXpathType.KEYSPEC, None);

    def test_conversion_XML_to_JSON_vcs(self):
        self.maxDiff = None
        url = "/api/operational/vcs/info"
        xml = _collapse_string('''
<data>
<rw-base:vcs xmlns:rw-base="http://riftio.com/ns/riftware-1.0/rw-base">
    <rw-base:info>
        <rw-base:components>
            <rw-base:component_info>
                <rw-base:instance_name>RW_VM_MASTER-2</rw-base:instance_name>
                <rw-base:component_type>RWVM</rw-base:component_type>
                <rw-base:component_name>RW_VM_MASTER</rw-base:component_name>
                <rw-base:instance_id>2</rw-base:instance_id>
                <rw-base:state>RUNNING</rw-base:state>
                <rw-base:config-ready>true</rw-base:config-ready>
                <rw-base:recovery-action>FAILCRITICAL</rw-base:recovery-action>
                <rw-base:rwcomponent_parent>rw.colony-102</rw-base:rwcomponent_parent>
                <rw-base:rwcomponent_children>msgbroker-100</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>dtsrouter-101</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>RW.Proc_1.uAgent-103</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>RW.TermIO-105</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>RW.CLI-104</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>RW.Proc_2.Restconf-106</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>RW.Proc_3.RestPortForward-108</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>RW.MC.UI-109</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>logd-110</rw-base:rwcomponent_children>
                <rw-base:rwcomponent_children>confd-114</rw-base:rwcomponent_children>
                <rw-base:vm_info>
                    <rw-base:vm-ip-address>127.0.2.1</rw-base:vm-ip-address>
                    <rw-base:pid>31610</rw-base:pid>
                    <rw-base:leader>true</rw-base:leader>
                </rw-base:vm_info>
            </rw-base:component_info>
        </rw-base:components>
    </rw-base:info>
</rw-base:vcs>
</data>
        ''')

        schema = load_multiple_schema_root(["rw-base"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(False, url, xpath, xml)

    def test_conversion_url_to_dts_xpath_2(self):
        self.maxDiff = None
        url = "/api/running/rwrestconf-configuration/" # trailing / on purpose
        expected = ""

        schema = load_multiple_schema_root(["rw-restconf"])

        actual = create_dts_xpath_from_url(url, schema)

        value = RwKeyspec.path_from_xpath(RwRestconfYang.get_schema(), actual, RwKeyspec.RwXpathType.KEYSPEC, None);

    def test_conversion_PUT_JSON_to_XML_199(self):
        self.maxDiff = None

        url = '/api/running/rwrestconf-configuration/log-timing'
        json = _collapse_string('''
{"log-timing" : "True"
}

       ''')

        root = load_schema_root("rw-restconf")

        converter = ConfdRestTranslator(root)

        actual_xml = converter.convert("POST", url, (json,"application/data+json"))

    def test_dts_xpath_multi_key(self):
        self.maxDiff = None
        url = "/api/config/multi-key/k1,k2"

        expected_xpath = "C,/vehicle-a:multi-key[foo = 'k1'][bar = 'k2']"

        schema = load_multiple_schema_root(["vehicle-a"])

        actual_xpath = create_dts_xpath_from_url(url, schema)

        self.assertEquals(actual_xpath, expected_xpath)


    def test_conversion_XML_to_JSON_version(self):
        self.maxDiff = None
        url = "/api/config/car/toyota/models"
        xml = _collapse_string('''
<data>
          <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
            <models>
              <name-m>Camry</name-m>	  
              <year>2015</year>	  
              <capacity>5</capacity>	  
              <version_str>2.0</version_str>
            </models>
          </car>
        </data>
        ''')
        #

        expected_json = _collapse_string('''
{"vehicle-a:models":[{"year" : 2015,"name-m" : "Camry","capacity" : 5, "version_str":"2.0"}]}
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(False, url, xpath, xml)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_XML_to_JSON_get_leaf(self):
        self.maxDiff = None
        url = "/api/config/car/toyota/models/Camry/year"
        xml = _collapse_string('''
<data>
          <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
            <models>
              <name-m>Camry</name-m>	  
              <year>2015</year>	  
              <capacity>5</capacity>	  
              <version_str>2.0</version_str>
            </models>
          </car>
        </data>
        ''')
        #

        expected_json = _collapse_string('''
        {"year" : "2015"}
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(False, url, xpath, xml)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)

    def test_conversion_PUT_JSON_to_XML_list_with_escaped_url(self):
        self.maxDiff = None

        url = '/api/config/car/subaru%252F1%252F1/models/WRX/'
        json = _collapse_string('''
{
            "models":{
                "name-m":"WRX",
                "year":2015,
                "capacity":5,
                "is-cool":"True"
            }


}
       ''')

        expected_xml = _collapse_string('''
<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0"><car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a"><brand>subaru/1/1</brand><models xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a" xc:operation="replace"><name-m xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">WRX</name-m><year xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">2015</year><capacity xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">5</capacity><is-cool xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">true</is-cool></models></car></config>
        ''')

        root = load_schema_root("vehicle-a")

        converter = ConfdRestTranslator(root)

        actual_xml = converter.convert("PUT", url, (json,"application/data+json"))

        self.assertEqual(actual_xml, expected_xml)

    def test_conversion_XML_to_JSON_get_list_with_name_of_child(self):
        self.maxDiff = None
        url = "/api/config/car/toyota/models/name-m"
        xml = _collapse_string('''
        <data>
          <car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
            <brand>toyota</brand>
            <models>
              <name-m>name-m</name-m>    
              <capacity>5</capacity>     
            </models>
          </car>
        </data>
        ''')
        #

        expected_json = _collapse_string('''
{"vehicle-a:models" :{"name-m" : "name-m","capacity" : 5}}
        ''')

        schema = load_multiple_schema_root(["vehicle-a","vehicle-augment-a"])

        converter = XmlToJsonTranslator(schema, logger)

        xpath = create_xpath_from_url(url, schema)
        
        actual_json = converter.convert(False, url, xpath, xml)

        actual = _ordered(json.loads(actual_json))
        expected = _ordered(json.loads(expected_json))

        self.assertEquals(actual, expected)


########################################
# BEGIN BOILERPLATE JENKINS INTEGRATION

def main():                                                                      
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])      
    unittest.main(testRunner=runner)   

if __name__ == '__main__':
    main()
