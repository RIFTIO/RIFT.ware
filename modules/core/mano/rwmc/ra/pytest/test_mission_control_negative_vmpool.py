#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_mission_control_negative.py
@author
@date 11/23/2015
@brief System test of negative (failure) mission control functionality
"""

import logging
import pytest


import gi
gi.require_version('RwMcYang', '1.0')

from rift.auto.session import ProxyRequestError
from gi.repository import RwMcYang

@pytest.fixture(scope='module')
def proxy(request, mgmt_session):
    '''Fixture which returns a proxy to RwMcYang

    Arguments:
        mgmt_session - mgmt_session fixture - instance of a rift.auto.session
                       class

    '''
    return mgmt_session.proxy(RwMcYang)

@pytest.fixture(scope='session')
def secondary_vm_pool_name(request):
    '''Fixture which returns the secondary vm pool name'''
    return 'vm-pool-2'

def show_cloud_account(logger, cloud_account):
    '''Helper method to output vm and network ids for debugging

        Here is a sample cloud account resources dict:
        resources= {'vm': [
                    {'name': 'rift-s1', 'available': True, 'id': '1'}]}

    Arguments:
        logger        - logging object which to send output
        cloud_account - cloud_account object which to interrogate

    '''
    logger.debug('Showing cloud account. name=%s' % cloud_account.name)
    logger.debug('account.resources=', cloud_account.resources)
    cloud_vm_ids = [vm.id for vm in cloud_account.resources.vm]
    logger.debug('cloud vm ids: %s' % cloud_vm_ids)
    cloud_network_ids = [network.id for network in cloud_account.resources.network]
    logger.debug('cloud network ids: %s' % cloud_network_ids)


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestVmPoolNegativeSetup:
    '''Performs module level setup'''

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
        #show_cloud_account(logger, cloud_account)

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

   # def test_stub(self, proxy):
   #     '''The decorator is a fix to prevent the test script from failing due
   #     to the following error:

   #         "Unable to resolve dependency ('launchpad',). failed to order test:"

   #     Arguments:
   #         proxy - a pytest fixture proxy to RwMcYang

   #     Asserts:
   #         True

   #     '''
   #     assert True


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestVmPoolNegative:
    '''This class is a container for testing VM pool negative cases.

    The following aspects are tested:
    * create a vm pool object
    * assign resources to a pool that have already been assigned to another pool

    '''

    #
    # Create: VM pool name tests
    #

    def test_create_vm_pool_with_missing_pool_name(self, proxy, cloud_account):
        '''Tests that a vm pool cannot be created without a name

        Arguments:
            proxy         - a pytest fixture proxy to RwMcYang
            cloud_account - a pytest fixture for the cloud account

        Asserts:
            cloud_account has a name
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert cloud_account.name is not None

        pool_config = RwMcYang.VmPool(
            cloud_account=cloud_account.name,
            dynamic_scaling=True,
        )
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/vm-pool/pool', pool_config)

    def test_create_vm_pool_with_blank_pool_name(self, proxy, cloud_account):
        '''Tests that a vm pool cannot be created without a name

        Arguments:
            proxy         - a pytest fixture proxy to RwMcYang
            cloud_account - a pytest fixture for the cloud account

        Asserts:
            Cloud account has a name
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert cloud_account.name is not None

        pool_config = RwMcYang.VmPool(
                name='',
                cloud_account=cloud_account.name,
                dynamic_scaling=True,
        )
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/vm-pool/pool', pool_config)

    def test_create_vm_pool_with_null_pool_name(self, proxy, cloud_account):
        '''Tests that a vm pool cannot be created without a name

        Arguments:
            proxy         - a pytest fixture proxy to RwMcYang
            cloud_account - a pytest fixture for the cloud account

        Asserts:
            Cloud account has a name
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert cloud_account.name is not None
        with pytest.raises(TypeError):
            pool_config = RwMcYang.VmPool(
                    name=None,
                    cloud_account=cloud_account.name,
                    dynamic_scaling=True,
            )
        #proxy.create_config('/vm-pool/pool', pool_config)

    def test_create_vm_pool_with_duplicate_name(self, proxy, vm_pool_name,
            cloud_account):
        '''Tests that a vm pool cannot be created with a name that already exists

        Arguments:
            proxy         - a pytest fixture proxy to RwMcYang
            vm_pool_name  - a pytest fixture for the vm pool name
            cloud_account - a pytest fixture for the cloud account

        Asserts:
            Cloud account has a name
            rift.auto.proxy.ProxyRequestError is raised

        '''
        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        cloud_account = proxy.get(xpath)
        assert cloud_account.name is not None

        pool_config = RwMcYang.VmPool(
                name=vm_pool_name,
                cloud_account=cloud_account.name,
                dynamic_scaling=True,
        )
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/vm-pool/pool', pool_config)

    #
    # Cloud name tests
    #

    @pytest.mark.xfail(raises=ProxyRequestError)
    def test_create_vm_pool_with_missing_cloud_name(self, proxy, secondary_vm_pool_name):
        '''Tests that a vm pool cannot be created with a name that already exists

        Arguments:
            proxy                  - a pytest fixture proxy to RwMcYang
            secondary_vm_pool_name - a pytest fixture for the secondary vm pool name

        Asserts:
            Secondary vm pool name exists
            Secondary vm pool does not exist
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert secondary_vm_pool_name is not None
        assert proxy.get("/vm-pool/pool[name='%s']" % secondary_vm_pool_name) is None

        pool_config = RwMcYang.VmPool(
            name=secondary_vm_pool_name,
            dynamic_scaling=True,
        )
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/vm-pool/pool', pool_config)

    def test_create_vm_pool_with_blank_cloud_name(self, proxy, secondary_vm_pool_name):
        '''Tests that a vm pool cannot be created with a name that already exists

        Arguments:
            proxy                  - a pytest fixture proxy to RwMcYang
            secondary_vm_pool_name - a pytest fixture for the secondary vm pool name

        Asserts:
            Secondary vm pool name exists
            Secondary vm pool does not exist
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert secondary_vm_pool_name is not None
        assert proxy.get("/vm-pool/pool[name='%s']" % secondary_vm_pool_name) is None

        pool_config = RwMcYang.VmPool(
            name=secondary_vm_pool_name,
            cloud_account='',
            dynamic_scaling=True,
        )
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/vm-pool/pool', pool_config)

    def _test_create_vm_pool_with_null_cloud_name(self, proxy, secondary_vm_pool_name):
        '''Tests that a vm pool cannot be created if the cloud name is None

        Arguments:
            proxy                  - a pytest fixture proxy to RwMcYang
            secondary_vm_pool_name - a pytest fixture for the secondary vm pool name

        Asserts:
            Secondary vm pool name exists
            Secondary vm pool does not exist
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert secondary_vm_pool_name is not None
        assert proxy.get("/vm-pool/pool[name='%s']" % secondary_vm_pool_name) is None
        with pytest.raises(TypeError):
            pool_config = RwMcYang.VmPool(
                name=secondary_vm_pool_name,
                cloud_account=None,
                dynamic_scaling=True,
            )
        #proxy.create_config('/vm-pool/pool', pool_config)

    def test_create_vm_pool_with_bogus_cloud_name(self, proxy, secondary_vm_pool_name):
        '''Tests that a vm pool cannot be created if the cloud name is None

        Arguments:
            proxy          - a pytest fixture proxy to RwMcYang
            secondary_vm_pool_name - a pytest fixture for the secondary vm pool name

        Asserts:
            Secondary vm pool name exists
            Secondary vm pool does not exist
            Cloud account does not exist for the bogus cloud account name
            rift.auto.proxy.ProxyRequestError is raised

        '''
        assert secondary_vm_pool_name is not None
        assert proxy.get("/vm-pool/pool[name='%s']" % secondary_vm_pool_name) is None

        bogus_cloud_account_name = 'bogus-cloud-account-name'
        cloud_account = proxy.get("/cloud-account/account[name='%s']" % bogus_cloud_account_name)
        assert cloud_account is None

        pool_config = RwMcYang.VmPool(
            name=secondary_vm_pool_name,
            cloud_account=bogus_cloud_account_name,
            dynamic_scaling=True,
        )
        with pytest.raises(ProxyRequestError):
            proxy.create_config('/vm-pool/pool', pool_config)

    #
    # Test VM pool assignments
    #

    def test_assign_vm_resource_to_vm_pool(self, proxy, cloud_account,
            vm_pool_name):
        '''Configure a vm resource by adding it to a vm pool

        Arguments:
            proxy              - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name
            vm_pool_name       - a pytest fixture for the primary vm pool name

        Asserts:
            Cloud account has available resources
            VM pool has has available resources
            Cloud account and vm pool agree on available resources
            Configured resource is reflected as assigned in operational data
            post assignment

        '''
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
            'assigned':[{'id':available_ids[0]}]}
        )
        proxy.replace_config("/vm-pool/pool[name='%s']" % vm_pool_name, pool_config)
        pool = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert available_ids[0] in assigned_ids # Configured resource shows as assigned

    def test_create_vm_pool_2(self, proxy, cloud_account, secondary_vm_pool_name):
        '''Configure vm pool
        Arguments:
            proxy                  - a pytest fixture proxy to RwMcYang
            cloud_account_name     - a pytest fixture for the cloud account name
            secondary_vm_pool_name - a pytest fixture for the secondary vm pool name

        Asserts:
            Cloud account exists for the given cloud_account_name
            Newly configured vm pool has no resources assigned to it

        '''
        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        cloud_account = proxy.get(xpath)
        assert cloud_account is not None

        pool_config = RwMcYang.VmPool(
            name=secondary_vm_pool_name,
            cloud_account=cloud_account.name,
            dynamic_scaling=True,
        )
        proxy.create_config('/vm-pool/pool', pool_config)
        pool = proxy.get("/vm-pool/pool[name='%s']" % secondary_vm_pool_name)
        assigned_ids = [vm.id for vm in pool.assigned]
        assert len(assigned_ids) == 0 # pool contained resources before any were assigned

    @pytest.mark.skipif(True, reason="Assigned VMS are able to be shared between VM pools")
    @pytest.mark.xfail(raises=ProxyRequestError)
    def test_assign_allocated_vm_to_vm_pool_2(self, proxy, cloud_account,
            vm_pool_name, secondary_vm_pool_name):
        '''This test tries to assign a vm from one vm pool to another vm pool

        Arguments:
            proxy                  - a pytest fixture proxy to RwMcYang
            cloud_account_name     - a pytest fixture for the cloud account name
            vm_pool_name           - a pytest fixture for the primary vm pool name
            secondary_vm_pool_name - a pytest fixture for the secondary vm pool name

        Asserts:
            Prior to Pool 2 assignment, verifies that pool 1 has assigned id(s)
            rift.auto.proxy.ProxyRequestError is raised

        '''
        pool_1 = proxy.get("/vm-pool/pool[name='%s']" % vm_pool_name)
        pool_2 = proxy.get("/vm-pool/pool[name='%s']" % secondary_vm_pool_name)
        assigned_ids = [vm.id for vm in pool_1.assigned]
        assert len(assigned_ids >= 1)

        pool_config = RwMcYang.VmPool.from_dict({
            'name':secondary_vm_pool_name,
            'assigned':[{'id':assigned_ids[0]}]})
        with pytest.raises(ProxyRequestError):
            proxy.merge_config(
                "/vm-pool/pool[name='%s']" % secondary_vm_pool_name, pool_config
            )


@pytest.mark.incremental
@pytest.mark.usefixtures('cloud_account')
class TestVmPoolNegativeTeardown:
    '''This class serves to do cleanup for the VM pool negative tests'''

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

    def test_delete_cloud_account_expect_fail(self, proxy, cloud_account):
        '''Unconfigure cloud_account

        This should fail because we have not deleted vm pool 2

        Arguments:
            proxy              - a pytest fixture proxy to RwMcYang
            cloud_account_name - a pytest fixture for the cloud account name

        Asserts:
            rift.auto.proxy.ProxyRequestError is raised

        '''
        xpath = "/cloud-account/account[name='%s']" % cloud_account.name
        with pytest.raises(ProxyRequestError):
            proxy.delete_config(xpath)

    def test_delete_vm_pool_2(self, proxy, secondary_vm_pool_name):
        '''Unconfigure secondary vm pool

        Arguments:
            proxy        - a pytest fixture proxy to RwMcYang
            vm_pool_name - a pytest fixture for the VM pool name

        Asserts:
            None

        '''
        xpath = "/vm-pool/pool[name='%s']" % secondary_vm_pool_name
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

