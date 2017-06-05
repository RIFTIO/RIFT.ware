# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 9/13/15
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

import json

import tornado.escape

from ..schema import collect_children

class InvalidSchemaException(Exception):
    pass

def json_to_xml(schema_node, json_string, strict=True):
    '''Used to convert rooted JSON into equivalent XML

    The json library's representation of JSON can be thought of as a tree of
    dicts, lists, and leaves. This function walks that tree shape and emits the
    equivalent XML.
    '''
    def _handle_dict(json_node, schema_node, key=None):
        '''Translate a dict representaion of JSON into XML

        In the json library dict's hold all the data. They either contain more
        "subtrees", or they map names to the value that is being set.

        In  rpcs the order the inputs are specified has to match the order
        they are defined in the yang file, so this will emit all XML in
        that order.

        json_node -- the current subtree being translated

        '''
        xml = list()
        visited_children = set()

        if schema_node.is_rpc():
            # the "input" and "output" elements don't appear in the xml so combine all their child nodes
            input_node = schema_node.get_first_child()
            input_children = collect_children(input_node)
            
            output_node = input_node.get_next_sibling()
            output_children = collect_children(output_node)

            schema_children = input_children + output_children
        else:
            schema_children = collect_children(schema_node)

        for child_schema_node in schema_children:

            schema_prefix = child_schema_node.get_prefix()
            schema_namespace = child_schema_node.get_ns()
            schema_node_name = child_schema_node.get_name()

            try:
                key = schema_node_name
                values = json_node[schema_node_name]
                visited_children.add(key)
            except KeyError:
                try:
                    prefixed_name = "%s:%s" % (schema_prefix, schema_node_name)
                    values = json_node[prefixed_name]
                    visited_children.add(prefixed_name)
                except KeyError:
                    continue

            # recurse
            if type(values) == list:
                xml.append(_handle_list(values, child_schema_node, key))
            elif child_schema_node.get_name() == "input" and child_schema_node.is_rpc_input():
                # don't specify "input" element
                xml.append(handler[type(values)](values, child_schema_node, key))
            else:
                xml.append('<%s xmlns="%s">' % (schema_node_name, schema_namespace))
                xml.append(handler[type(values)](values, child_schema_node, key))
                xml.append("</%s>" % (schema_node_name))

        if strict and (len(visited_children) != len(json_node)):
            missing_children = set(json_node.keys()) - visited_children
            raise InvalidSchemaException("Extra schema elements: %s" % missing_children)
        return ''.join(xml)

    def _handle_list(json_node, schema_node, key=None):
        # lists only aggregate other structures
        xml = list()

        for item in json_node:
            xml.append('<%s >' % (key))
            xml.append(handler[type(item)](item, schema_node))
            xml.append("</%s>" % (key))

        return ''.join(xml)

    def _handle_int(json_node, schema_node, key=None):
        return str(json_node)

    def _handle_str(json_node, schema_node, key=None):
        return tornado.escape.xhtml_escape(json_node)

    def _handle_none(json_node, schema_node, key=None):
        return ""

    handler = {dict : _handle_dict,
               int : _handle_int,
               bool : _handle_int,
               str : _handle_str,
               type(None): _handle_none,
               }




    parsed_json= json.loads(json_string)

    xml_str = "<data>" + handler[type(parsed_json)](parsed_json, schema_node) + "</data>"

    return xml_str
