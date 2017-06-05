# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 10/6/2015

# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

import asyncio
import lxml

import ncclient.asyncio_manager

from ..util import (
    Result,
    NetconfOperation,
)

def _check_exception(error_text):
    if ("session close" in error_text) or ("Not connected" in error_text) or ("Capabilities changed" in error_text) or ("capabilities-changed" in error_text) or ("Not connected to NETCONF server" in error_text):
        result = Result.Upgrade_Performed
    else:
        result = Result.Unknown_Error
    return result

def _check_netconf_response(netconf_response, commit_response):
    if netconf_response.ok:
        netconf_result = Result.OK
        if "data-exists" in netconf_response.xml:
            netconf_result = Result.Data_Exists
        elif "rpc-error" in netconf_response.xml:
            netconf_result = Result.Rpc_Error
    else:
        if "resource-denied" in netconf_response.error.tag:
            return Result.Upgrade_Performed
        else:
            return Result.Operation_Failed

    if commit_response is not None and "rpc-error" in commit_response.xml:
        return Result.Commit_Failed
    else:
        return netconf_result

class NetconfWrapper(object):
    '''
    This class handles the communication with the NETCONF server.
    '''
    def __init__(self, logger, loop, netconf_ip, netconf_port, username, password):
        self._log = logger
        self._loop = loop
        self._netconf_ip = netconf_ip 
        self._netconf_port = netconf_port
        self._username = username
        self._password = password
        self._netconf  = None

        self._operation_map = {
            NetconfOperation.DELETE : self.delete,
            NetconfOperation.GET : self.get,
            NetconfOperation.GET_CONFIG : self.get_config,
            NetconfOperation.CREATE : self.post,
            NetconfOperation.REPLACE : self.put,
            NetconfOperation.RPC : self.rpc,
        }

    def set_notification_callback(self, notification_cbk):
        """Sets the notification callback for the Netconf session.

        Arguments:
            notification_cbk - Callback that will he invoked when a Notification
                               message is recevied on the session.
        """
        self._netconf.register_notification_callback(notification_cbk)

    @asyncio.coroutine
    def close(self):
        """Closes the Netconf session.
        """
        if self._netconf is not None and self._netconf.connected:
            self._log.info("Closing Netconf session")
            try:
                yield from self._netconf.close_session()
            except OSError as exp:
                self._log.error("Error closing Netconf session - %s!", exp)

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
                    hostkey_verify=False
                )
                self._log.info("Connected to confd")
                break
            except ncclient.transport.errors.SSHError as e:
                self._log.error("Failed to connect to confd")
                yield from asyncio.sleep(2, loop=self._loop)

    @asyncio.coroutine
    def get(self, url, xml):
        # strip off <data> tags
        root = lxml.etree.fromstring(xml)
        xml = lxml.etree.tostring(root[0]).decode("utf-8") 

        if not self._netconf.connected:
            yield from self.connect()

        try:
            netconf_response = yield from self._netconf.get(('subtree',xml))
        except Exception as e:
            self._log.error("ncclient query failed: %s" % e)
            error_text = str(e)
            error_code = _check_exception(error_text)
            return error_code, error_text

        self._log.debug("netconf get response: %s", netconf_response.xml)
        result = _check_netconf_response(netconf_response, None)

        if result == Result.OK:
            response = netconf_response.data_xml
        else:
            response = netconf_response.xml

        return result, response

    @asyncio.coroutine
    def get_config(self, url, xml):
        # strip off <data> tags
        root = lxml.etree.fromstring(xml)
        xml = lxml.etree.tostring(root[0]).decode("utf-8") 

        if not self._netconf.connected:
            yield from self.connect()

        try:
            netconf_response = yield from self._netconf.get_config(
                source="running",
                filter=('subtree',xml))
        except Exception as e:
            self._log.error("ncclient query failed: %s" % e)
            error_text = str(e)
            error_code = _check_exception(error_text)
            return error_code, error_text

        self._log.debug("netconf get config response: %s", netconf_response.xml)
        result = _check_netconf_response(netconf_response, None)

        if result == Result.OK:
            response = netconf_response.data_xml
        else:
            response = netconf_response.xml

        return result, response

    @asyncio.coroutine
    def delete(self, url, xml):
        if not self._netconf.connected:
            yield from self.connect()

        netconf_response = yield from self._netconf.edit_config(
            target="running",
            config=xml,
            default_operation="none")

        self._log.debug("netconf delete response: %s", netconf_response.xml)

        result = _check_netconf_response(netconf_response, None)

        return result, netconf_response.xml

    @asyncio.coroutine
    def put(self, url, xml):
        if not self._netconf.connected:
            yield from self.connect()

        netconf_response = yield from self._netconf.edit_config(
            target="running",
            config=xml)
        self._log.debug("netconf put response: %s", netconf_response.xml)

        result = _check_netconf_response(netconf_response, None)

        return result, netconf_response.xml
            
    @asyncio.coroutine
    def rpc(self, url, xml):
        if not self._netconf.connected:
            yield from self.connect()

        netconf_response = yield from self._netconf.dispatch(lxml.etree.fromstring(xml))
        self._log.debug("netconf post-rpc response: %s", netconf_response.xml)

        result = _check_netconf_response(netconf_response, None)

        return result, netconf_response.xml

    @asyncio.coroutine
    def post(self, url, xml):
        if not self._netconf.connected:
            yield from self.connect()

        netconf_response = yield from self._netconf.edit_config(
            target="running",
            config=xml)

        result = _check_netconf_response(netconf_response, None)

        return result, netconf_response.xml

    @asyncio.coroutine
    def execute(self, operation, url, xml):
        functor = self._operation_map[operation]
        return functor(url, xml)

    @asyncio.coroutine
    def create_subscription(self, stream, filter, start_time, stop_time):
        """Creates a netconf notification subcription.

        Arguments:
            stream - stream-name for which a subscrption is needed
            filter - a subtree/xpath filter string to filter the notifications
            start-time - Start time for the notification replay
            stop-time  - End time for the notification replay

        Returns: Status result of the operation and the response XML
        """
        if not self._netconf.connected:
            yield from self.connect()

        netconf_response = yield from self._netconf.create_subscription(
                                  stream, filter, start_time, stop_time)
        result = _check_netconf_response(netconf_response, None)
        return result, netconf_response.xml
