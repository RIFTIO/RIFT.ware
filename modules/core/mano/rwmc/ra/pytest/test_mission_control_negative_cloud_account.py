#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_mission_control_negative_cloud_account.py
@author
@date 12/04/2015
@brief System test of negative (failure) mission control functionality
"""

import pytest

import gi
gi.require_version('RwMcYang', '1.0')

from gi.repository import GLib, RwMcYang
from rift.auto.session import ProxyRequestError


@pytest.fixture(scope='module')
def proxy(request, mgmt_session):
    '''fixture which returns a proxy to RwMcYang

    Arguments:
        request       - pytest fixture request
        mgmt_session  - mgmt_session fixture - instance of a rift.auto.session
        class

    '''
    return mgmt_session.proxy(RwMcYang)


@pytest.fixture(scope='session')
def cloud_account_type(request, cloud_type):
    '''Workaround for the mixed labeled 'lxc' and 'cloudsim'

    Arguments:
        cloud_type - The cloud type supplied via pytest command line parameter

    '''
    if cloud_type == 'lxc':
        return 'cloudsim'
    else:
        return cloud_type


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestCloudAccount:
    '''Tests behaviors and properties common to all cloud account types'''

    #
    # Test cloud_name
    #

    def test_create_cloud_account_with_no_name(self, proxy, cloud_account_type):
        '''Test that a cloud account cannot be created if no name is provided

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_type - a pytest fixture for the cloud account type

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        properties = {
                'account_type': cloud_account_type,
        }

        cloud_account = RwMcYang.CloudAccount.from_dict(properties)
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/cloud-account/account', cloud_account)

    def test_create_cloud_account_with_empty_name(self, proxy, cloud_account_type):
        '''Test that a cloud account cannot be created if name is an empty string

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_type - a pytest fixture for the cloud account type

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        properties = {
                'account_type': cloud_account_type,
                'name': '',
        }

        cloud_account = RwMcYang.CloudAccount.from_dict(properties)
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/cloud-account/account', cloud_account)

    def test_create_cloud_account_with_null_name(self, proxy, cloud_account_type):
        '''Test that a cloud account cannot be created if name is null

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_type - a pytest fixture for the cloud account type

        Asserts:
            TypeError is raised

        '''
        properties = {
                'account_type': cloud_account_type,
                'name': None,
        }

        with pytest.raises(TypeError):
            cloud_account = RwMcYang.CloudAccount.from_dict(properties)

    #
    # Test cloud account type
    #

    def _test_create_cloud_account_with_no_type(self, proxy, cloud_account_name):
        '''Test that a cloud account cannot be created if no type is provided

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        properties = {
                'name': cloud_account_name,
        }

        cloud_account = RwMcYang.CloudAccount.from_dict(properties)
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/cloud-account/account', cloud_account)

    def test_create_cloud_account_with_empty_type(self, proxy, cloud_account_name):
        '''Test that a cloud account cannot be created if cloud type is an empty string

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts:
            gi.repository.GLib.Error is raised

        '''
        properties = {
                'account_type': '',
                'name': cloud_account_name,
        }

        with pytest.raises(GLib.Error):
            cloud_account = RwMcYang.CloudAccount.from_dict(properties)

    def test_create_cloud_account_with_invaid_type(self, proxy, cloud_account_name):
        '''Test that a cloud account cannot be created if the cloud type is invalid

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts:
            gi.repository.GLib.Error is raised

        '''
        properties = {
                'account_type': 'Nemesis',
                'name': cloud_account_name,
        }

        with pytest.raises(GLib.Error):
            cloud_account = RwMcYang.CloudAccount.from_dict(properties)

    def test_create_cloud_account_with_null_type(self, proxy, cloud_account_name):
        '''Test that a cloud account cannot be created if the cloud type is null

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts:
            TypeError is raised

        '''
        properties = {
                'account_type': None,
                'name': cloud_account_name,
        }

        with pytest.raises(TypeError):
            cloud_account = RwMcYang.CloudAccount.from_dict(properties)

    #
    # Test change cloud type
    #

    def test_create_cloud_account(self, proxy, cloud_account):
        '''Creates a cloud account for subsequent tests
        
        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name
            cloud_account      - a pytest fixture for the cloud account

        Asserts:
            None
        
        '''
        proxy.create_config('/cloud-account/account', cloud_account)

    def test_change_cloud_account_type(self, proxy, cloud_account):
        '''Test that a cloud account type cannot be changed

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts: 
            rift.auto.proxy.ProxyRequestError is raised

        '''
        account_type_map = {
            'cloudsim': 'openstack',
            'openstack': 'cloudsim',
        }
        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        cloud_account = proxy.get(xpath)
        updated_cloud_account = RwMcYang.CloudAccount.from_dict({
            'name': cloud_account.name,
            'account_type': account_type_map[cloud_account.account_type],
        })
        with pytest.raises(ProxyRequestError):
            proxy.merge_config(xpath, updated_cloud_account)

    def test_create_cloud_account_with_duplicate_name(self, proxy,
            cloud_account):
        '''Attempt to create a cloud account with a duplicate name

        Arguments:
            proxy - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts: 
            rift.auto.proxy.ProxyRequestError is raised

        '''
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/cloud-account/account', cloud_account)

    def test_delete_cloud_account_with_vm_pool_with_vm_resources(self, proxy,
            cloud_account, vm_pool_name):
        '''Tests that a cloud account cannot be deleted if it has a vm pool

        Arguments:
            proxy              - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name
            vm_pool_name       - a pytest fixture for the primary vm pool name

        Asserts:
            A cloud account exists for the cloud_account_name
            Newly configured vm pool has no resources assigned to it
            Cloud account has available resources
            VM pool has has available resources
            Cloud account and vm pool agree on available resources
            Configured resource is reflected as assigned in operational data
            post assignment
            rift.auto.proxy.ProxyRequestError is raised

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
        assert len(assigned_ids) == 0 # pool contained resources before any were assigned

        account = proxy.get("/cloud-account/account[name='%s']" % cloud_account.name)
        cloud_vm_ids = [vm.id for vm in account.resources.vm]
        assert len(cloud_vm_ids) >= 1

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        available_ids = [vm.id for vm in pool.available]
        assert len(available_ids) >= 1 # Assert pool has available resources
        # Assert not split brain
        assert set(cloud_vm_ids).difference(set(available_ids)) == set([])

        pool_config = RwMcYang.VmPool.from_dict({
                'name':vm_pool_name,
                'cloud_account':cloud_account.name,
                'assigned':[{'id':available_ids[0]}]})
        proxy.replace_config("/vm-pool/pool[name='%s']" % vm_pool_name, pool_config)
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert available_ids[0] in assigned_ids # Configured resource shows as assigned

        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        with pytest.raises(ProxyRequestError):
            proxy.delete_config(xpath)


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestCloudAccountNegativeTeardown:

    def test_remove_vm_resource_from_vm_pool(self, proxy, vm_pool_name):
        '''Unconfigure vm_pool: Remove the primary vm pool resource(s)

        Arguments:
            proxy        - a pytest fixture proxy to RwMcYang
            vm_pool_name - a pytest fixture for the VM pool name

        Asserts:
            Resource is assigned before unassigning
            Resource is no longer assigned after being unconfigured

        '''
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert len(assigned_ids) >= 1 # Assert resource is still assigned

        for assigned_id in assigned_ids:
            xpath = "/vm-pool/pool[name='%s']/assigned[id='%s']" % (vm_pool_name, assigned_id)
            proxy.delete_config(xpath)

        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert len(assigned_ids) == 0 # Assert resource is not assigned

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

