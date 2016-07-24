#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_mission_control_negative_mgmt_domain.py
@author RIFT.io
@date 12/4/2015
@brief System test of negative (failure) mission control functionality
"""

import pytest

import gi
gi.require_version('RwMcYang', '1.0')

from gi.repository import RwMcYang
from rift.auto.session import ProxyRequestError
from rift.auto.session import ProxyExpectTimeoutError


def start_launchpad(proxy, mgmt_domain_name):
    '''Invoke start launchpad RPC

    Arguments:
        mgmt_domain_name - the management domain name string

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
            timeout=120,
            fail_on=['crashed'])


@pytest.fixture(scope='module')
def proxy(request, mgmt_session):
    '''fixture which returns a proxy to RwMcYang

    Arguments:
        request       - pytest fixture request
        mgmt_session  - mgmt_session fixture - instance of a rift.auto.session
        class

    '''
    return mgmt_session.proxy(RwMcYang)


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestMgmtDomainNegativeSetup:
    '''Stand up object needed for the lifecycle of this test script '''

    def test_create_cloud_account(self, proxy, logger, cloud_account):
        '''Configure a cloud account

        This creates a cloud account to test other objects

        Arguments:
            proxy         - a pytest fixture proxy to RwMcYang
            logger        - a pytest fixture to an instance of Logger
            cloud_account - a pytest fixture to a cloud account object

        Asserts:
            None

        '''
        proxy.create_config('/cloud-account/account', cloud_account)

    def test_create_vm_pool(self, proxy, cloud_account, vm_pool_name):
        '''Configure vm pool

        Arguments:
            proxy              - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name
            vm_pool_name       - a pytest fixture for the VM pool name

        Asserts:
            A cloud account exists for the cloud_account_name
            Newly configured vm pool has no resources assigned to it

        '''
        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        cloud_account = proxy.get(xpath)
        assert cloud_account is not None

        pool_config = RwMcYang.VmPool(
                name=vm_pool_name,
                cloud_account=cloud_account.name,
                dynamic_scaling=True,
        )
        proxy.create_config('/vm-pool/pool', pool_config)
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert len(assigned_ids) == 0

    def test_create_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Configure a management domain

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name

        Asserts:
            None

        '''
        domain_config = RwMcYang.MgmtDomain(name=mgmt_domain_name)
        proxy.create_config('/mgmt-domain/domain', domain_config)


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestMgmtDomain:
    '''Test negative cases for the management domain'''

    #
    # Creation tests
    #

    def test_create_mgmt_domain_with_no_name(self, proxy):
        '''Test that a mgmt domain cannot be created if name is not present

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        properties = { }
        mgmt_domain = RwMcYang.MgmtDomain.from_dict(properties)
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/mgmt-domain/domain', mgmt_domain)

    def test_create_mgmt_domain_with_blank_name(self, proxy):
        '''Test that a management domain cannot be created if name is an empty
        string

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        properties = {
                'name': '',
        }
        mgmt_domain = RwMcYang.MgmtDomain.from_dict(properties)
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/mgmt-domain/domain', mgmt_domain)

    def test_create_mgmt_domain_with_null_name(self, proxy):
        '''Test that a management domain cannot be created if name is null

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang

        Asserts:
            TypeError is raised

        '''
        properties = {
                'name':None,
        }
        with pytest.raises(TypeError):
            mgmt_domain = RwMcYang.MgmtDomain.from_dict(properties)

    def test_create_mgmt_domain_with_duplicate_name(self, proxy, mgmt_domain_name):
        '''Test that a management domain cannot be created when a management
        domain with the same name already exists

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name

        Asserts:
            management domain exists for the mgmt_domain_name
            rift.auto.proxy.ProxyRequestError is raised

        '''
        mgmt_domain = proxy.get("/mgmt-domain/domain[name='%s']" % mgmt_domain_name)
        assert mgmt_domain is not None

        properties = {
                'name': mgmt_domain.name,
        }
        duplicate_mgmt_domain = RwMcYang.MgmtDomain.from_dict(properties)
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/mgmt-domain/domain', duplicate_mgmt_domain)

    #
    # Launchpad related tests
    #

    def test_verify_launchpad_not_started(self, proxy, mgmt_domain_name):
        '''Verifies that the launchpad is not started

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name

        Asserts:
            rift.auto.session.ProxyExpectTimeoutError is raised

        '''
        with pytest.raises(ProxyExpectTimeoutError):
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

    def test_start_launchpad_when_no_vm_pool_assigned(self, proxy, mgmt_domain_name):
        '''Verify that the launchpad cannot start when the management domain
        does not have a vm pool assigned

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name

        Asserts:
            rift.auto.session.ProxyExpectTimeoutError is raised

        '''
        with pytest.raises(ProxyExpectTimeoutError):
            proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=120,
                fail_on=['crashed'])

    def test_start_lp_with_empty_vm_pool(self, proxy, mgmt_domain_name, vm_pool_name):
        '''Tests that starting launchpad fails when vm pool does not have a vm
        Configure mgmt_domain by adding a VM pool to it

        Arguments:
            mgmt_domain_name - a pytest fixture for the management domain name
            vm_pool_name     - a pytest fixture for the vm pool name

        Asserts:
            rift.auto.session.ProxyExpectTimeoutError is raised

        '''
        with pytest.raises(ProxyExpectTimeoutError):
            pool_config = RwMcYang.MgmtDomainPools_Vm(name=vm_pool_name)
            proxy.create_config(
                "/mgmt-domain/domain[name='%s']/pools/vm" % mgmt_domain_name,
                pool_config,
            )
            proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=120,
                fail_on=['crashed'])

    def test_launchpad_starts_when_vm_pool_has_a_vm_resource(self, proxy,
            cloud_account, vm_pool_name, mgmt_domain_name, network_pool_name):
        '''Tests that a launchpad can now start when the vm pool has a vm
        resource

        Arguments:
            proxy              - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name
            vm_pool_name       - a pytest fixture for the VM pool name
            mgmt_domain_name   - a pytest fixture for the management domain name

        Asserts:
            Cloud account has available resources
            VM pool has available resources
            Cloud account and vm pool agree on available resources
            Configured resource is reflected as assigned in operational data
            post assignment
            Launchpad reaches state 'started'

        '''
        account = proxy.get("/cloud-account/account[name='%s']" % cloud_account.name)
        cloud_vm_ids = [vm.id for vm in account.resources.vm]
        assert len(cloud_vm_ids) >= 1

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        available_ids = [vm.id for vm in pool.available]
        assert len(available_ids) >= 1
        # Assert not split brain
        assert set(cloud_vm_ids).difference(set(available_ids)) == set([])

        pool_config = RwMcYang.VmPool.from_dict({
            'name':vm_pool_name,
            'cloud_account':cloud_account.name,
            'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/vm-pool/pool[name='%s']" % vm_pool_name, pool_config)

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert available_ids[0] in assigned_ids

        # Create NW pool
        pool_config = RwMcYang.NetworkPool(
                name=network_pool_name,
                cloud_account=cloud_account.name,
                dynamic_scaling=True,
        )
        proxy.create_config('/network-pool/pool', pool_config)
        pool_config = RwMcYang.MgmtDomainPools_Network(name=network_pool_name)
        proxy.create_config("/mgmt-domain/domain[name='%s']/pools/network" % mgmt_domain_name, pool_config)


        proxy.wait_for(
            "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
            'started',
            timeout=400,
            fail_on=['crashed'])

    def test_delete_mgmt_domain_with_running_launchpad(self, proxy, mgmt_domain_name):
        '''Test that a management domain cannot be deleted when the launchpad
        is running

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        xpath = "/mgmt-domain/domain[name='%s']" % mgmt_domain_name
        with pytest.raises(ProxyRequestError):
            proxy.delete_config(xpath)

    def test_stop_launchpad(self, proxy, mgmt_domain_name):
        '''Stop launchpad before we leave this class

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain

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


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestMgmtDomainNegativeTeardown:

    @pytest.mark.xfail(raises=ProxyExpectTimeoutError)
    def test_delete_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Test that deleting a management domain while a pool is attached will
        fail

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        xpath = "/mgmt-domain/domain[name='%s']" % mgmt_domain_name
        proxy.delete_config(xpath)

    def test_remove_vm_pool_from_mgmt_domain(self, proxy, mgmt_domain_name,
            vm_pool_name):
        '''Unconfigure mgmt domain: remove a vm pool

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name
            vm_pool_name     - a pytest fixture for the vm pool name

        Asserts:
        '''
        xpath = "/mgmt-domain/domain[name='%s']/pools/vm[name='%s']" % (
                mgmt_domain_name, vm_pool_name)
        proxy.delete_config(xpath)

    def test_delete_mgmt_domain(self, proxy, mgmt_domain_name):
        '''Unconfigure mgmt_domain: delete mgmt_domain

        Arguments:
            proxy            - a pytest fixture proxy to RwMcYang
            mgmt_domain_name - a pytest fixture for the management domain name

        Asserts:
            None

        '''
        xpath = "/mgmt-domain/domain[name='%s']" % mgmt_domain_name
        proxy.delete_config(xpath)

    def test_remove_vm_resource_from_vm_pool(self, proxy, vm_pool_name):
        '''Unconfigure vm_pool: remove a vm resource

            proxy        - a pytest fixture proxy to RwMcYang
            vm_pool_name - a pytest fixture for the VM pool name

        Asserts:
            Resource is no longer assigned after being unconfigured

        '''
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert len(assigned_ids) >= 1

        for assigned_id in assigned_ids:
            xpath = "/vm-pool/pool[name='%s']/assigned[id='%s']" % (
                    vm_pool_name, assigned_id)
            proxy.delete_config(xpath)

    def test_delete_vm_pool(self, proxy, vm_pool_name):
        '''Unconfigure vm_pool: Remove the primary vm pool

        Arguments:
            proxy        - a pytest fixture proxy to RwMcYang
            vm_pool_name - a pytest fixture for the VM pool name


        Asserts:
            None

        '''
        xpath = "/vm-pool/pool[name='%s']" % vm_pool_name
        proxy.delete_config(xpath)

    def test_delete_nw_pool(self, proxy, network_pool_name):
        '''Unconfigure vm_pool: Remove the primary vm pool

        Arguments:
            proxy        - a pytest fixture proxy to RwMcYang
            vm_pool_name - a pytest fixture for the VM pool name


        Asserts:
            None

        '''
        xpath = "/network-pool/pool[name='%s']" % network_pool_name
        proxy.delete_config(xpath)

    def test_delete_cloud_account(self, proxy, cloud_account):
        '''Unconfigure cloud_account

        Arguments:
            proxy              - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts:
            None

        '''
        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        proxy.delete_config(xpath)

