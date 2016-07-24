#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import asyncio
import logging
import os
import sys
import types
import unittest
import uuid
import random

import xmlrunner

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwMain', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwCal', '1.0')


import gi.repository.CF as cf
import gi.repository.RwDts as rwdts
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.RwResourceMgrYang as rmgryang
from gi.repository import RwcalYang
from gi.repository import RwCloudYang
from gi.repository.RwTypes import RwStatus

import rw_peas
import rift.tasklets
import rift.test.dts

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


openstack_info = {
    'username'      : 'pluto',
    'password'      : 'mypasswd',
    'auth_url'      : 'http://10.66.4.14:5000/v3/',
    'project_name'  : 'demo',
    'mgmt_network'  : 'private',
    'image_id'      : '5cece2b1-1a49-42c5-8029-833c56574652',
    'vms'           : ['res-test-1', 'res-test-2'],
    'networks'      : ['testnet1', 'testnet2']}


def create_mock_resource_temaplate():
    ### Resource to be reuqested for 'mock'
    resource_requests = {'compute': {}, 'network': {}}

    ###### mycompute-0
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = str(uuid.uuid3(uuid.NAMESPACE_DNS, 'image-0'))
    msg.vm_flavor.vcpu_count = 4
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 40
    resource_requests['compute']['mycompute-0'] = msg

    ###### mycompute-1
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = str(uuid.uuid3(uuid.NAMESPACE_DNS, 'image-1'))
    msg.vm_flavor.vcpu_count = 2
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 20
    resource_requests['compute']['mycompute-1'] = msg

    ####### mynet-0
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    resource_requests['network']['mynet-0'] = msg
    
    ####### mynet-1
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    resource_requests['network']['mynet-1'] = msg

    return resource_requests


def create_cloudsim_resource_template():
    ### Resource to be reuqested for 'cloudsim'
    resource_requests = {'compute': {}, 'network': {}}

    ###### mycompute-0
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = "1"
    msg.vm_flavor.vcpu_count = 4
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 40
    resource_requests['compute']['mycompute-0'] = msg

    ###### mycompute-1
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = "1"
    msg.vm_flavor.vcpu_count = 2
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 20
    resource_requests['compute']['mycompute-1'] = msg

    ####### mynet-0
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    resource_requests['network']['mynet-0'] = msg
    
    ####### mynet-1
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    resource_requests['network']['mynet-1'] = msg

    return resource_requests

def create_mock_resource_temaplate():
    ### Resource to be reuqested for 'mock'
    resource_requests = {'compute': {}, 'network': {}}

    ###### mycompute-0
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = str(uuid.uuid3(uuid.NAMESPACE_DNS, 'image-0'))
    msg.vm_flavor.vcpu_count = 4
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 40
    resource_requests['compute']['mycompute-0'] = msg

    ###### mycompute-1
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = str(uuid.uuid3(uuid.NAMESPACE_DNS, 'image-1'))
    msg.vm_flavor.vcpu_count = 2
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 20
    resource_requests['compute']['mycompute-1'] = msg

    ####### mynet-0
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    resource_requests['network']['mynet-0'] = msg
    
    ####### mynet-1
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    resource_requests['network']['mynet-1'] = msg

    return resource_requests


def create_openstack_static_template():
    ### Resource to be reuqested for 'openstack_static'
    resource_requests = {'compute': {}, 'network': {}}

    ###### mycompute-0
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = openstack_info['image_id']
    msg.vm_flavor.vcpu_count = 4
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 80
    resource_requests['compute']['mycompute-0'] = msg

    ###### mycompute-1
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = openstack_info['image_id']
    msg.vm_flavor.vcpu_count = 2
    msg.vm_flavor.memory_mb = 4096
    msg.vm_flavor.storage_gb = 40
    resource_requests['compute']['mycompute-1'] = msg

    ####### mynet-0
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    msg.provider_network.physical_network = 'PHYSNET1'
    msg.provider_network.overlay_type = 'VLAN'
    msg.provider_network.segmentation_id = 17
    resource_requests['network']['mynet-0'] = msg
    
    ####### mynet-1
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    msg.provider_network.physical_network = 'PHYSNET1'
    msg.provider_network.overlay_type = 'VLAN'
    msg.provider_network.segmentation_id = 18
    resource_requests['network']['mynet-1'] = msg

    return resource_requests


def create_openstack_dynamic_template():
    ### Resource to be reuqested for 'openstack_dynamic'
    resource_requests = {'compute': {}, 'network': {}}

    ###### mycompute-0
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = openstack_info['image_id']
    msg.vm_flavor.vcpu_count = 2
    msg.vm_flavor.memory_mb = 4096
    msg.vm_flavor.storage_gb = 40
    msg.guest_epa.mempage_size = 'LARGE'
    msg.guest_epa.cpu_pinning_policy = 'DEDICATED'
    msg.allocate_public_address = True
    
    resource_requests['compute']['mycompute-0'] = msg

    ###### mycompute-1
    msg = rmgryang.VDUEventData_RequestInfo()
    msg.image_id  = openstack_info['image_id']
    msg.vm_flavor.vcpu_count = 4
    msg.vm_flavor.memory_mb = 8192
    msg.vm_flavor.storage_gb = 40
    msg.guest_epa.mempage_size = 'LARGE'
    msg.guest_epa.cpu_pinning_policy = 'DEDICATED'
    msg.allocate_public_address = True
    
    resource_requests['compute']['mycompute-1'] = msg

    ####### mynet-0
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    #msg.provider_network.overlay_type = 'VXLAN'
    #msg.provider_network.segmentation_id = 71

    resource_requests['network']['mynet-0'] = msg

    ####### mynet-1
    msg = rmgryang.VirtualLinkEventData_RequestInfo()
    #msg.provider_network.overlay_type = 'VXLAN'
    #msg.provider_network.segmentation_id = 73
    resource_requests['network']['mynet-1'] = msg

    return resource_requests




resource_requests = {
    'mock' : create_mock_resource_temaplate(),
    'openstack_static': create_openstack_static_template(),
    'openstack_dynamic': create_openstack_dynamic_template(),
    'cloudsim': create_cloudsim_resource_template(),
}


def get_cal_account(account_type):
    """
    Creates an object for class RwcalYang.CloudAccount()
    """
    account = RwcalYang.CloudAccount()
    if account_type == 'mock':
        account.name          = 'mock_account'
        account.account_type  = "mock"
        account.mock.username = "mock_user"
    elif ((account_type == 'openstack_static') or (account_type == 'openstack_dynamic')):
        account.name = 'openstack_cal'
        account.account_type = 'openstack'
        account.openstack.key = openstack_info['username']
        account.openstack.secret       = openstack_info['password']
        account.openstack.auth_url     = openstack_info['auth_url']
        account.openstack.tenant       = openstack_info['project_name']
        account.openstack.mgmt_network = openstack_info['mgmt_network']

    elif account_type == 'cloudsim':
        account.name          = 'cloudsim'
        account.account_type  = "cloudsim_proxy"

    return account

def create_cal_plugin(account, log_hdl):
    plugin_name = getattr(account, account.account_type).plugin_name
    plugin = rw_peas.PeasPlugin(plugin_name, 'RwCal-1.0')
    engine, info, extension = plugin()
    rwcal = plugin.get_interface("Cloud")
    try:
        rc = rwcal.init(log_hdl)
        assert rc == RwStatus.SUCCESS
    except Exception as e:
        raise
    return rwcal

    
class RMMgrTestCase(rift.test.dts.AbstractDTSTest):
    rwcal = None
    rwcal_acct_info = None

    @classmethod
    def configure_suite(cls, rwmain):
        rm_dir = os.environ.get('RM_DIR')
        cnt_mgr_dir = os.environ.get('CNTR_MGR_DIR')
        cal_proxy_dir = os.environ.get('CAL_PROXY_DIR')

        cls.rwmain.add_tasklet(cal_proxy_dir, 'rwcalproxytasklet')
        cls.rwmain.add_tasklet(rm_dir, 'rwresmgrtasklet')
        cls.rwmain.add_tasklet(cnt_mgr_dir, 'rwcntmgrtasklet')

    @classmethod
    def configure_schema(cls):
        return rmgryang.get_schema()
        
    @asyncio.coroutine
    def wait_tasklets(self):
        yield from asyncio.sleep(1, loop=self.loop)

    @classmethod
    def configure_timeout(cls):
        return 360

    def get_cloud_account_msg(self, acct_type):
        cloud_account = RwCloudYang.CloudAccount()
        acct = get_cal_account(acct_type)
        cloud_account.from_dict(acct.as_dict())
        cloud_account.name = acct.name
        return cloud_account
    
    def get_compute_pool_msg(self, name, pool_type, cloud_type):
        pool_config = rmgryang.ResourcePools()
        pool = pool_config.pools.add()
        pool.name = name
        pool.resource_type = "compute"
        if pool_type == "static":
            pool.pool_type = 'static'
            acct = get_cal_account(cloud_type)
            rwcal = create_cal_plugin(acct, self.tinfo.get_rwlog_ctx())
            rc, rsp = rwcal.get_vdu_list(acct)
            assert rc == RwStatus.SUCCESS
            
            if cloud_type == 'openstack_static':
                for vdu in rsp.vdu_info_list:
                    if vdu.name in openstack_info['vms']:
                        self.log.info("Adding the static compute resource: %s to compute pool", vdu.name)
                        r = pool.resources.add()
                        r.resource_id = vdu.vdu_id
            else:
                # 'mock', 'cloudsim' 'openstack_dynamic' etc
                for vdu in rsp.vdu_info_list:
                    self.log.info("Adding the static compute resource: %s to compute pool", vdu.name)
                    r = pool.resources.add()
                    r.resource_id = vdu.vdu_id
        else:
            pool.pool_type = 'dynamic'
            pool.max_size = 10
        return pool_config

    def get_network_pool_msg(self, name, pool_type, cloud_type):
        pool_config = rmgryang.ResourcePools()
        pool = pool_config.pools.add()
        pool.name = name
        pool.resource_type = "network"
        if pool_type == "static":
            pool.pool_type = 'static'
            acct = get_cal_account(cloud_type)
            rwcal = create_cal_plugin(acct, self.tinfo.get_rwlog_ctx())
            rc, rsp = rwcal.get_virtual_link_list(acct)
            assert rc == RwStatus.SUCCESS
            if cloud_type == 'openstack_static':
                for vlink in rsp.virtual_link_info_list:
                    if vlink.name in openstack_info['networks']:
                        self.log.info("Adding the static network resource: %s to network pool", vlink.name)
                        r = pool.resources.add()
                        r.resource_id = vlink.virtual_link_id
            else:
                # 'mock', 'cloudsim', 'openstack_dynamic' etc
                for vlink in rsp.virtual_link_info_list:
                    self.log.info("Adding the static network resource: %s to network pool", vlink.name)
                    r = pool.resources.add()
                    r.resource_id = vlink.virtual_link_id
        else:
            pool.pool_type = 'dynamic'
            pool.max_size = 4
        return pool_config


    def get_network_reserve_msg(self, name, cloud_type, xpath):
        event_id = str(uuid.uuid4())
        msg = rmgryang.VirtualLinkEventData()
        msg.event_id = event_id
        msg.request_info.name = name
        attributes = ['physical_network', 'name', 'overlay_type', 'segmentation_id']

        for attr in attributes:
            if resource_requests[cloud_type]['network'][name].has_field('provider_network'):
                if resource_requests[cloud_type]['network'][name].provider_network.has_field(attr):
                    setattr(msg.request_info.provider_network, attr,
                            getattr(resource_requests[cloud_type]['network'][name].provider_network ,attr))
                
        return msg, xpath.format(event_id)

    def get_compute_reserve_msg(self, name, cloud_type, xpath, vlinks):
        event_id = str(uuid.uuid4())
        msg = rmgryang.VDUEventData()
        msg.event_id = event_id
        msg.request_info.name = name
        msg.request_info.image_id = resource_requests[cloud_type]['compute'][name].image_id
        attributes = ['image_id', 'vcpu_count', 'memory_mb', 'storage_gb']
        
        if resource_requests[cloud_type]['compute'][name].has_field('vm_flavor'):
            for attr in attributes:
                if resource_requests[cloud_type]['compute'][name].vm_flavor.has_field(attr):
                    setattr(msg.request_info.vm_flavor,
                            attr,
                            getattr(resource_requests[cloud_type]['compute'][name].vm_flavor , attr))

        attributes = ['mempage_size', 'cpu_pinning_policy']
        
        if resource_requests[cloud_type]['compute'][name].has_field('guest_epa'):
            for attr in attributes:
                if resource_requests[cloud_type]['compute'][name].guest_epa.has_field(attr):
                    setattr(msg.request_info.guest_epa,
                            attr,
                            getattr(resource_requests[cloud_type]['compute'][name].guest_epa , attr))

        if resource_requests[cloud_type]['compute'][name].has_field('allocate_public_address'):
            msg.request_info.allocate_public_address = resource_requests[cloud_type]['compute'][name].allocate_public_address
            
        cnt = 0
        for link in vlinks:
            c1 = msg.request_info.connection_points.add()
            c1.name = name+"-port-"+str(cnt)
            cnt += 1
            c1.virtual_link_id = link

        self.log.info("Sending message :%s", msg)
        return msg, xpath.format(event_id)

    @asyncio.coroutine
    def configure_cloud_account(self, dts, acct_type):
        account_xpath = "C,/rw-cloud:cloud/account"
        msg = self.get_cloud_account_msg(acct_type)
        self.log.info("Configuring cloud-account: %s",msg)
        yield from dts.query_create(account_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    msg)

    @asyncio.coroutine
    def configure_compute_resource_pools(self, dts, resource_type, cloud_type):
        pool_xpath = "C,/rw-resource-mgr:resource-mgr-config/rw-resource-mgr:resource-pools"
        msg = self.get_compute_pool_msg("virtual-compute", resource_type, cloud_type)
        self.log.info("Configuring compute-resource-pool: %s",msg)
        yield from dts.query_create(pool_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    msg)
            

    @asyncio.coroutine
    def configure_network_resource_pools(self, dts, resource_type, cloud_type):
        pool_xpath = "C,/rw-resource-mgr:resource-mgr-config/rw-resource-mgr:resource-pools"
        msg = self.get_network_pool_msg("virtual-network", resource_type, cloud_type)
        self.log.info("Configuring network-resource-pool: %s",msg)
        yield from dts.query_create(pool_xpath,
                                    rwdts.XactFlag.ADVISE,
                                    msg)

    @asyncio.coroutine
    def verify_resource_pools_config(self, dts):
        pool_records_xpath = "D,/rw-resource-mgr:resource-pool-records"        
        self.log.debug("Verifying test_create_resource_pools results")
        res_iter = yield from dts.query_read(pool_records_xpath,)
        for result in res_iter:
            response = yield from result
            records = response.result.records
            #self.assertEqual(len(records), 2)
            #names = [i.name for i in records]
            #self.assertTrue('virtual-compute' in names)
            #self.assertTrue('virtual-network' in names)
            for record in records:
                self.log.debug("Received Pool Record, Name: %s, Resource Type: %s, Pool Status: %s, Pool Size: %d, Allocated Resources: %d, Free Resources: %d",
                               record.name,
                               record.resource_type,
                               record.pool_status,
                               record.total_resources,
                               record.allocated_resources,
                               record.free_resources)
                
    @asyncio.coroutine
    def read_resource(self, dts, xpath):
        self.log.debug("Reading data for XPATH:%s", xpath)
        result = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)
        msg = None
        for r in result:
            msg = yield from r
        self.log.debug("Received data: %s", msg.result)
        return msg.result
        
    @asyncio.coroutine
    def reserve_network_resources(self, name, dts, cloud_type):
        network_xpath = "D,/rw-resource-mgr:resource-mgmt/vlink-event/vlink-event-data[event-id='{}']"
        msg,xpath = self.get_network_reserve_msg(name, cloud_type, network_xpath)
        self.log.debug("Sending create event to network-event xpath %s with msg: %s" % (xpath, msg))
        yield from dts.query_create(xpath, 0, msg)
        return xpath
        
            
    @asyncio.coroutine
    def reserve_compute_resources(self, name, dts, cloud_type, vlinks = []):
        compute_xpath = "D,/rw-resource-mgr:resource-mgmt/vdu-event/vdu-event-data[event-id='{}']"
        msg,xpath = self.get_compute_reserve_msg(name, cloud_type, compute_xpath, vlinks)
        self.log.debug("Sending create event to compute-event xpath %s with msg: %s" % (xpath, msg))
        yield from dts.query_create(xpath, 0, msg)
        return xpath

    @asyncio.coroutine
    def release_network_resources(self, dts, xpath):
        self.log.debug("Initiating network resource release for  : %s ", xpath)
        yield from dts.query_delete(xpath, 0)
        
    @asyncio.coroutine
    def release_compute_resources(self, dts, xpath):
        self.log.debug("Initiating compute resource release for  : %s ", xpath)
        yield from dts.query_delete(xpath, 0)

    @unittest.skip("Skipping test_static_pool_resource_allocation")                            
    def test_static_pool_resource_allocation(self):
        self.log.debug("STARTING - test_static_pool_resource_allocation")
        tinfo = self.new_tinfo('static_mock')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
        
        @asyncio.coroutine
        def run_test():
            networks = []
            computes = []
            cloud_type = 'mock'
            yield from self.wait_tasklets()
            yield from self.configure_cloud_account(dts, cloud_type)
            
            yield from self.configure_network_resource_pools(dts, "static", cloud_type)
            yield from self.configure_compute_resource_pools(dts, "static", cloud_type)
            yield from self.verify_resource_pools_config(dts)
            
            r_xpath = yield from self.reserve_network_resources('mynet-0', dts, cloud_type)
            r_info = yield from self.read_resource(dts,r_xpath)
            networks.append((r_xpath, r_info.resource_info))

            for i in range(2):
                r_xpath = yield from self.reserve_compute_resources("mycompute-"+str(i),
                                                                    dts,
                                                                    cloud_type,
                                                                    [networks[0][1].virtual_link_id])
                r_info = yield from self.read_resource(dts,r_xpath)
                computes.append((r_xpath, r_info))

            yield from self.verify_resource_pools_config(dts)

            for r in computes:
                yield from self.release_compute_resources(dts, r[0])

            yield from self.release_network_resources(dts,networks[0][0])
            yield from self.verify_resource_pools_config(dts)
            
        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_static_pool_resource_allocation")

    @unittest.skip("Skipping test_dynamic_pool_resource_allocation")                            
    def test_dynamic_pool_resource_allocation(self):
        self.log.debug("STARTING - test_dynamic_pool_resource_allocation")
        tinfo = self.new_tinfo('dynamic_mock')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

        @asyncio.coroutine
        def run_test():
            networks = []
            computes = []
            cloud_type = 'mock'
            yield from self.wait_tasklets()
            yield from self.configure_cloud_account(dts, cloud_type)
            yield from self.configure_network_resource_pools(dts, "dynamic", cloud_type)
            yield from self.configure_compute_resource_pools(dts, "dynamic", cloud_type)
            yield from self.verify_resource_pools_config(dts)

            r_xpath = yield from self.reserve_network_resources('mynet-0', dts, cloud_type)
            r_info = yield from self.read_resource(dts,r_xpath)
            networks.append((r_xpath, r_info.resource_info))

            for i in range(2):
                r_xpath = yield from self.reserve_compute_resources("mycompute-"+str(i),
                                                                    dts,
                                                                    cloud_type,
                                                                    [networks[0][1].virtual_link_id])
                r_info = yield from self.read_resource(dts,r_xpath)
                computes.append((r_xpath, r_info))

            yield from self.verify_resource_pools_config(dts)

            for r in computes:
                self.log.debug("Releasing compute resource with id: %s", r[1].resource_info.vdu_id)
                yield from self.release_compute_resources(dts, r[0])

            yield from self.release_network_resources(dts,networks[0][0])
            yield from self.verify_resource_pools_config(dts)
            
        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_dynamic_pool_resource_allocation")

    @unittest.skip("Skipping test_dynamic_pool_resource_allocation")                            
    def test_dynamic_cloudsim_pool_resource_allocation(self):
        self.log.debug("STARTING - test_dynamic_pool_resource_allocation")
        tinfo = self.new_tinfo('dynamic_mock')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

        @asyncio.coroutine
        def run_test():
            networks = []
            computes = []
            cloud_type = 'cloudsim'

            yield from asyncio.sleep(120, loop=self.loop)
            yield from self.configure_cloud_account(dts, cloud_type)
            yield from self.configure_network_resource_pools(dts, "dynamic", cloud_type)
            yield from self.configure_compute_resource_pools(dts, "dynamic", cloud_type)
            yield from self.verify_resource_pools_config(dts)

            r_xpath = yield from self.reserve_network_resources('mynet-0', dts, cloud_type)
            r_info = yield from self.read_resource(dts,r_xpath)
            networks.append((r_xpath, r_info.resource_info))

            for i in range(2):
                r_xpath = yield from self.reserve_compute_resources("mycompute-"+str(i),
                                                                    dts,
                                                                    cloud_type,
                                                                    [networks[0][1].virtual_link_id])
                r_info = yield from self.read_resource(dts,r_xpath)
                computes.append((r_xpath, r_info))

            yield from self.verify_resource_pools_config(dts)

            for r in computes:
                self.log.debug("Releasing compute resource with id: %s", r[1].resource_info.vdu_id)
                yield from self.release_compute_resources(dts, r[0])

            yield from self.release_network_resources(dts,networks[0][0])
            yield from self.verify_resource_pools_config(dts)
            
        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_dynamic_pool_resource_allocation")

    @unittest.skip("Skipping test_static_pool_openstack_resource_allocation")
    def test_static_pool_openstack_resource_allocation(self):
        self.log.debug("STARTING - test_static_pool_openstack_resource_allocation")
        tinfo = self.new_tinfo('static_openstack')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
        
        @asyncio.coroutine
        def run_test():
            networks = []
            computes = []
            cloud_type = 'openstack_static'
            yield from self.wait_tasklets()
            yield from self.configure_cloud_account(dts, cloud_type)
            yield from self.configure_network_resource_pools(dts, "static", cloud_type)
            yield from self.configure_compute_resource_pools(dts, "static", cloud_type)
            yield from self.verify_resource_pools_config(dts)

            self.log.debug("Creating virtual-network-resources in openstack")
            r_xpath = yield from self.reserve_network_resources('mynet-0', dts, cloud_type)
            r_info = yield from self.read_resource(dts,r_xpath)
            networks.append((r_xpath, r_info.resource_info))
            self.log.debug("virtual-network-resources successfully created in openstack")

            self.log.debug("Creating virtual-network-compute in openstack")
            for i in range(2):
                r_xpath = yield from self.reserve_compute_resources("mycompute-" + str(i),
                                                                    dts,
                                                                    cloud_type,
                                                                    [networks[0][1].virtual_link_id])
                r_info = yield from self.read_resource(dts,r_xpath)
                computes.append((r_xpath, r_info))

            yield from self.verify_resource_pools_config(dts)
            for r in computes:
                self.log.debug("Releasing compute resource with id: %s", r[1].resource_info.vdu_id)
                yield from self.release_compute_resources(dts, r[0])

            yield from self.release_network_resources(dts,networks[0][0])
            yield from self.verify_resource_pools_config(dts)
            self.log.debug("Openstack static resource allocation completed")
            
        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_static_pool_openstack_resource_allocation")
        
    #@unittest.skip("Skipping test_dynamic_pool_openstack_resource_allocation")
    def test_dynamic_pool_openstack_resource_allocation(self):
        self.log.debug("STARTING - test_dynamic_pool_openstack_resource_allocation")
        tinfo = self.new_tinfo('dynamic_openstack')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
        
        @asyncio.coroutine
        def run_test():
            networks = []
            computes = []
            cloud_type = 'openstack_dynamic'
            yield from self.wait_tasklets()
            yield from self.configure_cloud_account(dts, cloud_type)
            yield from self.configure_network_resource_pools(dts, "dynamic", cloud_type)
            yield from self.configure_compute_resource_pools(dts, "dynamic", cloud_type)
            yield from self.verify_resource_pools_config(dts)

            self.log.debug("Creating virtual-network-resources in openstack")
            r_xpath = yield from self.reserve_network_resources('mynet-0', dts, cloud_type)
            r_info = yield from self.read_resource(dts,r_xpath)
            networks.append((r_xpath, r_info.resource_info))
            self.log.debug("virtual-network-resources successfully created in openstack")

            self.log.debug("Creating virtual-network-compute in openstack")
            for i in range(2):
                r_xpath = yield from self.reserve_compute_resources("mycompute-" + str(i),
                                                                    dts,
                                                                    cloud_type,
                                                                    [networks[0][1].virtual_link_id])
                r_info = yield from self.read_resource(dts,r_xpath)
                computes.append((r_xpath, r_info))

            yield from self.verify_resource_pools_config(dts)
            for r in computes:
                self.log.debug("Releasing compute resource with id: %s", r[1].resource_info.vdu_id)
                #yield from self.release_compute_resources(dts, r[0])

            self.log.debug("Releasing network resource with id: %s", r[1].resource_info.vdu_id)
            #yield from self.release_network_resources(dts,networks[0][0])
            #yield from self.verify_resource_pools_config(dts)
            self.log.debug("Openstack dynamic resource allocation completed")
            
        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_dynamic_pool_openstack_resource_allocation")
        

def main():
    top_dir = __file__[:__file__.find('/modules/core/')]
    build_dir = os.path.join(top_dir, '.build/modules/core/rwvx/src/core_rwvx-build')
    mc_build_dir = os.path.join(top_dir, '.build/modules/core/mc/core_mc-build')
    launchpad_build_dir = os.path.join(mc_build_dir, 'rwlaunchpad')
    cntr_mgr_build_dir = os.path.join(mc_build_dir, 'rwcntmgr')

    if 'MESSAGE_BROKER_DIR' not in os.environ:
        os.environ['MESSAGE_BROKER_DIR'] = os.path.join(build_dir, 'rwmsg/plugins/rwmsgbroker-c')

    if 'ROUTER_DIR' not in os.environ:
        os.environ['ROUTER_DIR'] = os.path.join(build_dir, 'rwdts/plugins/rwdtsrouter-c')

    if 'RM_DIR' not in os.environ:
        os.environ['RM_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwresmgrtasklet')

    if 'CAL_PROXY_DIR' not in os.environ:
        os.environ['CAL_PROXY_DIR'] = os.path.join(build_dir, 'plugins/rwcalproxytasklet')

    if 'CNTR_MGR_DIR' not in os.environ:
        os.environ['CNTR_MGR_DIR'] = os.path.join(cntr_mgr_build_dir, 'plugins/rwcntmgrtasklet')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()

