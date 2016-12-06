#!/usr/bin/env python

# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Austin Cormier
# Creation Date: 12/01/2014
# 
#
# XML Utility functions

import dicttoxml
import difflib
import xmltodict
import xml.dom.minidom as xml_minidom
import rift.auto.json_utils
import logging

# to prevent display of so many info/warning log messages from the 'dicttoxml' package
dicttoxml.LOG.propagate = False

def pretty_xml(xml_str):
    """Creates a more human readable XML string by using
    new lines and indentation where appropriate.

    Arguments:
        xml_str - An ugly, but valid XML string.
    """
    return xml_minidom.parseString(xml_str).toprettyxml('  ')


def compare_xml(xml_1, xml_2, ignore_order=True):
    """
    To compare xml contents

    Arguments:
       xml_1, xml_2 - xml strings to be compared
       ignore_order - Flag denotes whether to ignore the order
                      of xml elements
    """
    xml_equls = False
    if ignore_order:
        x1d = xmltodict.parse(xml_1)
        x2d = xmltodict.parse(xml_2)
        return rift.auto.json_utils.compare_json(x1d, x2d)
    else:
        xml_1 = pretty_xml(xml_1)
        xml_2 = pretty_xml(xml_2)
        return xml_1 == xml_2


def ordered_xml(xmlstr):
    """Re-oder the xml elements in sorted order

    Arguments:
        xmlstr - xml string to be re-ordered
    """
    xd = xmltodict.parse(xmlstr)
    od = rift.auto.json_utils.ordered_json(xd)
    return dicttoxml.dicttoxml(od)

def xml_diff(xml_1, xml_2, ignore_order=True):
    """This function will generate string showing the differences
       between two json data (similar to the Linux 'diff' command output)
       ignore_order - Flag denotes whether to ignore the
                      order of dictionary keys and list items

    Arguments:
        xml_1, xml_1 - XML contents (strings) to be compared

    Return:
        String describing the differences (similar to the
        Linux 'diff' command output)
    """
    if ignore_order:
        xml_1 = ordered_xml(xml_1)
        xml_2 = ordered_xml(xml_2)

    pp1 = pretty_xml(xml_1)
    pp2 = pretty_xml(xml_2)

    pp_lst1 = [item for item in pp1.split('\n') if item.strip()]
    pp_lst2 = [item for item in pp2.split('\n') if item.strip()]

    diff_lines = [line for line in difflib.context_diff(pp_lst1, pp_lst2)]
    return '\n'.join(diff_lines)
