# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 10/6/2015

# 

import asyncio
import base64
import hashlib

import ncclient
import ncclient.asyncio_manager
from .netconf_wrapper import NetconfWrapper
from .xml_wrapper import XmlWrapper

NCCLIENT_WORKER_COUNT = 5

ADMIN_HASH='c2c8b22e6749772fb890b5b46f216a9f6dd3e1bb34681ca5add142367239f37a591b90616854a8291456d2dae6662f143410e6e1a4c36c0b4462b559eb38dac4'
OPER_HASH='ed2eac589f7d85f9bcf4602932967b1575cb44757aab81b8369879aa0fad69f34a7aed385f466835745d08b48955aa4e80d34e168517f72fa64e889ab1a28e09'

class AuthenticationError(Exception):
    pass

class ConnectionManager(object):
    '''
    The ConnectionManager is what the http server interacts with to acquire a southbound connection.
    When NETCONF is the southbound connection, the ConnectionManager manages a pool of connections 
    for each username/password combination to allow concurrent requests.
    When the MGMTAGT is the southbound connection, a simple xml wrappers is constructer per request.
    '''

    def __init__(self, configuration, log, loop, netconf_ip, netconf_port, dts, schema_root):
        self._configuration = configuration
        self._log = log
        self._loop = loop
        self._netconf_ip = netconf_ip 
        self._netconf_port = netconf_port
        self._connections = {}
        self._dts = dts
        self._schema_root = schema_root
        self._active_users = set()

    def is_logged_in(self, encoded_username_and_password):
        return encoded_username_and_password in self._active_users

    def logout(self, encoded_username_and_password):
        if not self.is_logged_in(encoded_username_and_password):
            return
        
        auth_decoded = base64.decodestring(bytes(encoded_username_and_password, "utf-8")).decode("utf-8")
        username, password = auth_decoded.split(":",2)

        self._log.info("logging out user: %s", username)
        self._active_users.discard(encoded_username_and_password)
        try:
            del self._connections[encoded_username_and_password]
        except KeyError:
            # we're probably in xml mode
            pass

    @asyncio.coroutine
    def get_connection(self, encoded_username_and_password):
        '''
        Returns a netconf connection from the pool allocated for the username/password. If no pool 
        has been created, create the pool and return a connection from it.
        '''

        hashed_credentials = hashlib.sha512(bytes(encoded_username_and_password, "utf-8")).hexdigest()

        if not self._configuration.use_netconf:
            if hashed_credentials not in (ADMIN_HASH, OPER_HASH):
                raise AuthenticationError()
            self._active_users.add(encoded_username_and_password)
            return XmlWrapper(self._log, self._dts, self._schema_root, hashed_credentials==ADMIN_HASH)
        
        auth_decoded = base64.decodestring(bytes(encoded_username_and_password, "utf-8")).decode("utf-8")
        try:
            username, password = auth_decoded.split(":",2)
        except:
            raise ValueError("Invalid username and password")

        if encoded_username_and_password in self._connections.keys():
            # return a cached connection
            index_of_next_connection, connections = self._connections[encoded_username_and_password][:]
            ret = connections[index_of_next_connection]
            index_of_next_connection += 1

            if index_of_next_connection >= len(connections):
                index_of_next_connection = 0

            self._connections[encoded_username_and_password] = (index_of_next_connection, connections)
                
            return ret

        # this is a new username/password combo so cache a pool of connections
        wrappers = list()
        
        self._log.debug("starting new connection with confd for %s" % username)
        for _ in range(NCCLIENT_WORKER_COUNT):
            new_wrapper = NetconfWrapper(
                self._log,
                self._loop,
                self._netconf_ip,
                self._netconf_port,
                username,
                password)
            try:
                yield from new_wrapper.connect()
            except ncclient.transport.errors.AuthenticationError:
                raise AuthenticationError()
            wrappers.append(new_wrapper)

        self._connections[encoded_username_and_password] = (0, wrappers)
        
        self._active_users.add(encoded_username_and_password)

        return wrappers[0]

    @asyncio.coroutine
    def reconnect(self, encoded_username_and_password):
        for itr in range(NCCLIENT_WORKER_COUNT):
            try:
                yield from self._connections[encoded_username_and_password][1][itr].connect()
            except ncclient.transport.errors.AuthenticationError:
                raise AuthenticationError()

