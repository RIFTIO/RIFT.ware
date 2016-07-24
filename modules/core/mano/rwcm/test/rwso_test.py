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

import xmlrunner

import gi.repository.CF as cf
import gi.repository.RwDts as rwdts
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.RwConmanYang as conmanY
import gi.repository.RwLaunchpadYang as launchpadyang

import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class RWSOTestCase(unittest.TestCase):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    rwmain = None
    tinfo = None
    schema = None
    id_cnt = 0

    @classmethod
    def setUpClass(cls):
        msgbroker_dir = os.environ.get('MESSAGE_BROKER_DIR')
        router_dir = os.environ.get('ROUTER_DIR')
        cm_dir = os.environ.get('SO_DIR')

        manifest = rwmanifest.Manifest()
        manifest.init_phase.settings.rwdtsrouter.single_dtsrouter.enable = True

        cls.rwmain = rwmain.Gi.new(manifest)
        cls.tinfo = cls.rwmain.get_tasklet_info()

        # Run router in mainq.  Eliminates some ill-diagnosed bootstrap races.
        os.environ['RWDTS_ROUTER_MAINQ']='1'
        cls.rwmain.add_tasklet(msgbroker_dir, 'rwmsgbroker-c')
        cls.rwmain.add_tasklet(router_dir, 'rwdtsrouter-c')
        cls.rwmain.add_tasklet(cm_dir, 'rwconmantasklet')

        cls.log = rift.tasklets.logger_from_tasklet_info(cls.tinfo)
        cls.log.setLevel(logging.DEBUG)

        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler.setFormatter(fmt)
        cls.log.addHandler(stderr_handler)
        cls.schema = conmanY.get_schema()

    def setUp(self):
        def scheduler_tick(self, *args):
            self.call_soon(self.stop)
            self.run_forever()

        self.loop = asyncio.new_event_loop()
        self.loop.scheduler_tick = types.MethodType(scheduler_tick, self.loop)
        self.loop.set_debug(True)
        os.environ["PYTHONASYNCIODEBUG"] = "1"
        asyncio_logger = logging.getLogger("asyncio")
        asyncio_logger.setLevel(logging.DEBUG)

        self.asyncio_timer = None
        self.stop_timer = None
        self.id_cnt += 1

    @asyncio.coroutine
    def wait_tasklets(self):
        yield from asyncio.sleep(1, loop=self.loop)

    def run_until(self, test_done, timeout=30):
        """
        Attach the current asyncio event loop to rwsched and then run the
        scheduler until the test_done function returns True or timeout seconds
        pass.

        @param test_done  - function which should return True once the test is
                            complete and the scheduler no longer needs to run.
        @param timeout    - maximum number of seconds to run the test.
        """
        def shutdown(*args):
            if args:
                self.log.debug('Shutting down loop due to timeout')

            if self.asyncio_timer is not None:
                self.tinfo.rwsched_tasklet.CFRunLoopTimerRelease(self.asyncio_timer)
                self.asyncio_timer = None

            if self.stop_timer is not None:
                self.tinfo.rwsched_tasklet.CFRunLoopTimerRelease(self.stop_timer)
                self.stop_timer = None

            self.tinfo.rwsched_instance.CFRunLoopStop()

        def tick(*args):
            self.loop.call_later(0.1, self.loop.stop)
            self.loop.run_forever()
            if test_done():
                shutdown()

        self.asyncio_timer = self.tinfo.rwsched_tasklet.CFRunLoopTimer(
            cf.CFAbsoluteTimeGetCurrent(),
            0.1,
            tick,
            None)

        self.stop_timer = self.tinfo.rwsched_tasklet.CFRunLoopTimer(
            cf.CFAbsoluteTimeGetCurrent() + timeout,
            0,
            shutdown,
            None)

        self.tinfo.rwsched_tasklet.CFRunLoopAddTimer(
            self.tinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.stop_timer,
            self.tinfo.rwsched_instance.CFRunLoopGetMainMode())

        self.tinfo.rwsched_tasklet.CFRunLoopAddTimer(
            self.tinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.asyncio_timer,
            self.tinfo.rwsched_instance.CFRunLoopGetMainMode())

        self.tinfo.rwsched_instance.CFRunLoopRun()

        self.assertTrue(test_done())

    def new_tinfo(self, name):
        """
        Create a new tasklet info instance with a unique instance_id per test.
        It is up to each test to use unique names if more that one tasklet info
        instance is needed.

        @param name - name of the "tasklet"
        @return     - new tasklet info instance
        """
        ret = self.rwmain.new_tasklet_info(name, RWSOTestCase.id_cnt)
        
        log = rift.tasklets.logger_from_tasklet_info(ret)
        log.setLevel(logging.DEBUG)

        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler.setFormatter(fmt)
        log.addHandler(stderr_handler)

        return ret

    def get_cloud_account_msg(self):
        cloud_account = launchpadyang.CloudAccount()
        cloud_account.name = "cloudy"
        cloud_account.account_type = "mock"
        cloud_account.mock.username = "rainy"
        return cloud_account
    
    def get_compute_pool_msg(self, name, pool_type):
        pool_config = rmgryang.ResourcePools()
        pool = pool_config.pools.add()
        pool.name = name
        pool.resource_type = "compute"
        if pool_type == "static":
            # Need to query CAL for resource
            pass
        else:
            pool.max_size = 10
        return pool_config

    def get_network_pool_msg(self, name, pool_type):
        pool_config = rmgryang.ResourcePools()
        pool = pool_config.pools.add()
        pool.name = name
        pool.resource_type = "network"
        if pool_type == "static":
            # Need to query CAL for resource
            pass
        else:
            pool.max_size = 4
        return pool_config


    def get_network_reserve_msg(self, xpath):
        event_id = str(uuid.uuid4())
        msg = rmgryang.VirtualLinkEventData()
        msg.event_id = event_id
        msg.request_info.name = "mynet"
        msg.request_info.subnet = "1.1.1.0/24"
        return msg, xpath.format(event_id)

    def get_compute_reserve_msg(self,xpath):
        event_id = str(uuid.uuid4())
        msg = rmgryang.VDUEventData()
        msg.event_id = event_id
        msg.request_info.name = "mynet"
        msg.request_info.image_id  = "This is a image_id"
        msg.request_info.vm_flavor.vcpu_count = 4
        msg.request_info.vm_flavor.memory_mb = 8192*2
        msg.request_info.vm_flavor.storage_gb = 40
        c1 = msg.request_info.connection_points.add()
        c1.name = "myport1"
        c1.virtual_link_id = "This is a network_id"
        return msg, xpath.format(event_id)
        
    def test_create_resource_pools(self):
        self.log.debug("STARTING - test_create_resource_pools")
        tinfo = self.new_tinfo('poolconfig')
        dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
        pool_xpath = "C,/rw-resource-mgr:resource-mgr-config/rw-resource-mgr:resource-pools"
        pool_records_xpath = "D,/rw-resource-mgr:resource-pool-records"
        account_xpath = "C,/rw-launchpad:cloud-account"
        compute_xpath = "D,/rw-resource-mgr:resource-mgmt/vdu-event/vdu-event-data[event-id='{}']"
        network_xpath = "D,/rw-resource-mgr:resource-mgmt/vlink-event/vlink-event-data[event-id='{}']"
        
        @asyncio.coroutine
        def configure_cloud_account():
            msg = self.get_cloud_account_msg()
            self.log.info("Configuring cloud-account: %s",msg)
            yield from dts.query_create(account_xpath,
                                        rwdts.XactFlag.ADVISE,
                                        msg)
            yield from asyncio.sleep(3, loop=self.loop)
            
        @asyncio.coroutine
        def configure_compute_resource_pools():
            msg = self.get_compute_pool_msg("virtual-compute", "dynamic")
            self.log.info("Configuring compute-resource-pool: %s",msg)
            yield from dts.query_create(pool_xpath,
                                        rwdts.XactFlag.ADVISE,
                                        msg)
            yield from asyncio.sleep(3, loop=self.loop)
            

        @asyncio.coroutine
        def configure_network_resource_pools():
            msg = self.get_network_pool_msg("virtual-network", "dynamic")
            self.log.info("Configuring network-resource-pool: %s",msg)
            yield from dts.query_create(pool_xpath,
                                        rwdts.XactFlag.ADVISE,
                                        msg)
            yield from asyncio.sleep(3, loop=self.loop)
            
        
        @asyncio.coroutine
        def verify_resource_pools():
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
                    self.log.debug("Received Pool Record, Name: %s, Resource Type: %s, Pool Status: %s, Pool Size: %d, Busy Resources: %d",
                                   record.name,
                                   record.resource_type,
                                   record.pool_status,
                                   record.max_size,
                                   record.busy_resources)
        @asyncio.coroutine
        def reserve_network_resources():
            msg,xpath = self.get_network_reserve_msg(network_xpath)
            self.log.debug("Sending create event to network-event xpath %s with msg: %s" % (xpath, msg))
            yield from dts.query_create(xpath, rwdts.XactFlag.TRACE, msg)
            yield from asyncio.sleep(3, loop=self.loop)
            yield from dts.query_delete(xpath, rwdts.XactFlag.TRACE)
            
        @asyncio.coroutine
        def reserve_compute_resources():
            msg,xpath = self.get_compute_reserve_msg(compute_xpath)
            self.log.debug("Sending create event to compute-event xpath %s with msg: %s" % (xpath, msg))
            yield from dts.query_create(xpath, rwdts.XactFlag.TRACE, msg)
            yield from asyncio.sleep(3, loop=self.loop)
            yield from dts.query_delete(xpath, rwdts.XactFlag.TRACE)
        
        @asyncio.coroutine
        def run_test():
            yield from self.wait_tasklets()
            yield from configure_cloud_account()
            yield from configure_compute_resource_pools()
            yield from configure_network_resource_pools()
            yield from verify_resource_pools()
            yield from reserve_network_resources()
            yield from reserve_compute_resources()
            
        future = asyncio.ensure_future(run_test(), loop=self.loop)
        self.run_until(future.done)
        if future.exception() is not None:
            self.log.error("Caught exception during test")
            raise future.exception()

        self.log.debug("DONE - test_create_resource_pools")


def main():
    top_dir = __file__[:__file__.find('/modules/core/')]
    build_dir = os.path.join(top_dir, '.build/modules/core/rwvx/src/core_rwvx-build')
    launchpad_build_dir = os.path.join(top_dir, '.build/modules/core/mc/core_mc-build/rwlaunchpad')

    if 'MESSAGE_BROKER_DIR' not in os.environ:
        os.environ['MESSAGE_BROKER_DIR'] = os.path.join(build_dir, 'rwmsg/plugins/rwmsgbroker-c')

    if 'ROUTER_DIR' not in os.environ:
        os.environ['ROUTER_DIR'] = os.path.join(build_dir, 'rwdts/plugins/rwdtsrouter-c')

    if 'SO_DIR' not in os.environ:
        os.environ['SO_DIR'] = os.path.join(launchpad_build_dir, 'plugins/rwconmantasklet')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()

# vim: sw=4
