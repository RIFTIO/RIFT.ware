# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 9/5/2015
# 

from enum import Enum
import lxml.etree

import tornado.escape

from rift.rwlib.xml import (
    collect_siblings,
    get_xml_element_name,
)


class Result(Enum):
    OK = 1
    Upgrade_In_Progress = 2
    Upgrade_Performed = 3
    Operation_Failed = 4
    Rpc_Error = 5
    Data_Exists = 6
    Unknown_Error = 7
    Commit_Failed = 8

class PayloadType(Enum):
    XML = "application/vnd.yang.data+xml"
    XML_COLLECTION = "application/vnd.yang.collection+xml"
    JSON = "application/vnd.yang.data+json"
    JSON_COLLECTION = "application/vnd.yang.collection.json"

class NetconfOperation(Enum):
    DELETE = "delete"
    GET = "get"
    GET_CONFIG = "get_config"
    CREATE = "create"
    REPLACE = "replace"
    RPC = "rpc"

def iterate_with_lookahead(iterable):
    '''Provides a way of knowing if you are the first or last iteration.'''
    iterator = iter(iterable)
    last = next(iterator)
    for val in iterator:
        yield last, False
        last = val
    yield last, True

def naive_xml_to_json(xml_string):
    if xml_string in (b"<data/>", b"<root/>"):
        return '{"success":""}'

    def walk(xml_node, depth=0):
        json = list()

        children = xml_node.getchildren()

        name = xml_node.tag.split("}")[-1]
        body = xml_node.text
        if body is not None:
            body = tornado.escape.json_encode(body)

        has_children = len(children) > 0

        if has_children:
            json.append('"%s" : {' % name)
        else:
            if body is not None:
                json.append('"%s" : %s' % (name, body))
            else:
                json.append('"%s" : ""' % (name))

        for child, is_last in iterate_with_lookahead(children):
            
            name = xml_node.tag.split("{")[-1]
            body = xml_node.text

            json.append(walk(child, depth + 1))

            if not is_last:
                json.append(',')

        if has_children:
            json.append('}')


        return ''.join(json)

            
    
    try:
        xml_string = bytes(xml_string, "utf-8")
    except TypeError:
        pass

    root_xml_node = lxml.etree.fromstring(xml_string)
    root_name = get_xml_element_name(root_xml_node)
    
    if root_name in ("data", "root"):
        root_xml_node = root_xml_node[0]
        root_name = get_xml_element_name(root_xml_node)

    json_string = walk(root_xml_node)

    return "{%s}" % json_string

