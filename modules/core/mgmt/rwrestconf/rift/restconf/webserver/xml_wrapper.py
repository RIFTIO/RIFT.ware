# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 10/6/2015

# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

import asyncio
import lxml
import re

import gi
gi.require_version('RwMgmtagtDtsYang', '1.0')
from gi.repository import (
    RwMgmtagtDtsYang,
)

from ..util import (
    Result,
    create_dts_xpath_from_url,    
    NetconfOperation,
)

class XmlWrapper(object):
    '''
    This class handles the communication with the uAgent's internal xml-only request interface.
    '''
    def __init__(self, logger, dts, schema_root, is_admin):
        self._log = logger
        self._dts = dts
        self._schema_root = schema_root
        self._is_admin = is_admin
        self._restricted_urls_re = re.compile(
                                      r'^/api/(config|running)/(cloud|uagent)')

        self._operation_map = {
            NetconfOperation.DELETE : self.delete,
            NetconfOperation.GET : self.get,
            NetconfOperation.GET_CONFIG : self.get_config,
            NetconfOperation.CREATE : self.post,
            NetconfOperation.REPLACE : self.put,
            NetconfOperation.RPC : self.rpc,
        }

    @asyncio.coroutine
    def _query_mgmtagt(self, url, xml, operation, edit_type=None):
        xpath = create_dts_xpath_from_url(url, self._schema_root)
        
        try:
            request=RwMgmtagtDtsYang.YangInput_RwMgmtagtDts_MgmtAgentDts()
            request.pb_request.xpath=xpath 
            request.pb_request.request_type=operation
            if edit_type is not None:
                # oper restrictions
                if (not self._is_admin and 
                    self._restricted_urls_re.search(url) is not None):
                    return Result.Operation_Failed, "<resource-denied>%s</resource-denied>" % url
                request.pb_request.edit_type=edit_type
            request.pb_request.blobxml=xml

            query_iter = yield from self._dts.query_rpc(
                            xpath="I,/rw-mgmtagt-dts:mgmt-agent-dts",
                            flags=0,
                            msg=request)

            for fut_resp in query_iter:
               query_resp = yield from fut_resp
               result = query_resp.result
               break # the uAgent sends only a single response
            else:
                raise ValueError("empty response from uagent")
        except ValueError:
            raise
        except Exception as e:
            return Result.Operation_Failed, "<operation-failed>%s</operation-failed>" % str(e)

        try:
            response = result.pb_request.blobxml
        except:
            response = "<data/>"

        if result.pb_request.get_error() is None:
            return Result.OK, response
        else:
            return Result.Operation_Failed, result.pb_request.get_error()
        
    @asyncio.coroutine
    def get(self, url, xml):
        if url.startswith("/api/operational/restconf-state"):
          return Result.Operation_Failed, "<resource-denied>%s</resource-denied>" % url
        return self._query_mgmtagt(url, xml, "get")

    @asyncio.coroutine
    def get_config(self, url, xml):
        return self._query_mgmtagt(url, xml, "get_config")

    @asyncio.coroutine
    def delete(self, url, xml):
        # strip off <config> tags
        root = lxml.etree.fromstring(xml)
        xml = lxml.etree.tostring(root[0]).decode("utf-8") 
        xml = "<data>%s</data>" % xml


        return self._query_mgmtagt(url, xml, "edit_config", "delete")

    @asyncio.coroutine
    def put(self, url, xml):
        # strip off <config> tags
        root = lxml.etree.fromstring(xml)
        xml = lxml.etree.tostring(root[0]).decode("utf-8") 
        xml = "<data>%s</data>" % xml


        return self._query_mgmtagt(url, xml, "edit_config", "replace") #ATTN: fix merge

    @asyncio.coroutine
    def rpc(self, url, xml):
        xml = "<data>%s</data>" % xml


        return self._query_mgmtagt(url, xml, "rpc")

    @asyncio.coroutine
    def post(self, url, xml):
        # strip off <config> tags

        root = lxml.etree.fromstring(xml)
        xml = lxml.etree.tostring(root[0]).decode("utf-8") 
        xml = "<data>%s</data>" % xml


        return self._query_mgmtagt(url, xml, "edit_config", "create") #ATTN: fix merge

    @asyncio.coroutine
    def execute(self, operation, url, xml):
        functor = self._operation_map[operation]
        return functor(url, xml)
    
