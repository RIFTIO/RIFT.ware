#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import os


import rw_peas
#import gi.repository.YangEnums as yang_enums
from gi import require_version
require_version('YangModelPlugin', '1.0')
require_version('RwmanifestDYang', '1.0')
from gi.repository import RwmanifestDYang


def test_yangmodel_plugin_api_YangNcxBasicModel(yang):
  # Call the plugin function using the function pointer
  yang_model = yang.get_interface('Model')
  yang_module = yang.get_interface('Module')
  yang_node = yang.get_interface('Node')
  yang_key = yang.get_interface('Key')
  yang_type = yang.get_interface('Type')

  model = yang_model.alloc()
  print "yang_model.alloc() returned model = 0x%x" % model
  print

  module = yang_model.load_module(model, "testncx")
  print "yang_model.load_module(model) returned module = 0x%x" % module
  print "yang_module.location :", yang_module.location(module)
  print "yang_module.description :", yang_module.description(module)
  print

  root = yang_model.get_root_node(model)
  print "yang_model.get_root_node(model) returned node = 0x%x" % root
  node_name = ""
  node_name = yang_node.name(root)
  print "yang_node.name(root) :", node_name
  print

  top = yang_node.first_child(root)
  print "yang_node.first_child(root) returned node = 0x%x" % top
  node_name = yang_node.name(top)
  print "yang_node.name(top) :", node_name
  print "yang_node.is_leafy(top) = ", yang_node.is_leafy(top)
  print "yang_node.type(top) = ", yang_node.type(top)

  """
  stmt_type = yang_node.stmt_type(top)
  print stmt_type
  if stmt_type == yang_enums.StmtType.CONTAINER:
      print " yang_stmt_type (top) Container as expected "
  else:
      print " yang_stmt_type (top): ACTUAL", yang_node.stmt_type(top)
      print " Expected : ", yang_enums.StmtType.CONTAINER
  """

  top_sib = yang_node.next_sibling(top)
  print "yang_node.next_sibling(top) returned node = 0x%x" % top_sib
  print

  a = yang_node.first_child(top)
  print "yang_node.first_child(top) returned node = 0x%x" % a
  node_name = yang_node.name(a)
  print "yang_node.name(a) :", node_name
  print "yang_node.is_leafy(a) = ", yang_node.is_leafy(a)
  print "yang_node.type(a) = ", yang_node.type(a)
  print

  cont_in_a = yang_node.first_child(a)
  print "yang_node.first_child(a) returned node = 0x%x" % cont_in_a
  node_name = yang_node.name(cont_in_a)
  print "yang_node.name(cont_in_a) :", node_name
  print "yang_node.is_leafy(cont_in_a) = ", yang_node.is_leafy(cont_in_a)
  print "yang_node.type(cont_in_a) = ", yang_node.type(cont_in_a)
  print

  str = yang_node.first_child(cont_in_a)
  print "yang_node.first_child(cont_in_a) returned node = 0x%x" % str
  num1 = yang_node.next_sibling(str)
  print "yang_node.next_sibling(str) returned node = 0x%x" % num1
  ll = yang_node.next_sibling(num1)
  print "yang_node.next_sibling(num1) returned node = 0x%x" % ll
  lst = yang_node.next_sibling(ll)
  print "yang_node.next_sibling(ll) returned node = 0x%x" % lst
  lst_key = yang_node.first_key(lst)
  print "yang_node.first_key(lst) returned node = 0x%x" % lst_key
  print "yang_key.next(lst_key)", yang_key.next_key(lst_key)
  key_list_node = yang_key.list_node(lst_key)
  print "yang_key.list_node(lst_key) returned node = 0x%x" % key_list_node, key_list_node == lst
  num2 = yang_node.first_child(lst) 
  print "yang_node.first_child(lst) returned = 0x%x" % num2
  print "yang_node.is_key(num2) = ", yang_node.is_key(num2)
  num2_type = yang_node.type(num2)
  """
  if yang_type.leaf_type(num2_type) == yang_enums.LeafType.INT32:
      print "Got expected type for num 2"
  else:
      print "Expected :", yang_enums.LeafType.INT32
      print "Actual:", yang_type.leaf_type(num2)
  print
  """
  str2 = yang_node.next_sibling(num2)
  print "yang_node.next_sibling(num2) returned = 0x%x" % str2
  key_key_node = yang_key.key_node(lst_key)
  print "yang_key.key_node(lst_key) returned = 0x%x" % key_key_node, key_key_node == str2
  print "yang_node.is_key(str2) = ", yang_node.is_key(str2)
  print

  a1 = yang_node.next_sibling(a)
  print "yang_node.next_sibling(a) returned node = 0x%x" % a1
  a1_parent = yang_node.parent_node(a1)
  print "yang_node.parent_node(a1) returned node = 0x%x" % a1_parent, a1_parent == top
  node_name = yang_node.name(a1)
  print "yang_node.name(a1) :", node_name
  print "yang_node.is_leafy(a1) :%s yang_node.type(a1) :%d" % (yang_node.is_leafy(a1), yang_node.type(a1))
  print

  b = yang_node.next_sibling(a1)
  print "yang_node.next_sibling(a1) returned node = 0x%x" % b
  b_parent = yang_node.parent_node(b)
  print "yang_node.parent_node(b) returned node = 0x%x" % b_parent, b_parent == top
  node_name = yang_node.name(b)
  print "yang_node.name(b) :", node_name
  print "yang_node.is_leafy(b) :%s yang_node.type(b) :%d" % (yang_node.is_leafy(b), yang_node.type(b))
  print

  c = yang_node.next_sibling(b)
  print "yang_node.next_sibling(b) returned node = 0x%x" % c
  c_parent = yang_node.parent_node(c)
  print "yang_node.parent_node(c) returned node = 0x%x" % c_parent, c_parent == top
  node_name = yang_node.name(c)
  print "yang_node.name(c) :", node_name
  print "yang_node.is_leafy(c) :%s yang_node.type(c) :%d" % (yang_node.is_leafy(c), yang_node.type(c))
  print

  d = yang_node.next_sibling(c)
  print "yang_node.next_sibling(c) returned node = 0x%x" % d
  d_parent = yang_node.parent_node(d)
  print "yang_node.parent_node(d) returned node = 0x%x" % d_parent, d_parent == top
  node_name = yang_node.name(d)
  print "yang_node.name(d) :", node_name
  print "yang_node.is_leafy(d) :%s yang_node.type(d) :%d" % (yang_node.is_leafy(d), yang_node.type(d))
  print

  e = yang_node.next_sibling(d)
  print "yang_node.next_sibling(d) returned node = 0x%x" % e
  e_parent = yang_node.parent_node(e)
  print "yang_node.parent_node(e) returned node = 0x%x" % e_parent, e_parent == top
  node_name = yang_node.name(e)
  print "yang_node.name(e) :", node_name
  print "yang_node.is_leafy(e) :%s yang_node.type(e) :%d" % (yang_node.is_leafy(e), yang_node.type(e))
  print

  d_n1 = yang_node.first_child(d)
  e_n1 = yang_node.first_child(e)
  yang_node.is_leafy(d_n1)
  yang_node.is_leafy(e_n1)
  d_n1_node_name = yang_node.name(d_n1)
  print "yang_node.name(d_n1):", d_n1_node_name
  e_n1_node_name = yang_node.name(e_n1)
  print "yang_node.name(e_n1):", e_n1_node_name


  print "DONE"
  print

#
# Example YangNcx.YangNcxExt
#
#

def test_yangmodel_plugin_api_YangNcxYangNcxExt(yang):
  yang_model = yang.get_interface('Model')
  yang_module = yang.get_interface('Module')
  yang_node = yang.get_interface('Node')
  yang_key = yang.get_interface('Key')
  print
  print

  # Call the plugin function using the function pointer
  model = yang_model.alloc()
  print "yang_model.alloc() returned model = 0x%x" % model
  print

  module = yang_model.load_module(model, "testncx-ext")
  print "yang_model.load_module(model) returned module = 0x%x" % module
  print "yang_module.location :", yang_module.location(module)
  print "yang_module.description :", yang_module.description(module)
  print

  root = yang_model.get_root_node(model)
  print "yang_model.get_root_node(model) returned node = 0x%x" % root
  node_name = ""
  node_name = yang_node.name(root)
  print "yang_node.name(root) :", node_name
  print

  top1 = yang_node.first_child(root)
  print "yang_node.first_child(root) returned node = 0x%x" % top1
  node_name = yang_node.name(top1)
  print "yang_node.name(top1) :", node_name
  print "yang_node.is_leafy(top1) = ", yang_node.is_leafy(top1)
  print "yang_node.type(top1) = ", yang_node.type(top1)
  print

  top1cina = yang_node.first_child(top1)
  print "yang_node.first_child(top1) returned node = 0x%x" % top1cina
  node_name = yang_node.name(top1cina)
  print "yang_node.name(top1cina) :", node_name
  print "yang_node.is_leafy(top1cina) = ", yang_node.is_leafy(top1cina)
  print "yang_node.type(top1cina) = ", yang_node.type(top1cina)
  print

  top1lina = yang_node.next_sibling(top1cina)
  print "yang_node.next_sibling(top1cina) returned node = 0x%x" % top1lina
  node_name = yang_node.name(top1lina)
  print "yang_node.name(top1lina) :", node_name


  print "DONE"
  print


def test_node_name_with_augments(yang):
  # Call the plugin function using the function pointer
  yang_model = yang.get_interface('Model')
  yang_module = yang.get_interface('Module')
  yang_node = yang.get_interface('Node')
  yang_key = yang.get_interface('Key')

  model = yang_model.alloc()
  print "yang_model.alloc() returned model = 0x%x" % model
  print

  module = yang_model.load_module(model, "testncx")
  print "yang_model.load_module(model) returned module = 0x%x" % module
  print "yang_module.location :", yang_module.location(module)
  print "yang_module.description :", yang_module.description(module)
  print

  root = yang_model.get_root_node(model)
  print "yang_model.get_root_node(model) returned node = 0x%x" % root
  node_name = ""
  node_name = yang_node.name(root)
  print "yang_node.name(root) :", node_name
  print

  # FAILS STARTING HERE
  def dump_tree(indent, parent):
    if parent == root:
      # avoid segfault when asking for root node name
      print 'ROOT'
    else:
      print indent, parent, yang_node.name(parent)

    child = yang_node.first_child(parent)
    child_indent = ' ' + indent
    while child:
      dump_tree(child_indent, child)
      child = yang_node.next_sibling(child)

  dump_tree('', root)

def test_load_schema(yang):
  # Call the plugin function using the function pointer
  yang_model = yang.get_interface('Model')
  yang_module = yang.get_interface('Module')
  yang_node = yang.get_interface('Node')

  model = yang_model.alloc()
  print "yang_model.alloc() returned model = 0x%x" % model

  status = yang_model.load_schema_ypbc(model, RwmanifestDYang.get_schema())

  root = yang_model.get_root_node(model)
  print "yang_model.get_root_node(model) returned node = 0x%x" % root
  manifest = yang_node.first_child(root)
  print "yang_node.first_child(root) returned node = 0x%x" % manifest
  node_name = yang_node.name(manifest)
  print "node_name=", node_name

  print "DONE"
  print


def test_yangmodel_plugin_api_examples(yang):
  test_yangmodel_plugin_api_YangNcxBasicModel(yang)
  test_yangmodel_plugin_api_YangNcxYangNcxExt(yang)
  test_node_name_with_augments(yang)
  test_load_schema(yang)

def main():
  yangmodel_plugin = rw_peas.PeasPlugin(
      'yangmodel_plugin-c',
      'YangModelPlugin-1.0')

  test_yangmodel_plugin_api_examples(yangmodel_plugin)

if __name__ == "__main__":
    main()
