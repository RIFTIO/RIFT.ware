"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file rwcalproxytasklet.py
@author Austin Cormier(austin.cormier@riftio.com)
@date 2015-10-20
"""

import asyncio
import collections
import concurrent.futures
import logging
import os
import sys

import tornado
import tornado.httpserver
import tornado.web
import tornado.platform.asyncio

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwCal', '1.0')

from gi.repository import (
    RwDts as rwdts,
    RwcalYang,
    RwTypes,
)

import rw_peas
import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class CalCallFailure(Exception):
    pass


class RPCParam(object):
    def __init__(self, key, proto_type=None):
        self.key = key
        self.proto_type = proto_type


class CalRequestHandler(tornado.web.RequestHandler):
    def initialize(self, log, loop, cal, account, executor, cal_method,
                   input_params=None, output_params=None):
        self.log = log
        self.loop = loop
        self.cal = cal
        self.account = account
        self.executor = executor
        self.cal_method = cal_method
        self.input_params = input_params
        self.output_params = output_params

    def wrap_status_fn(self, fn, *args, **kwargs):
        ret = fn(*args, **kwargs)
        if not isinstance(ret, collections.Iterable):
            ret = [ret]

        rw_status = ret[0]
        if type(rw_status) != RwTypes.RwStatus:
            raise ValueError("First return value of %s function was not a RwStatus" %
                             fn.__name__)

        if rw_status != RwTypes.RwStatus.SUCCESS:
            msg = "%s returned %s" % (fn.__name__, str(rw_status))
            self.log.error(msg)
            raise CalCallFailure(msg)

        return ret[1:]

    @tornado.gen.coroutine
    def post(self):
        def body_to_cal_args():
            cal_args = []
            if self.input_params is None:
                return cal_args

            input_dict = tornado.escape.json_decode(self.request.body)
            if len(input_dict) != len(self.input_params):
                raise ValueError("Got %s parameters, expected %s" %
                                 (len(input_dict), len(self.input_params)))

            for input_param in self.input_params:
                key = input_param.key
                value = input_dict[key]
                proto_type = input_param.proto_type

                if proto_type is not None:
                    proto_cls = getattr(RwcalYang, proto_type)
                    self.log.debug("Deserializing into %s type", proto_cls)
                    value = proto_cls.from_dict(value)

                cal_args.append(value)

            return cal_args

        def cal_return_vals(return_vals):
            output_params = self.output_params
            if output_params is None:
                output_params = []

            if len(return_vals) != len(output_params):
                raise ValueError("Got %s return values.  Expected %s",
                                 len(return_vals), len(output_params))

            write_dict = {"return_vals": []}
            for i, output_param in enumerate(output_params):
                key = output_param.key
                proto_type = output_param.proto_type
                output_value = return_vals[i]

                if proto_type is not None:
                    output_value = output_value.as_dict()

                return_val = {
                        "key": key,
                        "value": output_value,
                        "proto_type": proto_type,
                        }

                write_dict["return_vals"].append(return_val)

            return write_dict

        @asyncio.coroutine
        def handle_request():
            self.log.debug("Got cloudsimproxy POST request: %s", self.request.body)
            cal_args = body_to_cal_args()

            # Execute the CAL request in a seperate thread to prevent
            # blocking the main loop.
            return_vals = yield from self.loop.run_in_executor(
                    self.executor,
                    self.wrap_status_fn,
                    getattr(self.cal, self.cal_method),
                    self.account,
                    *cal_args
                    )

            return cal_return_vals(return_vals)

        f = asyncio.ensure_future(handle_request(), loop=self.loop)
        return_dict = yield tornado.platform.asyncio.to_tornado_future(f)

        self.log.debug("Responding to %s RPC with %s", self.cal_method, return_dict)

        self.clear()
        self.set_status(200)
        self.write(return_dict)


class CalProxyApp(tornado.web.Application):
    def __init__(self, log, loop, cal_interface, cal_account):
        self.log = log
        self.loop = loop
        self.cal = cal_interface
        self.account = cal_account

        attrs = dict(
            log=self.log,
            loop=self.loop,
            cal=cal_interface,
            account=cal_account,
            # Create an executor with a single worker to prevent
            # having multiple simulteneous calls into CAL (which is not threadsafe)
            executor=concurrent.futures.ThreadPoolExecutor(1)
            )

        def mk_attrs(cal_method, input_params=None, output_params=None):
            new_attrs = {
                    "cal_method": cal_method,
                    "input_params": input_params,
                    "output_params": output_params
                    }
            new_attrs.update(attrs)

            return new_attrs

        super(CalProxyApp, self).__init__([
            (r"/api/get_image_list", CalRequestHandler,
                mk_attrs(
                    cal_method="get_image_list",
                    output_params=[
                        RPCParam("images", "VimResources"),
                        ]
                    ),
                ),

            (r"/api/create_image", CalRequestHandler,
                mk_attrs(
                    cal_method="create_image",
                    input_params=[
                        RPCParam("image", "ImageInfoItem"),
                        ],
                    output_params=[
                        RPCParam("image_id"),
                        ]
                    ),
                ),

            (r"/api/delete_image", CalRequestHandler,
                mk_attrs(
                    cal_method="delete_image",
                    input_params=[
                        RPCParam("image_id"),
                        ],
                    ),
                ),

            (r"/api/get_image", CalRequestHandler,
                mk_attrs(
                    cal_method="get_image",
                    input_params=[
                        RPCParam("image_id"),
                        ],
                    output_params=[
                        RPCParam("image", "ImageInfoItem"),
                        ],
                    ),
                ),

            (r"/api/create_vm", CalRequestHandler,
                mk_attrs(
                    cal_method="create_vm",
                    input_params=[
                        RPCParam("vm", "VMInfoItem"),
                        ],
                    output_params=[
                        RPCParam("vm_id"),
                        ],
                    ),
                ),

            (r"/api/start_vm", CalRequestHandler,
                    mk_attrs(
                        cal_method="start_vm",
                        input_params=[
                            RPCParam("vm_id"),
                            ],
                        ),
                    ),

            (r"/api/stop_vm", CalRequestHandler,
                    mk_attrs(
                        cal_method="stop_vm",
                        input_params=[
                            RPCParam("vm_id"),
                            ],
                        ),
                    ),

            (r"/api/delete_vm", CalRequestHandler,
                    mk_attrs(
                        cal_method="delete_vm",
                        input_params=[
                            RPCParam("vm_id"),
                            ],
                        ),
                    ),

            (r"/api/reboot_vm", CalRequestHandler,
                    mk_attrs(
                        cal_method="reboot_vm",
                        input_params=[
                            RPCParam("vm_id"),
                            ],
                        ),
                    ),

            (r"/api/get_vm_list", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_vm_list",
                        output_params=[
                            RPCParam("vms", "VimResources"),
                            ],
                        ),
                    ),

            (r"/api/get_vm", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_vm",
                        input_params=[
                            RPCParam("vm_id"),
                            ],
                        output_params=[
                            RPCParam("vms", "VMInfoItem"),
                            ],
                        ),
                    ),

            (r"/api/create_flavor", CalRequestHandler,
                    mk_attrs(
                        cal_method="create_flavor",
                        input_params=[
                            RPCParam("flavor", "FlavorInfoItem"),
                            ],
                        output_params=[
                            RPCParam("flavor_id"),
                            ],
                        ),
                    ),

            (r"/api/delete_flavor", CalRequestHandler,
                    mk_attrs(
                        cal_method="delete_flavor",
                        input_params=[
                            RPCParam("flavor_id"),
                            ],
                        ),
                    ),

            (r"/api/get_flavor_list", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_flavor_list",
                        output_params=[
                            RPCParam("flavors", "VimResources"),
                            ],
                        ),
                    ),

            (r"/api/get_flavor", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_flavor",
                        input_params=[
                            RPCParam("flavor_id"),
                            ],
                        output_params=[
                            RPCParam("flavor", "FlavorInfoItem"),
                            ],
                        ),
                    ),

            (r"/api/create_network", CalRequestHandler,
                    mk_attrs(
                        cal_method="create_network",
                        input_params=[
                            RPCParam("network", "NetworkInfoItem"),
                            ],
                        output_params=[
                            RPCParam("network_id"),
                            ],
                        ),
                    ),

            (r"/api/delete_network", CalRequestHandler,
                    mk_attrs(
                        cal_method="delete_network",
                        input_params=[
                            RPCParam("network_id"),
                            ],
                        ),
                    ),

            (r"/api/get_network", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_network",
                        input_params=[
                            RPCParam("network_id"),
                            ],
                        output_params=[
                            RPCParam("network", "NetworkInfoItem"),
                            ],
                        ),
                    ),

            (r"/api/get_network_list", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_network_list",
                        output_params=[
                            RPCParam("networks", "VimResources"),
                            ],
                        ),
                    ),

            (r"/api/get_management_network", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_management_network",
                        output_params=[
                            RPCParam("network", "NetworkInfoItem"),
                            ],
                        ),
                    ),

            (r"/api/create_port", CalRequestHandler,
                    mk_attrs(
                        cal_method="create_port",
                        input_params=[
                            RPCParam("port", "PortInfoItem"),
                            ],
                        output_params=[
                            RPCParam("port_id"),
                            ],
                        ),
                    ),

            (r"/api/delete_port", CalRequestHandler,
                    mk_attrs(
                        cal_method="delete_port",
                        input_params=[
                            RPCParam("port_id"),
                            ],
                        ),
                    ),

            (r"/api/get_port", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_port",
                        input_params=[
                            RPCParam("port_id"),
                            ],
                        output_params=[
                            RPCParam("port", "PortInfoItem"),
                            ],
                        ),
                    ),

            (r"/api/get_port_list", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_port_list",
                        output_params=[
                            RPCParam("ports", "VimResources"),
                            ],
                        ),
                    ),

            (r"/api/create_virtual_link", CalRequestHandler,
                    mk_attrs(
                        cal_method="create_virtual_link",
                        input_params=[
                            RPCParam("link_params", "VirtualLinkReqParams"),
                            ],
                        output_params=[
                            RPCParam("link_id"),
                            ],
                        ),
                    ),

            (r"/api/delete_virtual_link", CalRequestHandler,
                    mk_attrs(
                        cal_method="delete_virtual_link",
                        input_params=[
                            RPCParam("link_id"),
                            ],
                        ),
                    ),

            (r"/api/get_virtual_link", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_virtual_link",
                        input_params=[
                            RPCParam("link_id"),
                            ],
                        output_params=[
                            RPCParam("response", "VirtualLinkInfoParams"),
                            ],
                        ),
                    ),

            (r"/api/get_virtual_link_list", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_virtual_link_list",
                        output_params=[
                            RPCParam("resources", "VNFResources"),
                            ],
                        ),
                    ),

            (r"/api/create_vdu", CalRequestHandler,
                    mk_attrs(
                        cal_method="create_vdu",
                        input_params=[
                            RPCParam("vdu_params", "VDUInitParams"),
                            ],
                        output_params=[
                            RPCParam("vdu_id"),
                            ],
                        ),
                    ),

            (r"/api/modify_vdu", CalRequestHandler,
                    mk_attrs(
                        cal_method="modify_vdu",
                        input_params=[
                            RPCParam("vdu_params", "VDUModifyParams"),
                            ],
                        ),
                    ),

            (r"/api/delete_vdu", CalRequestHandler,
                    mk_attrs(
                        cal_method="delete_vdu",
                        input_params=[
                            RPCParam("vdu_id"),
                            ],
                        ),
                    ),

            (r"/api/get_vdu", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_vdu",
                        input_params=[
                            RPCParam("vdu_id"),
                            ],
                        output_params=[
                            RPCParam("response", "VDUInfoParams"),
                            ],
                        ),
                    ),

            (r"/api/get_vdu_list", CalRequestHandler,
                    mk_attrs(
                        cal_method="get_vdu_list",
                        output_params=[
                            RPCParam("resources", "VNFResources"),
                            ],
                        ),
                    )
            ])


class RwCalProxyTasklet(rift.tasklets.Tasklet):
    HTTP_PORT = 9002
    cal_interface = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.app = None
        self.server = None

    def get_cal_interface(self):
        if RwCalProxyTasklet.cal_interface is None:
            plugin = rw_peas.PeasPlugin('rwcal_cloudsim', 'RwCal-1.0')
            engine, info, extension = plugin()

            RwCalProxyTasklet.cal_interface = plugin.get_interface("Cloud")
            RwCalProxyTasklet.cal_interface.init(self.log_hdl)

        return RwCalProxyTasklet.cal_interface

    def start(self):
        """Tasklet entry point"""
        self.log.setLevel(logging.DEBUG)

        super().start()

        cal = self.get_cal_interface()
        account = RwcalYang.CloudAccount(account_type="cloudsim")

        self.app = CalProxyApp(self.log, self.loop, cal, account)
        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                RwcalYang.get_schema(),
                self.loop,
                self.on_dts_state_change
                )

        io_loop = rift.tasklets.tornado.TaskletAsyncIOLoop(asyncio_loop=self.loop)
        self.server = tornado.httpserver.HTTPServer(
                self.app,
                io_loop=io_loop,
                )

        self.log.info("Starting Cal Proxy Http Server on port %s",
                      RwCalProxyTasklet.HTTP_PORT)
        self.server.listen(RwCalProxyTasklet.HTTP_PORT)

    def stop(self):
      try:
         self.server.stop()
         self._dts.deinit()
      except Exception:
         print("Caught Exception in LP stop:", sys.exc_info()[0])
         raise

    @asyncio.coroutine
    def init(self):
        pass

    @asyncio.coroutine
    def run(self):
        pass

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application into the corresponding application state

        Arguments
            state - current dts state
        """

        switch = {
            rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
            rwdts.State.CONFIG: rwdts.State.RUN,
        }

        handlers = {
            rwdts.State.INIT: self.init,
            rwdts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers.get(state, None)
        if handler is not None:
            yield from handler()

        # Transition dts to next state
        next_state = switch.get(state, None)
        if next_state is not None:
            self._dts.handle.set_state(next_state)
