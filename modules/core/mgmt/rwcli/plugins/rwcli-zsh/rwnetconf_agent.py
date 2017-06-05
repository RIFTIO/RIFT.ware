"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file   rwnetconf_agent.py
@author Balaji Rajappa (balaji.rajappa@riftio.com)
@date   9/28/2015
@brief  Netconf Agent used by CLI to communicate with Netconf server

"""

import ncclient
import asyncio
import sys
import argparse
import logging
import signal
import os
import struct
import rwuagent_pb2 
from ncclient import asyncio_manager as ncmgr
import lxml.etree
import traceback

MAX_CONNECT_RETRY  = 120
CONNECT_RETRY_TIME = 5

CAP_CANDIDATE = "urn:ietf:params:netconf:capability:candidate:1.0"

logger = logging.getLogger('rwnetconf_agent')

RWCLI_MSG_TYPE_RW_NETCONF           = 0
RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS = 1

# TODO Should be able to get these from gi bindings 
RWTRACE_DISABLE  = 0
RWTRACE_CRITINFO = 1
RWTRACE_EMERG    = 2
RWTRACE_ALERT    = 3
RWTRACE_CRIT     = 4
RWTRACE_ERROR    = 5
RWTRACE_WARN     = 6
RWTRACE_NOTICE   = 7
RWTRACE_INFO     = 8 
RWTRACE_DEBUG    = 9

log_level_mapper = {
    RWTRACE_DISABLE  : logging.NOTSET,
    RWTRACE_CRITINFO : logging.CRITICAL,
    RWTRACE_EMERG    : logging.CRITICAL,
    RWTRACE_ALERT    : logging.CRITICAL,
    RWTRACE_CRIT     : logging.CRITICAL,
    RWTRACE_ERROR    : logging.ERROR,
    RWTRACE_WARN     : logging.WARNING,
    RWTRACE_NOTICE   : logging.INFO,
    RWTRACE_INFO     : logging.INFO,
    RWTRACE_DEBUG    : logging.DEBUG
}

def exception_catcher(func):
    """Decorator to catch all exception in a coroutine that uses asyncio.wait. 
    
    An exception in a asyncio.wait tasks goes uncaught. Use this decorator to
    catch the exception and exit gracefully. Usually used for unknown exception.
    """
    @asyncio.coroutine
    def wrapper(self):
        self._error = None
        try:
            yield from func(self)
        except Exception as err:
            logger.exception('Exception Caught: ')
            self._error = err
    return wrapper

class AgentStats(object):
    """Contains the Netconf Agent related statistics.
    """
    def __init__(self):
        """Constructor that initializes the agent statistics.
        """
        self.connection_attempts = 0
        self.disconnects = 0
        self.rpcs = 0
        self.gets = 0
        self.get_configs = 0
        self.edit_configs = 0
        self.requests = 0

    def __str__(self):
        """String representation of AgentStats."""
        return (
            "    Connection Attempts : {p.connection_attempts}\n"
            "    Disconnects         : {p.disconnects}\n"
            "    Requests            : {p.requests}\n"
            "    RPCs                : {p.rpcs}\n"
            "    Gets                : {p.gets}\n"
            "    Get Configs         : {p.get_configs}\n"
            "    Edit Configs        : {p.edit_configs}\n"   
            ).format(p=self)

class CliChannel(object):
    """Communication channel with the CLI process.

    CLI Controller and the Agent communicates using Pipes. Two pipes are
    currently used - one for sending a message from CLI to Agent and the other
    one for sending message from Agent to CLI. Serialized protobuf messages are
    exchanged between the CLI controller and the Agent.
    """
    def __init__(self, read_fd, write_fd):
        """Initializes the CliChannel

        Arguments:
            read_fd  - Pipe descriptor used for receiving messages from CLI (int)
            write_fd - Pipe descriptor used for sending messages to CLI (int)

        Opens the pipe transport with the CLI
        """
        self.read_fd = os.fdopen(read_fd)
        self.write_fd = os.fdopen(write_fd)

    @asyncio.coroutine
    def setup(self, loop):
        """Sets up the pipe descriptors with the asyncio loop.

        Arguments:
            loop - asyncio loop object
        """
        # Setup the read end of the pipe with a StreamReader
        self.stream_reader = asyncio.StreamReader()
        yield from loop.connect_read_pipe(
                        lambda: asyncio.StreamReaderProtocol(self.stream_reader),
                        self.read_fd)

        # Setup the write end of the pipe
        self.write_transport, _ = yield from loop.connect_write_pipe(
                        asyncio.Protocol,
                        self.write_fd)
        logger.info('Read pipe connected')

    @asyncio.coroutine
    def receive(self):
        """Receive a request message from the CLI.

        Returns message type and the request payload.
        """
        # Read the message length and type
        data = yield from self.stream_reader.read(8)
        if not data:
            logger.info('EOF Received')
            return None

        msglen, msg_type = struct.unpack('II', data)
        logger.debug('Message from CLI Len = {} type = {}'.format(msglen, msg_type))
        
        if msglen:
            msg = yield from self.stream_reader.read(msglen)
        else:
            msg = ''

        if msg_type == RWCLI_MSG_TYPE_RW_NETCONF:
            # Get the protobuf message and decode it
            req = rwuagent_pb2.NetconfReq()
            req.ParseFromString(msg)
        else:
            req = msg

        return msg_type, req

    def respond(self, str_msg, msg_type, req):
        """Respond to the CLI with the encoded message.

        Arguments:
            str_msg - Formatted response (can be XML or a printable string)
            msg_type - Type of the request message
            req - Request payload (NetconfReq protobuf for Netconf message type)
        """
        if msg_type == RWCLI_MSG_TYPE_RW_NETCONF: 
            ncrsp = rwuagent_pb2.NetconfRsp()
            ncrsp.operation = req.operation
            ncrsp.xml_response.xml_blob = str_msg 
            msg = ncrsp.SerializeToString()
        else:
            bytes_msg = bytes(str_msg, "utf-8")
            blen = len(bytes_msg)
            msg = struct.pack("II{}s".format(blen), 0, blen, bytes_msg)

        logger.debug('Serialized message size: %s', len(msg))
        self.write_transport.write(struct.pack('I',len(msg)))
        if msg:
            self.write_transport.write(msg)

class NetconfChannel(object):
    """Communication channel with the Netconf server.

    Uses the ncclient module to communicate with the Netconf server.
    """
    # Protobuf operation to ncclient operation map
    edit_conf_operation = {
        rwuagent_pb2.ec_merge   : 'merge',
        rwuagent_pb2.ec_replace : 'replace',
        rwuagent_pb2.ec_create  : 'create',
        rwuagent_pb2.ec_delete  : 'delete',
        rwuagent_pb2.ec_remove  : 'delete',
    }

    def __init__(self, ncmgr, nc_agent, stats):
        """Initializes the NetconfChannel

        Arguments:
            ncmgr    - ncclient asyncio manager object
            nc_agent - NetconfAgent object
        """
        self.netconf_mgr = ncmgr
        self.nc_agent = nc_agent
        self.stats = stats
        self.session_event = asyncio.Event()
        self.has_candidate = CAP_CANDIDATE in self.netconf_mgr.server_capabilities

        # Set the event listener for getting the Confd disconnect event
        self.session_event_listener = EventListener(nc_agent.loop, self.session_event)
        ncmgr._session.add_listener(self.session_event_listener)
        self.em_task = asyncio.async(self.error_monitor(nc_agent.loop), 
                                     loop=nc_agent.loop)

        # Netconf operations Map
        self.nc_op_map = {
            rwuagent_pb2.nc_default              : self.rpc,
            rwuagent_pb2.nc_get                  : self.get,
            rwuagent_pb2.nc_get_config           : self.get_config,
            rwuagent_pb2.nc_edit_config          : self.edit_config,
            rwuagent_pb2.nc_rpc_exec             : self.rpc,
            rwuagent_pb2.nc_commit               : self.commit,
            rwuagent_pb2.nc_discard_changes      : self.discard_changes,
            rwuagent_pb2.nc_get_candidate_config : self.get_config,
        }

    def close(self):
        """Close the NetconfChannel gracefully.
        """
        logger.info('Closing the Netconf Channel')
        self.em_task.cancel()
        self.netconf_mgr.close()

    @asyncio.coroutine
    def request(self, xml, ncreq):
        """Initiates a Netconf rpc request.

        Arguments:
            xml - Request XML as lxml etree element
            ncreq - decoded NetconfRequest object

        Returns the response xml in string format.
        """
        response_xml = ''
        logger.info('Operation = {}, EC Op = {}'.format(
                ncreq.operation, ncreq.edit_config_operation))
        if lxml.etree.iselement(xml):
            logger.info(' XML = %s', lxml.etree.tostring(xml))

        try:
            response_xml = yield from self.nc_op_map[ncreq.operation](xml, ncreq)
            self.stats.requests += 1
        except ncclient.transport.errors.SessionCloseError as e:
            print('Error: Netconf server disconnected. Trying to reconnect',
                 file=sys.stderr)
            self.netconf_mgr = None
            # Don't have to reconnect, the event listener will take care of it
        except Exception as err:
            logger.warning('NCClient operation failed. {}'.format(
                          traceback.format_exc()))
            print('Error: Netconf operation failed - {}'.format(err),
                 file=sys.stderr)
        return response_xml

    @asyncio.coroutine
    def rpc(self, xml, ncreq):
        """Sends a RPC request to the Netconf server

        Arguments:
            xml   - The XML string to be send in the RPC body
            ncreq - Decoded protobuf request received from CLI

        Returns:
            Returns the response XML.
        """
        # Remove the <data/> element
        xml = xml[0]
        rpc_rsp = yield from self.netconf_mgr.dispatch(xml)
        logger.debug('RPC response xml: %s', rpc_rsp.xml)

        self.stats.rpcs += 1
        return rpc_rsp.xml

    @asyncio.coroutine
    def get(self, xml, ncreq):
        """Sends a GET request to the Netconf server

        Arguments:
            xml   - The XML string to be send in the GET method
            ncreq - Decoded protobuf request received from CLI

        Returns:
            Returns the response XML.
        """
        # Remove the <data/> element
        xml = xml[0]
        get_rsp = yield from self.netconf_mgr.get(('subtree', xml))

        logger.debug('GET rpc response: %s', get_rsp.xml)

        if get_rsp.ok:
            # Don't have to strip the data tag, it will be ignored by the pretty
            # printer 
            root = get_rsp.data_ele
            rsp_xml = lxml.etree.tostring(root) if len(root) else ''
        else:
            # Error
            rsp_xml = get_rsp.xml

        logger.debug('GET response xml: %s', rsp_xml)
        self.stats.gets += 1
        return rsp_xml

    @asyncio.coroutine
    def get_config(self, xml, ncreq):
        """Sends a GET-CONFIG request to the Netconf server

        Arguments:
            xml   - The XML string to be send in the GET-CONFIG method
            ncreq - Decoded protobuf request received from CLI

        Returns:
            Returns the response XML.
        """
        if len(xml):
            # Remove the <data/> element
            xml = xml[0]
            gc_filter = ('subtree', xml)
            logger.debug('GET-CONFIG request: %s', lxml.etree.tostring(xml))
        else:
            gc_filter = None

        if ncreq.operation == rwuagent_pb2.nc_get_candidate_config:
            source = 'candidate'
        else:
            source = 'running'

        get_config_rsp = yield from self.netconf_mgr.get_config(
                                source=source,
                                filter=gc_filter)

        logger.debug('GET-CONFIG rpc response xml: %s', get_config_rsp.xml)
        self.stats.get_configs += 1

        # Remove the <aaa> and <nacm> elements.
        new_xml = lxml.etree.Element('data', 
                    nsmap={None:"http://riftio.com/ns/riftware-1.0/rw-base"})
        if get_config_rsp.ok:
            for element in get_config_rsp.data_ele:
                qual = lxml.etree.QName(element)
                if qual.localname != 'aaa' and qual.localname != 'nacm':
                    new_xml.append(element)
            rsp_xml = lxml.etree.tostring(new_xml)
        else:
            # Extract the error response
            parser = lxml.etree.XMLParser(remove_blank_text=True)
            err_xml = lxml.etree.fromstring(get_config_rsp.xml.encode(),
                                            parser)
            # get the error responses
            rpc_errors = err_xml.xpath('//nc:rpc-error',
                namespaces={'nc':'urn:ietf:params:xml:ns:netconf:base:1.0'})
            self.print_errors(rpc_errors)

            data = err_xml.xpath('//nc:data',
                namespaces={'nc':'urn:ietf:params:xml:ns:netconf:base:1.0'})
            if not data:
                return ''
            for element in data[0]:
                qual = lxml.etree.QName(element)
                if (qual.localname != 'aaa' and qual.localname != 'nacm' and 
                        qual.localname != 'rpc-error'):
                    new_xml.append(element)
            rsp_xml = lxml.etree.tostring(new_xml)

        logger.debug('GET-CONFIG processed response xml: %s', rsp_xml)
        return rsp_xml 
       
    @asyncio.coroutine
    def edit_config(self, xml, ncreq):
        """Sends a EDIT-CONFIG request to the Netconf server

        Arguments:
            xml   - The XML string to be send in the EDIT-CONFIG method
            ncreq - Decoded protobuf request received from CLI

        Returns:
            Returns the response XML.
        """
        # The default-operation if not specified defaults to "merge"
        default_op = None
        ec_op = NetconfChannel.edit_conf_operation[ncreq.edit_config_operation]

        # ATTN: If candidate is to be used check self.has_candidate  
        target = 'running'

        # The operation=delete would have been set by the CLI
        if ec_op == 'delete': default_op = "none"

        # Strip the root and add nc:config node instead
        ec_xml = lxml.etree.Element('config',
                    nsmap={'nc':'urn:ietf:params:xml:ns:netconf:base:1.0'})
        for element in xml:
            ec_xml.append(element)

        ec_rsp = yield from self.netconf_mgr.edit_config(
                target=target,
                config=ec_xml,
                default_operation=default_op)
        logger.debug('EDIT-CONFIG response xml: %s', ec_rsp.xml)
        self.stats.edit_configs += 1
        return ec_rsp.xml

    @asyncio.coroutine
    def commit(self, xml, ncreq):
        """Sends a COMMIT command to the netconf server
         
        Arguments:
            xml   - The XML string to be send in the EDIT-CONFIG method
            ncreq - Decoded protobuf request received from CLI

        Returns:
            Returns the response XML.
        """
        ec_rsp = yield from self.netconf_mgr.commit()
        logger.debug('COMMIT response xml: %s', ec_rsp.xml)
        return ec_rsp.xml

    @asyncio.coroutine
    def discard_changes(self, xml, ncreq):
        """Sends a DISCARD-CHANGES command to the netconf server
         
        Arguments:
            xml   - The XML string to be send in the EDIT-CONFIG method
            ncreq - Decoded protobuf request received from CLI

        Returns:
            Returns the response XML.
        """
        ec_rsp = yield from self.netconf_mgr.discard_changes()
        logger.debug('DISCARD-CHANGES response xml: %s', ec_rsp.xml)
        return ec_rsp.xml

    def print_errors(self, rpc_errors):
        """Print the rpc-errors that occured during get-config

        Used for displaying interleaved errors during get-config operation.

        Arguments:
            rpc_errors - List of rpc-error elements
        """
        if not rpc_errors: return

        print('Errors occurred on get-config:', file=sys.stderr)
        for rpc_error in rpc_errors:
            print(lxml.etree.tostring(rpc_error, 
                pretty_print=True).decode('utf-8'), file=sys.stderr)

    @asyncio.coroutine
    def error_monitor(self, loop):
        while loop.is_running():
            yield from self.session_event.wait()
            self.session_event.clear()
            if isinstance(self.session_event_listener.error,
                 ncclient.transport.errors.SessionCloseError):
                # Session closed
                logger.warning('Session close indication received. Reconnect')
                self.stats.disconnects += 1
                self.nc_agent.reconnect()
                self.session_event_listener.error = None


class NetconfConnector(object):
    """Continuoulsy retries to establish Netconf connection and when connected
    creats a NetconfChannel.
    """
    def __init__(self, nc_agent, stats):
        self.nc_agent  = nc_agent
        self.stats     = stats
        self.connected = None
        self.auth_failed = False
    
    @asyncio.coroutine
    def connect(self, loop):
        """Connect to the Netconf server.
        
        Returns only when the connection is established. Otherwise retries
        forever. Returns with the NetconfChannel object.

        This is a coroutine.
        """
        # Setup the netconf, try connecting till MAX_CONFD_RETRY is reached 
        retry_count = 0
        netconf_mgr = None
        self.connected = asyncio.Future(loop=loop)
        while netconf_mgr is None and loop.is_running():
            try:
                logger.info('Trying to connect to Netconf server - %s:%s',
                            self.nc_agent.host, self.nc_agent.port)
                self.stats.connection_attempts += 1
                self.auth_failed = False
                netconf_mgr = yield from ncmgr.asyncio_connect(
                        loop=loop,
                        host=self.nc_agent.host,
                        port=self.nc_agent.port,
                        username=self.nc_agent.username,
                        password=self.nc_agent.passwd,
                        allow_agent=False,
                        look_for_keys=False,
                        hostkey_verify=False)
                netconf_mgr._timeout = 60
                logger.info('CLI Connected to confd using Netconf')
                nc_channel = NetconfChannel(netconf_mgr, self.nc_agent, self.stats)
                self.connected.set_result(nc_channel)
                return
            except ncclient.transport.errors.SSHError as e:
                retry_count += 1
                logger.warning('CLI failed to connect to confd. Retrying')
                yield from asyncio.sleep(CONNECT_RETRY_TIME, loop=loop)
            except ncclient.transport.errors.AuthenticationError as e:
                print("\nERROR: Netconf server Authentication Failed")
                self.auth_failed = True
                break


class NetconfAgent(object):
    """Abstraction that is used for communicating with the Netconf server.

    This agent receives request from the CLI controller which is then converted
    to Netconf compliant form. The Netconf message is then sent to the Netconf
    server using the ncclient module.

    CliChannel is used for communicating with the CLI and the NetconfChannel is
    used for communicating with the Netconf server. NetconfConnector is used to
    connect with the Netconf server.
    """
    def __init__(self, loop, read_fd, write_fd, host, port, username, passwd):
        """Creates a NetconfAgent object

        Arguments:
            loop     - Asyncio loop object used for IO operations
            read_fd  - Read end of the pipe on which CLI sends a message to the
                       Agent
            write_fd - Write end of the pipe using which the agent sends a
                       message to CLI 
            host     - Netconf server Host IP address/hostname
            port     - Netconf server listen port
            username - Username to connect to the Netconf server
            passwd   - Password that is used to connect to Netconf server
        """
        self.loop = loop
        self.host = host
        self.port = port
        self.username = username
        self.passwd = passwd
        self.stats = AgentStats()
        self.cli_channel  = CliChannel(read_fd, write_fd)
        self.nc_connector = NetconfConnector(self, self.stats)

    @asyncio.coroutine
    def run(self):
        """Agent's main run loop.
        """
        # Setup the CliChannel with asyncio
        yield from self.cli_channel.setup(self.loop)

        # Schedule the Netconf connection. The connector retries the connection
        # asynchronously until connected
        asyncio.async(self.nc_connector.connect(self.loop), loop=self.loop)

        while self.loop.is_running():
            msg_type, req = yield from self.cli_channel.receive()
            if req is None:
                # EOF received
                self.stop()
                break

            if msg_type != RWCLI_MSG_TYPE_RW_NETCONF:
                self.handle_internal_msg(msg_type, req)
                continue

            if not self.netconf_connected:
                if self.auth_failed:
                    print('ERROR: Not connected to netconf server - Authentication Failed')
                else:
                    print('ERROR: Not connected to netconf server')
                self.cli_channel.respond('', msg_type, req)
                continue

            xml = self.prepare_xml(req.xml_blob.xml_blob)
            response_xml = yield from self.netconf_channel.request(xml, req)
            self.cli_channel.respond(response_xml, msg_type, req)

        if self.netconf_connected:
            self.netconf_channel.close()

    def handle_internal_msg(self, msg_type, req):
        """Handles the internal requests from rwcli controller.

        Arguments:
            msg_type - Internal message type
            req      - Internal request payload
        """
        if msg_type == RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS:
            if self.netconf_connected:
                conn_status = "Connected"
            elif self.auth_failed:
                conn_status = "Authentication Failed"
            else:
                conn_status = "Not Connected"

            ret_str = (
                "NETCONF Transport:\n"
                "  Connection Status: {}\n"
                "  Statistics:\n{}"
                ).format(conn_status, self.stats)
            self.cli_channel.respond(ret_str, msg_type, req)
        else:
            logger.error("Unknown Internal Message: %d", msg_type)
            self.cli_channel.respond('', msg_type, req)

    def reconnect(self):
        """Schedule the connector task, which will retry until connected
        """
        self.nc_connector.connected = None
        asyncio.async(self.nc_connector.connect(self.loop), loop=self.loop)

    def stop(self):
        """Stops the Netconf agent.
        """
        self.loop.stop()

    def prepare_xml(self, xml):
        """Format the XML created by CLI for use with lxml.
        """
        xml = xml.replace('''<?xml version= "1.0" encoding="UTF-8"?>''', '')
        xml = xml.replace('''<?xml version= "1.0"?>''', '')
        xml = xml.strip()
        if xml:
            xml = lxml.etree.fromstring(xml)
        return xml

    @property
    def netconf_connected(self):
        """Returns True if the netconf channel exists otherwise false
        """
        if (self.nc_connector.connected is not None and 
            self.nc_connector.connected.done()):
            return True
        return False

    @property
    def netconf_channel(self):
        """Returns the current netconf channel object.
        """
        return self.nc_connector.connected.result()

    @property
    def auth_failed(self):
        """Returns True if Authentication has failed with the Netconf server
        """
        return self.nc_connector.auth_failed


# NCClient only register for events only when an RPC operation is in progress,
# when there are no events, still the SSH session might be closed by the Netconf
# server. To capture such events this event handler is required.
# ATTN: Ideally this should be part of the NCClient Manager or AscynioManager
class EventListener(ncclient.transport.session.SessionListener):
    """Listener for SSH session events.

    Handles the session closer by the server.
    """
    def __init__(self, loop, event):
        """Constructor for the EventListener

        Arguments:
            loop - asyncio loop
            event - the event that will be notified to the asyncio loop thread
        """
        self.loop = loop
        self.event = event
        self.error = None

    def callback(self, root, raw):
        """Callback that will be invoked when a message is received by the
        session.

        Note: This will occur in SSHSession thread
        """
        # Not interested, will be handled by NCClient RPC
        pass

    def errback(self, err):
        """Callback that will be invoked when an error is encountered by the
        session.

        When an error is received, the NetconfAgent run loop will be notified.

        Arguments:
            err - Error that is reported by the session

        Note: This will be called from SShSession thread
        """
        logger.warning('Received error from Netconf Session - %s', err)
        self.error = err
        self.loop.call_soon_threadsafe(self.event.set)

def ignore():
    """Used to ignore the SIGINT signal.
    """
    pass

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--read-channel-fd', type=int)
    parser.add_argument('--write-channel-fd', type=int)
    parser.add_argument('--netconf-host', default='127.0.0.1')
    parser.add_argument('--netconf-port', default='2022')
    parser.add_argument('--username', default='admin')
    parser.add_argument('--passwd', default='admin')
    parser.add_argument('--trace-level', type=int, default=RWTRACE_ERROR)

    args = parser.parse_args()
    logger.setLevel(log_level_mapper[args.trace_level])
    logger.info('Args = {}'.format(args))

    loop = asyncio.get_event_loop()

    agent = NetconfAgent(loop, args.read_channel_fd, args.write_channel_fd,
                         args.netconf_host, args.netconf_port, 
                         args.username, args.passwd)
    asyncio.async(agent.run())
    loop.add_signal_handler(signal.SIGINT, ignore)
    loop.add_signal_handler(signal.SIGTERM, agent.stop)

    try:
        loop.run_forever()
    finally:
        loop.close()
    logger.info('Exiting...')

if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s:%(levelname)s:%(name)s:%(message)s',
            level=logging.WARNING)
    main()


# vim: ts=4 sw=4 et
