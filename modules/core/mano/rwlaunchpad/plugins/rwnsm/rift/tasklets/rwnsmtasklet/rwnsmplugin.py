# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import abc


class NsmPluginBase(object):
    """
        Abstract base class for the NSM plugin.
        There will be single instance of this plugin for each plugin type.
    """

    def __init__(self, dts, log, loop, nsm, plugin_name, dts_publisher):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm
        self._plugin_name = plugin_name
        self._dts_publisher = dts_publisher

    @property
    def dts(self):
        return self._dts

    @property
    def log(self):
        return self._log

    @property
    def loop(self):
        return self._loop

    @property
    def nsm(self):
        return self._nsm

    def create_nsr(self, nsr):
        """ Create an NSR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def deploy(self, nsr_msg):
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def instantiate_ns(self, nsr, xact):
        """ Instantiate the network service """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def instantiate_vnf(self, nsr, vnfr):
        """ Instantiate the virtual network function """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def instantiate_vl(self, nsr, vl):
        """ Instantiate the virtual link"""
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def get_nsr(self, nsr_path):
        """ Get the NSR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def get_vnfr(self, vnfr_path):
        """ Get the VNFR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def get_vlr(self, vlr_path):
        """ Get the VLR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def terminate_ns(self, nsr):
        """Terminate the network service """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def terminate_vnf(self, vnfr):
        """Terminate the VNF """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def terminate_vl(self, vlr):
        """Terminate the Virtual Link Record"""
        pass
