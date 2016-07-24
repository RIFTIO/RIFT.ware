# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import rift.tasklets

import gi
from gi.repository import (
    RwConfigAgentYang as rwcfg_agent,
)

from . import RiftCA
from . import jujuconf
import rift.mano.config_agent


class ConfigAgentError(Exception):
    pass


class ConfigAgentExistsError(ConfigAgentError):
    pass


class UnknownAgentTypeError(Exception):
    pass


class ConfigAgentVnfrAddError(Exception):
    pass


class ConfigAccountHandler(object):
    def __init__(self, dts, log, loop, on_add_config_agent, on_delete_config_agent):
        self._log = log
        self._dts = dts
        self._loop = loop
        self._on_add_config_agent = on_add_config_agent
        self._on_delete_config_agent = on_delete_config_agent

        self._log.debug("creating config account handler")
        self.cloud_cfg_handler = rift.mano.config_agent.ConfigAgentSubscriber(
            self._dts, self._log,
            rift.mano.config_agent.ConfigAgentCallbacks(
                on_add_apply=self.on_config_account_added,
                on_delete_apply=self.on_config_account_deleted,
            )
        )

    def on_config_account_deleted(self, account):
        self._log.debug("config account deleted: %s", account.name)
        self._on_delete_config_agent(account)

    def on_config_account_added(self, account):
        self._log.debug("config account added")
        self._log.debug(account.as_dict())
        self._on_add_config_agent(account)

    @asyncio.coroutine
    def register(self):
        self.cloud_cfg_handler.register()

class RiftCMConfigPlugins(object):
    """ NSM Config Agent Plugins """
    def __init__(self):
        self._plugin_classes = {
            "juju": jujuconf.JujuConfigPlugin,
            "riftca": RiftCA.RiftCAConfigPlugin,
        }

    @property
    def plugins(self):
        """ Plugin info """
        return self._plugin_classes

    def __getitem__(self, name):
        """ Get item """
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


class RiftCMConfigAgent(object):
    DEFAULT_CAP_TYPE = "riftca"
    def __init__(self, dts, log, loop, parent):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._ConfigManagerConfig = parent

        self._config_plugins = RiftCMConfigPlugins()
        self._config_handler = ConfigAccountHandler(
            self._dts, self._log, self._loop, self._on_config_agent, self._on_config_agent_delete)
        self._plugin_instances = {}
        self._default_account_added = False

    @asyncio.coroutine
    def invoke_config_agent_plugins(self, method, nsr, vnfr, *args):
        # Invoke the methods on all config agent plugins registered
        rc = False
        for agent in self._plugin_instances.values():
            if not agent.is_vnfr_managed(vnfr.id):
                continue
            try:
                self._log.debug("Invoke {} on {}".format(method, agent.name))
                rc = yield from agent.invoke(method, nsr, vnfr, *args)
                break
            except Exception as e:
                self._log.error("Error invoking {} on {} : {}".
                                format(method, agent.name, e))
                self._log.exception(e)

        self._log.info("vnfr({}), method={}, return rc={}"
                       .format(vnfr.name, method, rc))
        return rc

    def is_vnfr_config_agent_managed(self, vnfr):
        if (not vnfr.has_field('netconf') and
            not vnfr.has_field('juju') and
            not vnfr.has_field('script')):
            return False

        for agent in self._plugin_instances.values():
            try:
                if agent.is_vnfr_managed(vnfr.id):
                    return True
            except Exception as e:
                self._log.debug("Check if VNFR {} is config agent managed: {}".
                                format(vnfr.name, e))
        return False

    def _on_config_agent(self, config_agent):
        self._log.debug("Got nsm plugin config agent account: %s", config_agent)
        try:
            cap_name = config_agent.account_type
            cap_inst = self._config_plugins.class_by_plugin_name(
                config_agent.account_type)
        except KeyError as e:
            msg = "Config agent nsm plugin type not found: {}". \
                format(config_agent.account_type)
            self._log.error(msg)
            raise UnknownAgentTypeError(msg)

        # Check to see if the plugin was already instantiated
        if cap_name in self._plugin_instances:
            self._log.debug("Config agent nsm plugin {} already instantiated. " \
                            "Using existing.". format(cap_name))
        else:
            # Otherwise, instantiate a new plugin using the config agent account
            self._log.debug("Instantiting new config agent using class: %s", cap_inst)
            new_instance = cap_inst(self._dts, self._log, self._loop, config_agent)
            self._plugin_instances[cap_name] = new_instance

        # TODO (pjoseph): See why this was added, as this deletes the
        # Rift plugin account when Juju account is added
        # if self._default_account_added:
        #     # If the user has provided a config account, chuck the default one.
        #     if self.DEFAULT_CAP_TYPE in self._plugin_instances:
        #         del self._plugin_instances[self.DEFAULT_CAP_TYPE]

    def _on_config_agent_delete(self, config_agent):
        self._log.debug("Got nsm plugin config agent delete, account: %s, type: %s",
                config_agent.name, config_agent.account_type)
        cap_name = config_agent.account_type
        if cap_name in self._plugin_instances:
            self._log.debug("Config agent nsm plugin exists, deleting it.")
            del self._plugin_instances[cap_name]
        else:
            self._log.error("Error deleting - Config Agent nsm plugin %s does not exist.", cap_name)


    @asyncio.coroutine
    def register(self):
        self._log.debug("Registering for config agent nsm plugin manager")
        yield from self._config_handler.register()

        account = rwcfg_agent.ConfigAgentAccount()
        account.account_type = self.DEFAULT_CAP_TYPE
        account.name = "RiftCA"
        self._on_config_agent(account)
        self._default_account_added = True

        # Also grab any account already configured
        config_agents = yield from self._ConfigManagerConfig.cmdts_obj.get_config_agents(name=None)
        for account in config_agents:
            self._on_config_agent(account)

    def set_config_agent(self, nsr, vnfr, method):
        if method == 'juju':
            agent_type = 'juju'
        elif method in ['netconf', 'script']:
            agent_type = self.DEFAULT_CAP_TYPE
        else:
            msg = "Unsupported configuration method ({}) for VNF:{}/{}". \
                  format(method, nsr.name, vnfr.name)
            self._log.error(msg)
            raise UnknownAgentTypeError(msg)

        try:
            agent = self._plugin_instances[agent_type]
            agent.add_vnfr_managed(vnfr)
        except Exception as e:
            self._log.error("Error set_config_agent for type {}: {}".
                            format(agent_type, str(e)))
            self._log.exception(e)
            raise ConfigAgentVnfrAddError(e)
