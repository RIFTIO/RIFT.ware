# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 8/15/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

import asyncio
from enum import Enum
import sys
import socket

import tornado
import gi

gi.require_version('RwDynSchema', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwRestconfYang', '1.0')
gi.require_version('RwYang', '1.0')
gi.require_version('RwMgmtSchemaYang', '1.0')
gi.require_version('RwMgmtagtYang', '1.0')
gi.require_version('IetfRestconfMonitoringYang', '1.0')

from gi.repository import (
    RwDts,
    RwDynSchema,
    RwRestconfYang,
    RwYang,
    RwMgmtagtYang,
    RwMgmtSchemaYang,
    IetfRestconfMonitoringYang,
)
import rift.tasklets
import gi.repository.RwTypes as rwtypes

from rift.restconf import (
    ConfdRestTranslator,
    Configuration,
    HttpHandler,
    ConnectionManager,
    XmlToJsonTranslator,
    Statistics,
    load_schema_root,
    StateProvider,
    LogoutHandler,
    ReadOnlyHandler,
    ConfdRestTranslatorV2
)

import rift.restconf.webserver.event_source as event_source

class SchemaState(Enum):
    working = 'working'
    waiting = 'waiting'
    ready = 'ready'
    initializing = 'initializing'
    error = 'error'

def dyn_schema_callback(instance, numel, modules):
    instance._schema_state = SchemaState.waiting
    for module in modules:
        instance._pending_modules[module.module_name] = module.so_filename

    if not instance._initialized:
        instance._initialize_composite()

def _load_schema(schema_name):
    yang_model = RwYang.Model.create_libncx()
    schema = RwYang.Model.load_and_get_schema(schema_name)
    yang_model.load_schema_ypbc(schema)

    return schema

if sys.version_info < (3,4,4):
    asyncio.ensure_future = asyncio.async

class RestconfTasklet(rift.tasklets.Tasklet):
    NETCONF_SERVER_IP = "127.0.0.1"
    NETCONF_SERVER_PORT = "2022"
    RESTCONF_PORT = "8888"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.rwlog.set_category("rw-restconf-log")
        self._server = None
        self._state_provider = None

    def start(self):
        """Tasklet entry point"""
        super(RestconfTasklet, self).start()

        self._initialized = False
        self._tasklet_name = "RwRestconf"
        self.get_stats = 0
        self.schema_name = "rw-restconf"#
        self._schema_state = SchemaState.initializing

        self._dynamic_schema_publish="D,/rw-mgmt-schema:rw-mgmt-schema-state/rw-mgmt-schema:listening-apps[name='%s']" % self._tasklet_name
        self._dynamic_schema_response = RwMgmtSchemaYang.YangData_RwMgmtSchema_RwMgmtSchemaState_ListeningApps()
        self._dynamic_schema_response.app_type = "nb_interface"
        self._dynamic_schema_response.name = self._tasklet_name
        self._dynamic_schema_response.state = self._schema_state.value
        self._manifest = self.tasklet_info.get_pb_manifest()
        self._pending_modules = dict()
        self._ssl_cert = self._manifest.bootstrap_phase.rwsecurity.cert
        self._ssl_key = self._manifest.bootstrap_phase.rwsecurity.key
        self._stats_pb = RwRestconfYang.Restconfstats()

        self._dts = rift.tasklets.DTS(
            self.tasklet_info,
            RwRestconfYang.get_schema(),
            self.loop,
            self.on_dts_state_change)

    def stop(self):
      try:
         self._dts.deinit()
      except Exception:
         raise

    @asyncio.coroutine
    def start_server(self):

        timeo = float(self._agent_wait_timeout_secs)
        while timeo > 0:
            if self._agent_is_ready and self._initialized:
                self._log.info("Starting restconf server")
                self._start_server()
                return
            else:
                step = 0.1
                timeo = timeo - step
                yield from asyncio.sleep(step, loop = self.loop)

        # log error and and try again 
        if not self._initialized:
            self._log.error("Rwrest didn't load schema after %d seconds", self._agent_wait_timeout_secs)
        if not self._agent_is_ready:
            self._log.error("Rwrest didn't connect to agent after %d seconds", self._agent_wait_timeout_secs)

        self._agent_wait_timeout_secs = 15 # we've already waited a long time, start complaining more
        self.loop.create_task(self.start_server())

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application in to the corresponding application state
        """
        switch = {
            RwDts.State.INIT: RwDts.State.RUN,
        }

        handlers = {
            RwDts.State.INIT: self.init,
            RwDts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers[state]
        yield from handler()

        # Transition dts to next state
        try:
            next_state = switch[state]
        except KeyError:
            # we don't handle all state changes
            return

        self._dts.handle.set_state(next_state)

    @asyncio.coroutine
    def init(self):
        self._configuration = Configuration()
        self._configuration.use_https = self._manifest.bootstrap_phase.rwsecurity.use_ssl
        self._configuration.use_netconf = self._manifest.bootstrap_phase.rwmgmt.agent_mode == "CONFD"

        self._statistics = Statistics()
        self._messages = {}
        self._agent_is_ready = False
        self._agent_wait_timeout_secs = 360

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
            self._messages[xact.id] = msg
            acg.handle.prepare_complete_ok(xact_info.handle)

        @asyncio.coroutine
        def on_prepare_config_state(xact_info, action, path, msg):
            self._log.info("Agent is ready to receive requests")
            self._agent_is_ready = True
            xact_info.respond_xpath(RwDts.XactRspCode.ACK, path.create_string())

        def on_apply(dts, acg, xact, action, scratch):
            if action == RwDts.AppconfAction.INSTALL and xact.id is None:
                return

            if xact.id not in self._messages:
                raise KeyError("No stashed configuration found with transaction id [{}]".format(xact.id))

            toggles = self._messages[xact.id]

            if toggles.has_field("log_timing"):
                self._configuration.log_timing = toggles.log_timing

            if toggles.has_field("use_https"):
                self._configuration.use_https = toggles.use_https
                
            del self._messages[xact.id]

            if self._initialized:
                self._start_server()

        def agent_config_ready_cb(dts, acg, xact, action, scratch):
            # ATTN: Config ready needs to be handled here
            if action == RwDts.AppconfAction.INSTALL and xact.id is None:
                return
            self._log.info("Agent is ready to receive requests")
            self._agent_is_ready = True

            if xact.id in self._messages:
                del self._messages[xact.id]

        def agent_ready_read_cb(xact, xact_status, user_data):
            self._log.debug("agent_ready_cb")
            if self._agent_is_ready:
                return

            if xact is None:
                self._log.debug("xact is none")
                return

            query_result = xact.query_result()
            while query_result is not  None:
                pbcm = query_result.protobuf
                data = RwMgmtagtYang.State_ConfigState.from_pbcm(pbcm)
                if data.ready:
                    self._agent_is_ready = True
                else:
                    self._agent_is_ready = False
                break
        with self._dts.appconf_group_create(
                handler=rift.tasklets.AppConfGroup.Handler(
                    on_apply=on_apply)) as acg:
            acg.register(
                xpath="C,/rw-restconf:rwrestconf-configuration",
                flags=RwDts.Flag.SUBSCRIBER,
                on_prepare=on_prepare)

        yield from self._dts.register(
                flags=RwDts.Flag.SUBSCRIBER,
                xpath="D,/rw-mgmtagt:uagent/rw-mgmtagt:state/rw-mgmtagt:config-state",
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare_config_state))
        # Check for agent's state in case of restart
        xact = self._dts.handle.query(
                      "D,/rw-mgmtagt:uagent/rw-mgmtagt:state/rw-mgmtagt:config-state",
                      RwDts.QueryAction.READ,
                      0,
                      agent_ready_read_cb,
                      self)
            
        self._ready_for_schema = False

        #start waiting for the agent
        self.loop.create_task(self.start_server())

        @asyncio.coroutine
        def dynamic_schema_state(xact_info, action, ks_path, msg):
            self._dynamic_schema_response.state = self._schema_state.value
            xact_info.respond_xpath(RwDts.XactRspCode.ACK, self._dynamic_schema_publish, self._dynamic_schema_response)
            
        yield from self._dts.register(
                flags=RwDts.Flag.PUBLISHER,
                xpath=(self._dynamic_schema_publish),                       
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=dynamic_schema_state))

        @asyncio.coroutine
        def load_modules_prepare(xact_info, action, path, msg):
            if msg.state != "loading_nb_interfaces":
                xact_info.respond_xpath(RwDts.XactRspCode.ACK, path.create_string())
                return

            self._schema_state = SchemaState.working

            module_name = msg.name


            so_filename = self._pending_modules[module_name]

            del self._pending_modules[module_name]

            new_schema = RwYang.Model.load_and_merge_schema(self._schema, so_filename, module_name)
            self._schema = new_schema

            yang_model = RwYang.Model.create_libncx()
            yang_model.load_schema_ypbc(new_schema)    
           
            new_root = yang_model.get_root_node()

            self._schema_root = new_root
            self._confd_url_converter._schema = new_root
            self._confd_url_converter_v2._schema = new_root
            self._xml_to_json_translator._schema = new_root
            self._connection_manager._schema_root = new_root

            self._schema_state = SchemaState.ready
            xact_info.respond_xpath(RwDts.XactRspCode.ACK, path.create_string())

        yield from self._dts.register(
                flags=RwDts.Flag.SUBSCRIBER,
                xpath="D,/rw-mgmt-schema:rw-mgmt-schema-state/rw-mgmt-schema:dynamic-modules",
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=load_modules_prepare))

        def on_copy(shard, key, ctx):
            self._stats_pb.get_req = self._statistics.get_req
            self._stats_pb.put_req = self._statistics.put_req
            self._stats_pb.post_req = self._statistics.post_req
            self._stats_pb.del_req = self._statistics.del_req
            self._stats_pb.get_200_rsp = self._statistics.get_200_rsp
            self._stats_pb.get_404_rsp = self._statistics.get_404_rsp
            self._stats_pb.get_204_rsp = self._statistics.get_204_rsp
            self._stats_pb.get_500_rsp = self._statistics.get_500_rsp
            self._stats_pb.put_200_rsp = self._statistics.put_200_rsp
            self._stats_pb.put_404_rsp = self._statistics.put_404_rsp
            self._stats_pb.put_500_rsp = self._statistics.put_500_rsp
            self._stats_pb.post_200_rsp = self._statistics.post_200_rsp
            self._stats_pb.post_404_rsp = self._statistics.post_404_rsp
            self._stats_pb.post_500_rsp = self._statistics.post_500_rsp
            self._stats_pb.del_405_rsp = self._statistics.del_405_rsp
            self._stats_pb.del_404_rsp = self._statistics.del_404_rsp
            self._stats_pb.del_500_rsp = self._statistics.del_500_rsp
            self._stats_pb.del_200_rsp = self._statistics.del_200_rsp
            self._stats_pb.put_409_rsp = self._statistics.put_409_rsp
            self._stats_pb.put_405_rsp = self._statistics.put_405_rsp
            self._stats_pb.put_201_rsp = self._statistics.put_201_rsp
            self._stats_pb.post_409_rsp = self._statistics.post_409_rsp
            self._stats_pb.post_405_rsp = self._statistics.post_405_rsp
            self._stats_pb.post_201_rsp = self._statistics.post_201_rsp
            self._stats_pb.req_401_rsp = self._statistics.req_401_rsp

            evtsrc = self._stats_pb.eventsource_statistics
            evtsrc.websocket_stream_open = self._statistics.websocket_stream_open
            evtsrc.websocket_stream_close = self._statistics.websocket_stream_close
            evtsrc.websocket_events = self._statistics.websocket_events
            evtsrc.http_stream_open = self._statistics.http_stream_open
            evtsrc.http_stream_close = self._statistics.http_stream_close
            evtsrc.http_events = self._statistics.http_events
  
            return rwtypes.RwStatus.SUCCESS, self._stats_pb.to_pbcm()

        @asyncio.coroutine
        def get_prepare(xact_info, action, ks_path, msg):
             xact_info.respond_xpath(rwdts.XactRspCode.NA, xpath="D,/rw-restconf:rwrestconf-statistics")

        @asyncio.coroutine
        def get_restconf_state(xact_info, action, ks_path, msg):
            """Provides the RestConf state to the DTS.

            Invoked when UAgent requests DTS to provide the restconf-state.
            """
            xpath = "D,/rcmon:restconf-state"
            if self._state_provider is None:
              self._log.error("restconf-state provider not initialized")
              xact_info.send_error_xpath(rwtypes.RwStatus.FAILURE,
                          xpath, "RW.RESTCONF not ready")
              xact_info.respond_xpath(RwDts.XactRspCode.NACK, xpath)
              return

            try:
                restconf_state = yield from self._state_provider.get_state()
                xact_info.respond_xpath(RwDts.XactRspCode.ACK, xpath, restconf_state)
            except:
                self._log.exception("Fetching state failed")
                xact_info.send_error_xpath(rwtypes.RwStatus.FAILURE,
                          xpath, "RW.RESTCONF internal error")
                xact_info.respond_xpath(RwDts.XactRspCode.NACK, xpath)

        reg = yield from self._dts.register(
                  flags=RwDts.Flag.PUBLISHER|RwDts.Flag.NO_PREP_READ,
                  xpath="D,/rw-restconf:rwrestconf-statistics",
                  handler=rift.tasklets.DTS.RegistrationHandler(on_prepare=get_prepare))

        shard = yield from reg.shard_init(flags=RwDts.Flag.PUBLISHER)
        shard.appdata_register_queue_key(copy=on_copy)

        # Register with DTS for providing restconf-state operational data
        yield from self._dts.register(
                        flags=RwDts.Flag.PUBLISHER,
                        xpath="D,/rcmon:restconf-state",
                        handler=rift.tasklets.DTS.RegistrationHandler(
                        on_prepare=get_restconf_state))
       
        self._schema = RwRestconfYang.get_schema()

        self._dynamic_schema_registration = RwDynSchema.rwdynschema_instance_register(
            self._dts.handle,
            dyn_schema_callback,
            "RwRestconf",
            RwDynSchema.RwdynschemaAppType.NORTHBOUND,
            self)
    
    @asyncio.coroutine
    def run(self):
        self._schema_state = SchemaState.ready
        
    def _initialize_composite(self):
        for module_name, so_filename in self._pending_modules.items():
            new_schema = RwYang.Model.load_and_merge_schema(self._schema, so_filename, module_name)
            self._schema = new_schema
        self._pending_modules.clear()

        yang_model = RwYang.Model.create_libncx()
        yang_model.load_schema_ypbc(self._schema)    
        self._schema_root = yang_model.get_root_node()
        self._initialized = True

    def _start_server(self):
        if self._server is not None:
            self._server.stop()
        self._connection_manager = ConnectionManager(
                                              self._configuration,
                                              self._log,
                                              self.loop, 
                                              self.NETCONF_SERVER_IP,
                                              self.NETCONF_SERVER_PORT,
                                              self._dts,
                                              self._schema_root)
        self._confd_url_converter = ConfdRestTranslator(self._schema_root)
        self._confd_url_converter_v2 = ConfdRestTranslatorV2(self._schema_root)
        self._xml_to_json_translator = XmlToJsonTranslator(self._schema_root, self._log)

        webhost = "{}:{}".format(
                        socket.gethostbyname(socket.getfqdn()),
                        self.RESTCONF_PORT)
        self._state_provider = StateProvider(
                                  self._log,
                                  self.loop,
                                  self.NETCONF_SERVER_IP, 
                                  self.NETCONF_SERVER_PORT,
                                  webhost,
                                  self._configuration.use_https)

        http_handler_arguments = {
            "asyncio_loop" : self.loop,
            "confd_url_converter" : self._confd_url_converter,
            "configuration" : self._configuration,
            "connection_manager" : self._connection_manager,
            "logger" : self._log,
            "schema_root" : self._schema_root,
            "statistics" : self._statistics,
            "xml_to_json_translator" : self._xml_to_json_translator,
        }

        http_handler_arguments_v2 = http_handler_arguments.copy() 
        http_handler_arguments_v2["confd_url_converter"] = self._confd_url_converter_v2

        stream_handler_args = {
            "logger" : self._log,
            "loop" : self.loop,
            "netconf_ip" : self.NETCONF_SERVER_IP,
            "netconf_port" : self.NETCONF_SERVER_PORT, 
            "statistics" : self._statistics,
            "xml_to_json_translator" : self._xml_to_json_translator,
        }
        logout_handler_arguments = {
            "logger" : self._log,
            "connection_manager" : self._connection_manager,
            "asyncio_loop" : self.loop,
        }


        # provide a read-only handler that always uses the xml-mode for speed
        self._read_only_configuration = Configuration()
        self._read_only_configuration.use_netconf = False
        self._read_only_connection_manager = ConnectionManager(
                                              self._configuration,
                                              self._log,
                                              self.loop, 
                                              self.NETCONF_SERVER_IP,
                                              self.NETCONF_SERVER_PORT,
                                              self._dts,
                                              self._schema_root)
        read_only_handler_arguments = {
            "logger" : self._log,
            "connection_manager" : self._read_only_connection_manager,
            "schema_root" : self._schema_root,
            "confd_url_converter" : self._confd_url_converter,
            "xml_to_json_translator" : self._xml_to_json_translator,
            "asyncio_loop" : self.loop,
            "configuration" : self._configuration,
            "statistics" : self._statistics,
        }

        application = tornado.web.Application([
            (r"(/v1)?/api/readonly/(config|running|operational)/(.*)", ReadOnlyHandler, read_only_handler_arguments),
            (r"(/v1)?/api/(config|operational|operations|running|schema)/(.*)", HttpHandler, http_handler_arguments),                        
            (r"/v2/api/(config|running)(/)?(.*)", HttpHandler, http_handler_arguments_v2), 
            (r"(/v1)?/api/logout", LogoutHandler, logout_handler_arguments),            
            (r"(/v1)?/api/schema", HttpHandler, http_handler_arguments),
            (r"(/v1)?/streams/(.*)", event_source.HttpStreamHandler, stream_handler_args),
            (r"(/v1)?/ws_streams/(.*)", event_source.WebSocketStreamHandler, stream_handler_args),
        ], compress_response=True)


        io_loop = rift.tasklets.tornado.TaskletAsyncIOLoop(asyncio_loop=self.loop)
        if self._configuration.use_https:
            ssl_options = {
                "certfile" : self._ssl_cert,
                "keyfile" : self._ssl_key,
            }
            self._server = tornado.httpserver.HTTPServer(
                application,
                io_loop=io_loop,
                ssl_options=ssl_options,
            )
        else:
            self._server = tornado.httpserver.HTTPServer(
                application,
                io_loop=io_loop,
            )

        self._server.listen(self.RESTCONF_PORT)
        
