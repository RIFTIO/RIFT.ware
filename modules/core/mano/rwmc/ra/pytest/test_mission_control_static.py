#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_launchpad_startstop.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 06/19/2015
@brief System test of basic mission control functionality
"""

import pytest

import gi
gi.require_version('RwMcYang', '1.0')

from gi.repository import RwMcYang

@pytest.fixture(scope='module')
def proxy(request, mgmt_session):
    '''fixture which returns a proxy to RwMcYang

    Arguments:
        request       - pytest fixture request
        mgmt_session  - mgmt_session fixture - instance of a rift.auto.session class
    '''
    return mgmt_session.proxy(RwMcYang)

@pytest.mark.setup('launchpad')
@pytest.mark.incremental
class TestMissionControlSetup:
    def test_create_cloud_account(self, proxy, cloud_account):
        '''Configure a cloud account'''
        proxy.create_config('/cloud-account/account', cloud_account)

    def test_create_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Configure mgmt domain'''
        domain_config = RwMcYang.MgmtDomain(
                name=mgmt_domain_name)
        proxy.create_config('/mgmt-domain/domain', domain_config)

    def test_create_vm_pool(self, proxy, cloud_account_name, vm_pool_name):
        '''Configure vm pool

        Asserts :
            Newly configured vm pool has no resources assigned to it
        '''
        pool_config = RwMcYang.VmPool(
                name=vm_pool_name,
                cloud_account=cloud_account_name)
        proxy.create_config('/vm-pool/pool', pool_config)

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert assigned_ids == [] # pool contained resources before any were assigned

    def test_assign_vm_resource_to_vm_pool(self, proxy, cloud_account_name, vm_pool_name):
        '''Configure a vm resource by adding it to a vm pool

        Asserts:
            Cloud account has available resources
            VM pool has has available resources
            Cloud account and vm pool agree on available resources
            Configured resource is reflected as assigned in operational data post assignment
        '''
        account = proxy.get("/cloud-account/account[name='%s']" % cloud_account_name)
        cloud_vm_ids = [vm.id for vm in account.resources.vm]
        assert cloud_vm_ids != []

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        available_ids = [vm.id for vm in pool.available]
        # NOTE: Desired API - request for a list of leaf elements
        # available_ids = proxy.get("/vm-pool/pool[name='%s']/available/id" % vm_pool_name)
        assert available_ids != [] # Assert pool has available resources
        assert set(cloud_vm_ids).difference(set(available_ids)) == set([]) # Assert not split brain

        pool_config = RwMcYang.VmPool.from_dict({
                'name':vm_pool_name,
                'cloud_account':cloud_account_name,
                'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/vm-pool/pool[name='%s']" % vm_pool_name, pool_config)

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        print(available_ids[0], assigned_ids)
        assert available_ids[0] in assigned_ids # Configured resource shows as assigned

    def test_create_network_pool(self, proxy, cloud_account_name, network_pool_name):
        '''Configure network pool

        Asserts :
            Newly configured network pool has no resources assigned to it
        '''
        pool_config = RwMcYang.NetworkPool(
                name=network_pool_name,
                cloud_account=cloud_account_name)
        proxy.create_config('/network-pool/pool', pool_config)

        pool = proxy.get("/network-pool/pool[name='%s']" % network_pool_name)
        assigned_ids = [network.id for network in pool.assigned]
        assert assigned_ids == [] # pool contained resources before any were assigned

    def test_assign_network_resource_to_network_pool(self, proxy, cloud_account_name, network_pool_name):
        '''Configure a network resource by adding it to a network pool

        Asserts:
            Cloud account has available resources
            Network pool has has available resources
            Cloud account and network pool agree on available resources
            Configured resource is reflected as assigned in operational data post assignment
        '''
        account = proxy.get("/cloud-account/account[name='%s']" % cloud_account_name)
        cloud_network_ids = [network.id for network in account.resources.network]
        assert cloud_network_ids != []

        pool = proxy.get("/network-pool/pool[name='%s']" % network_pool_name)
        available_ids = [network.id for network in pool.available]
        assert available_ids != [] # Assert pool has available resources
        assert set(cloud_network_ids).difference(set(available_ids)) == set([]) # Assert not split brain

        pool_config = RwMcYang.NetworkPool.from_dict({
                'name':network_pool_name,
                'cloud_account':cloud_account_name,
                'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/network-pool/pool[name='%s']" % network_pool_name, pool_config)

        pool = proxy.get("/network-pool/pool[name='%s']" % network_pool_name)
        assigned_ids = [network.id for network in pool.assigned]
        assert available_ids[0] in assigned_ids # Configured resource shows as assigned

    def test_assign_network_pool_to_mgmt_domain(self, proxy, mgmt_domain_name, network_pool_name):
        '''Configure mgmt_domain by adding a network pool to it
        '''
        pool_config = RwMcYang.MgmtDomainPools_Network(name=network_pool_name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/network" % mgmt_domain_name, pool_config)

    def test_create_port_pool(self, proxy, cloud_account_name, port_pool_name):
        '''Configure port pool

        Asserts :
            Newly configured port pool has no resources assigned to it
        '''
        pool_config = RwMcYang.PortPool(
                name=port_pool_name,
                cloud_account=cloud_account_name)
        proxy.create_config('/port-pool/pool', pool_config)

        pool = proxy.get("/port-pool/pool[name='%s']" % port_pool_name)
        assigned_ids = [port.id for port in pool.assigned]
        assert assigned_ids == [] # pool contained resources before any were assigned

    def test_assign_port_resource_to_port_pool(self, proxy, cloud_account_name, port_pool_name):
        '''Configure a port resource by adding it to a port pool

        Asserts:
            Cloud account has available resources
            Port pool has has available resources
            Cloud account and port pool agree on available resources
            Configured resource is reflected as assigned in operational data post assignment
        '''
        account = proxy.get("/cloud-account/account[name='%s']" % cloud_account_name)
        cloud_port_ids = [port.id for port in account.resources.port]
        assert cloud_port_ids != []

        pool = proxy.get("/port-pool/pool[name='%s']" % port_pool_name)
        available_ids = [port.id for port in pool.available]
        assert available_ids != [] # Assert pool has available resources
        assert set(cloud_port_ids).difference(set(available_ids)) == set([]) # Assert not split brain

        pool_config = RwMcYang.PortPool.from_dict({
                'name':port_pool_name,
                'cloud_account':cloud_account_name,
                'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/port-pool/pool[name='%s']" % port_pool_name, pool_config)

        pool = proxy.get("/port-pool/pool[name='%s']" % port_pool_name)
        assigned_ids = [port.id for port in pool.assigned]
        assert available_ids[0] in assigned_ids # Configured resource shows as assigned


    def test_assign_port_pool_to_mgmt_domain(self, proxy, mgmt_domain_name, port_pool_name):
        '''Configure mgmt_domain by adding a port pool to it
        '''
        pool_config = RwMcYang.MgmtDomainPools_Port(name=port_pool_name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/port" % mgmt_domain_name, pool_config)

    def test_assign_vm_pool_to_mgmt_domain(self, proxy, mgmt_domain_name, vm_pool_name):
        '''Configure mgmt_domain by adding a VM pool to it
        '''
        pool_config = RwMcYang.MgmtDomainPools_Vm(name=vm_pool_name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/vm" % mgmt_domain_name, pool_config)

    def test_wait_for_launchpad_started(self, proxy, mgmt_domain_name):
        '''Wait for the launchpad to start
        
        Additionally begins the launchpad scraper.

        Asserts:
            Launchpad reaches state 'started'
        '''
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=200,
                fail_on=['crashed'])


@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestMissionControl:

    def test_stop_launchpad(self, proxy, mgmt_domain_name):
        '''Invoke stop launchpad RPC

        Asserts:
            Launchpad begins test in state 'started'
            Launchpad finishes test in state 'stopped'
        '''

        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=10,
                fail_on=['crashed'])
        stop_launchpad_input = RwMcYang.StopLaunchpadInput(mgmt_domain=mgmt_domain_name)
        stop_launchpad_output = proxy.rpc(stop_launchpad_input)
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'stopped',
                timeout=60,
                fail_on=['crashed'])

    def test_start_launchpad(self, proxy, mgmt_domain_name, launchpad_scraper):
        '''Invoke start launchpad RPC

        Asserts:
            Launchpad begins test in state 'stopped'
            Launchpad finishes test in state 'started'
        '''
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'stopped',
                timeout=10,
                fail_on=['crashed'])
        start_launchpad_input = RwMcYang.StartLaunchpadInput(mgmt_domain=mgmt_domain_name)
        start_launchpad_output = proxy.rpc(start_launchpad_input)
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=200,
                fail_on=['crashed'])
        launchpad_scraper.reset()

    def test_stop_launchpad_redux(self, proxy, mgmt_domain_name):
        '''Invoke stop launchpad RPC... Again...

        Asserts:
            Launchpad begins test in state 'started'
            Launchpad finishes test in state 'stopped'
        '''
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=10,
                fail_on=['crashed'])
        stop_launchpad_input = RwMcYang.StopLaunchpadInput(mgmt_domain=mgmt_domain_name)
        stop_launchpad_output = proxy.rpc(stop_launchpad_input)
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'stopped',
                timeout=60,
                fail_on=['crashed'])


@pytest.mark.teardown('launchpad')
@pytest.mark.incremental
class TestMissionControlTeardown:
    def test_remove_vm_pool_from_mgmt_domain(self, proxy, mgmt_domain_name, vm_pool_name):
        '''Unconfigure mgmt domain: remove a vm pool'''
        xpath = "/mgmt-domain/domain[name='%s']/pools/vm[name='%s']" % (mgmt_domain_name, vm_pool_name)
        proxy.delete_config(xpath)

    def test_remove_network_pool_from_mgmt_domain(self, proxy, mgmt_domain_name, network_pool_name):
        '''Unconfigure mgmt_domain: remove a network pool'''
        xpath = "/mgmt-domain/domain[name='%s']/pools/network[name='%s']" % (mgmt_domain_name, network_pool_name)
        proxy.delete_config(xpath)

    def test_remove_port_pool_from_mgmt_domain(self, proxy, mgmt_domain_name, port_pool_name):
        '''Unconfigure mgmt_domain: remove a port pool'''
        xpath = "/mgmt-domain/domain[name='%s']/pools/port[name='%s']" % (mgmt_domain_name, port_pool_name)
        proxy.delete_config(xpath)

    def test_delete_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Unconfigure mgmt_domain: delete mgmt_domain'''
        xpath = "/mgmt-domain/domain[name='%s']" % mgmt_domain_name
        proxy.delete_config(xpath)

    def test_remove_vm_resource_from_vm_pool(self, proxy, vm_pool_name):
        '''Unconfigure vm_pool: remove a vm resource

        Asserts:
            Resource is no longer assigned after being unconfigured
        '''
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert assigned_ids != [] # Assert resource is still assigned

        for assigned_id in assigned_ids:
            xpath = "/vm-pool/pool[name='%s']/assigned[id='%s']" % (vm_pool_name, assigned_id)
            proxy.delete_config(xpath)

    def test_remove_network_resource_from_network_pool(self, proxy, network_pool_name):
        '''Unconfigure network_pool: remove a network resource

        Asserts:
            Resource is no longer assigned after being unconfigured
        '''
        pool = proxy.get("/network-pool/pool[name='%s']" % network_pool_name)
        assigned_ids = [network.id for network in pool.assigned]
        assert assigned_ids != [] # Assert resource is still assigned

        for assigned_id in assigned_ids:
            xpath = "/network-pool/pool[name='%s']/assigned[id='%s']" % (network_pool_name, assigned_id)
            proxy.delete_config(xpath)

    def test_remove_port_resource_from_port_pool(self, proxy, port_pool_name):
        '''Unconfigure port_pool: remove a port resource

        Asserts:
            Resource is no longer assigned after being unconfigured
        '''
        pool = proxy.get("/port-pool/pool[name='%s']" % port_pool_name)
        assigned_ids = [port.id for port in pool.assigned]
        assert assigned_ids != [] # Assert resource is still assigned

        for assigned_id in assigned_ids:
            xpath = "/port-pool/pool[name='%s']/assigned[id='%s']" % (port_pool_name, assigned_id)
            proxy.delete_config(xpath)

    def test_delete_vm_pool(self, proxy, vm_pool_name):
        '''Unconfigure vm_pool'''
        xpath = "/vm-pool/pool[name='%s']" % vm_pool_name
        proxy.delete_config(xpath)

    def test_delete_network_pool(self, proxy, network_pool_name):
        '''Unconfigure network pool'''
        xpath = "/network-pool/pool[name='%s']" % network_pool_name
        proxy.delete_config(xpath)

    def test_delete_port_pool(self, proxy, port_pool_name):
        '''Unconfigure port_pool'''
        xpath = "/port-pool/pool[name='%s']" % port_pool_name
        proxy.delete_config(xpath)

    def test_delete_cloud_account(self, proxy, cloud_account_name):
        '''Unconfigure cloud_account'''
        xpath = "/cloud-account/account[name='%s']" % cloud_account_name
        proxy.delete_config(xpath)

    def test_create_odl_sdn_account(self, proxy, sdn_account_name, sdn_account_type):
        '''Configure sdn account'''
        sdn_account = RwMcYang.SDNAccount(
                name=sdn_account_name,
                account_type=sdn_account_type)
        xpath = "/sdn/account[name='%s']" % sdn_account_name
        proxy.create_config(xpath, sdn_account)

    def test_show_odl_sdn_account(self, proxy, sdn_account_name, sdn_account_type):
        '''Showing sdn account configuration

        Asserts:
            sdn_account.account_type is what was configured
        '''
        xpath = "/sdn/account[name='%s']" % sdn_account_name
        sdn_account = proxy.get_config(xpath)
        assert sdn_account.account_type == sdn_account_type

    def test_delete_odl_sdn_account(self, proxy, sdn_account_name):
        '''Unconfigure sdn account'''
        xpath = "/sdn/account[name='%s']" % sdn_account_name
        proxy.delete_config(xpath)
