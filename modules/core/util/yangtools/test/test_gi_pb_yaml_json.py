#!/usr/bin/env python3
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
"""
import argparse
import json
import locale
import logging
import lxml.etree
import os
import sys
import unittest
import xmlrunner
import yaml

import rift.rwlib.translation.json2xml

import gi
gi.require_version('TestVehicleAYang', '1.0')
gi.require_version('RwYang', '1.0')

from gi.repository import (
        TestVehicleAYang,
        RwYang,
        )

logger = logging.getLogger(__name__)


class VehicleModelTest(unittest.TestCase):
    def setUp(self):
        self._model = RwYang.Model.create_libncx()
        self._model.load_module("test-vehicle-a")

    def verify_strict_yaml_round_trip(self, msg):
        yaml_str = msg.to_yaml(self._model)
        logger.debug("Serialized to YAML: \n%s", yaml_str)

        #Validate YAML
        yaml.load(yaml_str)

        from_msg = msg.__class__.from_yaml(self._model, yaml_str)
        self.assertEqual(msg, from_msg)

    def verify_strict_json_round_trip(self, msg):
        json_str = msg.to_json(self._model)
        logger.debug("Serialized to JSON: \n%s", json_str)

        # Validate the serialized message is valid json.
        json.loads(json_str)

        from_msg = msg.__class__.from_json(self._model, json_str)
        logger.debug("Deserialized from JSON: \n%s", from_msg)
        self.assertEqual(msg, from_msg)

    def verify_strict_xml_round_trip(self, msg):
        xml_str = msg.to_xml_v2(self._model)
        logger.debug("Serialized to XML: \n%s", xml_str)

        # Validate the serialized message is valid xml.
        lxml.etree.fromstring(xml_str)

        from_msg = msg.__class__.from_xml_v2(self._model, xml_str)
        logger.debug("Deserialized from xml: \n%s", from_msg)
        self.assertEqual(msg, from_msg)

    def negative_yaml_round_trip(self, msg):
        yaml_str = msg.to_yaml(self._model)
        logger.debug("Serialized to YAML: \n%s", yaml_str)

        # insert non-schema element
        data = yaml.load(yaml_str)
        data['non schema element'] = 'asdf'

        yaml_str = yaml.dump(data)
        logger.debug("Non-schema YAML: \n%s", yaml_str)

        # this should fail
        from_msg = msg.__class__.from_yaml(self._model, yaml_str)

    def negative_json_round_trip(self, msg):
        json_str = msg.to_json(self._model)
        logger.debug("Serialized to JSON: \n%s", json_str)

        # insert non-schema element
        data = json.loads(json_str)
        data['non schema element'] = 'asdf'

        json_str = json.dumps(data, indent=4)
        logger.debug("Non-schema JSON: \n%s", json_str)

        # this should fail
        from_msg = msg.__class__.from_json(self._model, json_str)

    def negative_xml_round_trip(self, msg):
        xml_str = msg.to_xml_v2(self._model)
        logger.debug("Serialized to XML: \n%s", xml_str)

        # insert non-schema element
        data = lxml.etree.fromstring(xml_str)
        bad_element = lxml.etree.SubElement(data, "non-schema-element")
        bad_element.text = "asdf"
        
        xml_str = lxml.etree.tostring(data, pretty_print=True).decode("utf-8")
        logger.debug("Non-schema XML: \n%s", xml_str)

        # this should fail
        from_msg = msg.__class__.from_xml_v2(self._model, xml_str)

    def verify_non_strict_yaml_round_trip(self, msg):
        yaml_str = msg.to_yaml(self._model)
        logger.debug("Serialized to YAML: \n%s", yaml_str)

        # insert non-schema element
        data = yaml.load(yaml_str)
        data['non schema element'] = 'asdf'

        yaml_str = yaml.dump(data)
        logger.debug("Non-schema YAML: \n%s", yaml_str)

        from_msg = msg.__class__.from_yaml(self._model, yaml_str, strict=False)

        logger.debug("Deserialized from YAML: \n%s", yaml_str)
        self.assertEqual(msg, from_msg)

    def verify_non_strict_json_round_trip(self, msg):
        json_str = msg.to_json(self._model)
        logger.debug("Serialized to JSON: \n%s", json_str)

        # insert non-schema element
        data = json.loads(json_str)
        data['non schema element'] = 'asdf'

        json_str = json.dumps(data, indent=4)
        logger.debug("Non-schema JSON: \n%s", json_str)

        from_msg = msg.__class__.from_json(self._model, json_str, strict=False)

        logger.debug("Deserialized from JSON: \n%s", json_str)
        self.assertEqual(msg, from_msg)

    def verify_non_strict_xml_round_trip(self, msg):
        xml_str = msg.to_xml_v2(self._model)
        logger.debug("Serialized to XML: \n%s", xml_str)

        # insert non-schema element
        data = lxml.etree.fromstring(xml_str)
        bad_element = lxml.etree.SubElement(data, "non-schema-element")
        bad_element.text = "asdf"
        
        xml_str = lxml.etree.tostring(data, pretty_print=True).decode("utf-8")
        logger.debug("Non-schema XML: \n%s", xml_str)

        from_msg = msg.__class__.from_xml_v2(self._model, xml_str, strict=False)

        logger.debug("Deserializied from XML: \n%s", xml_str)
        self.assertEqual(msg, from_msg)

    def verify_round_trips(self, msg, skip_xml_negative=False):
        logger.debug("Original message : \n%s\n", msg)

        logger.debug("### verify strict conversion")
        self.verify_strict_xml_round_trip(msg)
        self.verify_strict_json_round_trip(msg)
        self.verify_strict_yaml_round_trip(msg)
        
        logger.debug("### assert non-schema stuff fails")
        with self.assertRaises(rift.rwlib.translation.json2xml.InvalidSchemaException):
            self.negative_yaml_round_trip(msg)
        with self.assertRaises(rift.rwlib.translation.json2xml.InvalidSchemaException):
            self.negative_json_round_trip(msg)
        if not skip_xml_negative:
            # implementation for from_xml on leaves always ignores non-schema content
            with self.assertRaises(Exception):
                self.negative_xml_round_trip(msg)

        logger.debug("### verify non-strict conversion")
        self.verify_non_strict_xml_round_trip(msg)
        self.verify_non_strict_json_round_trip(msg)
        self.verify_non_strict_yaml_round_trip(msg)


class TestStrings(VehicleModelTest):
    def test_string(self):
        car = TestVehicleAYang.Car(brand="oldsmobile")
        self.verify_round_trips(car)

    def test_empty_string(self):
        car = TestVehicleAYang.Car(brand="")
        self.verify_round_trips(car)

    def test_special_chars(self):
        locale.setlocale(locale.LC_ALL, "en_US.UTF-8")
        car = TestVehicleAYang.Car(brand='!@#$%^*()/\"\'\\:;-=~`{}[]<>?|_.,♠\n')
        self.verify_round_trips(car)

    def test_multiline_string(self):
        car = TestVehicleAYang.Car(brand="""
        This
        is
        a
        multiline
        string""")
        self.verify_round_trips(car)

    def test_xml_string(self):
        # Took this example from:
	  #http://www.w3schools.com/xml/tryit.asp?filename=tryxml_parsertest2
        car = TestVehicleAYang.Car(brand="""<!DOCTYPE html>
<html>
<body>

<h1>W3Schools Internal Note</h1>
<div>
<b>To:</b> <span id="to"></span><br>
<b>From:</b> <span id="from"></span><br>
<b>Message:</b> <span id="message"></span>
</div>

<script>
var txt, parser, xmlDoc;
txt = "<note>" +
"<to>Tove</to>" +
"<from>Jani</from>" +
"<heading>Reminder</heading>" +
"<body>Don't forget me this weekend!</body>" +
"</note>";

parser = new DOMParser();
xmlDoc = parser.parseFromString(txt,"text/xml");

document.getElementById("to").innerHTML =
xmlDoc.getElementsByTagName("to")[0].childNodes[0].nodeValue;
document.getElementById("from").innerHTML =
xmlDoc.getElementsByTagName("from")[0].childNodes[0].nodeValue;
document.getElementById("message").innerHTML =
xmlDoc.getElementsByTagName("body")[0].childNodes[0].nodeValue;
</script>

</body>
</html>""")
        self.verify_round_trips(car)

    def test_special_strings(self):
        special_strings = ["True", "False", "Yes", "No", "None", "null", "Null", "NULL"]
        for string in special_strings:
            car = TestVehicleAYang.Car(brand=string)
            self.verify_round_trips(car)


class TestNumbers(VehicleModelTest):
    def test_uint8(self):
        values = [0, 255]
        for value in values:
            number = TestVehicleAYang.Numbers(uint8_leaf=value)
            self.verify_round_trips(number, skip_xml_negative=True)

    def test_int8(self):
        values = [-127, -1, 0, 128]
        for value in values:
            number = TestVehicleAYang.Numbers(int8_leaf=value)
            self.verify_round_trips(number, skip_xml_negative=True)

    def test_decimal(self):
        values = [-0.0, 0.1, -0.11, 1000000.55]
        for value in values:
            number = TestVehicleAYang.Numbers(decimal_leaf=value)
            self.verify_round_trips(number, skip_xml_negative=True)


class TestLists(VehicleModelTest):
    def test_string_leaf_list(self):
        car = TestVehicleAYang.Car(passenger_names=["fred", "rose", "pam"])
        self.verify_round_trips(car)

    def test_list_inside_container(self):
        cont = TestVehicleAYang.YangData_TestVehicleA_TopContainerDeep()
        cont.inner_list_shallow.add().k = "asdf"
        cont.inner_list_shallow.add().k = "fdas"
        self.verify_round_trips(cont)

    def test_deep_list(self):
        cont = TestVehicleAYang.YangData_TestVehicleA_TopContainerDeep()
        cont.inner_container.a = "asdf"
        cont.inner_container.inner_list.add().k = "fdsa"

        self.verify_round_trips(cont)

class TestMisc(VehicleModelTest):
    def test_bool_leaf(self):
        values = [True, False]
        for v in values:
            cont = TestVehicleAYang.YangData_TestVehicleA_Misc(bool_leaf=v)
            self.verify_round_trips(cont)

    def test_empty_leaf(self):
        cont = TestVehicleAYang.YangData_TestVehicleA_Misc()
        cont.empty_leaf = True

        self.assertTrue(cont.has_field("empty_leaf"))
        self.verify_round_trips(cont)

    def test_binary_leaf(self):
        cont = TestVehicleAYang.YangData_TestVehicleA_Misc()
        cont.binary_leaf = bytes([100, 233])
        self.verify_round_trips(cont)

        cont2 = TestVehicleAYang.YangData_TestVehicleA_Misc()
        cont2.binary_leaf = bytearray([100, 233])
        self.assertEqual(cont, cont2)

        self.verify_round_trips(cont)

    def test_enum_leaf(self):
        cont = TestVehicleAYang.YangData_TestVehicleA_Misc()
        cont.enum_leaf = "a"
        self.verify_round_trips(cont)

    def test_rpc_input(self):
        ping = TestVehicleAYang.YangInput_TestVehicleA_Ping()
        # in is a reserved keyword in python so cannot set usually way
        setattr(ping, "in", True)
        self.verify_round_trips(ping)

    def test_rpc_output(self):
        ping = TestVehicleAYang.YangOutput_TestVehicleA_Ping()
        ping.out = True
        self.verify_round_trips(ping)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action="store_true")
    parser.add_argument('-n', '--no-runner', action="store_true")

    args, unknown = parser.parse_known_args(argv)
    if args.no_runner:
        runner = None

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + unknown, testRunner=runner)

if __name__ == '__main__':
    main()
