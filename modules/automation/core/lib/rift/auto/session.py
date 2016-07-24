
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import functools
import logging
import lxml.etree as etree
import os
import re
import requests
import time
import ncclient.manager

import xml.dom.minidom as xml_minidom

# from rift.auto.xml_utils import pretty_xml
def pretty_xml(xml_str):
    """Creates a more human readable XML string by using
    new lines and indentation where appropriate.

    Arguments:
        xml_str - An ugly, but valid XML string.
    """
    try:
        return xml_minidom.parseString(xml_str).toprettyxml('  ')
    except Exception:
        return xml_str


from rift.rwlib.util import certs
import rift.gi_utils

from gi import require_version
require_version('RwKeyspec', '1.0')
require_version('RwYang', '1.0')
from gi.repository import (
    RwKeyspec,
    RwYang,
)

logger = logging.getLogger(__name__)

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

def requests_exception_wrapper(func):
    @functools.wraps(func)
    def func_wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except requests.exceptions.ConnectionError as e:
            raise ProxyConnectionError(e)
        except requests.exceptions.ConnectTimeout as e:
            raise ProxyRequestTimeout(e)
        except requests.exceptions.RequestException as e:
            raise ProxyRequestError(e)
        except requests.exceptions.HTTPError as e:
            raise ProxyHttpError(e)

    return func_wrapper

class ProxyError(Exception):
    pass

class ProxyConnectionError(ProxyError):
    pass

class ProxyRequestError(ProxyError):
    pass

class ProxyRequestTimeout(ProxyRequestError):
    pass

class ProxyHttpError (ProxyRequestError):
    pass

class ProxyExpectTimeoutError(ProxyError):
    '''Exception class representing an expectation not being met
    within a supplied timeout
    '''
    pass

class ProxyExpectGIMismatchError(ProxyError):
    '''Exception class representing the type of the expected object
    not matching that of the response object
    '''
    pass

class ProxyWaitForError(ProxyError):
    '''Exception class representing a failure condition being encountered
    during a wait_for call'''
    pass

class ProxyInvalidXPathError(ProxyError):
    '''Exception class representing a supplied xpath being invalid'''
    pass

class ProxyParseError(ProxyError):
    '''Exception class reprsenting the failure to parse input documents'''
    pass

class Proxy(object):
    ''' Class that represents the interface to a proxy implementation

    Currently there are two flavors of proxy that have been implemented
    NetconfProxy and RestconfProxy. This class provides a single interface
    that can be used regardless of the underlying implementation chosen.
    '''
    XML_DECLARATION = '<?xml version="1.0" encoding="UTF-8"?>\n'
    RE_CAPTURE_KEY_VALUE = re.compile(r'\[[^=]*?\=[\"\']?([^\'\"\]]*?)[\'\"]?\]')
    RE_CAPTURE_RPC_REPLY_DATA = re.compile(r'.*?rpc-reply.*?<data>(.*)<\/data></rpc-reply>', re.MULTILINE|re.DOTALL)
    OPERATION_TIMEOUT_SECS = 70

    @staticmethod
    def _qualify_xpath(xpath, prefix):
        '''Create a fully qualified xpath

        This method makes the assumption that any elements not
        qualified exists within the same namespace.

        Arguments:
            xpath - xpath to make fully qualified
            prefix - prefix to use to qualify xpath

        Returns:
            A fully qualified xpath
        '''
        xpath = re.sub(r'(?<=/)([^/:]*(?:/|$))', r'{}:\1'.format(prefix), xpath)
        xpath = re.sub(r'(?<=\[)([^/:=]*=)', r'{}:\1'.format(prefix), xpath)
        xpath = re.sub(r'(=[\'\"][^/\'\"]+/){}:([^\'\"]*[\'\"]\])'.format(prefix), r'\1\2', xpath)
        return xpath

    @staticmethod
    def _xpath_strip_keys(xpath):
        '''Strip key-value pairs from the supplied xpath

        Arguments:
            xpath - xpath to be stripped of keys

        Returns:
            an xpath without keys
        '''
        return re.sub(Proxy.RE_CAPTURE_KEY_VALUE, '', xpath)

    @staticmethod
    def _xml_strip_rpc_reply(xml):
        '''Strip rpc-reply xml from the supplied xml

        Many responses from Netconf/Restconf are wrapped in
        <rpc-reply><data> ... </data></rpc-reply>. This method
        strips those containers from the response.

        Arguments:
            xml - xml to be stripped of rpc-reply

        Returns:
            xml contained within rpc-reply
            or the original xml if it was not wrapped with that element
        '''
        pos_data_tag = xml.find(r'<data', 0)
        pos_data_start = xml.find(r'>', pos_data_tag) + 1
        pos_data_end = xml.rfind(r'</data', pos_data_start)
        if pos_data_start >= 0 and pos_data_end >= 0:
            xml = xml[pos_data_start:pos_data_end]
        return xml

    @staticmethod
    def _create_gi_from_xpath(xpath, module):
        '''Create an instance GI object from the supplied module and xpath

        Arguments:
            xpath  - xpath to a resource
            module - module containing the resource targeted by the supplied xpath

        Returns:
            A blank GI object representing a resource as described by the
            supplied xpath
        '''
        stripped_xpath = Proxy._xpath_strip_keys(xpath)
        desc = RwKeyspec.get_pbcmd_from_xpath(stripped_xpath, module.get_schema())
        if desc:
            (module_name, obj_type) = desc.get_gi_typename().split(".", 1)
            create_object = getattr(module, obj_type)
            return create_object()

        path = RwKeyspec.path_from_xpath(
                module.get_schema(),
                xpath,
                RwKeyspec.RwXpathType.KEYSPEC)

        if path is None:
            msg = '%s does not identify a known resource'
            raise ProxyInvalidXPathError(msg % (xpath))

        return None

    @staticmethod
    def _rpc_fix_root(xml, rpc_name, prefix, namespace):
        '''Wrap the supplied xml with a root element having the supplied
        rpc_name, prefix and namespace.

        Addresses the fact that RPC replies don't contain the root element
        which we require in order to generate a GI type.

        Arguments:
            xml - xml to be wrapped by root element
            rpc_name - name of root element
            prefix - namespace prefix of root element
            namespace - namespace associated with supplied prefix

        Returns:
            xml wrapped with a root element constructed from the
            supplied rpc_name, prefix and namespace
        '''
        # Remove xml declaration in order to be parsed by etree.fromstring().
        xml.replace(Proxy.XML_DECLARATION, '')
        parser = etree.XMLParser(huge_tree=True)
        xml_root = etree.XML(xml.encode('ascii'), parser)

        rpc_response_element = etree.Element(
            rpc_name,
            nsmap={prefix:namespace}
        )

        for response_element in list(xml_root):
            rpc_response_element.append(response_element)

        return etree.tostring(rpc_response_element).decode()

    @staticmethod
    def _unroot_xml(xpath, xml, preserve_root=False, preserve_namespace=True, wrap_tag=None):
        '''Unroot rooted xml that is generated from GI objects

        Workaround for RIFT-9034

        Arguments:
            xpath  - xpath to root element
            xml    - xml to unroot
            preserve_root - flag specifying whether or not to preserve the root

        Returns:
            unrooted xml
        '''
        # Explanation:
        # 1. If a name space'd XML is used then the "[^:>]*:{elem}" regex will
        #    be used.
        # 2. Otherwise we will directly used the new_root name.
        unrooted_xml = None
        tag_name = "(?:(?=.+?:{elem})[^:>]*:{elem}|(?!:{elem}){elem})"
        re_evil = "<{tag}(?: [^>]*>|>)".format(tag=tag_name)
        re_evil += "(.*?)"
        re_evil += "<\/{tag}(?: [^>]*>|>)".format(tag=tag_name)

        xpath = xpath.lstrip('/')
        xpath = Proxy._xpath_strip_keys(xpath)
        xpath_elems = xpath.split('/')

        newroot = xpath_elems[-1]
        if ":" in newroot:
            (_, newroot) = newroot.split(":", 1)

        if preserve_root:
            # Include root element
            match = re.search(re_evil.format(elem=newroot), xml, re.MULTILINE|re.DOTALL)
            if match:
                unrooted_xml = match.group(0)
        else:
            # Don't include root element
            match = re.search(re_evil.format(elem=newroot), xml, re.MULTILINE|re.DOTALL)
            if match:
                unrooted_xml = match.group(1)

        if unrooted_xml is None:
            raise ProxyParseError('Failed to modify root of xml document')

        if wrap_tag is not None:
            unrooted_xml = "<%s>%s</%s>" % (wrap_tag, unrooted_xml, wrap_tag)

        if preserve_namespace:
            prefixes = set(re.findall(r'[\<\ ]\/?([^:\<\>]*?):', unrooted_xml, re.MULTILINE|re.DOTALL))
            for prefix in prefixes:
                if not re.match('^[-\w]+$', prefix, re.MULTILINE|re.DOTALL):
                    continue
                re_namespace = r'(xmlns:{}\=[^\ >]*)'.format(prefix)
                if not re.search(re_namespace, unrooted_xml, re.MULTILINE|re.DOTALL):
                    match = re.search(re_namespace, xml, re.MULTILINE|re.DOTALL)
                    if match:
                        namespace_decl = match.group(1)
                        pos = unrooted_xml.index('>')
                        unrooted_xml = unrooted_xml[:pos] + " {}".format(namespace_decl) + unrooted_xml[pos:]

        return unrooted_xml

    def __init__(self, proxy_impl):
        '''Initialize an instance of the Proxy class

        Arguments:
            proxy_impl - An instance of a proxy class providing the underlying
                         implementation

        Returns:
            New Instance of Proxy class
        '''
        self.proxy_impl = proxy_impl

    def get(self, xpath, list_obj=False):
        '''Get operational data from the supplied xpath

        Arguments:
            xpath     - xpath to operational data
            list_obj  - set to true if xpath points to a list node to
                        retrieve the full list

        Returns:
            A GI object representing the requested resource
            or None if the response contained no data

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.get(xpath, list_obj)

    @staticmethod
    def _default_compare(expected, response):
        '''Traverse the expected, for fields that are set within the
        expected verify that they match the corresponding fields in
        the response.

        Arguments:
            expected - object representing the expected response
            response  - the response object

        Returns:
            True or False depending on the outcome of the comparison

        Raises:
            ProxyExpectGIMismatchError - if expectation and response are not of matching types
        '''
        if isinstance(expected, rift.gi_utils.GIList):
            for expected_elem in expected:
                match = False
                for response_elem in response:
                    match = Proxy._default_compare(expected_elem, response_elem)
                    if match:
                        break
                return match

        elif rift.gi_utils.isgibox(expected):
            desc = expected.retrieve_descriptor()
            fields = desc.get_field_names()
            for field in fields:
                if field not in expected:
                    continue
                if field in expected and field not in response:
                    return False
                expected_value = getattr(expected, field)
                response_value = getattr(response, field)
                return Proxy._default_compare(expected_value, response_value)

        elif expected != response:
            return False

        return True

    def wait_for(self, xpath, expected, timeout, fail_on=None, compare=None, list_obj=False, retry_interval=10):
        '''Get operational data from the supplied xpath until the
        comparison operation succeeds or the time elapsed exceeds
        the supplied timeout

        Arguments:
            xpath     - xpath to operational data
            expected  - object or value representing the expected result
                        for use in comparison operation
            timeout   - maximum time allowed before expection is not met
            fail_on   - result or list of results that should throw an
                        exception if encountered
            compare   - method to compare expected to query result
            list_obj  - set to true if xpath points to a list node to
                          retrieve the full list
            retry_interval  - interval between comparison attempts

        Returns:
            A GI object representing the requested resource


        Raises:
            ProxyRequestError       - if response contains errors
            ProxyExpectTimeoutError - if expected was not met within time allowed
            ProxyWaitforError       - if a failure case is encountered
        '''
        match = False
        start_time = time.time()

        failure_cases = []
        if fail_on:
            failure_cases = fail_on

        if type(failure_cases) != type([]):
            failure_cases = [failure_cases]

        if compare:
            compare_method = compare
        else:
            compare_method = Proxy._default_compare

        while True:
            response = self.get(xpath, list_obj=list_obj)

            for failure_case in failure_cases:
                match = compare_method(failure_case, response)
                if match:
                    msg = ('Encountered failure case: %s\n'
                           'While waiting for %s to reach expected state: %s')
                    raise ProxyWaitForError(msg % (failure_case, xpath, expected))

            match = compare_method(expected, response)

            if match:
                break

            time_elapsed = time.time() - start_time
            time_remaining = timeout - time_elapsed
            if time_remaining <= 0:
                msg = 'Timed out after %d seconds while waiting for %s to reach expected state: %s'
                raise ProxyExpectTimeoutError(msg % (time_elapsed, xpath, expected))

            time.sleep(min(time_remaining, retry_interval))

        return response

    def wait_for_interval(self, xpath, compare, timeout, interval=5, fail_on=None, list_obj=False):
        '''Wait for the comparison of the sample collected at the start of
        the interval with the sample collected at the end of the interval to
        return True

        Arguments:
            xpath - path of resource being waited on
            compare - comparison method that describes success criteria
            timeout - maximum time allowed for operation to succeed
            interval - time to wait between comparisons
            fail_on - comparison method that describes failure criteria

        Returns:
            A GI object containing the last sample collected

        Raises:
            ProxyRequestError       - if response contains errors
            ProxyExpectTimeoutError - if expected was not met within time allowed
            ProxyWaitforError       - if a failure case is encountered
        '''

        previous_sample = None
        current_sample = None
        time_start = time.time()
        time_elapsed = time.time() - time_start
        time_remaining = timeout - time_elapsed

        while True:
            if current_sample:
                previous_sample = current_sample
            else:
                previous_sample = self.get(xpath, list_obj=list_obj)

            time.sleep(min(time_remaining, interval))

            current_sample = self.get(xpath)
            success = compare(previous_sample, current_sample)
            if success:
                break

            if fail_on:
                failure = fail_on(previous_sample, current_sample)
                if failure:
                    msg = 'Comparison resulting in failure while waiting for interval transition on %s'
                    raise ProxyWaitForError(msg % (xpath))

            time_elapsed = time.time() - time_start
            time_remaining = timeout - time_elapsed

            if time_remaining <= 0:
                msg = 'Timed out after %d seconds while waiting for interval transition on %s'
                raise ProxyExpectTimeoutError(msg % (time_elapsed, xpath))

        return current_sample


    def get_config(self, xpath, source='running', list_obj=False):
        '''Get configuration data from the supplied xpath

        Arguments:
            xpath - xpath to configuration data

        Returns:
            A GI object representing the requested resource
            or None if the response contained no data

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.get_config(xpath, source, list_obj)

    def create_config(self, xpath, obj, target='candidate'):
        '''Create configuration defined by the supplied object at the
        supplied xpath.

        Arugments:
            xpath  - xpath to the parent of the provided configuration object
            obj    - object representing configuration to create
            target - configuration database to target [running, candidate]

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.create_config(xpath, obj, target)

    def merge_config(self, xpath, obj, target='candidate'):
        '''Merge configuration defined by the supplied object at the
        supplied xpath

        Arugments:
            xpath  - xpath to configuration data
            obj    - object representing configuration to merge
            target - configuration database to target [running, candidate]

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.merge_config(xpath, obj, target)

    def replace_config(self, xpath, obj, target='candidate'):
        '''Replace configuration defined by the supplied object at the
        supplied xpath

        Arugments:
            xpath  - xpath to configuration data
            obj    - object representing new configuration
            target - configuration database to target [running, candidate]

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.replace_config(xpath, obj, target)

    def delete_config(self, xpath, target='candidate'):
        '''Delete configuration defined by the supplied object at the
        supplied xpath

        Arugments:
            xpath  - xpath to configuration data
            target - configuration database to target [running, candidate]

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.delete_config(xpath, target)

    def rpc(self, input_obj, rpc_name=None, output_obj=None):
        '''Invoke RPC at the supplied xpath utilizing parameters defined
        by the supplied object

        Arguments:
            input_obj   - object representing the RPC input
            rpc_name    - Optional, If specified the xpath is derived from this
                          parameter. Useful in cases of augumented RPCs, as the
                          xml element that wraps the RPC will be a container
                          instead of the actual RPC.

        Returns:
            A GI object representing the RPC output
            or None if the response contained no data

        Raises:
            ProxyRequestError if response contains errors
        '''
        return self.proxy_impl.rpc(input_obj, rpc_name=rpc_name, output_obj=output_obj)


class NetconfProxy(Proxy):
    '''Netconf based implementation of Proxy interface'''

    @staticmethod
    def populate_keys(xpath, xml, prefix, namespace):
        '''Populate keys within xml dom from an xpath

        There's a lot of assumptions in this implementation that won't
        necessarily hold up against complex modeling.
        '''

        def _get_key_paths(xpath):
            '''get the set of xpath's that can be used to
            query each key that should be present in the xml

            Arguments:
                xpath - xpath to get key paths from

            Returns:
                A list of xpaths that confirm the presence of each key
            '''
            pos = 0
            key_paths = []
            while pos >= 0:
                pos = xpath.find(']', pos+1)
                if pos >= 0:
                    key_paths.append(xpath[:pos+1])
            return key_paths

        # Get a set of xpath sub expressions that evaluate
        # to each key provided within the full xpath
        qxpath = Proxy._qualify_xpath(xpath, prefix)
        key_paths = _get_key_paths(qxpath)

        # Find all the prefix/namespaces
        namespaces = dict(re.findall('xmlns:([^=]+)=[\"\']([^\'\"]+)[\"\']', xml, re.MULTILINE|re.DOTALL))

        # Make sure dom tree contains all keys
        tree = etree.fromstring(xml)

        def strip_outer_keys(xpath):
            while(xpath[-1] == ']'):
                new_xpath = re.sub(r'''\[[^=]*=['"][^\'\"]*['"]\]$''', '', xpath, re.MULTILINE|re.DOTALL)
                assert xpath != new_xpath
                xpath = new_xpath
            return xpath

        parent_xpath = None
        inserts = []
        for key_path in key_paths:

            if parent_xpath:
                # Emit keys only once all keys for a path have been evaluated
                new_parent_xpath = strip_outer_keys(key_path)
                if new_parent_xpath != parent_xpath:
                    for parent, key_element in reversed(inserts):
                        for element in list(parent):
                            if element.tag == key_element.tag:
                                parent.remove(element)
                                break
                        parent.insert(0, key_element)
                    parent_xpath = None
                    inserts = []

            # Try to find the key
            match = tree.xpath(key_path, namespaces=namespaces)

            if not match:
                # Determine the xpath currently being evaluated (without keys)
                parent_xpath = strip_outer_keys(key_path)

                # Its missing, try to patch the dom
                match = re.match(r'(?P<xpath>.*)\[(?P<key>[^\=]*)\=[\'\"]?(?P<value>[^\]]*?)[\'\"]?\]$', key_path, re.MULTILINE|re.DOTALL)
                parent = tree.xpath(parent_xpath, namespaces=namespaces)[0]
                (key_prefix, elem_name) = match.group('key').split(':')
                key_element = etree.Element('{'+namespaces[key_prefix]+'}'+elem_name)
                key_element.text = match.group('value')
                inserts.append((parent, key_element))

        for parent, key_element in reversed(inserts):
            # Emit any keys that are left over after key_path traversal
            for element in list(parent):
                if element.tag == key_element.tag:
                    parent.remove(element)
                    break
            parent.insert(0, key_element)

        return etree.tostring(tree).decode('utf-8')


    def __init__(self, session, module):
        self.session = session
        self.module = module
        self.schema = module.get_schema()
        self.model = RwYang.Model.create_libncx()
        self.model.load_schema_ypbc(self.schema)

    @ncclient_exception_wrapper
    def _get_xml(self, xpath):
        logger.debug("Request GET %s", xpath)
        response = self.session.ncclient_manager.get(('xpath', xpath))

        if hasattr(response, 'xml'):
            response_xml = response.xml
        else:
            response_xml = response.data_xml.decode()


        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request GET %s - Response contains errors' % xpath
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    def get(self, xpath, list_obj=False):
        response_xml = self._get_xml(xpath)

        if (list_obj):
            obj_xpath = xpath.rsplit('/', 1)[0]
            response_obj = Proxy._create_gi_from_xpath(obj_xpath, self.module)
        else:
            response_obj = Proxy._create_gi_from_xpath(xpath, self.module)

        if response_xml == "":
            logger.warning("Response contained no data")
            return None

        if rift.gi_utils.isgibox(response_obj):
            response_obj.from_xml_v2(self.model, response_xml)
        else:
            # Response obj is not a gi object, if we aren't in a corner case that means the xpath refers to a leaf element
            try:
                response_obj = Proxy._unroot_xml(xpath, response_xml, preserve_root=False, preserve_namespace=False)
            except ProxyParseError:
                response_obj = None

        return response_obj

    @ncclient_exception_wrapper
    def _get_config_xml(self, xpath, source='running'):
        logger.debug("Request GET CONFIG %s %s", source, xpath)
        response = self.session.ncclient_manager.get_config(
                source=source,
                filter=('xpath', xpath))

        if hasattr(response, 'xml'):
            response_xml = response.xml
        else:
            response_xml = response.data_xml.decode()

        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request GET CONFIG %s - Response contains errors' % xpath
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    def get_config(self, xpath, source='running', list_obj=False):
        response_xml = self._get_config_xml(xpath, source)

        if (list_obj):
            obj_xpath = xpath.rsplit('/', 1)[0]
            response_obj = Proxy._create_gi_from_xpath(obj_xpath, self.module)
        else:
            response_obj = Proxy._create_gi_from_xpath(xpath, self.module)

        if response_xml == "":
            logger.warning("Response contained no data")
            return None

        if rift.gi_utils.isgibox(response_obj):
            response_obj.from_xml_v2(self.model, response_xml)
        else:
            # Response obj is not a gi object, if we aren't in a corner case that means the xpath refers to a leaf element
            try:
                response_obj = Proxy._unroot_xml(xpath, response_xml, preserve_root=False, preserve_namespace=False)
            except ProxyParseError:
                response_obj = None

        return response_obj


    @ncclient_exception_wrapper
    def _edit_config_xml(self, xpath, xml, operation=None, target='candidate'):
        if target == 'candidate':
            target = 'running'

        # In leiu of protobuf delta support, try to place the attribute in the correct place
        def add_attribute(xpath, xml, operation):
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

        xml = add_attribute(xpath, xml, operation)

        xml = '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">{}</config>'.format(xml)

        logger.debug("Request EDIT CONFIG %s %s %s", operation, target, xpath)
        logger.debug("Request Content: %s", pretty_xml(xml))
        response = self.session.ncclient_manager.edit_config(
                target=target,
                config=xml,
                )

        if hasattr(response, 'xml'):
            response_xml = response.xml
        else:
            response_xml = response.data_xml.decode()

        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request EDIT CONFIG %s %s %s - Response contains errors' % (
                    operation,
                    target,
                    xpath)
            raise ProxyRequestError(msg)

        if target == 'candidate':
            commit_response = self.session.ncclient_manager.commit()

            if hasattr(commit_response, 'xml'):
                commit_response_xml = commit_response.xml
            else:
                commit_response_xml = commit_response.data_xml.decode()

            if 'rpc-error>' in commit_response_xml:
                msg = 'Request COMMIT EDIT CONFIG %s %s %s - Response contains errors' % (
                        operation,
                        target,
                        xpath)
                logger.debug("Commit Error Response: %s", pretty_xml(commit_response_xml))
                raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    def create_config(self, xpath, obj, target='candidate'):
        desc = obj.retrieve_descriptor()
        xml = obj.to_xml_v2(self.model)
        xml = NetconfProxy.populate_keys(xpath, xml, desc.xml_prefix(), desc.xml_ns())
        response = self._edit_config_xml(
                xpath=xpath,
                xml=xml,
                operation='create',
                target=target)
        return response

    def merge_config(self, xpath, obj, target='candidate'):
        desc = obj.retrieve_descriptor()
        xml = obj.to_xml_v2(self.model)
        xml = NetconfProxy.populate_keys(xpath, xml, desc.xml_prefix(), desc.xml_ns())
        response = self._edit_config_xml(
                xpath=xpath,
                xml=xml,
                operation='merge',
                target=target)
        return response

    def replace_config(self, xpath, obj, target='candidate'):
        desc = obj.retrieve_descriptor()
        xml = obj.to_xml_v2(self.model)
        xml = NetconfProxy.populate_keys(xpath, xml, desc.xml_prefix(), desc.xml_ns())
        response = self._edit_config_xml(
                xpath=xpath,
                xml=xml,
                operation='replace',
                target=target)
        return response

    def delete_config(self, xpath, target='candidate'):
        stripped_xpath = Proxy._xpath_strip_keys(xpath)
        desc = RwKeyspec.get_pbcmd_from_xpath(stripped_xpath, self.schema)
        (module_name, obj_type) = desc.get_gi_typename().split(".", 1)
        create_object = getattr(self.module, obj_type)
        obj = create_object()

        match = re.search('\[(?P<key>[^\=\]]*?)\=[\'\"]?(?P<value>[^\]]*?)[\'\"]?\]$', xpath)
        if match:
            # Try to fill in instance key on the generated object
            key = match.group('key').replace('-','_')
            key = key.split(':', 1)[-1]
            current_value = getattr(obj, key)
            value_type = type(current_value)
            if value_type == type(None):
                setattr(obj, key, match.group('value'))
            else:
                setattr(obj, key, value_type(match.group('value')))

        xml = obj.to_xml_v2(self.model)
        xml = NetconfProxy.populate_keys(xpath, xml, desc.xml_prefix(), desc.xml_ns())
        response = self._edit_config_xml(
                xpath=xpath,
                xml=xml,
                operation='delete',
                target=target)
        return response

    def _rpc_xml(self, xpath, xml):
        logger.debug("Request RPC %s", xpath)
        logger.debug("Request Content: %s", pretty_xml(xml))
        response = self.session.ncclient_manager.dispatch(
                etree.fromstring(xml)
                )

        if hasattr(response, 'xml'):
            response_xml = response.xml
        else:
            response_xml = response.data_xml.decode()

        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request RPC %s - Response contains errors' % xpath
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    @ncclient_exception_wrapper
    def rpc(self, input_obj, rpc_name=None, output_obj=None):
        in_desc = input_obj.retrieve_descriptor()
        xpath = '/{}'.format(rpc_name or in_desc.xml_element_name())

        if output_obj:
            out_desc = output_obj.retrieve_descriptor()
        else:
            rpc_output_xpath = 'O,{}'.format(xpath)
            stripped_xpath = Proxy._xpath_strip_keys(rpc_output_xpath)
            out_desc = RwKeyspec.get_pbcmd_from_xpath(stripped_xpath, self.schema)
            (module_name, obj_type) = out_desc.get_gi_typename().split(".", 1)
            create_object = getattr(self.module, obj_type)
            output_obj = create_object()

        request_xml = input_obj.to_xml_v2(self.model)
        response_xml = self._rpc_xml(xpath, request_xml)

        if response_xml == "":
            logger.warning("Response contained no data")
            output_obj = None
        elif '<ok/>' in response_xml:
            logger.debug('RPC replied with ACK instead of output container.')
            output_obj = None
        elif '<ok xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0"/>' in response_xml:
            logger.debug('RPC replied with ACK instead of output container.')
            output_obj = None
        else:
            output_obj.from_xml_v2(
                    self.model,
                    Proxy._rpc_fix_root(
                        xml=response_xml,
                        rpc_name=out_desc.xml_element_name(),
                        prefix=out_desc.xml_prefix(),
                        namespace=out_desc.xml_ns()))

        return output_obj

class RestconfProxy(Proxy):
    '''Restconf based implementation of Proxy interface'''

    @staticmethod
    def _xpath_to_rcpath(xpath):
        '''Convert an xpath to a restconf path

        Arguments:
            xpath - xpath to convert

        Returns:
            A restconf path to the resource described by the
            supplied xpath
        '''

        def keyval_to_rcpath(match_obj):
            return '/' + requests.utils.quote(match_obj.group(1), safe='')

        rcpath = xpath.lstrip('/')
        rcpath = re.sub(r'\]\[', '],[', rcpath)
        rcpath = re.sub(Proxy.RE_CAPTURE_KEY_VALUE, keyval_to_rcpath, rcpath)
        rcpath = re.sub(',/', ',', rcpath)
        return rcpath

    def __init__(self, session, module):
        self.session = session
        self.module = module
        self.schema = module.get_schema()
        self.model = RwYang.Model.create_libncx()
        self.model.load_schema_ypbc(self.schema)

    def _get_xml(self, xpath):
        logger.debug("Request GET %s", xpath)

        uri = self.session.uri_format.format(
                'operational',
                self._xpath_to_rcpath(xpath))
        uri += '?deep'
        response = self.session.request("GET", uri)
        response_xml = response.text

        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request GET %s - Response contains errors' % xpath
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    def get(self, xpath, list_obj=False):
        response_xml = self._get_xml(xpath)

        if (list_obj):
            obj_xpath = xpath.rsplit('/', 1)[0]
            response_obj = Proxy._create_gi_from_xpath(obj_xpath, self.module)
        else:
            response_obj = Proxy._create_gi_from_xpath(xpath, self.module)

        if response_xml == "":
            logger.warning("Response contained no data")
            return None

        if rift.gi_utils.isgibox(response_obj):
            response_obj.from_xml_v2(self.model, response_xml)
        else:
            # Response obj is not a gi object, if we aren't in a corner case that means the xpath refers to a leaf element
            try:
                response_obj = Proxy._unroot_xml(xpath, response_xml, preserve_root=False, preserve_namespace=False)
            except ProxyParseError:
                response_obj = None

        return response_obj

    def _get_config_xml(self, xpath, source='running'):
        logger.debug("Request GET CONFIG %s %s", source, xpath)

        uri = self.session.uri_format.format(
                source,
                self._xpath_to_rcpath(xpath))
        uri += '?deep'
        response = self.session.request("GET", uri)
        response_xml = response.text

        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request GET CONFIG %s %s - Response contains errors' % (source, xpath)
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    def get_config(self, xpath, source='running', list_obj=False):
        response_xml = self._get_config_xml(xpath, source)

        if (list_obj):
            obj_xpath = xpath.rsplit('/', 1)[0]
            response_obj = Proxy._create_gi_from_xpath(obj_xpath, self.module)
        else:
            response_obj = Proxy._create_gi_from_xpath(xpath, self.module)

        if response_xml == "":
            logger.warning("Response contained no data")
            return None

        if rift.gi_utils.isgibox(response_obj):
            response_obj.from_xml_v2(self.model, response_xml)
        else:
            # Response obj is not a gi object, if we aren't in a corner case that means the xpath refers to a leaf element
            try:
                response_obj = Proxy._unroot_xml(xpath, response_xml, preserve_root=False, preserve_namespace=False)
            except ProxyParseError:
                response_obj = None

        return response_obj

    @requests_exception_wrapper
    def create_config(self, xpath, obj, target='candidate'):
        rctarget = 'config'
        if target != 'candidate':
            rctarget = target

        xml = obj.to_xml_v2(self.model)
        xml = Proxy._unroot_xml(xpath, xml, preserve_root=True)
        uri = self.session.uri_format.format(
                rctarget,
                self._xpath_to_rcpath(xpath))

        logger.debug("Request EDIT CONFIG create %s %s", rctarget, xpath)
        logger.debug("Request Content: %s", pretty_xml(xml))
        response = self.session.request("POST", uri, data=xml)
        response_xml = response.text
        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request EDIT CONFIG %s %s - Response contains errors' % (rctarget, xpath)
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    @requests_exception_wrapper
    def merge_config(self, xpath, obj, target='candidate'):
        rctarget = 'config'
        if target != 'candidate':
            rctarget = target

        xml = obj.to_xml_v2(self.model)
        xml = Proxy._unroot_xml(xpath, xml)
        uri = self.session.uri_format.format(
                rctarget,
                self._xpath_to_rcpath(xpath))
        logger.debug("Request EDIT CONFIG merge %s %s", rctarget, xpath)
        logger.debug("Request Content: %s", pretty_xml(xml))
        response = self.session.request("PATCH", uri, data=xml)
        response_xml = response.text
        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request EDIT CONFIG %s %s - Response contains errors' % (rctarget, xpath)
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    @requests_exception_wrapper
    def replace_config(self, xpath, obj, target='candidate'):
        rctarget = 'config'
        if target != 'candidate':
            rctarget = target

        xml = obj.to_xml_v2(self.model)
        xml = Proxy._unroot_xml(xpath, xml, preserve_root=True)
        uri = self.session.uri_format.format(
                rctarget,
                self._xpath_to_rcpath(xpath))
        logger.debug("Request EDIT CONFIG replace %s %s", rctarget, xpath)
        logger.debug("Request Content: %s", pretty_xml(xml))
        response = self.session.request("PUT", uri, data=xml)
        response_xml = response.text
        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request EDIT CONFIG %s %s - Response contains errors' % (rctarget, xpath)
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    @requests_exception_wrapper
    def delete_config(self, xpath, target='candidate'):
        rctarget = 'config'
        if target != 'candidate':
            rctarget = target

        uri = self.session.uri_format.format(
                rctarget,
                self._xpath_to_rcpath(xpath))
        logger.debug("Request EDIT CONFIG delete %s %s", rctarget, xpath)
        response = self.session.request("DELETE", uri)
        response_xml = response.text
        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request EDIT CONFIG %s %s - Response contains errors' % (rctarget, xpath)
            raise ProxyRequestError(msg)

        return Proxy._xml_strip_rpc_reply(response_xml)

    @requests_exception_wrapper
    def rpc(self, input_obj, rpc_name=None, output_obj=None):
        def _xml_strip_rpc_output(xml):
            '''Strip rpc-reply xml from the supplied xml

            RPC replies from rwrest are wrapped in "output" tags like this:
            <output>...</output>
            This function strips those containers from the response.

            Arguments:
                xml - xml to be stripped of rpc-reply

            Returns:
                xml contained within rpc-reply
                or the original xml if it was not wrapped with that element
            '''
            pos_data_tag = xml.find(r'<output', 0)
            pos_data_start = xml.find(r'>', pos_data_tag) + 1
            pos_data_end = xml.rfind(r'</output', pos_data_start)
            if pos_data_start >= 0 and pos_data_end >= 0:
                xml = xml[pos_data_start:pos_data_end]

            return xml

        in_desc = input_obj.retrieve_descriptor()
        xpath = '/{}'.format(rpc_name or in_desc.xml_element_name())

        if output_obj:
            out_desc = output_obj.retrieve_descriptor()
        else:
            rpc_output_xpath = 'O,{}'.format(xpath)
            stripped_xpath = Proxy._xpath_strip_keys(rpc_output_xpath)
            out_desc = RwKeyspec.get_pbcmd_from_xpath(stripped_xpath, self.schema)
            (module_name, obj_type) = out_desc.get_gi_typename().split(".", 1)
            create_object = getattr(self.module, obj_type)
            output_obj = create_object()

        xml = input_obj.to_xml_v2(self.model)
        xml = Proxy._unroot_xml(xpath, xml, preserve_root=False, wrap_tag='input')
        uri = self.session.uri_format.format(
                'operations',
                self._xpath_to_rcpath(xpath))

        logger.debug("Request RPC %s", xpath)
        logger.debug("Request Content: %s", pretty_xml(xml))
        response = self.session.request("POST", uri, data=xml)
        response_xml = response.text
        logger.debug("Response: (len: %s)", len(response_xml))
        logger.debug("Response Content: %s", pretty_xml(response_xml))

        if 'rpc-error>' in response_xml:
            msg = 'Request RPC %s - Response contains errors' % xpath
            raise ProxyRequestError(msg)

        response_xml = _xml_strip_rpc_output(response_xml)
        response_xml = '<%s:%s xmlns:%s="%s">%s</%s:%s>' % (out_desc.xml_prefix(),
                                                            out_desc.xml_element_name(),
                                                            out_desc.xml_prefix(),
                                                            out_desc.xml_ns(),
                                                            response_xml,
                                                            out_desc.xml_prefix(),
                                                            out_desc.xml_element_name())

        if response_xml == "":
            logger.warning("Response contained no data")
            output_obj = None
        elif '<ok/>' in response_xml:
            logger.debug('RPC replied with ACK instead of output container.')
            output_obj = None
        elif '<ok xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0"/>' in response_xml:
            logger.debug('RPC replied with ACK instead of output container.')
            output_obj = None
        else:
            output_obj.from_xml_v2(
                    self.model,
                response_xml)

        return output_obj


class Session(object):
    '''Class that represents the interface to a session implementation

    Currently there are two flavors of session implemented. NetconfSession and
    RestconfSession. This class provides a single interfce that can be used
    regardless of the underlying implementation
    '''

    DEFAULT_CONNECTION_TIMEOUT = 360
    DEFAULT_ONBOARD_TIMEOUT = 600
    DEFAULT_ONBOARD_RETRY_INTERVAL = 10

    @property
    def host(self):
        return self.session_impl.host

    @property
    def port(self):
        return self.session_impl.port

    def __init__(self, session_impl):
        '''Initialize an instance of the Session class

        Arguments:
            session_impl - An instance of a session class providing the
                    underlying implementation.

        Returns:
            New Instance of Session
        '''
        self.session_impl = session_impl

    def connect(self, timeout=DEFAULT_CONNECTION_TIMEOUT):
        '''Establish session connection

        Arguments:
            timeout - maximum time allowed before connect fails
        '''
        self.session_impl.connect(timeout=timeout)

    def close(self):
        '''Close session connection
        '''
        self.session_impl.close()

    def proxy(self, module):
        """ Get a proxy interface to the supplied module

        This proxy can be used to access resources defined by the
        supplied module's schema

        Arguments:
            module - module that defines resources to be accessed

        Returns:
            A proxy instance
        """
        return self.session_impl.proxy(module=module)


class NetconfSession(Session):
    DEFAULT_CONNECTION_TIMEOUT = 360
    DEFAULT_PORT = 2022
    DEFAULT_USERNAME = 'admin'
    DEFAULT_PASSWORD = 'admin'

    def __init__(
            self,
            host='127.0.0.1',
            port=DEFAULT_PORT,
            username=DEFAULT_USERNAME,
            password=DEFAULT_PASSWORD):
        ''' Class representing a Netconf based session

        Arguments:
            host - address of Netconf host
            port - port of Netconf host
            username - username credential
            password - password credential
        '''
        super(NetconfSession, self).__init__(
                NetconfSessionImpl(
                    host=host,
                    port=port,
                    username=username,
                    password=password,
        ))

    @property
    def ncmgr(self):
        return self.session_impl._nc_mgr


class RestconfSession(Session):
    DEFAULT_CONNECTION_TIMEOUT = 360
    DEFAULT_PORT = 8008
    DEFAULT_USERNAME = 'admin'
    DEFAULT_PASSWORD = 'admin'

    def __init__(
            self,
            host='127.0.0.1',
            port=DEFAULT_PORT,
            username=DEFAULT_USERNAME,
            password=DEFAULT_PASSWORD):
        ''' Class representing a Restconf based session

        Arguments:
            host - address of Restconf host
            port - port of Restconf host
            username - username credential
            password - password credential
        '''
        super(RestconfSession, self).__init__(
                RestconfSessionImpl(
                    host=host,
                    port=port,
                    username=username,
                    password=password,
        ))



class NetconfSessionImpl(object):
    '''Class representing a Netconf Session'''

    OPERATION_TIMEOUT_SECS = Proxy.OPERATION_TIMEOUT_SECS

    def __init__(self, host, port, username, password):
        '''Initialize a new Netconf Session instance

        Arguments:
            host - host ip
            port - host port
            username - credentials for accessing the host, username
            password - credentials for accessing the host, password

        Returns:
            A newly initialized Netconf session instance
        '''
        self.host = host
        self.port = port
        self.username = username
        self.password = password

    def connect(self, timeout):
        '''Connect Netconf Session

        Arguments:
            timeout - maximum time allowed before connect fails [default 30s]
        '''
        logger.info("Connecting to confd (%s) SSH port (%s)", self.host, self.port)

        start_time = time.time()
        while True:
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

                logger.info("Successfully connected to confd (%s) SSH port (%s)", self.host, self.port)
                self._nc_mgr.timeout = NetconfSessionImpl.OPERATION_TIMEOUT_SECS
                return

            except ncclient.NCClientError as e:
                logger.debug("Could not connect to (%s) confd ssh port (%s): %s",
                        self.host, self.port, str(e))

            time_elapsed = time.time() - start_time
            time_remaining = timeout - time_elapsed
            if time_remaining <= 0:
                break

            time.sleep(min(time_remaining, 10))


        raise ProxyConnectionError("Could not connect to Confd ({}) ssh port ({}): within the timeout {} sec.".format(
                self.host, self.port, timeout))

    def close(self):
        """ Close Netconf Session
        """

        if self._nc_mgr is not None:
            try:
                logger.info("Closing confd SSH session to %s", self.host)
                if self._nc_mgr.connected:
                    self._nc_mgr.close_session()
                else:
                    logger.warning("confd SSH session to %s already closed.", self.host)
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
            self.connect(timeout=10)

        return self._nc_mgr


    def proxy(self, module):
        """ Get a netconf proxy interface to the supplied module

        This proxy can be used to access resources defined by the
        supplied module's schema

        Arguments:
            module - module that defines resources to be accessed via netconf

        Returns:
            A NetconfProxy instance
        """
        return Proxy(NetconfProxy(self, module))


class RestconfSessionImpl(object):
    '''Class representing a Restconf Session'''

    URI_FORMAT = '{}://{}:{}/api/{}/{}'
    OPERATION_TIMEOUT_SECS = Proxy.OPERATION_TIMEOUT_SECS
    RESTCONF_HEADERS = {
        'Accept':'application/vnd.yang.data+xml',
        'Content-Type':'application/vnd.yang.data+xml',
    }

    def __init__(self, host, port, username, password):
        '''Initialize a new Restconf Session instance

        Arguments:
            host - host ip
            port - host port
            username - credentials for accessing the host, username
            password - credentials for accessing the host, password

        Returns:
            A newly initialized Restconf session instance
        '''
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.auth = (self.username, self.password)
        https = certs.USE_SSL

        scheme = "https" if https else "http"
        self.uri_format = self.URI_FORMAT.format(scheme, self.host, self.port, '{}', '{}')

        self.session = requests.Session()
        self.session.auth = self.auth
        self.session.timeout = RestconfSessionImpl.OPERATION_TIMEOUT_SECS
        self.session.headers = self.RESTCONF_HEADERS

        if https:

            # If no certifcate is available, then get the certificates used in
            # the manifest using the certs lib.
            self.session.verify = False

            # Ignore the SSL flag, we want the session to configure the option.
            _, cert, key = certs.get_bootstrap_cert_and_key()
            self.session.cert = (cert, key)

    @requests_exception_wrapper
    def request(self, method, url, **kwargs):
        """Trigger the http/https request

        Arguments:
            method - Request method (GET, PUT ...)
            url (str): Url of the request
            **kwargs: Any other kwargs to be passed to requests library.

        Returns:
            request.Response
        """
        response = self.session.request(method, url, **kwargs)

        return response

    def connect(self, timeout):
        '''Connect Restconf Session

        This is a noop, because restconf session does not maintain a persistent connection

        Arguments:
            timeout - maximum time allowed before connect fails [default 30s]
        '''
        pass

    def close(self):
        """ Close Restconf Session

        Also a noop
        """
        pass

    def proxy(self, module):
        """ Get a restconf proxy interface to the supplied module

        This proxy can be used to access resources defined by the
        supplied module's schema

        Arguments:
            module - module that defines resources to be accessed via restconf

        Returns:
            A Proxy instance that communicates via restconf and understands the
            contents of the supplied module
        """
        return Proxy(RestconfProxy(self, module))
