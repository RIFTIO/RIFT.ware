# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 11/9/15
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

import lxml.etree as et
from rift.rwlib.xml import (
    collect_siblings,
    get_xml_element_name,
)

def collect_children(node):
    children = list()
    child = node.get_first_child()

    while child is not None:
        children.append(child)

        child = child.get_next_sibling()

    return children

def collect_rpc_child_names(node):
    input_child = node.get_first_child()
    output_child = input_child.get_next_sibling()
    
    input_child_names = collect_child_names(input_child)
    output_child_names = collect_child_names(output_child)

    return input_child_names.union(output_child_names)

def collect_child_names(node):
    child_names = set()
    if node.is_rpc():
        return collect_rpc_child_names(node)

    child = node.get_first_child()

    while child is not None:

        child_names.add(child.get_name())

        child = child.get_next_sibling()

    return child_names

def find_child_by_name(node, name):
    if node is None:
        return None
    if name is (None or ""):
        return None    

    if ":" in name:
        pieces = name.split(":")
        prefix = pieces[0]
        name = pieces[1]
    else:
        prefix = None

    return node.search_child_with_prefix(name, prefix)

def get_keys(schema_node):
    assert(schema_node.is_listy())
    
    keys = list()
    cur_key = schema_node.get_first_key()

    while cur_key is not None:
        keys.append(cur_key.get_key_node())

        cur_key = cur_key.next_key

    return keys

def get_key_names(schema_node):
    assert(schema_node.is_listy())
    
    keys = list()
    cur_key = schema_node.get_first_key()

    while cur_key is not None:
        key_name = cur_key.get_key_node().get_name()
        keys.append(key_name)
        cur_key = cur_key.next_key

    return keys

def prune_non_schema_xml(root_schema_node, xml_string):

    def _walk(schema_node, xml_node):
        xml_children = collect_siblings(xml_node)
        actual_names = set(xml_children.keys())
        expected_names = collect_child_names(schema_node)

        remove_names = actual_names - expected_names

        for name, siblings in xml_children.items():
            if name in remove_names:
                for sib in siblings:
                    xml_node.remove(sib)
            else:
                child_schema_node = find_child_by_name(schema_node, name)
                for sib in siblings:
                    _walk(child_schema_node, sib)

    # check root node
    schema_children = collect_children(root_schema_node)

    root_xml_node = et.fromstring(xml_string)
    root_name = get_xml_element_name(root_xml_node)

    for child in schema_children:
        if child.get_name() == root_name:
            root_schema_node = child
            break
    else:
        # root node isn't in the schema
        return "<data/>"

    _walk(root_schema_node, root_xml_node)
    xml_string = et.tostring(root_xml_node, pretty_print=True).decode("utf-8")

    return xml_string
