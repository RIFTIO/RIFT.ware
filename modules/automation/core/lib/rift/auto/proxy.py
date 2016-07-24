"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file proxy.py
@author Joshua Downer (joshua.downer@riftio.com)
@date 10/27/2014
@brief proxy classes

The Proxy class defines a minimal interface that is used to communicate with the
RIFT.ware system. The Proxy interface is implemented by a set of subclasses that
communicate with the system via a set of different mechanisms. Management
classes define domain-specific commands and queries, and use the Proxy interface
to communicate with the RIFT.ware system. The Proxy interface encapsulates the
communication mechanism so that the management classes are independent of the
actual mechanism used.

    Proxy
    |-- uAgentProxy
    |-- FileProxy
    \-- NetconfProxy

"""

import logging
import os
import re
import time
import functools
import gi

import lxml.etree as etree
import ncclient.manager
gi.require_version('RwYang', '1.0')
from gi.repository import RwYang

logger = logging.getLogger(__name__)


class ProxyError(Exception):
    pass

class ProxyConnectionError(ProxyError):
    pass

class ProxyRequestError(ProxyError):
    pass

class ProxyRequestTimeout(ProxyRequestError):
    pass


class Proxy(object):
    """
    This class defines the interface that proxy classes are required to provided
    to be used by management classes. The interface is minimal and is a subset of the
    Netconf protocol.
    """
    OPERATION_TIMEOUT_SECS = 90

    def get(self, filter):
        """Queries the "running" configuration and operational state of the system.

        Passes a query in the form of XML to a RIFT.ware system.

        Arguments:
            filter - a filter that specifies what to get

        Returns:
            A string containing an XML response

        """
        raise NotImplementedError()

    def connect(self):
        """ Create the connection to the management interface.

        A derived class should only implement if the class requires a connection
        to be made to a backend that can fail.  This is to prevent the Proxy
        constructor from doing too much.
        """
        pass

    def close(self):
        """ Close the connection to the management interface.

        A derived class should implement if it is neccessary to disconnect from
        the backend cleanly (usually when the Proxy implements connect.)
        """
        pass

    def get_config(self, xml):
        """Queries the "running" configuration of the system.

        Passes a query in the form of XML to a RIFT.ware system.

        Arguments:
            xml - a string containing an XML document

        Returns:
            A string containing an XML response

        """
        raise NotImplementedError()

    def merge_config(self, xml):
        """ Load the xml and merge into the candidate configuration.

        Passes a merge config command in the form of XML to a RIFT.ware system.
        This operation loads and merges the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document

        Returns:
            A string containing an XML response

        """
        raise NotImplementedError()

    def create_config(self, xml):
        """  the xml into the candidate configuration.

        Passes a edit config command in the form of XML to a RIFT.ware system.
        This operation loads the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document
            default_operation - a string representing the intended edit
                    config semantics (merge, replace, create, delete, remove)

        Returns:
            A string containing an XML response

        """
        raise NotImplementedError()

    def delete_config(self, xml):
        """ Load and delete the xml into the candidate configuration.

        Passes a delete config command in the form of XML to a RIFT.ware system.
        This operation deletes the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document

        Returns:
            an string containing an XML response

        """
        raise NotImplementedError()

    def remove_config(self, xml):
        """ Load and remove the xml into the candidate configuration.

        Passes a remove config command in the form of XML to a riftware system.
        This operation deletes the specified XML configuration to the "candidate" datastore.

        Remove will not throw an error if the config does not exist.

        Arguments:
            xml - a string containing an XML document

        Returns:
            an string containing an XML response

        """
        raise NotImplementedError()


    def replace_config(self, xml):
        """ Load and replace the xml into the candidate configuration.

        Passes a replace config command in the form of XML to a RIFT.ware system.
        This operation replaces the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document

        Returns:
            an string containing an XML response

        """
        raise NotImplementedError()

    def rpc(self, xml):
        """ Send a RPC request to the server

        Passes an rpc command in the form of XML to a RIFT.ware system.
        The XML is expected to contain both the rpc method name and parameters
        neccessary to invoke the said method.

        Arguments:
            xml - a string containing an XML document

        Returns:
            A string containing the XML response

        """
        raise NotImplementedError()

    def get_GI_compatible_xml(self, xml, rpc_name=None):
        """ Returns a GI compatible xml to be parsed by from_xml().

        protobuf GI's from_xml() method expects a response xml in a certain way
        for constructing a protobuf object. This method takes the xml and
        transforms it, if necessary, to an xml suitable for from_xml().

        Arguments:
            xml      - the xml to be made GI comptabile
            rpc_name - rpc name, such as 'trafsim-start', 'trafsim-stop'

        Returns:
            A string containing GI compatible xml

        """
        raise NotImplementedError()


def ncclient_exception_wrapper(func):
    @functools.wraps(func)
    def func_wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except ncclient.operations.errors.TimeoutExpiredError as e:
            raise ProxyRequestTimeout(e)
        except ncclient.NCClientError as e:
            raise ProxyRequestError(e)

    return func_wrapper


class NetconfProxy(Proxy):
    """
    The NetconfProxy class communicates with a netconf server
    (e.g. confd) via netconf.
    """

    VCS_RUN_SCHEDULER_TIME_SLICE = 0.1

    DEFAULT_CONFD_NETCONF_PORT = 2022
    DEFAULT_CONFD_USER_NAME = 'admin'
    DEFAULT_CONFD_USER_PW = 'admin'

    DEFAULT_CONFD_CONNECTION_TIMEOUT = 300
    XML_DECLARATION = '<?xml version="1.0" encoding="UTF-8"?>\n'

    def __init__(self, netconf_ip_addr='127.0.0.1', netconf_port=DEFAULT_CONFD_NETCONF_PORT,
                 username=DEFAULT_CONFD_USER_NAME, password=DEFAULT_CONFD_USER_PW):
        """Create a NetconfProxy object.

        Arguments:
            netconf_ip_addr - netconf server's ip address
            netconf_port    - netconf server's port
            username        - netconf server username
            password        - netconf server password
        """
        self.model = RwYang.Model.create_libncx()
        self.host = netconf_ip_addr
        self.port = netconf_port
        self.username = username
        self.password = password

        self._nc_mgr = None

    def connect(self, timeout_secs=DEFAULT_CONFD_CONNECTION_TIMEOUT):
        logging.info("Connecting to confd (%s) SSH port (%s)", self.host, self.port)

        start_time = time.time()
        while (time.time() - start_time) < timeout_secs:
            try:
                self._nc_mgr = ncclient.manager.connect(
                    host=self.host,
                    port=self.port,
                    username=self.username,
                    password=self.password,
                    # Setting allow_agent and look_for_keys to false will skip public key
                    # authentication, and use password authentication.
                    allow_agent=False,
                    look_for_keys=False,
                    hostkey_verify=False)

                logging.info("Successfully connected to confd (%s) SSH port (%s)", self.host, self.port)
                self._nc_mgr.timeout = self.OPERATION_TIMEOUT_SECS
                return

            except ncclient.NCClientError as e:
                logger.debug("Could not connect to (%s) confd ssh port (%s): %s",
                        self.host, self.port, str(e))

            time.sleep(5)

        raise ProxyConnectionError("Could not connect to Confd ({}) ssh port ({}): within the timeout {} sec.".format(
                self.host, self.port, timeout_secs))

    def close(self):
        """ Close the connection to the netconf management interface.
        """

        if self._nc_mgr is not None:
            try:
                logging.info("Closing confd SSH session to %s", self.host)
                if self._nc_mgr.connected:
                    self._nc_mgr.close_session()
                else:
                    logging.warn("confd SSH session to %s already closed.", self.host)
            except Exception as e:
                logger.exception("Failed to close netconf client session: %s", str(e))

            self._nc_mgr = None

    @property
    def ncclient_manager(self):
        """ The handle to the ncclient Manager instance.

        This property will attempt reconnect if the Manager instance is not connected
        for any reason.
        """
        if self._nc_mgr is None:
            raise ProxyConnectionError("Not connected to confd")

        # During initialization it is possible for the netconf connection to
        # disconnect randomly.  If the Proxy client has caught the
        # ProxyRequestError and continues execution, lets attempt to reconnect
        # automatically.
        if not self._nc_mgr.connected:
            self.connect(timeout_secs=10)

        return self._nc_mgr

    def load_schema(self, schema):
        """Add a schema to the proxy's model instance

        Arguments:
          schema - yang schema to load
        """
        self.model.load_schema_ypbc(schema)

    @ncclient_exception_wrapper
    def get_from_xml(self, filter_xml):
        """Queries the operational state of the system.

        Passes a query in the form of XML.

        Arguments:
            filter_xml - an xml element that specifies what to get

        Returns:
            a string containing an XML response

        """
        logger.debug("Sending get XML request: (len: %s)", len(filter_xml))
        get_reply = self.ncclient_manager.get(('subtree', filter_xml))
        logger.debug("Received get response: (len: %s)", len(get_reply.data_xml))

        response_xml = get_reply.data_xml.decode()

        return response_xml

    @ncclient_exception_wrapper
    def get_from_xpath(self, filter_xpath):
        """Queries the operational state of the system.

        Passes a query in the form of xpath.

        Arguments:
            filter_xpath - an xpath that specifies what to get

        Returns:
            a string containing an XML response

        """
        logger.debug("Sending get xpath request: %s", filter_xpath)
        get_reply = self.ncclient_manager.get(('xpath', filter_xpath))
        logger.debug("Received get response: (len: %s)", len(get_reply.data_xml))

        response_xml = get_reply.data_xml.decode()

        return response_xml

    @ncclient_exception_wrapper
    def get_from_gi(self, obj):
        """Queries the operational state of the system.

        Helper method to hide the complexity of request generation / response handling

        Arguments:
            obj - A GI object representing a part of the yang model

        Returns:
            A new GI object representing the state of the system
        """
        response_xml = self.get_from_xml(obj.to_xml_v2(self.model))
        response_obj = obj.new()
        response_obj.from_xml_v2(self.model, response_xml)
        return response_obj

    @ncclient_exception_wrapper
    def get_config_from_xml(self, source="running", filter_xml=None):
        """Queries the "running" configuration of the system.

        Passes a query in the form of xpath to a RIFT.ware system.

        Arguments:
            source - name of the configuration datastore being queried
            filter_xml - a string containing an xml specification

        Returns:
            a string containing an XML response
        """
        logger.debug("Sending get config xml request: %s", filter_xml)
        get_reply = self.ncclient_manager.get_config(
                source=source,
                filter=('subtree', filter_xml) if filter_xml is not None else None)
        logger.debug("Received get config response: (len: %s)", len(get_reply.data_xml))
        response_xml = get_reply.data_xml.decode()

        return response_xml

    @ncclient_exception_wrapper
    def get_config(self, source="running", filter_xpath=None):
        """Queries the "running" configuration of the system.

        Passes a query in the form of xpath to a RIFT.ware system.

        Arguments:
            source - name of the configuration datastore being queried
            filter_xpath - a string containing an xpath specification

        Returns:
            an string containing an XML response

        """
        logger.debug("Sending get config xpath request: %s", filter_xpath)
        get_reply = self.ncclient_manager.get_config(
                source=source,
                filter=('xpath', filter_xpath) if filter_xpath is not None else None)

        logger.debug("Received get config response: (len: %s)", len(get_reply.data_xml))
        response_xml = get_reply.data_xml.decode()

        return response_xml

    def get_config_from_gi(self, obj, source="running"):
        """Queries the "running" configuration of the system.

        Helper method to hide the complexity of request generation / response handling

        Arguments:
            obj - A GI object representing a part of the yang model
            source - name of the configuration datastore being queried

        Returns:
            A new GI object representing the state of the system
        """
        response_xml = self.get_config_from_xml(
                source=source,
                filter_xml=obj.to_xml_v2(self.model))
        response_obj = obj.new()
        response_obj.from_xml_v2(self.model, response_xml)
        return response_obj

    @ncclient_exception_wrapper
    def _edit_config(self, xml, operation, target="candidate", element=None):
        """ Wrapper for all edit_config sub-operations (create, delete, remove, merge, replace)

        Arguments:
            xml - a string containing an XML document
            operation - A edit_config operation that corresponds to ncclient
                        edit_config() ncclient operations (create, delete, merge, replace)
            target    - The target netconf configuration datastore to apply the operation to.
        """

        if target == 'candidate':
            target = 'running'

        assert operation in ["create", "merge", "replace", "delete", "remove"]

        if element is not None:
            xml = xml.replace(element, '{} xc:operation="{}"'.format(element, operation), 1)
        else:
            xml = xml.replace('>', ' xc:operation="{}">'.format(operation), 1)

        xml = '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">{}</config>'.format(xml)

        logger.debug("Sending edit config XML request: (%s)", xml)

        """
        RIFT-10435 quick fix
        """
        retry_count = 30
        retry = True
        response = None

        while retry_count > 0 and retry:
            try:
                response = self.ncclient_manager.edit_config(
                        target=target,
                        config=xml,
                        )
                retry = False
            except ncclient.operations.rpc.RPCError as e:
                logger.debug("Caught exception: {}".format(str(e)))
                retry_count = retry_count - 1
                logger.debug("Retrying again for {} time".format(30 - retry_count))
                time.sleep(2)


        if response is None:
            raise ProxyRequestError("Operation {} failed after retrying multiple times".format(operation))

        logger.debug("Received edit config response: (len: %s)", len(str(response)))

        response_xml = response.xml

        if 'rpc-error>' in response_xml:
            msg = 'Request EDIT CONFIG %s %s - Response contains errors\n%s' % (
                    operation,
                    target,
                    response_xml)
            raise ProxyRequestError(msg)

        if target == 'candidate':
            commit_response = self.ncclient_manager.commit()

            if 'rpc-error>' in commit_response.xml:
                msg = 'Request COMMIT EDIT CONFIG %s %s - Response contains error\n%s' % (
                        operation,
                        target,
                        commit_response.xml)
                raise ProxyRequestError(msg)

        return response_xml

    def create_config(self, xml, target="candidate", element=None):
        """ Load and create the xml into the candidate configuration.

        Passes a create config command in the form of XML to a RIFT.ware system.
        This operation creates specified XML configuration to the "candidate" datastore.

        Arguments:
            xml    - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns: an string containing an XML response

        """
        return self._edit_config(xml, "create", element=element)

    def delete_config(self, xml, target="candidate", element=None):
        """ Load and delete the xml into the candidate configuration.

        Passes a delete config command in the form of XML to a RIFT.ware system.
        This operation deletes the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            an string containing an XML response

        """
        return self._edit_config(xml, "delete", target, element=element)

    def remove_config(self, xml, target="candidate"):
        """ Load and remove the xml into the candidate configuration.

        Passes a remove config command in the form of XML to a riftware system.
        This operation deletes the specified XML configuration to the "candidate" datastore.

        Remove silently ignores missing config.

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            an string containing an XML response

        """
        return self._edit_config(xml, "remove", target)

    def merge_config(self, xml, target="candidate"):
        """ Load and merge the xml into the candidate configuration.

        Passes a merge config command in the form of XML to a RIFT.ware system.
        This operation merges the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            an string containing an XML response

        """
        return self._edit_config(xml, "merge", target)

    def replace_config(self, xml, target="candidate"):
        """ Load and replace the xml into the candidate configuration.

        Passes a replace config command in the form of XML to a RIFT.ware system.
        This operation replaces the specified XML configuration to the "candidate" datastore.

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            an string containing an XML response

        """
        return self._edit_config(xml, "replace")

    @ncclient_exception_wrapper
    def rpc(self, xml):
        """ Send a RPC request to the server

        Passes an rpc command in the form of XML to a RIFT.ware system.
        The XML is expected to contain both the rpc method name and parameters
        neccessary to invoke the said method.

        Arguments:
            xml - a string containing an XML document

        Returns:
            A string containing the XML response

        """
        logger.debug("Sending RPC XML request: %s", xml)
        rpc_response = self.ncclient_manager.dispatch(
                etree.fromstring(xml)
                )

        logger.debug("Received RPC response: %s", rpc_response.xml)
        response_xml = rpc_response.xml

        return response_xml

    @ncclient_exception_wrapper
    def rpc_from_gi(self, input_obj, output_obj=None):
        """ Send a RPC request to the server

        Helper method to hide the complexity of request generation / response handling

        Arguments:
            input_obj - A GI object representing the input to an RPC call
            output_obj - An empty GI object representing the output of the RPC call

        Returns:
            The output GI object populated by the called RPC
        """
        request_xml = input_obj.to_xml_v2(self.model)

        response_xml = self.rpc(request_xml)
        if output_obj:
            schema = output_obj.schema()
            desc = schema.retrieve_descriptor()

            output_obj.from_xml_v2(
                    self.model,
                    self.get_GI_compatible_xml(
                        xml=response_xml,
                        rpc_name=desc.xml_element_name(),
                        prefix=desc.xml_prefix(),
                        namespace=desc.xml_ns()))

        return output_obj

    def get_GI_compatible_xml(self, xml, rpc_name=None, prefix=None, namespace=None):
        """ Returns a GI compatible xml to be parsed by from_xml().

        The response xml from NetconfProxy, which receives its response
        from Confd, is not what from_xml() expects, so tranformation to a
        from_xml() compatible format is needed.

        Below is an example rpc response xml received from confd:

          <?xml version="1.0" encoding="UTF-8"?>
          <rpc-reply xmlns="urn:ietf:params:xml:ns:netconf:base:1.0"
           message-id="urn:uuid:9bcf1a20-e7c3-11e4-9594-fa163e955c0e"
           xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
             <script xmlns='http://riftio.com/ns/riftware-1.0/rwappmgr'>
               <execution-id>1</execution-id>
             </script>
          </rpc-reply>

        It needs to be transformed to the following for from_xml()

          <trafsim-start
           xmlns:rwappmgr="http://riftio.com/ns/riftware-1.0/rwappmgr">
             <rwappmgr:script>
               <rwappmgr:execution-id>1</rwappmgr:execution-id>
             </rwappmgr:script>
          </trafsim-start>

        The transformation is as follows:
          1. Take the the response body inside rpc-reply, in this case,
             <script>...</script>.
          2. Wrap the response body with rpc-name tag, in this case,
             <trafsim-start ...>...</trafsim-start>

        Arguments:
            xml      - the xml to be made GI comptabile
            rpc_name - rpc name, such as 'trafsim-start', 'trafsim-stop'

        Returns:
            A string containing GI compatible xml

        """
        # Remove xml declaration in order to be parsed by etree.fromstring().
        xml.replace(NetconfProxy.XML_DECLARATION, '')
        xml_root = etree.fromstring(xml.encode('ascii'))

        if prefix is None or namespace is None:
            # Strip prefix from rpc_name if the prefix and namespace were not
            # defined by the caller. If the prefix is used without a matching
            # namespace declaration the result will be invalid xml.
            rpc_name = re.sub(r'^.*:', '', rpc_name)
            rpc_response_element = etree.Element(rpc_name)
        else:
            rpc_response_element = etree.Element(
                rpc_name,
                nsmap={prefix:namespace}
            )

        for response_element in list(xml_root):
            rpc_response_element.append(response_element)

        return etree.tostring(rpc_response_element).decode()


    @ncclient_exception_wrapper
    def request_xml(self, filter_xml):
        '''Asynchronous variant of get_from_xml

        Arguments:
            filter_xml - an xml element that specifies what to get

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return ncclient.operations.Get(
                self._nc_mgr._session,
                self._nc_mgr._device_handler,
                async=True).request(('subtree', filter_xml))

    @ncclient_exception_wrapper
    def request_xpath(self, filter_xpath):
        '''Asynchronous variant of get_from_xpath

        Arguments:
            filter_xpath - an xpath string that specifies what to get

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return ncclient.operations.Get(
                self._nc_mgr._session,
                self._nc_mgr._device_handler,
                async=True).request(('xpath', filter_xpath))

    @ncclient_exception_wrapper
    def request_config_xml(self, source="running", filter_xml=None):
        '''Asynchronous variant of get_config_from_xml

        Arguments:
            source - name of the configuration datastore being queried
            filter_xml - an xml element that specifies what to get

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return ncclient.operations.GetConfig(
                self._nc_mgr._session,
                self._nc_mgr._device_handler,
                async=True).request(
                    source=source,
                    filter=('subtree', filter_xml) if filter_xml is not None else None)

    @ncclient_exception_wrapper
    def request_config_xpath(self, source="running", filter_xpath=None):
        '''Asynchronous variant of get_config

        Arguments:
            source - name of the configuration datastore being queried
            filter_xpath - an xpath string that specifies what to get

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return ncclient.operations.GetConfig(
                self._nc_mgr._session,
                self._nc_mgr._device_handler,
                async=True).request(
                    source=source,
                    filter=('xpath', filter_xpath) if filter_xpath is not None else None)

    @ncclient_exception_wrapper
    def request_rpc_xml(self, xml):
        '''Asynchronous variant of rpc

        Arguments:
            xml - an xml element that specifies what rpc to invoke

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return ncclient.operations.Dispatch(
                self._nc_mgr._session,
                self._nc_mgr._device_handler,
                async=True).request(etree.fromstring(xml))

    @ncclient_exception_wrapper
    def request_gi(self, obj):
        '''Asynchronous variant of get_from_gi

        Arguments:
            obj - a GI object that specifies what to get

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return self.request_xml(obj.to_xml_v2(self.model))

    @ncclient_exception_wrapper
    def request_config_gi(self, obj, source="running"):
        '''Asynchronous variant of get_config_from_gi

        Arguments:
            obj - a GI object that specifies what to get
            source - name of the configuration datastore being queried

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return self.request_config_xml(
                source=source,
                filter_xml=obj.to_xml_v2(self.model))

    @ncclient_exception_wrapper
    def request_rpc_gi(self, obj):
        '''Asynchronous variant of rpc_from_gi

        Arguments:
            obj - a GI object that specifies what rpc to invoke

        Returns:
            A request object from which a reply can be gotten in the future
        '''
        return self.request_rpc_xml(obj.to_xml_v2(self.model))

    @ncclient_exception_wrapper
    def _request_edit_config(self, xml, operation, target="running"):
        """ Asynchronous variant of _edit_config

        Arguments:
            xml - a string containing an XML document
            operation - A edit_config operation that corresponds to ncclient
                        edit_config() ncclient operations (create, delete, merge, replace)
            target    - The target netconf configuration datastore to apply the operation to.
        """

        assert operation in ["create", "merge", "replace", "delete"]

        xml = '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">{}</config>'.format(xml)

        logger.debug("Sending edit config XML request: (len: %s)", len(xml))
        request = ncclient.operations.EditConfig(
                self._nc_mgr._session,
                self._nc_mgr._device_handler,
                async=True).request(
                    target=target,
                    config=xml,
                    default_operation=operation)

        return request

    def request_create_config(self, xml, target="running"):
        """ Asynchronous variant of create_config

        Arguments:
            xml    - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            A request object from which a reply can be gotten in the future
        """
        return self._request_edit_config(xml, "create")

    def request_delete_config(self, xml, target="running"):
        """ Asynchronous variant of delete_config

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            A request object from which a reply can be gotten in the future
        """
        return self._request_edit_config(xml, "delete", target)

    def request_merge_config(self, xml, target="running"):
        """ Asynchronous variant of merge_config

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            A request object from which a reply can be gotten in the future
        """
        return self._request_edit_config(xml, "merge", target)

    def request_replace_config(self, xml, target="running"):
        """ Asynchronous variant of replace_config

        Arguments:
            xml - a string containing an XML document
            target - The target netconf configuration datastore to apply the operation to.

        Returns:
            A request object from which a reply can be gotten in the future
        """
        return self._request_edit_config(xml, "replace")

    @ncclient_exception_wrapper
    def get_reply_xml(self, request):
        '''Retrieve an xml reply from an asynchronous request

        Arguments:
            request - a request object generated by a request API call

        Returns:
            A string containing an XML response
        '''
        request.event.wait()
        if hasattr(request.reply, 'xml'):
          response_xml = request.reply.xml
        else:
          response_xml = request.reply.data_xml.decode()
        return response_xml

    @ncclient_exception_wrapper
    def get_reply_gi(self, request, response_obj):
        '''Retrieve a GI object reply from an asynchronous request

        Arguments:
            request - a request object generated by a request API call
            response_obj - an empty GI object to hold the response

        Returns:
            A GI object containing the response
        '''
        response_obj.from_xml_v2(self.model, self.get_reply_xml(request))
        return response_obj


class FileProxy(Proxy):
    """
    The FileProxy class writes the xml configuration to a file.
    """

    def __init__(self, file_path):
        """Create a FileProxy object.

        Arguments:
            file_path - the xml file path
        """
        self._file_path = file_path
        self._xml_list = []

    def merge_config(self, xml):
        """Sets the state of the system.

        Passes a command in the form of XML to a RIFT.ware system. Calling this
        function implies that the caller directly defines the state to take
        effect in the system.

        Append the provied xml to the xml list to be written to self._file_path
        when write() is called.  The provided xml must be a valid xml as generated
        by the Riftware proto-gi tool chain.

        Arguments:
            xml - a string containing an XML document

        Returns:
            None
        """
        self._xml_list.append(xml)

    def rpc(self, xml):
        """ Send a RPC request to the server

        Passes an rpc command in the form of XML to a RIFT.ware system.
        The XML is expected to contain both the rpc method name and parameters
        neccessary to invoke the said method.

        Arguments:
            xml - a string containing an XML document

        Returns:
            A string containing the XML response
            Note: Currently, this method does nothing, so return an OK rpc-reply
            XML to indicate success.

        """
        # Ignore rpc xml when the rpc method is called.
        return r'<rpc-reply><ok/></rpc-reply>'

    def write(self):
        """Write all the xmls in the xml list to the specified file.

        Before writing the xmls to the file, they are wrapped inside <root></root>
        to make a valid xml document.

        Returns:
            None

        Raises:
            OSError
        """
        self._xml_list.insert(0, '<root>')
        self._xml_list.append('</root>')
        xml_string = ''.join(self._xml_list)
        xml = etree.fromstring(xml_string)
        pretty_xml_string = etree.tostring(xml, pretty_print=True)
        file_descriptor = os.open(self._file_path, os.O_WRONLY | os.O_CREAT)
        os.write(file_descriptor, pretty_xml_string)
