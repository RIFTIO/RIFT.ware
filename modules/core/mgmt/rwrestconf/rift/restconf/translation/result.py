# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 7/10/2015
# 

import json    
import lxml.etree

import tornado.escape

from ..util import (
    find_child_by_name,
    find_child_by_path,
    find_target_type,
    split_url,
    TargetType,
)

import gi
gi.require_version('RwYang', '1.0')

from gi.repository import RwYang
from rift.rwlib.schema import collect_children
from rift.rwlib.util import iterate_with_lookahead
from rift.rwlib.xml import (
    collect_siblings,
)

def _split_namespace(namespace):
    split_namespace = namespace.split("/")

    if len(split_namespace) == 1:
        split_namespace = namespace.split(":")            

    prefix = '%s:' % split_namespace[-1]
    return prefix

def _split_tag_and_update_prefixes(element, prefixes):
    ''' Strip out unqualified name, and it's associated prefix, and maintain prefixes stack'''
    pieces = element.tag.split("}")

    try:
        name = pieces[-1]
        namespace = pieces[0]
        prefix = _split_namespace(namespace)

        if prefix.startswith("{"):
            # prune leading "{"
            prefix = prefix[1:] 

        if not prefixes:
            prefixes.append(prefix)
        elif prefixes[-1] == prefix:
            prefix = ""
        else:
            prefixes.append(prefix)
    except IndexError as e:
        name = pieces[0]
        prefix = ""

    return name, prefix, prefixes


class XmlToJsonTranslator(object):
    ''' Converts the results of a NETCONF query into JSON.'''

    def __init__(self, schema, logger):
        self._schema = schema
        self._logger = logger

    def convert(self, is_collection, url, xpath, xml_string):
        ''' Entry for XML -> JSON conversion
        
        is_collection -- True if the results is in the collection+json format
        xpath -- the xpath corresponding to the query results being translated
        xml_string -- the XML results to translate
        '''

        url_pieces, *_ = split_url(url)

        schema_root = find_child_by_path(self._schema, url_pieces)
        target_type = find_target_type(self._schema, url_pieces)
        xml_root = lxml.etree.fromstring(xml_string)

        elements_to_translate = xml_root.xpath(xpath)
        json = list()
        prefixes = list()

        is_list = schema_root.is_listy() and target_type == TargetType.List
        is_leaf = schema_root.is_leafy()

        json.append("{")
        if is_collection or is_list:

            pieces = elements_to_translate[0].tag.split("}")
            try:
                target_name = pieces[1]
                target_prefix = _split_namespace(schema_root.get_ns())
                prefixes.append(target_prefix)
            except IndexError:
                target_name = pieces[0]
                target_prefix = ""

                
        if is_collection:
            json.append('"collection":{')
            json.append('"%s%s":[' % (target_prefix, target_name))

        elif is_list:
            json.append('"%s%s":[' % (target_prefix, target_name))


        for element, is_last in iterate_with_lookahead(elements_to_translate):

            name, prefix, prefixes = _split_tag_and_update_prefixes(element, prefixes)

            schema_node = schema_root

            if is_collection or is_list:
                json.append('{')
            elif is_leaf:
                pass
            else:
                json.append('"%s%s" :{' % (prefix,name))

            json.append(self._translate_node(is_collection, schema_root, element, prefixes))
            
            if not is_leaf:
                json.append('}')

            if not is_last:
                json.append(',')

            if prefix != "":
                prefixes.pop()

        if is_collection:
            json.append(']')
            json.append('}')
        elif is_list:
            json.append(']')            
        

        json.append("}")

        return ''.join(json)


    def convert_notification(self, xml_string):
        '''Converts the given XML Notification string into JSON

        Returns converted JSON string 
        '''
        # Assuming 'notification' tag and 'eventTime' to be present, since the
        # ncclient would have done the correct validation
        notification = lxml.etree.fromstring(xml_string)

        json = []

        json.append('{"notification" : ')
        json.append('{"eventTime" : ')
        # The first element in notification is always 'eventTime'
        json.append('"' + notification[0].text + '"') 
        json.append(",")
        prefixes = []
        for element in notification[1:]:
            qelem = lxml.etree.QName(element)
            name, prefix, prefixes = _split_tag_and_update_prefixes(element, prefixes)
            schema_node = self._schema.search_child(qelem.localname,
                                                    qelem.namespace)
            if schema_node is None:
                self._logger.error(
                    "Notification: Unable to find node %s{%s} in loaded schema",
                    qelem.localname, qelem.namespace)
                return None

            # Node should be of type notification
            if schema_node.get_stmt_type() != RwYang.StmtType.STMT_TYPE_NOTIF:
                self._logger.error(
                    "Notification schema node - invalid type. Expected "+\
                    "STMT_TYPE_NOTIF, got %s", schema_node.get_stmt_type())
                return None

            json.append('"%s%s" : {' % (prefix, name))
            tstr = self._translate_node(False, schema_node, element, prefixes)
            json.append(tstr)
            json.append("}")
            json.append(",")
        json.pop()  # Pops the last ','

        json.append("}")
        json.append("}")

        result = ''.join(json)
        return result


    def _translate_node(self, is_collection, schema_root, xml_node, prefixes, depth=0):
        ''' Translates the given XML node into JSON

        is_collection -- True if the results is in the collection+json format
        xml_node -- the current XML element to translate
        '''
        json = list()
        first_child = False

        current_list = None
        current_name = schema_root.get_name()        
        if schema_root.is_leafy():
            schema_root = schema_root.get_parent()

        siblings = collect_siblings(xml_node)

        if len(siblings) == 0:
            if xml_node.text is None:
                return '"empty":""' 
            else:
                return '"%s":"%s"' % (current_name,xml_node.text)


        for sibling_tag, is_last in iterate_with_lookahead(siblings):
            
            sib_xml_nodes = siblings[sibling_tag]            

            child_schema_node = find_child_by_name(schema_root, sibling_tag)

            has_siblings = child_schema_node.is_listy()

            if has_siblings:
                sib_name, sib_prefix, prefixes = _split_tag_and_update_prefixes(sib_xml_nodes[0], prefixes)

                json.append('"%s%s" : [' % (sib_prefix, sib_name))

            for child_xml_node, sib_is_last  in iterate_with_lookahead(sib_xml_nodes):

                name, prefix, prefixes = _split_tag_and_update_prefixes(child_xml_node, prefixes)
                value = child_xml_node.text
                if not child_schema_node.is_leafy():
                    # it's a container, so iterate over children
                    if has_siblings:
                        json.append('{')
                    else:
                        json.append('"%s%s":{' % (prefix,name))
    
                    body = self._translate_node(is_collection,
                                                     child_schema_node,
                                                     child_xml_node,
                                                     prefixes,
                                                     depth+1,
                                            )

                    json.append(body)
                    json.append('}')
                            
                else:
                    if child_schema_node.node_type().leaf_type == RwYang.LeafType.LEAF_TYPE_EMPTY:
                        value = "[null]"
                    elif child_schema_node.node_type().leaf_type == RwYang.LeafType.LEAF_TYPE_STRING:
                        if not value:
                            value = '""'
                        else:
                            value = tornado.escape.json_encode(value)
                    else:

                        if not value:
                            value = '""'
                        else:
                            try:
                                float(value)
                            except ValueError:
                                value = tornado.escape.json_encode(value)
                    
                    if has_siblings:
                        json.append('%s' % (value))
                    else:
                        json.append('"%s" : %s' % (name, value))

                if not sib_is_last:
                    json.append(',')

                if prefix != "":
                    prefixes.pop()

            if has_siblings:
                json.append(']')
                if sib_prefix != "":
                    prefixes.pop()        

            if not is_last:
                json.append(',')

        return ''.join(json)

def convert_xml_to_collection(url, xml_string):
    '''Translate the XML into a collection+xml format

    This strips off the elements corresponding to the given url and wraps the 
    remainder in a "collection" element.

    url -- the url corresponding to the query results being translated
    xml_string -- the XML results to translate    
    '''
    def find_collection_start(url_pieces, xml_node):
        ''' Walk down the XML tree and strip off the pieces coresponding to the url
        url_pieces -- the remaining url being converted, split on  "/"
        xml_node -- the current XML element to transform
        '''
        xml = list()
        if url_pieces:
            current_piece = url_pieces.pop(0)
            for child in xml_node:
                xml.append(find_collection_start(url_pieces, child))
        else:
            xml.append(lxml.etree.tostring(xml_node).decode("utf-8"))            

        return ''.join(xml)

    url_pieces, *_ = split_url(url)
    
    root = lxml.etree.fromstring(xml_string)    

    pruned_xml = find_collection_start(url_pieces, root)

    return "<collection>%s</collection>" % pruned_xml

def convert_rpc_to_xml_output(xml):
    # strip off <rpc-reply> tags
    root = lxml.etree.fromstring(bytes(xml, "utf-8"))
    top_levels = list()
    for e in root[:]:
        top_levels.append(lxml.etree.tostring(e).decode("utf-8"))

    return "<output>%s</output>" % "".join(top_levels)

def convert_rpc_to_json_output(json_string):
    real_dict = dict()

    json_dict = json.loads(json_string)
    real_dict['output'] = list(json_dict.values())[0]

    return json.dumps(real_dict)

        
