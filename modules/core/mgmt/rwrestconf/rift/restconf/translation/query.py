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
# Creation Date: 8/8/2015
# 

from collections import namedtuple
import itertools
import json
import urllib.parse
import lxml.etree as et
import tornado.escape

from gi.repository import RwYang
from rift.rwlib.schema import (
    get_key_names,
)

from rift.rwlib.translation import (
    json_to_xml,
)

from ..util import (
    find_child_by_name,
    split_url,
    collect_children,
    NetconfOperation,
    create_xpath_from_url,
)

OPERATION_NAMESPACE = "urn:ietf:params:xml:ns:netconf:base:1.0"

def get_prefixed_node_name(schema_node):
    return "%s:%s" % (schema_node.get_prefix(),
		      schema_node.get_name())



class JsonToXmlTranslator(object):
    '''Used to convert rooted JSON into equivalent NETCONF XML

    The json library's representation of JSON can be thought of as a tree of
    dicts, lists, and leaves. This class walks that tree shape and emits the
    equivalent NETCONF XML.
    '''
    def __init__(self, schema_root, config_operation = None):
        '''
        schema_root : the schema node parent of the json to translate
        config_operation : the netconf edit operation
        '''
        self._config_operation = config_operation

        self._schema_node = schema_root

        self._handler = {
            type(None) : self._handle_empty,
            bool : self._handle_bool,
            dict : self._handle_dict,
            float : self._handle_numeric,
            int : self._handle_numeric,
            str : self._handle_str,
        }

    def convert(self, json_string):
        json_dicts = json.loads(json_string)
        
        # make sure RPC's have "input" field
        if self._schema_node.is_rpc():
            try:
                json_dicts["input"]
            except KeyError:
                raise ValueError("rpc must have input element")

        return self._handle_dict(json_dicts, self._schema_node)

    def convert_at_target(self, json_string):
        """Converts the JSON string to XML at the translator's schema node.
        
        Arguments:
          json_string - JSON string to be converted to XML

        This is used for PUT(v2) operation on leaf's and containers.
        """
        json_dicts = json.loads(json_string)

        if self._schema_node.get_name() in json_dicts:
            key = self._schema_node.get_name()
            value = json_dicts[key]
        elif get_prefixed_node_name(self._schema_node) in json_dicts:
            key = get_prefixed_node_name(self._schema_node)
            value = json_dicts[key]
        else:
            raise ValueError( "Message body resource should match the "
                "target resource %s" % get_prefixed_node_name(self._schema_node))

        return self._handler[type(value)](value, self._schema_node, key)

    def _handle_dict(self, json_node, schema_node, key=None):
        '''Translate a dict representaion of JSON into NETCONF XML

        In the json library dict's hold all the data. They either contain more
        "subtrees", or they map names to the value that is being set.

        In NETCONF rpcs the order the inputs are specified has to match the order
        they are defined in the yang file, so this will emit all NETCONF XML in
        that order.

        json_node -- the current subtree being translated

        '''

        xml = list()

        schema_children = collect_children(schema_node)

        if schema_children:
            # When using the to_json() to serialize a Proto-GI list element,
            # the parent container name becomes the top-most element (A rooted element).
            # Since we do not have a mgmt API to create a unrooted XML/JSON(RIFT-9034). To prevent
            # a client from having to manually strip the parent element, add logic here
            # to detect that exact situation and move the json dict foward to the child.
            #
            # If a child key is not at the top of the message, check
            # if the parent key is at the top and ONLY contains a single child.
            # If so, move json_node down to the parent dictionary value.

            child_node_names = [n.get_name() for n in schema_children]
            child_prefixed_node_names = [get_prefixed_node_name(n) for n in schema_children]

            child_unprefixed_nodes_in_json = [n for n in child_node_names if n in json_node]
            child_prefixed_nodes_in_json = [n for n in child_prefixed_node_names if n in json_node]

            child_nodes_in_json = child_prefixed_nodes_in_json or child_unprefixed_nodes_in_json
            if not child_nodes_in_json:
                parent_node_prefixed_name = get_prefixed_node_name(schema_node)
                if parent_node_prefixed_name in json_node:
                    child_prefixed_nodes_under_parent = [n for n in child_prefixed_node_names if n in json_node[parent_node_prefixed_name]]
                    if len(child_prefixed_nodes_under_parent) == 1:
                        json_node = json_node[parent_node_prefixed_name]


        for child_schema_node in schema_children:

            try:
                values = json_node[child_schema_node.get_name()]
                key = child_schema_node.get_name()
                del json_node[child_schema_node.get_name()]
            except KeyError:
                try:
                    values = json_node[get_prefixed_node_name(child_schema_node)]
                    key = child_schema_node.get_name()
                    del json_node["%s:%s" %(child_schema_node.get_prefix(),child_schema_node.get_name())]
                except KeyError:
                    continue



            schema_prefix = child_schema_node.get_prefix()
            schema_namespace = child_schema_node.get_ns()
            schema_node_name = child_schema_node.get_name()

            if type(values) == list:
                if child_schema_node.is_listy():
                    xml.append(self._handle_list(values, child_schema_node, key))
                elif child_schema_node.node_type().leaf_type == RwYang.LeafType.LEAF_TYPE_EMPTY:
                    # it's an empty type
                    xml.append(self._handle_empty(values, child_schema_node, key))
                else:
                    raise ValueError("%s isn't a list" % child_schema_node.get_name())

            elif child_schema_node.get_name() == "input" and child_schema_node.is_rpc_input():
                # don't specify "input" element
                xml.append(self._handler[type(values)](values, child_schema_node, key))
            else:
                xml.append('<%s xmlns="%s">' % (schema_node_name, schema_namespace))
                xml.append(self._handler[type(values)](values, child_schema_node, key))
                xml.append("</%s>" % (schema_node_name))

        if len(schema_children) == 0:
            xml.append('')
        if len(json_node.keys()) > 0:
            raise ValueError("extra fields given %s" % json_node.keys())
        
        return ''.join(xml)

    def _handle_list(self, json_node, schema_node, key=None):
        # lists only aggregate other structures
        xml = list()

        namespace = schema_node.get_ns()
        for item in json_node:
            xml.append('<%s xmlns="%s">' % (key, namespace))
            #if schema_node.is_rpc_input():
            #    xml.append('<%s xmlns="%s">' % (key, namespace))
            #else:
            #    xml.append('<%s xmlns="%s" xc:operation="%s">' % (key, namespace, self._config_operation))

            xml.append(self._handler[type(item)](item, schema_node))
            xml.append("</%s>" % (key))

        return ''.join(xml)

    def _handle_bool(self, json_node, schema_node, key=None):
        if json_node:
            return "true"
        else:
            return "false"

    def _handle_empty(self, json_node, schema_node, key=None):
        return '<%s xmlns="%s"></%s>' % (schema_node.get_name(), schema_node.get_ns(), schema_node.get_name())

    def _handle_numeric(self, json_node, schema_node, key=None):
        return str(json_node)

    def _handle_str(self, json_node, schema_node, key=None):
        actual_type = schema_node.node_type().get_leaf_type()

        if actual_type is RwYang.LeafType.LEAF_TYPE_BOOLEAN:
            return json_node.lower()
        else:
            return tornado.escape.xhtml_escape(json_node)

class ConfdRestTranslator(object):
    """Used to convert CONFD REST requests into valid NETCONF xml markup."""

    def __init__(self, schema):
        """
        schema -- the schema to use for the translation
        """

        if not schema:
            raise ValueError("schema not loaded")

        self._schema = schema

    def convert(self, operation, url, body):
        """Entry point for the recursive translation of the URL.

        operation -- the HTTP operation being converted
        url -- the URL to be translated
        body -- the body of the HTML request

        XML elements have both an opening, and closing syntax. So the natural
        way to ensure these elements end up in the correct position is to do the
        generation recursively.
        """
        operation = operation
        body = body

        pieces, _, queries, is_operation, api_ver = split_url(url)

        if pieces[-1] == '':
            del pieces[-1]

        root_node = self._schema

        root_namespace = root_node.get_ns()

        xml = list()

        if operation in ("DELETE","PUT","POST") and not is_operation:
            xml.append('<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">')

        xml.append(self._convert_pieces(root_node, operation, body, pieces, queries, is_operation))

        if operation in ("DELETE","PUT","POST") and not is_operation:
            xml.append('</config>')

        return ''.join(xml)

    def _convert_pieces(self, root_node, operation, body, pieces, queries, is_operation, depth=0):
        """Used to convert everything in the URL except the last element, and queries
        root_node -- the YangNode that is the parent of possible elements to translate
        pieces -- the remaining pieces of the URL to translate
        """
        xml = list()

        if pieces:
            current_name = pieces.pop(0)
            current_node = find_child_by_name(root_node, current_name)
        else:
            current_name = ''
            current_node = root_node


        # PUTs and non-rpc POSTs contain the target element in the body so it need not be generated from the url
        if len(pieces) <= 0:
            # top level GETs are pruned for only the keys
            if operation == "GET":
                return self._convert_target(root_node, operation, body, queries, is_operation, current_name, depth < 1)
            else:
                return self._convert_target(root_node, operation, body, queries, is_operation, current_name, False)


        current_namespace = current_node.get_ns()
        current_name_wo_prefix = current_name[(current_name.find(':') + 1):]

        if current_node != root_node:
            xml.append('<%s xmlns="%s">' % (current_name_wo_prefix, current_namespace))

        if current_node.is_listy() and len(pieces) > 0:
            possible_keys = pieces.pop(0)
            url_keys =possible_keys.split(",")
            actual_keys = get_key_names(current_node)

            if len(url_keys) != len(actual_keys):
                raise ValueError("url keys don't match actual keys")
          
            if operation == "DELETE" and len(pieces) == 0:
                xml[-1] = xml[-1].replace('>', ' xc:operation="delete">', 1)

            for key_name, key_string in zip(actual_keys, url_keys):
                while "%" in key_string:
                # names can be doubly encoded
                # ex: trafgen%252F2%252F1 gets unquoted into trafgen%2F2%2F1 which
                #     finally gets unquoted into trafgen/2/1
                    key_string = urllib.parse.unquote(key_string)

                xml.append('<%s>%s</%s>' % (key_name, key_string, key_name))

            if len(pieces) == 0 and operation in ["PUT", "POST"]:
                return self._convert_target(root_node, operation, body, queries, is_operation, root_node.get_name(), False)

        if len(pieces) > 0:
            xml.append(self._convert_pieces(current_node, operation, body, pieces, queries, is_operation, depth+1))

        xml.append('</%s>' % (current_name_wo_prefix))

        return ''.join(xml)

    def _convert_target(self, root_node, operation, body, queries, is_operation, target, prune_for_keys):
        """Determins how to convert the last element, and queries

        root_node -- the YangNode that is the parent of the target
        """
        if operation == "GET":
            return self._convert_GET_DELETE_target(root_node, operation, body, queries, is_operation, target, prune_for_keys)
        elif operation == "DELETE":
            xml = self._convert_GET_DELETE_target(root_node, operation, body, queries, is_operation, target, prune_for_keys)
            if xml.find("/>"):
                xml = xml.replace("/>", ' xc:operation="delete"/>', 1)
            else:
                xml = xml.replace('>', ' xc:operation="delete">', 1)

            xml = xml.replace('" / xc', '" xc', 1)
            return xml

        elif operation in ("POST", "PUT"):
            return self._convert_POST_PUT_target(root_node, operation, body, queries, is_operation, target)
        else:
            raise ValueError("unknown RESTCONF operation: %s" % operation)

    def _convert_GET_container(self, root_node, queries):
        xml = list()
        children = collect_children(root_node)
        name = root_node.get_name()
        namespace = root_node.get_ns()

        if queries.select is not None:
            if name in queries.select:
                return '<%s xmlns="%s" />' % (name, namespace)
        else:
            # open the container
            xml.append('<%s xmlns="%s">' % (name,namespace))

        for child in children:
            if queries.select is not None:
                if not child.get_name() in queries.select:
                    continue

            # select the child
            if child.is_listy():
                child_xml = self._convert_GET_list(child, queries)
            elif child.is_leafy():
                child_xml = self._convert_GET_leaf(child, queries)
            else: # it's a container
                child_xml = self._convert_GET_container(child, queries)

            xml.append(child_xml)

        if queries.select is None:
            # close the container
            xml.append('</%s>' % name)

        return ''.join(xml)

    def _convert_GET_list(self, root_node, queries):
        xml = list()
        children = collect_children(root_node)
        name = root_node.get_name()
        namespace = root_node.get_ns()

        if queries.select is not None:
            if name in queries.select:
                return '<%s xmlns="%s" />' % (name, namespace)
        else:
            # open the container
            xml.append('<%s xmlns="%s">' % (name,namespace))

        for child in children:
            if queries.select is not None:
                if not child.get_name() in queries.select:
                    continue

            # select the child
            if child.is_listy():
                child_xml = self._convert_GET_list(child, queries)
            elif child.is_leafy():
                child_xml = self._convert_GET_leaf(child, queries)
            else: # it's a container
                child_xml = self._convert_GET_container(child, queries)

            xml.append(child_xml)

        if queries.select is None:
            # close the container
            xml.append('</%s>' % name)

        return ''.join(xml)

    def _convert_GET_leaf(self, root_node, queries, is_target=False):
        xml = list()
        name = root_node.get_name()

        emit = is_target or (queries.deep) or ((not queries.deep) and root_node.is_key())

        if queries.select is not None:
            emit = emit or name in queries.select

        if emit:
            return'<%s />' % root_node.get_name()
        return ''

    def _convert_GET_DELETE_target(self, root_node, operation, body, queries, is_operation, target, prune_for_keys):
        """Used to convert the last element and queries for GETs

        root_node -- the YangNode that is the parent of the target
        """
        treat_target_as_key = False

        root_namespace = root_node.get_ns()
        root_name = root_node.get_name()

        _queries = _parse_queries(queries)

        target_node = find_child_by_name(root_node, target)

        if target_node is None:
            treat_target_as_key = root_node.has_keys() and root_name != target
            target_node = root_node

            if not treat_target_as_key and target != root_node.get_name():
                raise ValueError("node %s is not a child of %s" % (target, root_node.get_name()))

        target_namespace = target_node.get_ns()
        target_name = target_node.get_name()

        if treat_target_as_key:
            key_name = root_node.get_first_key().get_key_node().get_name()
            return '<%s>%s</%s>' % (key_name, target, key_name)

        target_is_container = not (target_node.is_listy() or target_node.is_leafy())
        if operation == "DELETE" and (not treat_target_as_key) and target_is_container:
            raise ValueError("can't delete container")

        if _queries.deep:
            name = target_node.get_name()
            namespace = target_node.get_ns()
            return '<%s xmlns="%s" />' % (name, namespace)

        if not prune_for_keys and _queries.select is None:
            name = target_node.get_name()
            namespace = target_node.get_ns()
            return '<%s xmlns="%s" />' % (name, namespace)

        if target_node.is_listy():
            xml = self._convert_GET_list(target_node, _queries)
        elif target_node.is_leafy():
            xml = self._convert_GET_leaf(target_node, _queries, is_target=True)
        else:
            # it's a container
            xml = self._convert_GET_container(target_node, _queries)

        if (_queries.select is not None) and (not target_node.is_leafy()):
            xml =  '<%s xmlns="%s">%s</%s>' % (target_name, target_namespace, xml, target_name)

        return xml


    def _convert_POST_PUT_target(self, root_node, operation, body, queries, is_operation, target):
        """Used to convert the last element and queries for POSTs, and PUTs

        root_node -- the YangNode that is the parent of the target
        """

        if operation == "PUT":
            config_operation = "replace"
        elif operation == "POST":
            config_operation = "create"
        else:
            raise ValueErro("unknown operation %s" % operation)

        target_node = find_child_by_name(root_node, target)

        if target_node is None:
            target_node = root_node
            emit_target = False
        else:
            emit_target = True


        if "xml" in body[1]:
            is_xml = True
            xml_body = body[0]

            if not is_operation:
                emit_target = False
            elif is_operation:
                emit_target = True
                if "<input" not in xml_body:
                    raise ValueError("improper rpc input")
                # remove <input> and </input>
                input_start = xml_body.find("<input")
                start = xml_body.find('>', input_start + 1) + 1
                end = xml_body.rfind('</input>', start)
                xml_body = xml_body[start:end]

        else:
            if target_node.is_listy():
                translate_node = root_node
                emit_target = False
            elif target_node.is_leafy():
                translate_node = root_node
            else:
                translate_node = target_node

            xml_body = JsonToXmlTranslator(translate_node, config_operation).convert(body[0])
            is_xml = False

        target_name = target_node.get_name()
        target_namespace = target_node.get_ns()
        target_prefix = target_node.get_prefix()

        if not is_operation: 
            # json translator adds operation
            if is_xml:
                xml_body = xml_body.replace('>', ' xmlns="%s" xc:operation="%s">' %(target_namespace,config_operation), 1)
            else:
                xml_body = xml_body.replace('>', ' xc:operation="%s">' % config_operation, 1)

            should_have_translated = body[0] != ""
            did_translate = xml_body != ""
            if should_have_translated and not did_translate:
                raise ValueError("can't translate json body")

        if emit_target:
            return '<%s:%s xmlns:%s="%s">%s</%s:%s>' % (target_prefix,target_name, target_prefix, target_namespace, xml_body, target_prefix, target_name)
        else:
            return xml_body


class ConfdRestTranslatorV2(ConfdRestTranslator):
    """Specialized version for API v2. In this API there is a marked difference
    between PUT and POST semantics. In PUT the target URL points to the
    resource to be replaced. However in POST, the target URL point to the
    parent resource into which a new resource is to be created.
    """
    def __init__(self, schema):
        """Constructor for the RestTranslator V2
        
        Arguments:
          schema - the schema to use for the translation
        """
        super(ConfdRestTranslatorV2, self).__init__(schema)

    def _json_to_xml(self, root_node, target_node, body):
        """Convert the JSON body at the given target_node to XML.

        Arguments:
          root_node   - Parent node of the target
          target_node - Node at which convertion to be performed
          body        - JSON body
        """
        if target_node.is_listy() or target_node.is_leafy():
            translate_node = root_node
        else:
            translate_node = target_node

        xml_body = JsonToXmlTranslator(translate_node).convert(body[0])
        return xml_body

    def _convert_pieces(self,
             root_node,
             operation,
             body,
             pieces,
             queries,
             is_operation,
             depth=0):
        """Specialized version of _convert_pieces for API V2.

        Arguments:
          root_node - Parent schema node
          operation - HTTP Method (PUT/POST/GET etc...)
          body      - HTTP Message body
          pieces    - URL Target split by '/'
          queries   - URL queries
          is_operation - Is this an operation (RPC)
          depth     - Depth of recursion

        In this version, the operation is tagged on the target, rather than on
        the first message-body node. Specialized for PUT(v2).
        """
        xml_body = super(ConfdRestTranslatorV2, self)._convert_pieces(
                      root_node, operation, body, pieces, queries,
                      is_operation, depth)

        if operation == "PUT":
            if not xml_body:
                raise ValueError("PUT target not found")
            if ' xc:operation="replace">' not in xml_body:
                # For PUT the target is required and the replace is applied on the
                # target URL node
                xml_body = xml_body.replace('>', ' xc:operation="replace">', 1)
        return xml_body

    def _convert_POST_PUT_target(self, 
            root_node,
            operation,
            body,
            queries,
            is_operation,
            target):
        """Used to convert the last element and queries for POSTs, and PUTs

        Arguments:
          root_node - the YangNode that is the parent of the target

        Specialized version for API v2. In this API there is a marked difference
        between PUT and POST semantics. In PUT the target URL points to the
        resource to be replaced. However in POST, the target URL point to the
        parent resource into which a new resource is to be created.
        """
        if operation == "POST" and is_operation:
            # This would never happen, v2 doesn't support /api/operation
            raise ValueError("V2 API does not support operation")

        target_node = find_child_by_name(root_node, target)
        if target_node is None:
            target_node = root_node
            emit_target = False
        else:
            emit_target = True

        if operation == "PUT":
            xml_body = self._convert_PUT_target(body, root_node, target_node)
        elif operation == "POST":
            xml_body = self._convert_POST_target(body, root_node, target_node)
        else:
            raise ValueError("unknown operation %s" % operation)

        if emit_target and not target_node.is_listy():
            target_name = target_node.get_name()
            target_namespace = target_node.get_ns()
            target_prefix = target_node.get_prefix()
            xml_body = '<%s:%s xmlns:%s="%s">%s</%s:%s>' % (target_prefix,
                      target_name, target_prefix, target_namespace, xml_body,
                      target_prefix, target_name)

        return xml_body

    def _convert_PUT_target(self, body, root_node, target_node):
        """Used to convert PUT target and the message-body to Netconf XML.

        Arguments:
          body        - Message body (can be xml/json)
          root_node   - Parent node of the target node
          target_node - Node pointed to by the Target URL
        """
        # For a PUT, the target must be specified in the URL
        if "xml" in body[1]:
            return body[0]
        
        if get_prefixed_node_name(target_node) == get_prefixed_node_name(root_node):
            # A list with Key is the target, fallback to V1 approach which already
            # handles it
            xml_body = self._json_to_xml(root_node, target_node, body)
        else:
            xml_body = JsonToXmlTranslator(target_node).convert_at_target(body[0])
            if target_node.is_listy():
                target_name = target_node.get_name()
                target_namespace = target_node.get_ns()
                target_prefix = target_node.get_prefix()
                xml_body = '<%s:%s xmlns:%s="%s">%s</%s:%s>' % (target_prefix,
                      target_name, target_prefix, target_namespace, xml_body,
                      target_prefix, target_name)

        return xml_body

    def _convert_POST_target(self, body, root_node, target_node):
        """Used to convert the POST target and the message body to Netconf XML.

        Arguments:
          body        - Message body (can be xml/json)
          root_node   - Parent node of the target node
          target_node - Node pointed to by the Target URL

        This is primarly the same as the v1.
        """
        if "xml" in body[1]:
            xml_body = body[0]
        else:
            xml_body = self._json_to_xml(root_node, target_node, body)
        
        xml_body = xml_body.replace('>', ' xc:operation="create">', 1)
        return xml_body


def _parse_queries(queries):
    parts = queries.split("=")
    query = parts[0]

    result = namedtuple('Queries', ['deep','select'])

    result.deep = False
    result.select = None

    if query == "":
        return result

    try:
        params = parts[1].split(";")
    except IndexError:
        params = None


    if query == "deep":
        result.deep = True
    elif query == "select":
        result.select = list()
        for param in params:
            if param == "":
                continue
            elif "(" in param and "(*)" not in param:
                raise ValueError("unsupported query option")

            if param.endswith("(*)"):
                param = param[:-3] # prune off '(*)'
            result.select.append(param)

    else:
        raise ValueError("unsupported query")

    return result

def convert_get_request_to_xml(schema_root, url):
    pieces, _, raw_queries, _, _ = split_url(url)
    queries = _parse_queries(raw_queries)

    ns_map = dict()
    
    # setup root node
    root_xml_node = et.Element("data", nsmap=ns_map)
    
    def walk_url(schema_root, xml_root, pieces, ns_map, depth=0):
        if len(pieces) == 0:
            # base case
            return

        current_piece = pieces[0]
        schema_node = find_child_by_name(schema_root, current_piece)

        parent_is_keyed_list = schema_root.is_listy() and not schema_root.is_leafy()
        if schema_node is None and parent_is_keyed_list:
            # the piece is intended to be a key
            actual_keys = get_key_names(schema_root)
            possible_keys = pieces[0]
            url_keys = possible_keys.split(",")

            for k,v in zip(actual_keys, url_keys):
                xml_node = et.SubElement(xml_root, k)
                xml_node.text = v
            return walk_url(schema_root, xml_root, pieces[1:], ns_map, depth+1)

        elif schema_node is None:
            # the piece isn't a key, and there is no node by that name
            raise ValueError("(%s) has no child (%s)" % (schema_root.get_name(), current_piece))
        
        # make sure the prefix -> namespace mapping is in our namespace dict
        schema_prefix = schema_node.get_prefix()
        schema_ns = schema_node.get_ns()
        if schema_prefix not in ns_map:
            ns_map[schema_prefix] = schema_ns

        schema_name = schema_node.get_name()

        xml_node = et.SubElement(xml_root, "{%s}%s" % (schema_ns, schema_name), nsmap=ns_map)

        return walk_url(schema_node, xml_node, pieces[1:], ns_map, depth+1)
        
    walk_url(schema_root, root_xml_node, pieces, ns_map)

    return et.tostring(root_xml_node).decode("utf-8")
