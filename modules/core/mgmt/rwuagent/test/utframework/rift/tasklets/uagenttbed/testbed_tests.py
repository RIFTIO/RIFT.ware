
"""
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

@file testbed_tests.py
@date 16/03/2016
"""
import asyncio
import logging
import collections
import time
import gi
import ncclient.asyncio_manager
import random
from random import choice
from string import ascii_lowercase

import lxml.etree as etree

import websockets
import ssl
from rift.rwlib.util import certs

from rift.auto.session import NetconfProxy
from rift.auto.session import Proxy

import rift.tasklets
import rift.tasklets.dts
import gi.repository.RwTypes as rwtypes

gi.require_version('InterfacesYang', '1.0')
gi.require_version('RoutesYang', '1.0')
gi.require_version('UtCompositeYang', '1.0')
gi.require_version('RwAgentTestbedYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwMgmtagtYang', '1.0')
gi.require_version('RwMgmtagtDtsYang', '1.0')
gi.require_version('DnsYang', '1.0')
gi.require_version('NotifYang', '1.0')

from gi.repository import UtCompositeYang, RwAgentTestbedYang, InterfacesYang, RoutesYang, RwMgmtagtYang, DnsYang, NotifYang
from gi.repository import RwMgmtagtDtsYang
from gi.repository import ProtobufC
from gi.repository import RwKeyspec
from gi.repository import RwDts as rwdts

class TBNetconfProxy(NetconfProxy):
   """
   Support class to send netconf request using asyncio
   """

   def __init__(self, session, module, logger):
       NetconfProxy.__init__(self, session, module)
       self._log = logger

   def _add_attribute(self, xpath, xml, operation):
       xpath = xpath.lstrip('/')
       xpath = Proxy._xpath_strip_keys(xpath)
       xpath_elems = xpath.split('/')
       pos = 0
       for elem in xpath_elems:
           pos = xml.index(elem, pos)
           pos = xml.index('>', pos)
       if xml[pos-1] == '/':
          pos -= 1
       xml = xml[:pos] + " xc:operation='{}'".format(operation) + xml[pos:]
       return xml

   @asyncio.coroutine
   def merge_config(self, xpath, pb, target='running'):
       desc = pb.retrieve_descriptor()
       xml = pb.to_xml_v2(self.model)
       xml = NetconfProxy.populate_keys(xpath, xml, desc.xml_prefix(), desc.xml_ns())
       
       xml = self._add_attribute(xpath, xml, 'merge')
       xml = '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">{}</config>'.format(xml)

       if self.session.ncclient_manager is None:
          yield from self.session.connect()

       netconf_response = yield from self.session.ncclient_manager.edit_config(
                                     target=target,
                                     config=xml) 

       self._log.info("netconf merge-config response: %s", netconf_response.xml)
       if netconf_response.ok:
          return True

       return False

   @asyncio.coroutine
   def get(self, xpath, module):
       if self.session.ncclient_manager is None:
          yield from self.session.connect()

       netconf_response = yield from self.session.ncclient_manager.get(('xpath', xpath)) 
       self._log.info("netconf get response: %s", netconf_response.xml)
      
       response_obj = None
       if netconf_response.ok is True:
          response_obj = Proxy._create_gi_from_xpath(xpath, module)
          response_obj.from_xml_v2(self.model, Proxy._xml_strip_rpc_reply(netconf_response.xml))

       return response_obj


   @asyncio.coroutine
   def get_config(self, xpath, module):
       if self.session.ncclient_manager is None:
          yield from self.session.connect()

       netconf_response = yield from self.session.ncclient_manager.get_config(
                                     source='running',
                                     filter=('xpath', xpath))

       self._log.info("netconf get_config response: %s", netconf_response.xml)
       response_obj = None
       if netconf_response.ok is True:
          response_obj = Proxy._create_gi_from_xpath(xpath, module)
          response_obj.from_xml_v2(self.model, Proxy._xml_strip_rpc_reply(netconf_response.xml))

       return response_obj

   @asyncio.coroutine
   def rpc(self, input_obj, module):
       if self.session.ncclient_manager is None:
          yield from self.session.connect()

       request_xml = input_obj.to_xml_v2(self.model)
       netconf_response = yield from self.session.ncclient_manager.dispatch(
                                     etree.fromstring(request_xml)) 

       self._log.info("netconf rpc response: %s", netconf_response.xml)
       output_obj = None

       if netconf_response.ok is True:
          in_desc = input_obj.retrieve_descriptor()
          rpc_output_xpath = 'O,/{}'.format(in_desc.xml_element_name())
          out_desc = RwKeyspec.get_pbcmd_from_xpath(rpc_output_xpath, self.schema)
          (module_name, obj_type) = out_desc.get_gi_typename().split(".", 1)
          create_object = getattr(module, obj_type)
          output_obj = create_object()
        
          output_obj.from_xml_v2(self.model, 
                                 Proxy._rpc_fix_root(xml=Proxy._xml_strip_rpc_reply(netconf_response.xml),
                                 rpc_name=out_desc.xml_element_name(),
                                 prefix=out_desc.xml_prefix(),
                                 namespace=out_desc.xml_ns()))

       return output_obj


class TBNetconfSession(object):

   """
   Netconf session using Asyncio NC manager
   """

   DEFAULT_PORT = 2022
   DEFAULT_IP = '127.0.0.1'
   DEFAULT_USERNAME = 'admin'
   DEFAULT_PASSWORD = 'admin'

   def __init__(self, logger, loop, 
                netconf_ip=DEFAULT_IP, 
                netconf_port=DEFAULT_PORT, 
                username=DEFAULT_USERNAME, 
                password=DEFAULT_PASSWORD):
       self._log = logger
       self._loop = loop
       self._netconf_ip = netconf_ip
       self._netconf_port = netconf_port
       self._username = username
       self._password = password
       self._netconf = None

   @asyncio.coroutine
   def close(self):
       if self._netconf.connected:
          yield from self._netconf.close_session()

   @asyncio.coroutine
   def connect(self):
       while True:
          try:
            self._netconf = yield from ncclient.asyncio_manager.asyncio_connect(
                            loop=self._loop,
                            host=self._netconf_ip,
                            port=self._netconf_port,
                            username=self._username,
                            password=self._password,
                            allow_agent=False,
                            look_for_keys=False,
                            hostkey_verify=False)
            
            self._log.info("Connected to confd")         
            break
          except Exception as e:
            self._log.error("Failed to connect to confd %s", str(e))
            yield from asyncio.sleep(2, loop=self._loop)

   @property
   def ncclient_manager(self):
       return self._netconf


class TestBase(object):

   """
   Test base class which all tests must inherit
   """

   def __init__(self, dtsh, logh, loop):
       self._dtsh = dtsh
       self._logh = logh
       self._name = "NA"
       self._loop = loop

   def run_test(self):
       return True 

   def create_test_response(self, result):
       ro = RwAgentTestbedYang.AgentTestsOp()
       ro.total_tests = 1
       if result is False:
          self._logh.error("{} test failed".format(self._name)) 
          ro.failed_count = 1
          ro.failed_tests = [ self._name ]
       else:
          self._logh.info("{} test passed".format(self._name))
          ro.passed_count = 1

       return ro

   @property
   def name(self):
       return self._name

class TestEditConf(TestBase):

   """
   Test Netconf edit conf request
   """
 
   def __init__(self, dtsh, logh, nc_proxy, loop):
       TestBase.__init__(self, dtsh, logh, loop)
       self._nc_proxy = nc_proxy
       self._name = "TEST_EDIT_CONFIG"

   @asyncio.coroutine
   def run_test(self):
       # Configure new interfaces
       interface = InterfacesYang.Interface()
       interface.name = "eth0"
       interface.enabled = "true"
       interface.speed = "hundred"
       interface.duplex = "full"
       interface.mtu = 1500

       res = yield from self._nc_proxy.merge_config("/interfaces/interface", interface);
       if res is True:
          res = False
          #verify the configuration through a get
          pb = yield from self._nc_proxy.get_config("/interfaces", InterfacesYang)
          if pb is not None and len(pb.interface) is 1:
             int1 = pb.interface[0]
             if ProtobufC.Message.is_equal_deep(None, int1.to_pbcm(), interface.to_pbcm()) is not 0:
                res = True

       return res

   @asyncio.coroutine
   def dts_self_register(self):

       @asyncio.coroutine
       def on_edit_config(xact_info, action, ks_path, msg):
           result = yield from self.run_test()
           ro = self.create_test_response(result)
           xpath = "O,/agt-tb:agent-tests"
           xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ro)

       # Self register with the DTS
       yield from self._dtsh.register(
                xpath = "I,/agt-tb:agent-tests/agt-tb:netconf-tests/agt-tb:edit-config",
                flags = rwdts.Flag.PUBLISHER,
                handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_edit_config))

class TestLargeBinaryBlob(TestBase):
   """
   Test large binary blob
   """
 
   def __init__(self, dtsh, logh, nc_proxy, loop):
       TestBase.__init__(self, dtsh, logh, loop)
       self._nc_proxy = nc_proxy
       self._name = "TEST_LARGE_BINARY_BLOB"

   @asyncio.coroutine
   def run_test(self):
       routes = RoutesYang.Routes()
       routes.binaryblob=bytearray(random.getrandbits(8) for _ in range(5000))
       routes.bigstring=''.join([choice(ascii_lowercase) for _ in range(10000)])

       res = yield from self._nc_proxy.merge_config("/routes", routes);
       if res is True:
          res = False
          #verify the configuration through a get
          pb = yield from self._nc_proxy.get_config("/routes", RoutesYang)
          if pb is not None:
             if ProtobufC.Message.is_equal_deep(None, pb.to_pbcm(), routes.to_pbcm()) is not 0:
                res = True
             
       return res

   @asyncio.coroutine
   def dts_self_register(self):

       @asyncio.coroutine
       def on_large_binary_blob(xact_info, action, ks_path, msg):
           result = yield from self.run_test()
           ro = self.create_test_response(result)
           xpath = "O,/agt-tb:agent-tests"
           xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ro)

       # Self register with the DTS
       yield from self._dtsh.register(
                xpath = "I,/agt-tb:agent-tests/agt-tb:netconf-tests/agt-tb:large-binary-blob",
                flags = rwdts.Flag.PUBLISHER,
                handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_large_binary_blob))

class TestPBRQECMerge(TestBase):
   """
   Test PB Requests EC Merge
   """
 
   def __init__(self, dtsh, logh, nc_proxy, loop):
       TestBase.__init__(self, dtsh, logh, loop)
       self._nc_proxy = nc_proxy
       self._name = "TEST_PB_REQUEST_EC_MERGE"

   @asyncio.coroutine
   def get_agent_dom(self):
       a_rpc = RwMgmtagtYang.YangInput_RwMgmtagt_MgmtAgent()
       a_rpc.show_agent_dom = True
       res_obj = yield from self._nc_proxy.rpc(a_rpc, RwMgmtagtYang)
       return res_obj

   @asyncio.coroutine
   def run_test(self): 
        res = yield from self.test_pbreq_ec_merge()
        if res is False:
           self._logh.error("PB_REQUEST EC Merge Failed")
           return False

        res = yield from self.test_pbreq_ec_container()
        if res is False:
           self._logh.error("PB_REQUEST EC Container Merge Failed")

        return res

   @asyncio.coroutine
   def test_pbreq_ec_container(self):
       ec_rpc = RwMgmtagtDtsYang.YangInput_RwMgmtagtDts_MgmtAgentDts()
       ec_rpc.pb_request.xpath = "C,/dns:dns/dns:options"
       op_pb = DnsYang.DnsOptions()
       op_pb.ndots = 10
       op_pb.timeout = 5
       op_pb.attempts = 10
       ec_rpc.pb_request.data = op_pb.to_pbuf()
       ec_rpc.pb_request.request_type = "edit_config"
       ec_rpc.pb_request.edit_type = "merge"

       query_iter = yield from self._dtsh.query_rpc(
                        xpath="I,/rw-mgmtagt-dts:mgmt-agent-dts",
                        flags=0,
                        msg=ec_rpc)

       for fut_resp in query_iter:
           query_resp = yield from fut_resp
           result = query_resp.result
           if result.pb_request.error is not None:
              return False
           #Verify the config in agent dom
           agent_dom_pb = yield from self.get_agent_dom()
           if agent_dom_pb is not None:
              dop_pb = DnsYang.DnsOptions()
              dop_pb.from_xml_v2(self._nc_proxy.model, agent_dom_pb.agent_dom)
              my_pb = DnsYang.DnsOptions()
              my_pb.from_pbuf(ec_rpc.pb_request.data)
              if ProtobufC.Message.is_equal_deep(None, my_pb.to_pbcm(), dop_pb.to_pbcm()) is not 0:
                 return True
           break # the uAgent sends only a single response

       return False

   @asyncio.coroutine
   def test_pbreq_ec_merge(self):

       def create_ec_pb_req():
           ec_rpc = RwMgmtagtDtsYang.YangInput_RwMgmtagtDts_MgmtAgentDts()
           ec_rpc.pb_request.xpath = "C,/dns:dns"
           dnspb = DnsYang.Dns()
           for i in range(10):
             s1 = dnspb.search.add()
             s1.name = 100
             s1.domain = "abc.com"

           for i in range(11):
             serv1 = dnspb.server.add()
             serv1.address = "10.0.1." + str(i)
             serv1.ttl = 254

           ec_rpc.pb_request.data = dnspb.to_pbuf()
           ec_rpc.pb_request.request_type = "edit_config"
           ec_rpc.pb_request.edit_type = "merge"
  
           return ec_rpc

       ec_rpc = create_ec_pb_req()
       query_iter = yield from self._dtsh.query_rpc(
                        xpath="I,/rw-mgmtagt-dts:mgmt-agent-dts",
                        flags=0,
                        msg=ec_rpc)

       for fut_resp in query_iter:
           query_resp = yield from fut_resp
           result = query_resp.result
           if result.pb_request.error is not None:
              return False
           #Verify the config in agent dom
           agent_dom_pb = yield from self.get_agent_dom()
           if agent_dom_pb is not None:
              dns_pb = DnsYang.Dns()
              dns_pb.from_xml_v2(self._nc_proxy.model, agent_dom_pb.agent_dom)
              my_pb = DnsYang.Dns()
              my_pb.from_pbuf(ec_rpc.pb_request.data)
              if ProtobufC.Message.is_equal_deep(None, my_pb.to_pbcm(), dns_pb.to_pbcm()) is not 0:
                 return True
           break # the uAgent sends only a single response

       return False

   @asyncio.coroutine
   def dts_self_register(self):

       @asyncio.coroutine
       def on_pb_request(xact_info, action, ks_path, msg):
           result = yield from self.run_test()
           ro = self.create_test_response(result)
           xpath = "O,/agt-tb:agent-tests"
           xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ro)

       # Self register with the DTS
       yield from self._dtsh.register(
                xpath = "I,/agt-tb:agent-tests/agt-tb:pb-request-tests/agt-tb:ec-merge",
                flags = rwdts.Flag.PUBLISHER,
                handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_pb_request))

class TestPBRQECDelete(TestBase):
   """
   Test PB Requests edit-conf delete
   """
   def __init__(self, dtsh, logh, nc_proxy, loop):
       TestBase.__init__(self, dtsh, logh, loop)
       self._nc_proxy = nc_proxy
       self._name = "TEST_PB_REQUEST_EC_DEL"

   @asyncio.coroutine
   def get_agent_dom(self):
       a_rpc = RwMgmtagtYang.YangInput_RwMgmtagt_MgmtAgent()
       a_rpc.show_agent_dom = True
       res_obj = yield from self._nc_proxy.rpc(a_rpc, RwMgmtagtYang)
       return res_obj

   @asyncio.coroutine
   def run_test(self): 
       res = yield from self.test_pbreq_ec_del()
       if res is False:
          self._logh.error("PB_REQUEST EC Delete Failed")
          return False

       res = yield from self.test_pbreq_ec_del_cont()
       if res is False:
          self._logh.error("PB_REQUEST EC Delete Container Failed")
          return False

       res = yield from self.test_pbreq_ec_del_leaf()
       if res is False:
          self._logh.error("PB_REQUEST EC Delete leaf Failed")

       return res

   @asyncio.coroutine
   def test_pbreq_ec_del_leaf(self):
       dc_rpc = RwMgmtagtDtsYang.YangInput_RwMgmtagtDts_MgmtAgentDts()
       dc_rpc.pb_request.xpath = "C,/dns:dns/dns:server[dns:address='10.0.1.0']/dns:ttl"
       dc_rpc.pb_request.request_type = "edit_config"
       dc_rpc.pb_request.edit_type = "delete"
       
       query_iter = yield from self._dtsh.query_rpc(
                        xpath="I,/rw-mgmtagt-dts:mgmt-agent-dts",
                        flags=0,
                        msg=dc_rpc)

       for fut_resp in query_iter:
           query_resp = yield from fut_resp
           result = query_resp.result
           if result.pb_request.error is not None:
              return False
           #Verify the deletion in agent dom
           agent_dom_pb = yield from self.get_agent_dom()
           if agent_dom_pb is not None:
              dns_pb = DnsYang.Dns()
              dns_pb.from_xml_v2(self._nc_proxy.model, agent_dom_pb.agent_dom)
              if dns_pb.server[0].ttl is 0:
                 return True;
           break # the uAgent sends only a single response

       return False

   @asyncio.coroutine
   def test_pbreq_ec_del_cont(self):
       dc_rpc = RwMgmtagtDtsYang.YangInput_RwMgmtagtDts_MgmtAgentDts()
       dc_rpc.pb_request.xpath = "C,/dns:dns/dns:options"
       dc_rpc.pb_request.request_type = "edit_config"
       dc_rpc.pb_request.edit_type = "delete"
       
       query_iter = yield from self._dtsh.query_rpc(
                        xpath="I,/rw-mgmtagt-dts:mgmt-agent-dts",
                        flags=0,
                        msg=dc_rpc)

       for fut_resp in query_iter:
           query_resp = yield from fut_resp
           result = query_resp.result
           if result.pb_request.error is not None:
              return False
           #Verify the deletion in agent dom
           agent_dom_pb = yield from self.get_agent_dom()
           if agent_dom_pb is not None:
              dns_pb = DnsYang.Dns()
              dns_pb.from_xml_v2(self._nc_proxy.model, agent_dom_pb.agent_dom)
              if dns_pb.options.ndots is 0:
                 return True;
           break # the uAgent sends only a single response

       return False

   def test_pbreq_ec_del(self):
       dc_rpc = RwMgmtagtDtsYang.YangInput_RwMgmtagtDts_MgmtAgentDts()
       dc_rpc.pb_request.xpath = "C,/dns:dns/dns:server[dns:address='10.0.1.10']"
       dc_rpc.pb_request.request_type = "edit_config"
       dc_rpc.pb_request.edit_type = "delete"
       
       query_iter = yield from self._dtsh.query_rpc(
                        xpath="I,/rw-mgmtagt-dts:mgmt-agent-dts",
                        flags=0,
                        msg=dc_rpc)

       for fut_resp in query_iter:
           query_resp = yield from fut_resp
           result = query_resp.result
           if result.pb_request.error is not None:
              return False
           #Verify the deletion in agent dom
           agent_dom_pb = yield from self.get_agent_dom()
           if agent_dom_pb is not None:
              dns_pb = DnsYang.Dns()
              dns_pb.from_xml_v2(self._nc_proxy.model, agent_dom_pb.agent_dom)
              if len(dns_pb.server) is 10:
                 return True;
           break # the uAgent sends only a single response

       return False

   @asyncio.coroutine
   def dts_self_register(self):

       @asyncio.coroutine
       def on_pb_request(xact_info, action, ks_path, msg):
           result = yield from self.run_test()
           ro = self.create_test_response(result)
           xpath = "O,/agt-tb:agent-tests"
           xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ro)

       # Self register with the DTS
       yield from self._dtsh.register(
                  xpath = "I,/agt-tb:agent-tests/agt-tb:pb-request-tests/agt-tb:ec-delete",
                  flags = rwdts.Flag.PUBLISHER,
                  handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_pb_request))

class TestNotification(TestBase):
   """
   Test Netconf notifications
   """
 
   def __init__(self, dtsh, logh, nc_proxy, loop):
       TestBase.__init__(self, dtsh, logh, loop)
       self._nc_proxy = nc_proxy
       self._name = "TEST_NOTIFICATIONS"
       use_ssl, cert, key = certs.get_bootstrap_cert_and_key()
       if use_ssl:
          self.prefix = "wss"
          self.ssl_context = ssl.SSLContext(ssl.PROTOCOL_SSLv23)
          self.ssl_context.verify_mode = ssl.CERT_NONE
       else:
          self.prefix = "ws"
          self.ssl_context = None

   @asyncio.coroutine
   def uagent_stream_client(self):
      """Asyncio Task that listens to uagent_notification events.

      Connects to the uagent stream using websocket and then receives 4 events.
      """
      ws = yield from websockets.connect(
                  "{}://127.0.0.1:8888/ws_streams/uagent_notification-JSON".format(self.prefix),
                  loop=self._loop,
                  ssl=self.ssl_context)
      self.uagent_client_started.set()
      for i in range(0,4):
         msg = yield from ws.recv()
         print('Uagent Stream Received:', msg)
      self.uagent_success.set()
      yield from ws.close()

   @asyncio.coroutine
   def ha_stream_client(self):
      """Asyncio Task that listens to ha_notification events.

      Connects to the ha stream and expects 1 event.
      """
      ws = yield from websockets.connect(
                  "{}://127.0.0.1:8888/ws_streams/ha_notification-JSON".format(self.prefix),
                  loop=self._loop,
                  ssl=self.ssl_context)
      self.ha_client_started.set()

      msg = yield from ws.recv()
      print("HA Stream Received:", msg)
      self.ha_success.set()
      yield from ws.close()

   @asyncio.coroutine
   def run_test(self):
       # These events are required to sync with the uagent_stream and ha_stream
       # tasks
       self.uagent_client_started = asyncio.Event(loop=self._loop)
       self.ha_client_started = asyncio.Event(loop=self._loop)
       self.uagent_success = asyncio.Event(loop=self._loop)
       self.ha_success = asyncio.Event(loop=self._loop)

       # Launch the uagent_stream and ha_stream tasks that listen on respective
       # streams using websockets
       asyncio.async(self.uagent_stream_client(), loop=self._loop)
       asyncio.async(self.ha_stream_client(), loop=self._loop)

       # Wait till the streams are connected. Also wait for 2 seconds to ensure
       # that the events are not missed
       await_tasks = [ self.uagent_client_started.wait(),
                       self.ha_client_started.wait(),
                       asyncio.sleep(2, loop=self._loop) ]
       yield from asyncio.wait(await_tasks, timeout=5, loop=self._loop)

       # Raise 2 NewRoute alarm and 2 Temp alarm and 1 TaskletFailed alarm 
       xpath = "N,/notif:new-route"
       notif = NotifYang.YangNotif_Notif_NewRoute()
       notif.name = "New Route 1/2WX Added"

       yield from self._dtsh.query_create(xpath, rwdts.XactFlag.ADVISE, notif)
       yield from self._dtsh.query_update(xpath, rwdts.XactFlag.ADVISE, notif)

       xpath = "N,/notif:temp-alarm"
       alarm = NotifYang.YangNotif_Notif_TempAlarm()
       alarm.message = "Temperature above threshold"
       alarm.curr_temp = 100
       alarm.thresh_temp = 80

       yield from self._dtsh.query_create(xpath, rwdts.XactFlag.ADVISE, alarm)
       yield from self._dtsh.query_create(xpath, rwdts.XactFlag.ADVISE, alarm)

       xpath = "N,/notif:test-tasklet-failed"
       failed = NotifYang.YangNotif_Notif_TestTaskletFailed()
       failed.message = "Test Tasklet Failed"
       yield from self._dtsh.query_create(xpath, rwdts.XactFlag.ADVISE, failed)

       # Wait till the stream tasks received the notifications or a timeout
       await_tasks = [ self.uagent_success.wait(),
                       self.ha_success.wait() ]
       yield from asyncio.wait(await_tasks, timeout=5, loop=self._loop)

       return True

   @asyncio.coroutine
   def dts_self_register(self):

       @asyncio.coroutine
       def on_notif(xact_info, action, ks_path, msg):
           result = yield from self.run_test()
           ro = self.create_test_response(result)
           xpath = "O,/agt-tb:agent-tests"
           xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ro)

       # Self register with the DTS
       yield from self._dtsh.register(
                xpath = "I,/agt-tb:agent-tests/agt-tb:netconf-tests/agt-tb:notif",
                flags = rwdts.Flag.PUBLISHER,
                handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_notif))
