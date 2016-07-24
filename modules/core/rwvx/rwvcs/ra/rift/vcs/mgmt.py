"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file mgmt.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 04/21/2015
# @brief VCS Management Helpers
# @details This file contains functions that simplify using Protobuf to
# interact with external VCS interfaces.
#
"""
import logging
import os
import time

import rift.auto.proxy
from . import yang_model

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwVcsYang', '1.0')

from gi.repository import RwYang

from gi.repository import (
        RwBaseYang,
        RwManifestYang,
        RwVcsYang,
        )

from gi.repository.RwManifestYang import RwmgmtAgentMode

SYSTEM_STARTUP_TIMEOUT_SECS = 5*60

logger = logging.getLogger(__name__)

class SystemStartError(Exception):
    pass

class ComponentInfoError(Exception):
    pass

class VcsResourceError(Exception):
    pass

class VcsComponentInfo(object):
    """ This class deals with the """
    def __init__(self, mgmt_proxy):
        self._mgmt_proxy = mgmt_proxy
        self.model = RwYang.Model.create_libncx()
    @property
    def mgmt_proxy(self):
        """ The MgmtProxy instance """
        return self._mgmt_proxy

    def get_components(self):
        """ Get the list of component_info objects

        Returns:
            A list of ComponentInfo proto-GI objects (rw-base.yang)
        """

        vcs_info = RwBaseYang.VcsInfo()
        response_xml = self.mgmt_proxy.get_from_xpath('/vcs/info')
        response_xml = response_xml[response_xml.index('<data'):]
        self.model.load_schema_ypbc(RwBaseYang.get_schema())

        try:
            vcs_info.from_xml_v2(self.model, response_xml)
        except Exception as e:
            raise ComponentInfoError("Could not convert tasklet info XML response: {}".format(str(e)))

        components = vcs_info.components.component_info
        if not components:
            raise ComponentInfoError("No components returned from VcsInfo request.")

        return components

    def get_unstarted_components(self):
        """ Get the list of component_info objects which are not in RUNNING state

        Returns:
            A list of ComponentInfo proto-GI objects (rw-base.yang)
        """

        components = self.get_components()
        pending_components = [component for component in components if component.state not in ["RUNNING"]]

        return pending_components

    def is_system_ready(self):
        """ Return True if all components are RUNNING, False otherwise

        Returns:
            A boolean indicating if system is ready

        Raises:
            SystemStartError - If a component enters a state other than STARTING
        """

        try:
            unstarted_components = self.get_unstarted_components()
            if not unstarted_components:
                logger.info("All components have entered RUNNING state")
                return True
            else:
                logger.info("All components have NOT YET entered RUNNING state")

            for component in unstarted_components:
                if not component.state in ["STARTING", "INITIALIZING", "RUNNING", "RECOVERING"]:
                    raise SystemStartError("Component (%s) entered a invalid startup state: %s",
                            component.instance_name, component.state)

                logger.info("Component (%s) state is %s",
                        component.instance_name, component.state)

        # During initialization (especially collapsed mode) it is possible that the system
        # is not yet in a state to receive confd requests.  In turn, this will bubble up
        # as either a ProxyTimeoutError or ProxyRequestError which we want to catch here.
        except rift.auto.proxy.ProxyConnectionError as e:
            logger.warning("ProxyConnectionError: %s", str(e))
            self.mgmt_proxy.close()
            self.mgmt_proxy.connect()
        except rift.auto.proxy.ProxyRequestTimeout as e:
            logger.warning("ProxyRequestTimeout: %s", str(e))
            self.mgmt_proxy.close()
            self.mgmt_proxy.connect()
        except rift.auto.proxy.ProxyRequestError as e:
            logger.warning("ProxyRequestError: %s", str(e))
            self.mgmt_proxy.close()
            self.mgmt_proxy.connect()
        except ComponentInfoError as e:
            logger.warning("ComponentInfoError: %s", str(e))
            self.mgmt_proxy.close()
            self.mgmt_proxy.connect()

        return False

    def wait_until_system_started(self, timeout_secs=SYSTEM_STARTUP_TIMEOUT_SECS):
        """ Wait until all components are RUNNING

        Arguments:
            timeout_secs - Number of seconds until a SystemStartError is raised
                           indicating a timeout.

        Raises:
            SystemStartError - If a component enters a state other than STARTING or
                               we timeout waiting for all components to start.
        """
        start_time = time.time()
        while True:
            if self.is_system_ready():
                return

            time_elapsed = time.time() - start_time
            time_remaining = timeout_secs - time_elapsed
            if time_remaining <= 0:
                break

            time.sleep(min(time_remaining, 10))

        unstarted_components = self.get_unstarted_components()
        unstarted_names = [comp.instance_name for comp in unstarted_components]

        raise SystemStartError("Timed out (%s seconds) waiting for components to "
                "enter RUNNING state: %s", timeout_secs, unstarted_names)


def wait_until_system_started(mgmt_proxy, timeout_secs=SYSTEM_STARTUP_TIMEOUT_SECS):
    """ Convenience wrapper to wait until system has started

    Arguments:
        mgmt_proxy - A MgmtProxy instance
    """
    VcsComponentInfo(mgmt_proxy).wait_until_system_started(timeout_secs=timeout_secs)


class VcsResource(object):
    def __init__(self, mgmt_proxy):
        """This class provides an interface to vcs resource through a
        management proxy
        """
        self._mgmt_proxy = mgmt_proxy

    @property
    def mgmt_proxy(self):
        """The MgmtProxy instance"""
        return self._mgmt_proxy

    def get_vcs_resource(self):
        """Get the vcs resource container object

        Returns:
            A proto-GI object (rw-base.yang)
        """

        vcs_resource = RwBaseYang.VcsResource()
        response_xml = self.mgmt_proxy.get_from_xpath('/vcs')
        vcs_resource.from_xml(yang_model.yang_model, response_xml)

        return vcs_resource


class VcsComponent(object):
    def __init__(self, mgmt_proxy):
        self._mgmt_proxy = mgmt_proxy

    @property
    def mgmt_proxy(self):
        """The MgmtProxy instance"""
        return self._mgmt_proxy

    def configure_tasklet(self, component_name, plugin_directory,
                          plugin_name, plugin_version):
        """Publish tasklet configuration

        Arguments:
            component_name      - Name to give the vcs component being configured
            plugin_directory    - Plugin directory containing the tasklet
            plugin_name         - Name of the tasklet plugin
            plugin_version      - Version of the tasklet plugin
        """

        #vcs_component = RwManifestYang.VcsComponent()
        vcs_component = RwManifestYang.Inventory_Component()
        vcs_component.component_name = component_name
        vcs_component.component_type = "RWTASKLET"
        vcs_component.rwtasklet.plugin_directory = plugin_directory
        vcs_component.rwtasklet.plugin_name = plugin_name
        vcs_component.rwtasklet.plugin_version = plugin_version

        self.mgmt_proxy.merge_config(vcs_component.to_xml(yang_model.yang_model))

    def start(self, component_name, parent_instance):
        """Start a vcs component

        Arguments:
            component_name  - Name of a configured vcs component
            parent_instance - Start component under this instance

        Returns:
            A proto-GI object (rw-vcs.yang)
        """

        vstart_input = RwVcsYang.VStartInput()
        vstart_input.component_name = component_name
        vstart_input.parent_instance = parent_instance

        response_xml = self.mgmt_proxy.rpc(vstart_input.to_xml(yang_model.yang_model))
        response_xml = self.mgmt_proxy.get_GI_compatible_xml(response_xml, rpc_name='vstart')

        vstart_output = RwVcsYang.VStartOutput()
        vstart_output.from_xml(yang_model.yang_model, response_xml)

        return vstart_output

    def stop(self, instance_name):
        """Stop a vcs component

        Arguments:
            instance_name   - Instance name of a started vcs component

        Returns:
            A proto-GI object (rw-vcs.yang)
        """

        vstop_input = RwVcsYang.VStopInput()
        vstop_input.instance_name = instance_name

        response_xml = self.mgmt_proxy.rpc(vstop_input.to_xml(yang_model.yang_model))
        response_xml = self.mgmt_proxy.get_GI_compatible_xml(response_xml, rpc_name='vstop')

        vstop_output = RwVcsYang.VStopOutput()
        vstop_output.from_xml(yang_model.yang_model, response_xml)

        return vstop_output


def default_agent_mode():
    """Determine the default management agent mode."""

    rift_install = os.environ['RIFT_INSTALL']
    confd_conf = os.path.join(rift_install, "etc/rw_confd_prototype.conf")

    if os.path.exists(confd_conf):
        return RwmgmtAgentMode.CONFD
    else:
        return RwmgmtAgentMode.RWXML

