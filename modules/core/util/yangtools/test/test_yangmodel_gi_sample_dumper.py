#!/usr/bin/env python
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file test_yangmodel_gi.py
# @author Rajesh Ramankutty (Rajesh.Ramankutty@riftio.com)
# @date 2015/03/10
#
"""
import argparse
import logging
import os
import io
import sys
import unittest
import gi
from gi.repository import RwYang
import xmlrunner
import rwlogger

class Dumper(object):
  def __init__(self, base):
    # presume this sets up what meta is loaded
    yang_model = RwYang.Model.create_libncx()
    yang_module = yang_model.load_module(base)

    self.root = yang_model.get_root_node()

  def dump(self, node, ident=''):
    t = node.node_type()
    type_name = ''
    if t is not None:
      type_name = t.get_description()
    print('%s{"name":"%s,"type":%s,' % (ident, node.get_name(), type_name))
    if node.is_leafy():
      print(ident + '"description":"' + node.node_type().get_description() + '"}')
      print(ident + '"help":"' + node.node_type().get_help_full() + '"}')
    else:
      child = node.get_first_child()
      print(ident + '"list":[')
      while child is not None:
        self.dump(child, ident + '  ')
        child = child.get_next_sibling()
        if child is not None:
          print(ident + '  ,')
      print(ident + ']}')

d = Dumper("rw-base")
d.dump(d.root)
