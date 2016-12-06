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
# Author(s): Max Beckett
# Creation Date: 9/5/2015
# 

import lxml.etree
import urllib.parse

import tornado.escape

from .schema import find_child_by_name
from .web import (
    split_url,
    is_config,
)
from rift.rwlib.schema import (
    get_key_names,
)

def create_xpath_from_url(url, schema):

    block = "/*[local-name()='%s']"

    def _build_xpath(url_pieces, schema_node):

        if not url_pieces:
            return ""

        current_piece = url_pieces.pop(0)

        if schema_node.is_listy():
            if len(url_pieces) == 0:
                return ""
            else:
                current_piece = url_pieces.pop(0)

        child_schema_node = find_child_by_name(schema_node, current_piece)
        
        if child_schema_node is None:
            if len(url_pieces) == 0:
                # it's a key at the end of the url
                return ""
            else:
                # it's a key in the middle of the url
                return _build_xpath(url_pieces, schema_node)

        current_prefix = child_schema_node.get_prefix()

        if schema_node.is_listy():
            child_schema_node = find_child_by_name(schema_node, current_piece)
            
        xpath_snippet = block % (child_schema_node.get_name())
        return xpath_snippet + _build_xpath(url_pieces, child_schema_node)

    url_pieces, *_ = split_url(url)

    xpath = _build_xpath(url_pieces, schema)

    xpath = "/*[local-name()='root' or local-name()='data']" + xpath

    return xpath

def create_dts_xpath_from_url(url, schema):
    def _build_xpath(url_pieces, schema_node):
        if not url_pieces:
            return ""

        if len(url_pieces) <= 0 :
            return ""

        current_piece = url_pieces.pop(0)

        if current_piece == "":
            return ""

        child_schema_node = find_child_by_name(schema_node, current_piece)
        piece_is_key = False

        xpath_snippet = ""

        
        prefix = child_schema_node.get_prefix()
        if child_schema_node.is_listy() and (len(url_pieces) > 0 and url_pieces[0] != ""):
            possible_keys = url_pieces.pop(0)
            url_keys = possible_keys.split(",")
            actual_keys = get_key_names(child_schema_node)
            first =  True
            predicate = "["
            for key_name, key_string in zip(actual_keys, url_keys):
                while "%" in key_string:
                # names can be doubly encoded
                # ex: trafgen%252F2%252F1 gets unquoted into trafgen%2F2%2F1 which
                #     finally gets unquoted into trafgen/2/1
                    key_string = urllib.parse.unquote(key_string)
                if not first:
                    predicate = predicate + "][%s = '%s'" % (key_name, tornado.escape.xhtml_escape(key_string))
                else:
                    predicate = predicate + "%s = '%s'" % (key_name, tornado.escape.xhtml_escape(key_string))
                    first = False
            predicate = predicate + "]"
            xpath_snippet = "%s:%s%s"%(prefix, child_schema_node.get_name(), predicate)

        else:
            xpath_snippet = "%s:%s"%(prefix,child_schema_node.get_name())

        if len(url_pieces) == 0:
            return "/" + xpath_snippet

        return "/" + xpath_snippet + _build_xpath(url_pieces, child_schema_node)


    url_pieces, _, _, is_operation, _ = split_url(url)
    config = is_config(url)


    xpath = _build_xpath(url_pieces, schema)

    if config:
        xpath = "C," + xpath
    elif is_operation:
        xpath = "O," + xpath
    elif not config:
        xpath = "D," + xpath

    return xpath

