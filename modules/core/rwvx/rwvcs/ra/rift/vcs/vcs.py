"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file vcs.py
# @date 10/14/2015
# @brief VCS Management Helpers
# @details This file contains functions that simplify using Protobuf to
# interact with external VCS interfaces.
#
"""
import logging
import time

import rift.auto.session
import gi
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwVcsYang', '1.0')

from gi.repository import (
        RwBaseYang,
        RwManifestYang,
        RwVcsYang,
        )

SYSTEM_STARTUP_TIMEOUT_SECS = 10*60

logger = logging.getLogger(__name__)

class SystemStartError(Exception):
    pass

class ComponentInfoError(Exception):
    pass

class VcsResourceError(Exception):
    pass

class VcsComponentInfo(object):
    """ This class deals with the """
    def __init__(self, session):
        self._mgmt_proxy = session.proxy(RwBaseYang)

    @property
    def mgmt_proxy(self):
        """ The MgmtProxy instance """
        return self._mgmt_proxy

    def get_components(self):
        """ Get the list of component_info objects

        Returns:
            A list of ComponentInfo proto-GI objects (rw-base.yang)
        """

        vcs_info = self.mgmt_proxy.get('/vcs/info')

        components = vcs_info.components.component_info
        #if not components:
            #raise ComponentInfoError("No components returned from VcsInfo request.")

        return components

    def get_unstarted_components(self, components=None):
        """ Get the list of component_info objects which are not in RUNNING state

        Arguments:
            components - A list of components to check for running state (optional)

        Returns:
            A list of ComponentInfo proto-GI objects (rw-base.yang)
        """

        if not components:
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
        components = self.get_components()
        if not components:
            logger.info("Component list not yet populated")
            return False

        unstarted_components = self.get_unstarted_components(components)
        if not unstarted_components:
            logger.info("All components have entered RUNNING state")
            return True

        logger.info("All components have NOT YET entered RUNNING state")
        for component in unstarted_components:
            if not component.state in ["STARTING", "INITIALIZING", "RUNNING", "RECOVERING", "TO_RECOVER"]:
                raise SystemStartError("Component (%s) entered a invalid startup state: %s",
                        component.instance_name, component.state)

            logger.info("Component (%s) state is %s",
                    component.instance_name, component.state)

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
            try:
                if self.is_system_ready():
                    return
            except rift.auto.session.ProxyConnectionError as e:
                logger.info("ProxyConnectionError: {}".format(e))
            except rift.auto.session.ProxyRequestTimeout as e:
                logger.info("ProxyRequestTimeout: {}".format(e))
            except rift.auto.session.ProxyRequestError as e:
                logger.info("ProxyRequestError: {}".format(e))
            except ComponentInfoError as e:
                logger.info("ComponentInfoError: {}".format(e))
            except ConnectionRefusedError as e:
                logger.info("ConnectionRefusedError: {}".format(e))

            time_elapsed = time.time() - start_time
            time_remaining = timeout_secs - time_elapsed
            if time_remaining <= 0:
                break

            time.sleep(min(time_remaining, 10))

        unstarted_components = self.get_unstarted_components()
        unstarted_names = [comp.instance_name for comp in unstarted_components]

        raise SystemStartError("Timed out (%s seconds) waiting for components to "
                "enter RUNNING state: %s", timeout_secs, unstarted_names)


def wait_until_system_started(session, quiet=False, timeout_secs=SYSTEM_STARTUP_TIMEOUT_SECS):
    """ Convenience wrapper to wait until system has started

    Arguments:
        session - A session instance
    """
    if quiet:
        logger.setLevel(logging.ERROR)

    VcsComponentInfo(session).wait_until_system_started(timeout_secs=timeout_secs)


class VcsResource(object):
    def __init__(self, session):
        """This class provides an interface to vcs resource through a
        management proxy
        """
        self._mgmt_proxy = session.proxy(RwBaseYang)

    @property
    def mgmt_proxy(self):
        """The MgmtProxy instance"""
        return self._mgmt_proxy

    def get_vcs_resource(self):
        """Get the vcs resource container object

        Returns:
            A proto-GI object (rw-base.yang)
        """

        return self.mgmt_proxy.get('/vcs/resource/utilization')


class VcsComponent(object):
    def __init__(self, session):
        self._mgmt_proxy = session.proxy(RwVcsYang)

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

        vcs_component = RwManifestYang.Inventory.from_dict({
            "component": [{
                "component_name": component_name,
                "component_type": "RWTASKLET",
                "rwtasklet": {
                    "plugin_directory": plugin_directory,
                    "plugin_name": plugin_name,
                    "plugin_version": plugin_version,
                    },
                }]
            })

        self.mgmt_proxy.create_config('/manifest/inventory/component', vcs_component)

    def start(self, component_name, parent_instance):
        """Start a vcs component

        Arguments:
            component_name  - Name of a configured vcs component
            parent_instance - Start component under this instance

        Returns:
            A proto-GI object (rw-vcs.yang)
        """

        vstart_input = RwVcsYang.VStartInput.from_dict({
            "component_name": component_name,
            "parent_instance": parent_instance,
            })
        return self.mgmt_proxy.rpc(vstart_input)


    def stop(self, instance_name):
        """Stop a vcs component

        Arguments:
            instance_name   - Instance name of a started vcs component

        Returns:
            A proto-GI object (rw-vcs.yang)
        """

        vstop_input = RwVcsYang.VStopInput.from_dict({"instance_name": instance_name })
        return self.mgmt_proxy.rpc(vstop_input)
