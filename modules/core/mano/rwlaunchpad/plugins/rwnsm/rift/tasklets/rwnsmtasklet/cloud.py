
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
from gi.repository import (
    RwDts as rwdts,
    RwcalYang as rwcal,
    RwTypes,
    ProtobufC,
    )

import rift.tasklets
import rift.mano.cloud

from . import openmano_nsm
from . import rwnsmplugin


class CloudAccountNotFoundError(Exception):
    pass


class RwNsPlugin(rwnsmplugin.NsmPluginBase):
    """
        RW Implentation of the NsmPluginBase
    """
    def __init__(self, dts, log, loop, publisher, cloud_account):
        self._dts = dts
        self._log = log
        self._loop = loop

    def create_nsr(self, nsr_msg, nsd):
        """
        Create Network service record
        """
        pass

    @asyncio.coroutine
    def deploy(self, nsr):
        pass

    @asyncio.coroutine
    def instantiate_ns(self, nsr, config_xact):
        """
        Instantiate NSR with the passed nsr id
        """
        yield from nsr.instantiate(config_xact)

    @asyncio.coroutine
    def instantiate_vnf(self, nsr, vnfr):
        """
        Instantiate NSR with the passed nsr id
        """
        yield from vnfr.instantiate(nsr)

    @asyncio.coroutine
    def instantiate_vl(self, nsr, vlr):
        """
        Instantiate NSR with the passed nsr id
        """
        yield from vlr.instantiate()

    @asyncio.coroutine
    def terminate_ns(self, nsr):
        """
        Terminate the network service
        """
        pass

    @asyncio.coroutine
    def terminate_vnf(self, vnfr):
        """
        Terminate the network service
        """
        yield from vnfr.terminate()

    @asyncio.coroutine
    def terminate_vl(self, vlr):
        """
        Terminate the virtual link
        """
        yield from vlr.terminate()


class NsmPlugins(object):
    """ NSM Plugins """
    def __init__(self):
        self._plugin_classes = {
                "openmano": openmano_nsm.OpenmanoNsPlugin,
                }

    @property
    def plugins(self):
        """ Plugin info """
        return self._plugin_classes

    def __getitem__(self, name):
        """ Get item """
        print("%s", self._plugin_classes)
        return self._plugin_classes[name]

    def register(self, plugin_name, plugin_class, *args):
        """ Register a plugin to this Nsm"""
        self._plugin_classes[plugin_name] = plugin_class

    def deregister(self, plugin_name, plugin_class, *args):
        """ Deregister a plugin to this Nsm"""
        if plugin_name in self._plugin_classes:
            del self._plugin_classes[plugin_name]

    def class_by_plugin_name(self, name):
        """ Get class by plugin name """
        return self._plugin_classes[name]


class CloudAccountNsmPluginSelector(object):
    def __init__(self, dts, log, log_hdl, loop, records_publisher):
        self._dts = dts
        self._log = log
        self._log_hdl = log_hdl
        self._loop = loop
        self._records_publisher = records_publisher

        self._nsm_plugins = NsmPlugins()

        self._cloud_sub = rift.mano.cloud.CloudAccountConfigSubscriber(
                self._dts,
                self._log,
                self._log_hdl,
                rift.mano.cloud.CloudAccountConfigCallbacks(
                    on_add_apply=self._on_cloud_account_added,
                    on_delete_apply=self._on_cloud_account_deleted,
                    )
                )

        self._cloud_plugins = {}
        self._plugin_instances = {}

    def _on_cloud_account_added(self, cloud_account):
        self._log.debug("Got nsm plugin cloud account: %s", cloud_account)
        try:
            nsm_cls = self._nsm_plugins.class_by_plugin_name(
                    cloud_account.account_type
                    )
        except KeyError as e:
            self._log.debug(
                "Cloud account nsm plugin not found: %s.  Using standard rift nsm.",
                cloud_account.name
                )
            nsm_cls = RwNsPlugin

        # Check to see if the plugin was already instantiated
        if nsm_cls in self._plugin_instances:
            self._log.debug("Cloud account nsm plugin already instantiated.  Using existing.")
            self._cloud_plugins[cloud_account.name] = self._plugin_instances[nsm_cls]
            return

        # Otherwise, instantiate a new plugin using the cloud account
        self._log.debug("Instantiating new cloud account using class: %s", nsm_cls)
        nsm_instance = nsm_cls(self._dts, self._log, self._loop,
                               self._records_publisher, cloud_account.account_msg)
        self._plugin_instances[nsm_cls] = nsm_instance

        self._cloud_plugins[cloud_account.name] = nsm_instance

    def _on_cloud_account_deleted(self, account_name):
        del self._cloud_plugins[account_name]

    def get_cloud_account_plugin_instance(self, account_name):
        if account_name not in self._cloud_plugins:
            msg = "Account %s was not configured" % account_name
            self._log.error(msg)
            raise CloudAccountNotFoundError(msg)

        instance = self._cloud_plugins[account_name]
        self._log.debug("Got NSM plugin instance for account %s: %s",
                account_name, instance)

        return instance

    def get_cloud_account_sdn_name(self, account_name):
        if account_name in self._cloud_sub.accounts:
            self._log.debug("Cloud accnt msg is %s",self._cloud_sub.accounts[account_name].account_msg)
            if self._cloud_sub.accounts[account_name].account_msg.has_field("sdn_account"):
                sdn_account = self._cloud_sub.accounts[account_name].account_msg.sdn_account 
                self._log.info("SDN associated with Cloud name %s is %s", account_name, sdn_account)
                return sdn_account
            else:
                self._log.debug("No SDN Account associated with Cloud name %s", account_name)
                return None


    @asyncio.coroutine
    def register(self):
        self._cloud_sub.register()
