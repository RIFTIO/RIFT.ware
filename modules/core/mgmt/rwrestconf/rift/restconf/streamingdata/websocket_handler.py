# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 8/27/2015
# 

import asyncio
import signal
import time

import lxml.etree 
import tornado.gen
import tornado.httpserver
import tornado.ioloop
import tornado.platform.asyncio
import tornado.web
import tornado.websocket

import gi.repository.RwDts as rwdts
from ..translation import SyntaxError

class ClientRegistry(object):
    def __init__(self, logger, message_queue):
        self._logger = logger
        self._clients = set()
        self._registry = dict()

    def un_register(self, client):
        self._clients.remove(client)
        del self._registry[client]
        self._logger.debug("%s is un-registered" % client)

    def register(self, new_client):
        self._clients.add(new_client)
        self._registry[new_client] = list()
        self._logger.debug("%s is registered" % new_client)

    def add_message(self, client, message):
        self._registry[client].append(message)
        
        self._logger.debug("add message: %s : %s" % (client, message))
        

class WebSocketHandler(tornado.websocket.WebSocketHandler):
    def initialize(self, asyncio_loop, logger, client_registry, message_parser, dts):
        self._asyncio_loop = asyncio_loop
        self._client_registry = client_registry
        self._logger = logger
        self._message_parser = message_parser
        self._dts = dts

    @tornado.gen.coroutine
    def open(self):
        self._client_registry.register(self)

    @tornado.gen.coroutine
    def on_message(self, message):
        self._logger.debug("got message: %s %s" % (self, message))

        @asyncio.coroutine
        def impl():
            try:
                xpath = self._message_parser.parse(message)
                self._logger.debug("translated xpath: %s" % xpath)
            except SyntaxError as error:
                self._logger.debug("syntax error in path")
                self.write_message('{"ERROR" : %s}' % error.message)
                return
    
            if self._dts is not None:
                try:
                    res_iter = yield from self._dts.query_read(xpath)
                except:
                    self._logger.error("dts query failed")
                    self.write_message('{"ERROR" : dts query failed}')
                    res_iter = None
                    return
                
                results = list()
                for i in res_iter:
                    result = yield from i
                    results.append(str(result))
            
                result_str = " -- ".join(results)

                self._logger.debug("results: %s" % result_str)

                self.write_message(result_str)

        f = asyncio.async(impl(), loop=self._asyncio_loop)
        response = yield tornado.platform.asyncio.to_tornado_future(f)

    @tornado.gen.coroutine
    def on_close(self):
        self._logger.debug("%s closed" % self)
        self._client_registry.un_register(self)

    @tornado.gen.coroutine
    def check_origin(self, origin):
        return True
    

