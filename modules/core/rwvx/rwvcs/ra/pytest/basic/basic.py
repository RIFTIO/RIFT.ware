#!/usr/bin/env python3
"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file basic_test.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 06/19/2015
@brief basic test cases
"""
import time

import rift.auto.session
import rift.vcs.vcs

import gi
gi.require_version('RwManifestYang', '1.0')

from gi.repository import RwManifestYang

def test_show_tasklet_info(mgmt_session):
    """Tests retrieving tasklet info"""
    vcs_component_info = rift.vcs.vcs.VcsComponentInfo(mgmt_session)
    components = None

    # Get tasklet info
    components = vcs_component_info.get_components()
    component_names = [component.component_name for component in components]

    # Verify running components
    verify_running = ["vm-launchpad-1"]

    for component_name in verify_running:
        if component_name not in component_names:
            raise AssertionError("{} not found in vcs component info"
                                 .format(component_name))


def test_show_vcs_resources(mgmt_session):
    """Tests retrieving vcs resources"""
    vcs_resource = rift.vcs.vcs.VcsResource(mgmt_session)
    vcs_resource_info = None

    # Get vcs resources
    vcs_resource_info = vcs_resource.get_vcs_resource()

    # Verify there are VM entries in the vcs resource info container
    vms = [vm for vm in vcs_resource_info.vm]
    if len(vms) == 0:
        raise AssertionError("No entries found in vcs resource info")


def test_start_stop_tasklet(mgmt_session):
    """Tests configuring and controling a tasklet"""
    vcs_component = rift.vcs.vcs.VcsComponent(mgmt_session)
    vcs_component_info = rift.vcs.vcs.VcsComponentInfo(mgmt_session)
    tasklet = None

    # Configure pytoytasklet
    vcs_component.configure_tasklet("pytoytasklet", "pytoytasklet", "pytoytasklet", "1")

    # VStart tasklet
    tasklet = vcs_component.start("pytoytasklet", "vm-launchpad-1-2")

    # Verify tasklet running
    components = vcs_component_info.get_components()
    if "pytoytasklet" not in [component.component_name for component in components]:
        raise AssertionError("pytoytasklet not found in running components after startup")

    # VStop tasklet
    vcs_component.stop(tasklet.instance_name)

    # Give tasklet some time to stop
    time.sleep(2)

    # Verify tasklet stopped
    components = vcs_component_info.get_components()
    if "pytoytasklet" in [component.component_name for component in components]:
        raise AssertionError("pytoytasklet found in running components after shutdown")

    # Remove pytoytasklet configuration
    mgmt_session.proxy(RwManifestYang).delete_config(
            "/manifest/inventory/component[component-name='pytoytasklet']")

def main():

    mgmt_session = rift.auto.session.NetconfSession("127.0.0.1")
    mgmt_session.connect()

    test_start_stop_tasklet(mgmt_session)

if __name__ == "__main__":
    main()
