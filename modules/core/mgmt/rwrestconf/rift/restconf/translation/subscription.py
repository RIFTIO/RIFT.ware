# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 9/2/2015
# 

import json
import urllib.parse

from rift.rwlib.schema import (
    collect_children,
)

from ..util import (
    find_child_by_name,
)

class SyntaxError(Exception):
    def __init__(self, message):
        super(SyntaxError, self).__init__()

        self.message = message

class SubscriptionParser(object):
    def __init__(self, logger, schema):
        self._logger = logger
        self._schema = schema

    def parse(self, message_text):

        try:
            message = json.loads(message_text)
        except ValueError as e:
            raise SyntaxError("%s" % e)

        # verify the 3 fields of the mesage are present
        required_fields = {"path", "channel", "fields"}
        actual_fields = set(message.keys())

        if actual_fields < required_fields:
            missing_fields = required_fields - actual_fields
            raise SyntaxError("Missing field(s): %s" % missing_fields)
        if actual_fields > required_fields:
            extra_fields = actual_fields - required_fields
            raise SyntaxError("Extra field(s): %s" % extra_fields)

        pieces = message["path"].split("/")

        xpath = ["D,"] # only subscribing to operational data

        root_node = self._schema
        for piece in pieces:
            if piece is "":
                # skip leading and trailing slashes
                continue
            
            while "%" in piece:
                # un-decode the url fragment
                # NOTE: names can be doubly encoded
                # ex: trafgen%252F2%252F1 gets unquoted into trafgen%2F2%2F1 which
                #     finally gets unquoted into trafgen/2/1
                piece = urllib.parse.unquote(piece)

            current_node = find_child_by_name(root_node, piece)
            treat_piece_as_key = root_node.has_keys() and not current_node
            
            if treat_piece_as_key:
                xpath.append(emit_key_xpath(root_node, piece))
            elif current_node is not None:
                xpath.append(emit_object_xpath(current_node, piece))
                root_node = current_node
            else:
                raise SyntaxError("Unknown schema element: %s" % piece)

        return "".join(xpath)

    
def emit_key_xpath(schema_node, key_value):
    node_prefix = schema_node.get_prefix()
    key_name = schema_node.get_first_key().get_key_node().get_name()

    return '[%s:%s="%s"]' % (node_prefix, key_name, key_value)


def emit_object_xpath(schema_node, object_name):
    node_prefix = schema_node.get_prefix()

    return "/%s:%s" % (node_prefix, object_name)
