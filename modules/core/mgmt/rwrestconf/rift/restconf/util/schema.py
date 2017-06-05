# RIFT_IO_STANDARD_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 7/10/2015
# RIFT_IO_STANDARD_COPYRIGHT_HEADER(END)

from enum import Enum

import gi

gi.require_version('RwYang', '1.0')
from gi.repository import RwYang

from rift.rwlib.schema import (
    get_keys,
)

class TargetType(Enum):
    ListEntry =  0
    List = 1
    Container = 2
    Leaf = 3

def collect_children(node):
    children = list()
    child = node.get_first_child()

    if node.is_listy():
        children = get_keys(node)

    while child is not None:
        if child in children:
            child = child.get_next_sibling()
            continue

        children.append(child)

        child = child.get_next_sibling()

    return children

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



def find_target_node(top_node, path_list=[]):
    """
      Parameters:
      @top_node : Top level yang node of the schema
      @path_list: The schema path extracted from the URL
      Eg: If URL specified is '/api/schema/X/Y/Z',
          path_list will have entries [X, Y, Z]

      This routine traverses through the schema in the order
      dictated by the path_list to find the yang node associated
      with the lowest node i.e. last element of the path_list.
    """
    if path_list == []:
        return None

    target_node = find_child_by_name(top_node, path_list[0])
    if target_node is None:
        return None

    for name in path_list[1:]:
        target_node = find_child_by_name(target_node, name)

    return target_node


def find_child_by_path(node, path):
    ''' walks the tree based on the path

    Arguments:
    node -- the root node
    path -- the path to walk, must be a list
    '''

    assert(type(path) == list)
    runner = node
    path_itr = 0
    while path_itr < len(path):
        piece = path[path_itr]
        path_itr += 1
        if runner.is_listy():
            # skip keys
            if path_itr == len(path):
                break
            piece = path[path_itr]
            path_itr += 1

        child = find_child_by_name(runner, piece)

        if child is not None:
            runner = child

    return runner

def find_target_type(node, path):
    assert(type(path) == list)

    target_node = find_child_by_path(node, path)

    if target_node.is_listy():
        target = path[-1]
        if target == "":
            target = path[-2]
        if target_node.get_name() == target:
            return TargetType.List
        else:
            return TargetType.ListEntry
    elif target_node.is_leafy():
        return TargetType.Leaf        
    else:
        return TargetType.Container

def load_multiple_schema_root(schema_names):
    yang_model = RwYang.Model.create_libncx()
    
    for name in schema_names:
        yang_model.load_module(name)

    return yang_model.get_root_node()

def load_schema_root(schema_name):
    yang_model = RwYang.Model.create_libncx()
    yang_model.load_module(schema_name)

    return yang_model.get_root_node()

## helper functions for debugging

def _print_children(node,
                    recurse_on_child=True,
                    depth=0):
    """ print the children of the input node """

    child = node.get_first_child()

    if not child:
        return None

    while child is not None:
        print("%s%s"%(" "*depth, child.get_name()))

        if recurse_on_child:
            _print_children(child, recurse_on_child, depth+1);

        child = child.get_next_sibling()
