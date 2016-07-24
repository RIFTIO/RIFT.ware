#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_mission_control_delete.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 06/19/2015
@brief System test exercising delete of mission control configuration
"""

import pytest
import rift.auto.proxy
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

@pytest.fixture(scope='module')
def mgmt_domain(mgmt_domain_name):
    mgmt_domain = RwMcYang.MgmtDomain(name=mgmt_domain_name)
    return mgmt_domain

@pytest.fixture(scope='module')
def vm_pool(vm_pool_name, cloud_account_name):
    vm_pool = RwMcYang.VmPool(
            name=vm_pool_name,
            cloud_account=cloud_account_name,
            dynamic_scaling=True,
    )
    return vm_pool

@pytest.fixture(scope='module')
def network_pool(network_pool_name, cloud_account_name):
    network_pool = RwMcYang.NetworkPool(
            name=network_pool_name,
            cloud_account=cloud_account_name,
            dynamic_scaling=True,
    )
    return network_pool

@pytest.fixture(scope='module')
def sdn_account(sdn_account_name, sdn_account_type):
    sdn_account = RwMcYang.SDNAccount(
            name=sdn_account_name,
            account_type=sdn_account_type,
    )
    return sdn_account

@pytest.fixture(scope='function', autouse=True)
def launchpad_setup(request, proxy, cloud_account, mgmt_domain, vm_pool, network_pool, sdn_account):
    def _teardown():
        launchpad_state = proxy.get("/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain.name)
        if launchpad_state:
            if launchpad_state in ['configuring', 'starting']:
                launchpad_state = proxy.wait_for(
                        "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain.name,
                        'started',
                        timeout=200,
                        fail_on=['crashed'])

            if launchpad_state == 'started':
                stop_launchpad_input = RwMcYang.StopLaunchpadInput(mgmt_domain=mgmt_domain.name)
                stop_launchpad_output = proxy.rpc(stop_launchpad_input)
                launchpad_state = proxy.wait_for(
                        "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain.name,
                        'stopped',
                        timeout=200,
                        fail_on=['crashed'])

        if proxy.get_config("/mgmt-domain/domain[name='%s']/pools/vm[name='%s']" % (mgmt_domain.name, vm_pool.name)):
            proxy.delete_config("/mgmt-domain/domain[name='%s']/pools/vm[name='%s']" % (mgmt_domain.name, vm_pool.name))

        if proxy.get_config("/mgmt-domain/domain[name='%s']/pools/network[name='%s']" % (mgmt_domain.name, network_pool.name)):
            proxy.delete_config("/mgmt-domain/domain[name='%s']/pools/network[name='%s']" % (mgmt_domain.name, network_pool.name))

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool.name)
        if pool:
            for vm_id in [vm.id for vm in pool.assigned]:
                proxy.delete_config("/vm-pool/pool[name='%s']/assigned[id='%s']" % (vm_pool.name, vm_id))

        if proxy.get_config("/vm-pool/pool[name='%s']" % vm_pool.name):
            proxy.delete_config("/vm-pool/pool[name='%s']" % vm_pool.name)

        if proxy.get_config("/network-pool/pool[name='%s']" % network_pool.name):
            proxy.delete_config("/network-pool/pool[name='%s']" % network_pool.name)

        if proxy.get_config("/mgmt-domain/domain[name='%s']" % mgmt_domain.name):
            proxy.delete_config("/mgmt-domain/domain[name='%s']" % mgmt_domain.name)

        if proxy.get_config("/cloud-account/account[name='%s']" % cloud_account.name):
            proxy.delete_config("/cloud-account/account[name='%s']" % cloud_account.name)

        if proxy.get_config("/sdn/account[name='%s']" % sdn_account.name):
            proxy.delete_config("/sdn/account[name='%s']" % sdn_account.name)

    def _setup():
        proxy.create_config('/cloud-account/account', cloud_account)
        proxy.create_config('/mgmt-domain/domain', mgmt_domain)
        proxy.create_config('/vm-pool/pool', vm_pool)
        proxy.create_config('/network-pool/pool', network_pool)
        proxy.create_config('/sdn/account', sdn_account)
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool.name)
        available_ids = [vm.id for vm in pool.available]
        pool_config = RwMcYang.VmPool.from_dict({
                'name':vm_pool.name,
                'cloud_account':cloud_account.name,
                'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/vm-pool/pool[name='%s']" % vm_pool.name, pool_config)

        mgmt_vm_pool = RwMcYang.MgmtDomainPools_Vm(name=vm_pool.name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/vm" % mgmt_domain.name, mgmt_vm_pool)

        mgmt_network_pool = RwMcYang.MgmtDomainPools_Network(name=network_pool.name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/network" % mgmt_domain.name, mgmt_network_pool)

    # Teardown any existing launchpad configuration, and set it back up again
    _teardown()
    _setup()
   


class DeleteResources:
    def test_remove_vm_pool_from_mgmt_domain(self, proxy, mgmt_domain, vm_pool):
        '''Unconfigure mgmt domain: remove a vm pool'''
        # Can't remove vm pool without removing resources first
#        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool.name)
#        if pool:
#            for vm_id in [vm.id for vm in pool.assigned]:
#                proxy.delete_config("/vm-pool/pool[name='%s']/assigned[id='%s']" % (vm_pool.name, vm_id))

        xpath = "/mgmt-domain/domain[name='%s']/pools/vm[name='%s']" % (mgmt_domain.name, vm_pool.name)
        proxy.delete_config(xpath)

    def test_remove_network_pool_from_mgmt_domain(self, proxy, mgmt_domain, network_pool):
        '''Unconfigure mgmt_domain: remove a network pool'''
        xpath = "/mgmt-domain/domain[name='%s']/pools/network[name='%s']" % (mgmt_domain.name, network_pool.name)
        proxy.delete_config(xpath)

    def test_delete_mgmt_domain(self, proxy, vm_pool, mgmt_domain):
        '''Unconfigure mgmt_domain: delete mgmt_domain'''
        # Can't remove vm pool without removing resources first
#        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool.name)
#        if pool:
#            for vm_id in [vm.id for vm in pool.assigned]:
#                proxy.delete_config("/vm-pool/pool[name='%s']/assigned[id='%s']" % (vm_pool.name, vm_id))

        xpath = "/mgmt-domain/domain[name='%s']" % mgmt_domain.name
        proxy.delete_config(xpath)

    def test_remove_vm_resource_from_vm_pool(self, proxy, vm_pool):
        '''Unconfigure vm_pool: remove a vm resource

        Asserts:
            Resource is no longer assigned after being unconfigured
        '''
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool.name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert assigned_ids != [] # Assert resource is still assigned

        for assigned_id in assigned_ids:
            xpath = "/vm-pool/pool[name='%s']/assigned[id='%s']" % (vm_pool.name, assigned_id)
            proxy.delete_config(xpath)

    def test_delete_vm_pool(self, proxy, vm_pool):
        '''Unconfigure vm_pool'''
        # Can't delete vm pool without removing it from mgmt domain first
        with pytest.raises(rift.auto.proxy.ProxyRequestError) as excinfo:
            xpath = "/vm-pool/pool[name='%s']" % vm_pool.name
            proxy.delete_config(xpath)
        assert 'illegal reference' in str(excinfo.value)

    def test_delete_network_pool(self, proxy, network_pool):
        '''Unconfigure network pool'''
        # Can't delete network pool without removing it from mgmt domain first
        with pytest.raises(rift.auto.proxy.ProxyRequestError) as excinfo:
            xpath = "/network-pool/pool[name='%s']" % network_pool.name
            proxy.delete_config(xpath)
        assert 'illegal reference' in str(excinfo.value)

    def test_delete_cloud_account(self, proxy, cloud_account):
        '''Unconfigure cloud_account'''
        # Can't delete cloud account without first deleting all of the pools associated with it
        with pytest.raises(rift.auto.proxy.ProxyRequestError) as excinfo:
            xpath = "/cloud-account/account[name='%s']" % cloud_account.name
            proxy.delete_config(xpath)
        assert 'illegal reference' in str(excinfo.value)


    def test_delete_odl_sdn_account(self, proxy, sdn_account):
        '''Unconfigure sdn account'''
        xpath = "/sdn/account[name='%s']" % sdn_account.name
        proxy.delete_config(xpath)


class TestDeleteFromStartingLaunchpad(DeleteResources):
    pass

@pytest.mark.slow
class TestDeleteFromStoppedLaunchpad(DeleteResources):
    @pytest.fixture(scope='function', autouse=True)
    def launchpad_stopped(self, launchpad_setup, proxy, mgmt_domain):
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain.name,
                'started',
                timeout=200,
                fail_on=['crashed'])
        stop_launchpad_input = RwMcYang.StopLaunchpadInput(mgmt_domain=mgmt_domain.name)
        stop_launchpad_output = proxy.rpc(stop_launchpad_input)
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain.name,
                'stopped',
                timeout=200,
                fail_on=['crashed'])
