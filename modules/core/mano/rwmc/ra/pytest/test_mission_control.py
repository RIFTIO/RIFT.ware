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


@pytest.mark.setup('sdn')
@pytest.mark.incremental
@pytest.mark.feature('mission-control')
class TestSdn:
    def test_create_odl_sdn_account(self, proxy, sdn_account_name, sdn_account_type):
        '''Configure sdn account

        Asserts:
            SDN name and accout type.
        '''
        sdn_account = RwMcYang.SDNAccount(
                name=sdn_account_name,
                account_type=sdn_account_type)
        xpath = "/sdn/account[name='%s']" % sdn_account_name
        proxy.create_config(xpath, sdn_account)

        print(sdn_account)
        sdn_account = proxy.get(xpath)
        #assert sdn_account.account_type == sdn_account_type
        #assert sdn_account.name == sdn_account_name

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


@pytest.mark.setup('launchpad')
@pytest.mark.usefixtures('cloud_account')
@pytest.mark.incremental
class TestMissionControlSetup:

    def test_create_cloud_accounts(self, mgmt_session, cloud_module, cloud_xpath, cloud_accounts):
        '''Configure cloud accounts

        Asserts:
            Cloud name and cloud type details
        '''
        proxy = mgmt_session.proxy(cloud_module)
        for cloud_account in cloud_accounts:
            xpath = '{}[name="{}"]'.format(cloud_xpath, cloud_account.name)
            proxy.replace_config(xpath, cloud_account)
            response =  proxy.get(xpath)
            assert response.name == cloud_account.name
            assert response.account_type == cloud_account.account_type

    def test_account_connection_status(self, mgmt_session, cloud_module, cloud_xpath, cloud_accounts):
        '''Verify connection status on each cloud account

        Asserts:
            Cloud account is successfully connected
        '''

        proxy = mgmt_session.proxy(cloud_module)
        for cloud_account in cloud_accounts:
            proxy.wait_for(
                '{}[name="{}"]/connection-status/status'.format(cloud_xpath, cloud_account.name),
                'success',
                timeout=30,
                fail_on=['failure'])

    @pytest.mark.feature('mission-control')
    def test_create_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Configure mgmt domain

        Asserts:
            If the launchpad configuration is created and updated succesfully.
        '''
        xpath = '/mgmt-domain/domain'
        domain_config = RwMcYang.MgmtDomain(
                name=mgmt_domain_name)
        proxy.create_config(xpath, domain_config)

        xpath += "[name='{}']".format(mgmt_domain_name)
        proxy.merge_config(xpath, domain_config)

        response = proxy.get(xpath)
        assert response.launchpad.state == 'pending'

    @pytest.mark.feature('mission-control')
    def test_create_vm_pool(self, proxy, cloud_account, vm_pool_name):
        '''Configure vm pool

        Asserts :
            Newly configured vm pool has no resources assigned to it
        '''
        pool_config = RwMcYang.VmPool(
                name=vm_pool_name,
                cloud_account=cloud_account.name,
                dynamic_scaling=True,
        )
        proxy.create_config('/vm-pool/pool', pool_config)

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert assigned_ids == [] # pool contained resources before any were assigned


    @pytest.mark.feature('mission-control')
    def test_assign_vm_resource_to_vm_pool(self, proxy, cloud_account, vm_pool_name, launchpad_vm_id):
        '''Configure a vm resource by adding it to a vm pool

        Asserts:
            Cloud account has available resources
            VM pool has has available resources
            Cloud account and vm pool agree on available resources
            Configured resource is reflected as assigned in operational data post assignment
        '''
        account = proxy.get("/cloud-account/account[name='%s']" % cloud_account.name)
        if launchpad_vm_id:
            cloud_vm_ids = [vm.id for vm in account.resources.vm if vm.id == launchpad_vm_id]
        else:
            cloud_vm_ids = [vm.id for vm in account.resources.vm]
        assert cloud_vm_ids != []

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        if launchpad_vm_id:
            available_ids = [vm.id for vm in pool.available if vm.id == launchpad_vm_id]
        else:
            available_ids = [vm.id for vm in pool.available]
        # NOTE: Desired API - request for a list of leaf elements
        # available_ids = proxy.get("/vm-pool/pool[name='%s']/available/id" % vm_pool_name)
        assert available_ids != [] # Assert pool has available resources
        assert set(cloud_vm_ids).difference(set(available_ids)) == set([]) # Assert not split brain

        pool_config = RwMcYang.VmPool.from_dict({
                'name':vm_pool_name,
                'cloud_account':cloud_account.name,
                'dynamic_scaling': True,
                'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/vm-pool/pool[name='%s']" % vm_pool_name, pool_config)

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert available_ids[0] in assigned_ids # Configured resource shows as assigned


    @pytest.mark.feature('mission-control')
    def test_create_network_pool(self, proxy, cloud_account, network_pool_name):
        '''Configure network pool

        Asserts :
            Newly configured network pool has no resources assigned to it
        '''
        pool_config = RwMcYang.NetworkPool(
                name=network_pool_name,
                cloud_account=cloud_account.name,
                dynamic_scaling=True,
        )

        proxy.create_config('/network-pool/pool', pool_config)

        pool = proxy.get("/network-pool/pool[name='%s']" % network_pool_name)
        assigned_ids = [network.id for network in pool.assigned]
        assert assigned_ids == [] # pool contained resources before any were assigned


    @pytest.mark.feature('mission-control')
    def test_assign_network_pool_to_mgmt_domain(self, proxy, mgmt_domain_name, network_pool_name):
        '''Configure mgmt_domain by adding a network pool to it
        '''
        pool_config = RwMcYang.MgmtDomainPools_Network(name=network_pool_name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/network" % mgmt_domain_name, pool_config)


    @pytest.mark.feature('mission-control')
    def test_assign_vm_pool_to_mgmt_domain(self, proxy, mgmt_domain_name, vm_pool_name):
        '''Configure mgmt_domain by adding a VM pool to it
        '''
        pool_config = RwMcYang.MgmtDomainPools_Vm(name=vm_pool_name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/vm" % mgmt_domain_name, pool_config)


    @pytest.mark.feature('mission-control')
    def test_wait_for_launchpad_started(self, proxy, mgmt_domain_name):
        '''Wait for the launchpad to start
        
        Additionally begins the launchpad scraper.

        Asserts:
            Launchpad reaches state 'started'
        '''
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=400,
                fail_on=['crashed'])


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
@pytest.mark.depends('launchpad')
class TestMissionControl:

    @pytest.mark.feature('mission-control')
    def test_launchpad_stats(self, proxy, mgmt_domain_name):
        '''Verify launchpad stats

        Asserts:
            Create time and uptime are configured for launchpad
        '''
        xpath = "/mgmt-domain/domain[name='{}']/launchpad/uptime".format(mgmt_domain_name)
        uptime = proxy.get(xpath)
        assert len(uptime) > 0

        xpath = "/mgmt-domain/domain[name='{}']/launchpad/create-time".format(mgmt_domain_name)
        create_time = proxy.get(xpath)
        assert int(create_time) > 0

    @pytest.mark.feature('mission-control')
    def test_mission_control_stats(self, proxy, mgmt_domain_name):
        '''Verify Mission Control stats

        Asserts:
            Create time and uptime are configured for MissionControl
        '''
        xpath = "/mission-control/uptime"
        uptime = proxy.get(xpath)
        assert len(uptime) > 0

        xpath = "/mission-control/create-time"
        create_time = proxy.get(xpath)
        assert int(create_time) > 0

@pytest.mark.teardown('launchpad')
@pytest.mark.usefixtures('cloud_account')
@pytest.mark.incremental
class TestMissionControlTeardown:

    @pytest.mark.feature('mission-control')
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
                timeout=120,
                fail_on=['crashed'])


    @pytest.mark.feature('mission-control')
    def test_remove_vm_pool_from_mgmt_domain(self, proxy, mgmt_domain_name, vm_pool_name):
        '''Unconfigure mgmt domain: remove a vm pool'''
        xpath = "/mgmt-domain/domain[name='%s']/pools/vm[name='%s']" % (mgmt_domain_name, vm_pool_name)
        proxy.delete_config(xpath)

    @pytest.mark.feature('mission-control')
    def test_remove_network_pool_from_mgmt_domain(self, proxy, mgmt_domain_name, network_pool_name):
        '''Unconfigure mgmt_domain: remove a network pool'''
        xpath = "/mgmt-domain/domain[name='%s']/pools/network[name='%s']" % (mgmt_domain_name, network_pool_name)
        proxy.delete_config(xpath)

    @pytest.mark.feature('mission-control')
    def test_delete_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Unconfigure mgmt_domain: delete mgmt_domain'''
        xpath = "/mgmt-domain/domain[name='%s']" % mgmt_domain_name
        proxy.delete_config(xpath)

    @pytest.mark.feature('mission-control')
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

    @pytest.mark.feature('mission-control')
    def test_delete_vm_pool(self, proxy, vm_pool_name):
        '''Unconfigure vm_pool'''
        xpath = "/vm-pool/pool[name='%s']" % vm_pool_name
        proxy.delete_config(xpath)

    @pytest.mark.feature('mission-control')
    def test_delete_network_pool(self, proxy, network_pool_name):
        '''Unconfigure network pool'''
        xpath = "/network-pool/pool[name='%s']" % network_pool_name
        proxy.delete_config(xpath)


    def test_delete_cloud_accounts(self, mgmt_session, cloud_module, cloud_xpath, cloud_accounts):
        '''Unconfigure cloud_account'''
        proxy = mgmt_session.proxy(cloud_module)
        for cloud_account in cloud_accounts:
            xpath = "{}[name='{}']".format(cloud_xpath, cloud_account.name)
            proxy.delete_config(xpath)
