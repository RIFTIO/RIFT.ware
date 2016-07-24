# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 9/5/2015
# 

import urllib.parse
import base64

import tornado.escape

from .util import (
    PayloadType,
    Result,
    naive_xml_to_json,
    NetconfOperation,
)

from .schema import (
    find_target_node,
)

ERROR_MAP = {
    Result.Data_Exists : 409,
    Result.Operation_Failed : 405,
    Result.Rpc_Error : 405,
    Result.Unknown_Error : 500,
    Result.Upgrade_In_Progress : 405,
    Result.Upgrade_Performed : 405,
}


def determine_payload_type(accept_type):
    if accept_type is None:
        return PayloadType.XML
    else:
        if "xml" in accept_type:
            if "collection" in accept_type:
                return PayloadType.XML_COLLECTION
            else:
                return PayloadType.XML
        elif "json" in accept_type:
            if "collection" in accept_type:
                return PayloadType.JSON_COLLECTION
            else:
                return PayloadType.JSON
        else:
            return None

def format_error_message(accept_type, message):
    if accept_type in (PayloadType.JSON, PayloadType.JSON_COLLECTION):
        return naive_xml_to_json(message)
    else:
        return message

def get_username_and_password_from_auth_header(auth_header):
    """Gets the username and password from the auth_header

    Arguments:
    auth_header - Encoded auth_header received for basic auth

    Returns: Decoded username and password.
    """
    if not auth_header.startswith("Basic "):
        raise ValueError("only supoprt for basic authorization")

    encoded = auth_header[6:]
    decoded = base64.decodestring(bytes(encoded, "utf-8")).decode("utf-8")
    username, password = decoded.split(":", 2)
    return username, password

def split_url(url):
    is_operation = False
    url_parts = urllib.parse.urlsplit(url)
    whole_url = url_parts[2]
    if whole_url.startswith("/api/operational/"):
        relevant_url = whole_url.split("/api/operational/", 1)[-1]
    elif whole_url.startswith("/api/operations/"):
        is_operation = True
        relevant_url = whole_url.split("/api/operations/", 1)[-1]
    elif whole_url.startswith("/api/running/"):
        relevant_url = whole_url.split("/api/running/", 1)[-1]
    elif whole_url.startswith("/api/running"):
        relevant_url = whole_url.split("/api/running", 1)[-1]
    elif whole_url.startswith("/api/config/"):
        relevant_url = whole_url.split("/api/config/", 1)[-1]
    elif whole_url.startswith("/api/config"):
        relevant_url = whole_url.split("/api/config", 1)[-1]
    elif whole_url.startswith("/api/schema"):
        relevant_url = whole_url.split("/api/schema", 1)[-1]
    else:
        raise ValueError("unknown url component in %s" % whole_url)

    url_pieces = relevant_url.split("/")

    unescaped_url_pieces = list()
    for p in url_pieces:
        unescaped_url_pieces.append(tornado.escape.url_unescape(p))

    return unescaped_url_pieces, relevant_url, url_parts[3], is_operation

def split_stream_url(url):
    """Splits the stream URL.

    Arguments:
        url - stream URL received when a stream is opened

    Stream URL has the following parts
        stream protocol - indicates HTTP or Websocket stream
        stream name - name of the stream
        optional filter - filter to be applied on the stream
        optional start-time - start-time for the replay feature
        optional stop-time - stop-time for the replay feature

    Reference: draft-ietf-netconf-restconf-09
    Sample:
      For filter = /event/event-class='fault'
      URL: /streams/NETCONF?filter=%2Fevent%2Fevent-class%3D'fault'

      For start-time = 2014-10-25T10:02:00Z
      URL: /ws_streams/NETCONF?start-time=2014-10-25T10%3A02%3A00Z
    """
    url_parts = urllib.parse.urlsplit(url)
    stream_name = url_parts.path.split('/')[2]
    if stream_name.endswith("-JSON"):
        stream_name = stream_name[:-5]
        encoding = "json"
    else:
        encoding = "xml"

    query = urllib.parse.parse_qs(url_parts.query)
    filt = query["filter"] if "filter" in query else None
    start_time = query["start-time"] if "start-time" in query else None
    stop_time = query["stop-time"] if "stop-time" in query else None

    return stream_name, encoding, filt, start_time, stop_time

def is_config(url):
    url_parts = urllib.parse.urlsplit(url)
    whole_url = url_parts[2]
    return whole_url.startswith("/api/running") or whole_url.startswith("/api/config")

def is_operation(url):
    url_parts = urllib.parse.urlsplit(url)
    whole_url = url_parts[2]
    return whole_url.startswith("/api/operations")

def is_schema_api(url):
    url_parts = urllib.parse.urlsplit(url)
    whole_url = url_parts[2]
    return whole_url.startswith("/api/schema")

def map_error_to_http_code(result):
    assert result is not Result.OK

    return ERROR_MAP.get(result, (500))

def map_request_to_netconf_operation(request):
    if request.method == "DELETE":
        return NetconfOperation.DELETE
    elif request.method == "GET":
        if is_config(request.uri):
            return NetconfOperation.GET_CONFIG
        else:
            return NetconfOperation.GET
    elif request.method == "POST":
        if is_operation(request.uri):
            return NetconfOperation.RPC
        else:
            return NetconfOperation.CREATE
    elif request.method == "PUT":
        return NetconfOperation.REPLACE
    else:
        raise ValueError("unknown request type %s" % request.method)
    
def get_json_schema(schema_root, url):
    actual_url = urllib.parse.urlsplit(url)[2];
    url_parts = actual_url.split('/');
    if url_parts[-1] == '':
        del(url_parts[-1])

    if len(url_parts) == 3:
        # Convert the entire schema
        target_node = schema_root.get_first_child()
        resp_node = []
        while target_node is not None:
            resp = target_node.to_json_schema(True)
            if len(resp) > 0:
              resp_node.append(resp[1:len(resp)-1])
            target_node = target_node.get_next_sibling()

        json_resp = ','.join(resp_node)
        return "{" + json_resp + "}"
    else:
        target_node = find_target_node(schema_root, url_parts[3:])
        if target_node is None:
            return ValueError("Conversion failed")

        return target_node.to_json_schema(True)
    
