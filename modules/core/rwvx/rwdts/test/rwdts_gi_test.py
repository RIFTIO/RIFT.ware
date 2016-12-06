#!/usr/bin/env python3

# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

import argparse
import asyncio
import itertools
import logging
import os
import socket
import sys
import types
import unittest

import xmlrunner

import gi
gi.require_version('RwBaseYang', '1.0')
gi.require_version('CF', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwDtsToyTaskletYang', '1.0')
gi.require_version('RwMain', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('rwlib', '1.0')
gi.require_version('RwVcsYang', '1.0')

import gi.repository.CF as cf
import gi.repository.RwBaseYang as rwbase
import gi.repository.RwDts as rwdts
import gi.repository.RwDtsToyTaskletYang as toyyang
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.rwlib as rwlib
import gi.repository.RwTypes as rwtypes
import gi.repository.RwVcsYang as rwvcs
from gi.repository import ProtobufC

import rift.tasklets
import rift.test.dts

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async

@unittest.skipUnless('FORCE' in os.environ, 'Useless until DTS can actually be deallocated, RIFT-7085')
class DtsTestCase(rift.test.dts.AbstractDTSTest):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    @classmethod
    def configure_schema(cls):
        return toyyang.get_schema()

    def tearDown(self):
        self.loop.stop()
        self.loop.close()

    def test_no_recovery_advance_without_cached_registrations(self):
        """
        Verify if the DTS recovery state machine advances all the way through in cases where 
        there are no registrations with a cache to recover
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        @asyncio.coroutine
        def on_dts_state_change(state):
            """Handle DTS state change

            Take action according to current DTS state to transition application
            into the corresponding application state

            Arguments
                state - current dts state

            """

            print("on_dts_state_change got state ", state, "\n")

            switch = {
                rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
                rwdts.State.CONFIG: rwdts.State.RUN,
            }

            handlers = {
            #    rwdts.State.INIT: self.init,
            #    rwdts.State.RUN: self.run,
            }

            # Transition application to next state
            handler = handlers.get(state, None)
            if handler is not None:
                yield from handler()

            # Transition dts to next state
            next_state = switch.get(state, None)
            if next_state is not None:
                dts.handle.set_state(next_state)

            if next_state is rwdts.State.RUN:
                events[2].set()

        dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop, on_dts_state_change)
        yield from asyncio.sleep(1, loop=self.loop)
        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        results = []

        @asyncio.coroutine
        def pub():
            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath, ret)
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready)

            self.reg = yield from dts.register(
                xpath,
                flags=rwdts.Flag.PUBLISHER,
                handler=handler)
            #events[0].set()
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results
            yield from asyncio.gather(dts.ready.wait(), events[0].wait(), loop=self.loop)

            res_iter = yield from dts.query_read(xpath, flags=rwdts.XactFlag.TRACE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)
        self.assertEqual(dts.handle.get_state(), rwdts.State.RUN);
        self.run_until(events[1].is_set)
        self.assertTrue(len(results) > 0)
        self.assertEqual(results[0], ret)
        self.reg.deregister() 

    def test_no_recovery_advance_with_no_registrations(self):
        """
        Verify if the DTS recovery state machine advances all the way through in cases where 
        there are no registrations at all
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        @asyncio.coroutine
        def on_dts_state_change(state):
            print("on_dts_state_change got state ", state, "\n")

            switch = {
                rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
                rwdts.State.CONFIG: rwdts.State.RUN,
            }

            handlers = {
            #    rwdts.State.INIT: self.init,
            #    rwdts.State.RUN: self.run,
            }

            # Transition application to next state
            handler = handlers.get(state, None)
            if handler is not None:
                yield from handler()

            # Transition dts to next state
            next_state = switch.get(state, None)
            if next_state is not None:
                dts.handle.set_state(next_state)

            if next_state is rwdts.State.RUN:
                events[2].set()

        dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop, on_dts_state_change)
        yield from asyncio.sleep(1, loop=self.loop)

        @asyncio.coroutine
        def pub():
            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()
            events[0].set()
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            yield from asyncio.gather(dts.ready.wait(), events[0].wait(), loop=self.loop)
            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)
        self.assertEqual(dts.handle.get_state(), rwdts.State.RUN);
        self.run_until(events[1].is_set)


    def test_no_prepare(self):
        """
        Verify if NO_PREP_READ flag is added in case a Publisher is registered
        with no on_prepare callback.

        The test does now have any explicit assert statements, the result status
        is determined by test_done timeout.
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        results = []

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath, ret)
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready)

            reg = yield from dts.register(
                xpath,
                flags=rwdts.Flag.PUBLISHER,
                handler=handler)
            #events[0].set()
            yield from events[1].wait()
            reg.deregister()


        @asyncio.coroutine
        def sub():
            nonlocal results
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from asyncio.gather(dts.ready.wait(), events[0].wait(), loop=self.loop)

            res_iter = yield from dts.query_read(xpath, flags=0)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)
        self.assertTrue(len(results) > 0)
        self.assertEqual(results[0], ret)

    def test_local_pub_sub(self):
        """
        Verify that pub/sub is working when both publisher and subscriber are
        using the same dts api handle.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_local_pub_sub")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(*args):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
#               Read query not transactional
                self.assertFalse(xact_info.transactional())
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)

            self.reg = yield from dts.register(
                xpath,
                flags=rwdts.Flag.PUBLISHER,
                handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from asyncio.gather(dts.ready.wait(), events[0].wait(), loop=self.loop)

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(1, len(results))
        self.assertEqual(str(results[0]), str(ret))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_local_pub_sub")

    def test_local_pub_w_block(self):
        """
        Verify that pub/sub is working when both publisher and subscriber are
        using the same dts api handle.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_local_pub_w_block")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_local_pub_w_block'
        results = {
            'corrid': None,
            'read': [],
            'failures': 0,
        }

        dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop)

        @asyncio.coroutine
        def pub():
            #tinfo = self.new_tinfo('pub')
            #dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            prepares = 0

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prepares

                if prepares == 0:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                else:
                    xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)
                prepares += 1


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            nonlocal results

            #tinfo = self.new_tinfo('sub')
            #dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_read(xpath)
                results['corrid'] = block.add_query_read(xpath)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    try:
                        r = yield from i
                        results['read'].append(str(r.result))
                    except rift.tasklets.dts.TransactionFailed:
                        results['failures'] += 1

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        print(results)
        self.assertEqual(results['corrid'], 2)
        self.assertEqual(len(results['read']), 1)
        self.assertEqual(results['read'][0], str(ret))
        self.assertEqual(results['failures'], 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_local_pub_w_block")


    def test_separate_pub_sub(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_separate_pub_sub")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            @asyncio.coroutine
            def on_ready(*args):
                yield from asyncio.sleep(1, loop=self.loop)
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(1, len(results))
        self.assertEqual(str(results[0]), str(ret))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_separate_pub_sub")

    def test_pub_list(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_pub_list")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]



        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)

                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

            reg.deregister() 

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)


            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)
        self.assertEqual(2, len(results))
        print(results[0])
        self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_pub_list0')
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_pub_list")

    def test_2_pub_sublist(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_2_pub_sublist")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]



        #employees = [toyyang.Employee() for _ in range(2)]
        #for i, employee in enumerate(employees):
            #employee.name = self.id() + str(i)

        company_profile = toyyang.Company().create_profile()
        company_profile.revenue = '10000'

        sub_xpath = 'D,/rw-dts-toy-tasklet:company'
        pub1_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile'
        pub2_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB2"]/profile'

        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(
                        pub1_xpath,
                        company_profile)

                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            self.reg1 = yield from dts.register(
                    pub1_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(
                        pub2_xpath,
                        company_profile)

                events[2].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            self.reg2 = yield from dts.register(
                    pub2_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()


        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            yield from events[2].wait()

            res_iter = yield from dts.query_read(sub_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[1].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)


        self.assertEqual(2, len(results))
        print(str(results[0]), str(results[1]))
        #self.assertEqual(results[0].name, '__main__.DtsTestCase.test_pub_list0')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_2_pub_sublist")

    def test_ident_pub_2_sublist(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_2_pub_sublist")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]



        company_profile1 = toyyang.Company().create_profile()
        company_profile2 = toyyang.Company().create_profile()
        company_profile1.revenue = '10000'
        company_profile2.revenue = '20000'

        sub_xpath =  'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile/office-locations[rw-dts-toy-tasklet:location-code="1"]'
        pub1_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile'
        pub2_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB2"]/profile'
        key1_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile/office-locations[rw-dts-toy-tasklet:location-code="1"]'
        key2_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB2"]/profile/office-locations[rw-dts-toy-tasklet:location-code="1"]'

#       sub_xpath =  'D,/rw-dts-toy-tasklet:company'
#       pub1_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile/office-locations[rw-dts-toy-tasklet:location-code="1"]'
#       pub2_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile/office-locations[rw-dts-toy-tasklet:location-code="2"]'
        
        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(
                        pub1_xpath,
                        company_profile1)

                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            key, keylength = dts.ident_key(key1_xpath,2)
            self.reg1 = yield from dts.register(
                    pub1_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ|rwdts.Flag.SHARED,
                    flavor=rwdts.ShardFlavor.IDENT, keyin=key, keylen=keylength,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(
                        pub2_xpath,
                        company_profile2)

                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            key, keylength = dts.ident_key(key2_xpath,2)
            self.reg2 = yield from dts.register(
                    pub2_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ|rwdts.Flag.SHARED,
                    flavor=rwdts.ShardFlavor.IDENT, keyin=key, keylen=keylength,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[1].wait()

            res_iter = yield from dts.query_read(sub_xpath, rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[2].set()
        
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=50)
        self.reg1.deregister()
        self.reg2.deregister()

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_2_ident_pub_sublist")

    def test_1_pub_2_sublist(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_1_pub_2_sublist")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]


        company_profile = toyyang.Company().create_profile()
        company_profile.revenue = '10000'

        sub_xpath = 'D,/rw-dts-toy-tasklet:company'
        reg1_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="REG1"]/profile'
        reg2_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="REG2"]/profile'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready_reg1(regh, status):
                regh.create_element(
                        reg1_xpath,
                        company_profile)

                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready_reg1)
            self.reg1 = yield from dts.register(
                    reg1_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            @asyncio.coroutine
            def on_ready_reg2(regh, status):
                regh.create_element(
                        reg2_xpath,
                        company_profile)

                events[2].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready_reg2)
            self.reg2 = yield from dts.register(
                    reg2_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()


        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            yield from events[2].wait()

            res_iter = yield from dts.query_read(sub_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)


        self.assertEqual(2, len(results))
        print(str(results[0]), str(results[1]))
        #self.assertEqual(results[0].name, '__main__.DtsTestCase.test_pub_list0')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_1_pub_2_sublist")

    def test_na_on_prepare(self):
        """
        Verify that the publisher can issue a response with the NA return code.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_na_on_prepare")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        @asyncio.coroutine
        def pub():
            @asyncio.coroutine
            def on_ready(*args):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.NA, xpath)

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(0, len(results))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_na_on_prepare")

    def test_nack_on_prepare(self):
        """
        Verify that the publisher can issue a response with the NACK return code.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_nack_on_prepare")
        results = []
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        @asyncio.coroutine
        def pub():
            @asyncio.coroutine
            def on_ready(*args):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results
            nonlocal exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(0, len(results))
        self.assertEqual(exceptions, 1)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_nack_on_prepare")

    def test_registration_element(self):
        """
        Verify that the element functions on a registration work

        The test will progress through stages defined by the events list:
            0:  The publisher is up and hit the on_ready() callback
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.query_read() iterator
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_registration_element")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath, ret)
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready)
            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                results.append(regh.get_element(xpath))
                print("Inside sub on_Ready")
                print(len(results))
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready)
            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            res_iter = yield from dts.query_read(
                    xpath,
                    rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0]), str(ret))
        self.assertEqual(str(results[1]), str(ret))
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_registration_element")

    def test_precommit(self):
        """
        Verify that precommit/commit works; the update() triggers a precommit/commit

        The test will progress through stages defined by the events list:
            0:  The publisher is up and hit the on_ready() callback
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.query_read() iterator
            3:  The publisher finished with the update(), which causes precommit/commit
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_precommit")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from dts.query_create(xpath, rwdts.XactFlag.ADVISE, ret1)
            events[0].set()
            yield from events[1].wait()
            yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret2)
            events[3].set()

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[3].wait()
            results.append(self.reg.get_element(xpath))
            res_iter = yield from dts.query_read(
                    xpath,
                    rwdts.XactFlag.MERGE|rwdts.Flag.CACHE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 1)
        self.assertEqual(str(results[0]), str(ret2))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_precommit")

    def test_prepare_failure(self):
        """
        Verify that error in prepare callback returns a NACK flag and
        raises a TransactionAborted exception ultimately.

        Also tests if the exception is send back to the the client.
        1\ As exception message
        2\ and fetched using the query_error API
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_prepare_failure")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id() + '1'

        failures = 0
        expected_exp_message = "Test Error"
        expected_exp_message1 = "sending nack response"
        exp_message = None
        error_msgs = []

        @asyncio.coroutine
        def pub():
            nonlocal failures, exp_message, error_msgs
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            try:
                yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret)
            except rift.tasklets.dts.TransactionAborted as e:
                failures += 1
                exp_message = str(e)
                for path, status, err_msg in e.query_error():
                    error_msgs.append(err_msg)
            events[1].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                raise ValueError("Test Error")

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_message in exp_message)
#       self.assertEqual(len(error_msgs), 2)
        self.assertTrue(expected_exp_message in error_msgs[0])
#       self.assertTrue(expected_exp_message1 in error_msgs[1])
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE -test_prepare_failure")

    def test_precommit_failure(self):
        """
        Verify that error in precommit callback returns a NOT_OK flag and
        raises a TransactionAborted exception ultimately.

        Also verifies that the exception is send back to the client
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_precommit_failure")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id() + '1'

        failures = 0
        expected_exp_message = "Test Error"
        exp_message = None

        @asyncio.coroutine
        def pub():
            nonlocal failures, exp_message
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            try:
                yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret)
            except rift.tasklets.dts.TransactionAborted as e:
                failures += 1
                exp_message = str(e)

            events[1].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                raise ValueError("Test Error")

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_message in exp_message)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_precommit_failure")

    def test_precommit_w_blocks(self):
        """
        Verify that precommit/commit works; the update() triggers a precommit/commit

        The test will progress through stages defined by the events list:
            0:  The publisher is up and hit the on_ready() callback
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.query_read() iterator
            3:  The publisher finished with the update(), which causes precommit/commit
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_precommit_w_blocks")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                corrid = block.add_query_create(xpath, ret1, rwdts.XactFlag.ADVISE)

                #block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)
                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

            itr = yield from transaction.commit()
            for r in itr:
                result = yield from r

            print('------PUB: dts.create DONE')

            events[0].set()
            yield from events[1].wait()


            print('------PUB: block.update STARTING')
            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                corrid = block.add_query_update(xpath, ret2, rwdts.XactFlag.ADVISE)

                print('------PUB: add_update DONE')
                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

            itr = yield from transaction.commit()
            for r in itr:
                result = yield from r

            print('------PUB: dts.update DONE')
            events[3].set()

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[3].wait()
            results.append(reg.get_element(xpath))
            res_iter = yield from dts.query_read(
                    xpath,
                    rwdts.XactFlag.MERGE|rwdts.Flag.CACHE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=60)

        self.assertEqual(len(results), 1)
        self.assertEqual(str(results[0]), str(ret2))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_precommit_w_blocks")

    def test_both_commit_and_read(self):
        """
        Verify that precommit/commit works as well as read - via PUB works;
        The update() triggers a precommit/commit - this sends ret2
        The read() is creating a transaction to the PUB - this returns ret1

        The test will progress through stages defined by the events list:
            0:  The publisher created the xpath element
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.query_read() iterator
            3:  The publisher is up and hit the on_ready() callback
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_both_commit_and_read")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from dts.query_create(xpath, rwdts.XactFlag.ADVISE, ret1)
            events[0].set()

            yield from events[1].wait()
            yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret2)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[3].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret1)


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[3].wait()

            results.append(self.reg2.get_element(xpath))

            res_iter = yield from dts.query_read(
                    xpath,
                    rwdts.XactFlag.MERGE)
            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0]), str(ret2))
        self.assertEqual(str(results[1]), str(ret1))
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_both_commit_and_read")

    def test_abort(self):
        """
        Verify that abort exception coming back on dts.query_update()
        when the subscriber is sending a NACK in _prepare()

        The test will progress through stages defined by the events list:
            0:  The publisher created the xpath element
            1:  The subscriber is up and hit the on_ready() callback
            2:  The publisher finished running through
            3:  The subscriber finished running through
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_abort")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        abort_exception = False
        abort_callback = False

        @asyncio.coroutine
        def pub():
            nonlocal abort_exception

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from dts.query_create(xpath, rwdts.XactFlag.ADVISE, ret)
            events[0].set()
            yield from events[1].wait()

            try:
                yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret)
            except rift.tasklets.dts.TransactionAborted:
                abort_exception = True
            events[2].set()

            yield from events[3].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_abort(*args):
                nonlocal abort_callback

                abort_callback = True
                return rwdts.MemberRspCode.ACTION_OK

            def on_precommit(*args):
                return rwdts.MemberRspCode.ACTION_NOT_OK

            def on_commit(*args):
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_abort=on_abort,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit
                )

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[2].wait()

            events[3].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[3].is_set)

        self.assertTrue(abort_exception)
        self.assertTrue(abort_callback)
        self.assertEqual(len(results), 0)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_abort")

    def test_appconf_prepare_exception(self):
        """
        Verify an expection in on_prepare deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_prepare_exception")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        failures = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, failures
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            try:
                yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                failures += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                pass

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                raise ValueError("Test Error")

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_prepare_exception")

    def test_appconf_apply_exception(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_apply_exception")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        failures = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, failures
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            try:
                yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                failures += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                pass

            def on_apply(dts, acg, xact, action, scratch):
                if xact.handle is not None:
                    raise ValueError("Test Error")

                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_apply_exception")

    def test_appconf_validate_exception(self):
        """
        Verify an expection on deinit callback causes a transaction to abort
        Also verify if the exception message is sent back to the client
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_validate_exception")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        failures = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, failures
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            try:
                yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                failures += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                pass

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            def on_validate(*args):
                raise ValueError("Test Error")

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply,
                    on_validate=on_validate)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_validate_exception")

    def test_appconf_init_exception(self):
        """
        Verify an exception in init callback triggers a trasaction exception. Also verify if the
        exception message is sent back to the client.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_init_exception")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        failures = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, failures
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            try:
                yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                failures += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                raise ValueError("Test Error")

            def on_deinit(acg, xact, scratch):
                pass

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_init_exception")

    def test_appconf_group(self):
        """
        Verify that we hit all stages in an appconf group with a single
        registration, that the callback arguments are all wrapped correctly and
        that we get the correct configuration.

        The test will progress through stages defined by the events list:
            0:  Subscriber has created the appconf group and hit the first
                on_apply callback.
            1:  Publisher has finished dts.query_update()
            2:  Subscriber finished handling the dts.query_update()
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_group")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'init': '',
            'deinit': '',
            'validate': '',
            'apply': '',
            'on_prepare': '',
            'apply_action_1': None,
            'apply_action_2': None,
            'elements_iter': [],
            'scratch': {}
        }

        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()
        expected_scratch = {'on_validate': '1', 'on_apply': '1', 'msg': str(ret)}


        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                results['init'] = '%s' % (type(acg),)

            def on_deinit(acg, xact, scratch):
                results['deinit'] = '%s' % (type(acg),)

                for reg in acg.registrations:
                    for e in reg.elements:
                        results['elements_iter'].append(str(e))

                results['scratch'] = scratch
                events[1].set()

            def on_validate(dts, acg, xact, scratch):
                results['validate'] = '%s %s' % (type(dts), type(acg))
                scratch['on_validate'] = '1'

            def on_apply(dts, acg, xact, action, scratch):
                if not events[0].is_set():
                    # This will be called once the appconf group is actually subscribed
                    # with the router.
                    results['apply_action_1'] = action
                    events[0].set()
                else:
                    results['apply'] = '%s %s' % (type(dts), type(acg))
                    results['apply_action_2'] = action
                    scratch['on_apply'] = '1'


            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_validate=on_validate,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_ok(xact_info.handle)
                results['on_prepare'] = str(msg)
                results['xact_id'] = xact.id
                scratch['msg'] = str(msg)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['init'], str(rift.tasklets.AppConfGroup))
        self.assertEqual(results['deinit'], str(rift.tasklets.AppConfGroup))
        self.assertEqual(results['validate'], '%s %s' % (str(rift.tasklets.DTS), str(rift.tasklets.AppConfGroup)))
        self.assertEqual(results['apply'], '%s %s' % (str(rift.tasklets.DTS), str(rift.tasklets.AppConfGroup)))
        self.assertEqual(results['apply_action_1'], rwdts.AppconfAction.INSTALL)
        self.assertEqual(results['apply_action_2'], rwdts.AppconfAction.RECONCILE)
        self.assertEqual(len(results['elements_iter']), 1)
        self.assertEqual(results['elements_iter'][0], str(ret))
        self.assertEqual(str(ret), results['on_prepare'])


        self.assertEqual(expected_scratch, results['scratch'])
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_group")

    def test_group_registration(self):
        """
        Registers a publisher with 2 implementations and verifies the following

        1. Ensure the callbacks for each published gets called
        2. The on_init callback get called.

        """

        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_group_registration")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id()

        another_xpath = '/rw-dts-toy-tasklet:another-container'

        ret2 = toyyang.AnotherContainer()
        ret2.val = self.id()

        results = []
        on_init_callback = 0

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare1(xact_info, *args):
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        xpath,
                        ret1)

            @asyncio.coroutine
            def on_prepare2(xact_info, *args):
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        another_xpath,
                        ret2)

            def on_init(*args):
                nonlocal on_init_callback
                on_init_callback += 1

            handlers = rift.tasklets.Group.Handler(
                    on_init=on_init)

            reg_handler1 = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare1)

            reg_handler2 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready, on_prepare=on_prepare2)

            with dts.group_create(handler=handlers) as greg:
                greg.register(xpath,
                        reg_handler1,
                        flags=rwdts.Flag.PUBLISHER)

                greg.register(another_xpath,
                        reg_handler2,
                        flags=rwdts.Flag.PUBLISHER)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)
            ares_iter = yield from dts.query_read(another_xpath, rwdts.XactFlag.MERGE)

            for f in itertools.chain(res_iter, ares_iter):
                result = yield from f
                results.append(result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)
        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0].result), str(ret1))
        self.assertEqual(str(results[1].result), str(ret2))
        self.assertEqual(on_init_callback, 2)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_group_registration")

    def test_group_reg_subquery(self):
        """
        Registers a publisher with 2 implementations and verifies the following

        1. Ensure the callbacks for each published gets called
        2. The on_init callback get called.

        Flow:
        sub -->(Query:Read)<-->pub2(xpath1)
        |
         ----->(Query:Read)<-->pub2(another_xpath)<-(Sub-Query)->pub1(y_another_xpath)

        Events:
        1. event[0] & event[1] are triggered once the pubs are UP
        2. event[2] is triggered once the sub is done reading

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_group_registration")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        xpath = '/rw-dts-toy-tasklet:another-container'

        ret1 = toyyang.AnotherContainer()
        ret1.val = self.id()

        another_xpath = 'C,/rw-dts-toy-tasklet:a-container'
        y_another_xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id()

        results = []
        on_init_callback = 0

        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(*args):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------PUB1:on_prepare', args)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        y_another_xpath,
                        ret2)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready, on_prepare=on_prepare)

            self.reg = yield from dts.register(
                    y_another_xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------PUB2:on_ready', status)
                events[1].set()

            @asyncio.coroutine
            def on_prepare1(xact_info, *args):
                print('------PUB2:on_prepare1', args)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        xpath,
                        ret1)

            @asyncio.coroutine
            def on_prepare2(xact_info, *args):
                print('------PUB2:on_prepare2', args)
                # Sub query
                itr = yield from dts.query_read(y_another_xpath)
                for f in itr:
                    result = yield from f
                    break
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        another_xpath,
                        result.result)

            def on_init(*args):
                nonlocal on_init_callback
                print('------GROUP:on_init', args)
                on_init_callback += 1

            handlers = rift.tasklets.Group.Handler(
                    on_init=on_init)

            reg_handler1 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready, on_prepare=on_prepare1)

            reg_handler2 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready, on_prepare=on_prepare2)

            with dts.group_create(handler=handlers) as greg:
                greg.register(xpath,
                        reg_handler1,
                        flags=rwdts.Flag.PUBLISHER)

                greg.register(another_xpath,
                        reg_handler2,
                        flags=rwdts.Flag.PUBLISHER)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from asyncio.gather(events[0].wait(), events[1].wait(),
                    loop=self.loop)

            res_iter = yield from dts.query_read(xpath)
            ares_iter = yield from dts.query_read(another_xpath)

            for f in itertools.chain(res_iter, ares_iter):
                result = yield from f
                results.append(result)

            events[2].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)
        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0].result), str(ret1))
        self.assertEqual(str(results[1].result), str(ret2))
        self.assertEqual(on_init_callback, 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_group_registration")

    def test_group_registration_event_failure(self):
        """
        Verifies the following:
        1\ Exception in on_event, triggers TransactionAborted
        2\ The exception message is sent back to the client
        3\ The on_abort get called
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_group_registration_event_failure")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        client_failure = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        abort_callback = 0

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, client_failure
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            try:
                yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret1)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                client_failure += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                nonlocal abort_callback
                abort_callback += 1
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_init(group, *args):
                print('------GROUP:on_init', args)

            def on_event(dts, g_reg, xact, xact_event, scratch_data):
                print('------GROUP:on_event')
                raise ValueError("Test Error")

            def on_deinit(g_reg, xact, scratch_raw):
                print('------GROUP:on_deinit', xact)

            handlers = rift.tasklets.Group.Handler(
                    on_init=on_init,  on_deinit=on_deinit, on_event=on_event)

            reg_handle = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare,
                    on_precommit=on_precommit,
                    on_commit=on_commit,
                    on_abort=on_abort)

            reg = None
            with dts.group_create(handler=handlers) as greg:
                reg = greg.register(xpath,
                        reg_handle,
                        flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(abort_callback, 1)
        self.assertEqual(client_failure, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_group_registration_event_failure")

    def test_group_registration_init_failure(self):
        """
        Verifies the following:
        1\ Exception in on_init, triggers TransactionAborted
        2\ The exception message is sent back to the client
        3\ The on_abort get called
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_group_registration_init_failure")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        client_failure = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        abort_callback = 0

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, client_failure
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            try:
                yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret1)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                client_failure += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                nonlocal abort_callback
                abort_callback += 1
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_init(group, *args):
                print('------GROUP:on_init', args)
                raise ValueError("Test Errordts.")

            def on_event(dts, g_reg, xact, xact_event, scratch_data):
                print('------GROUP:on_event')
                return rwdts.MemberRspCode.ACTION_OK

            def on_deinit(g_reg, xact, scratch_raw):
                print('------GROUP:on_deinit', xact)

            handlers = rift.tasklets.Group.Handler(
                    on_init=on_init,  on_deinit=on_deinit, on_event=on_event)

            reg_handle = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare,
                    on_precommit=on_precommit,
                    on_commit=on_commit,
                    on_abort=on_abort)

            reg = None
            with dts.group_create(handler=handlers) as greg:
                reg = greg.register(xpath,
                        reg_handle,
                        flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(abort_callback, 1)
        self.assertEqual(client_failure, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_group_registration_init_failure")


    def test_group_registration_commit(self):
        """
        Verifies the following for Group api (Happy case!)
        1. Ensures that on_event/on_deinit is called
        2. Members in Group receive on_commit/on_precommit
        3. Ensures the scratch created for a transaction is passed over to
           the on_event callback

        The test uses 3 events:
        1. event[0] is triggered once the pub has started the transaction
        2. event[1] is triggered once the sub is up and ready
        3. Pub waits for event[1], upon completion triggers and update query

        The test is complete once event[2] is triggered by the sub on deinit

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_group_registration_commit")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        on_precommit_callback = 0
        on_commit_callback = 0

        expected_scratch = {"foo": "bar"}
        scratch = None

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from dts.query_create(xpath, rwdts.XactFlag.ADVISE, ret1)
            events[0].set()
            yield from events[1].wait()
            yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret2)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                nonlocal on_precommit_callback
                on_precommit_callback += 1
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                nonlocal on_commit_callback
                on_commit_callback += 1
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_init(group, *args):
                print('------GROUP:on_init', args)

            def on_event(dts, g_reg, xact, xact_event, scratch_data):
                print('------GROUP:on_event')
                scratch_data['foo'] = 'bar'
                return rwdts.MemberRspCode.ACTION_OK

            def on_deinit(g_reg, xact, scratch_raw):
                nonlocal scratch
                print('------GROUP:on_deinit', xact)
                scratch = scratch_raw
                for reg in g_reg.registrations:
                    for e in reg.elements:
                        results.append(e)
                events[2].set()

            handlers = rift.tasklets.Group.Handler(
                    on_init=on_init,  on_deinit=on_deinit, on_event=on_event)

            reg_handle = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare,
                    on_precommit=on_precommit,
                    on_commit=on_commit,
                    on_abort=on_abort)

            reg = None
            with dts.group_create(handler=handlers) as greg:
                reg = greg.register(xpath,
                        reg_handle,
                        flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 1)
        self.assertEqual(on_precommit_callback, 1)
        self.assertEqual(on_commit_callback, 1)
        self.assertEqual(str(results[0]), str(ret2))
        self.assertEqual(scratch, expected_scratch)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_group_registration_commit")

    def test_group_registration_nack(self):
        """
        Verifies the following for the Group api

        1. on_event is called
        2. If NOT_OK flag in on_event triggers a transaction abort
        3. Abort exception is raised?

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_group_registration_nack")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + "1"

        results = []
        abort_callback = 0
        abort_exception = False

        @asyncio.coroutine
        def pub():
            nonlocal abort_exception
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from dts.query_create(xpath, rwdts.XactFlag.ADVISE, ret)
            events[0].set()
            yield from events[1].wait()

            try:
                yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE, ret1)
            except rift.tasklets.dts.TransactionAborted:
                abort_exception = True
                events[2].set()

            yield from events[2].wait()
            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            def on_init(*args):
                print('------GROUP:on_init', args)

            def on_event(dts, grp, xact, event, scratch):
                print('------GROUP:on_event', event)
                if event == rwdts.MemberEvent.PRECOMMIT:
                    return rwdts.MemberRspCode.ACTION_NOT_OK
                return rwdts.MemberRspCode.ACTION_OK

            def on_deinit(*args):
                print('------GROUP:on_deinit', *args)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                nonlocal abort_callback
                abort_callback += 1
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handlers = rift.tasklets.Group.Handler(
                    on_init=on_init, on_event=on_event, on_deinit=on_deinit)

            reg_handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare,
                    on_precommit=on_precommit,
                    on_commit=on_commit,
                    on_abort=on_abort)

            with dts.group_create(handler=handlers) as greg:
                greg.register(xpath, reg_handler,
                        flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE)
                greg.register(xpath, reg_handler,
                        flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE)

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[3].is_set)
        self.assertEqual(len(results), 0)
        self.assertEqual(abort_callback, 2)
        self.assertTrue(abort_exception)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_group_registration_nack")


    def test_simple_block(self):
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_simple_block")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_simple_block'
        results = {
            'corrid': None,
            'read': [],
            'failures': 0,
        }

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            prepares = 0

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prepares

                if prepares == 0:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                else:
                    xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)
                prepares += 1


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_read(xpath)
                results['corrid'] = block.add_query_read(xpath)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    try:
                        r = yield from i
                        results['read'].append(str(r.result))
                    except rift.tasklets.dts.TransactionFailed:
                        results['failures'] += 1

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(results['corrid'], 2)
        self.assertEqual(len(results['read']), 1)
        self.assertEqual(results['read'][0], str(ret))
        self.assertEqual(results['failures'], 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_simple_block")

    def test_simple_block_corrid(self):
        """
        Test coverage:
        1. Ensure CORRID based fetching works
        2. Ensure that the iterators are exhausted accordingly
        3. Ensure ANY_CORRID based fetch works even in partial conditions (i.e)
           when other corrid are done
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_simple_block_corrid")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_simple_block_corrid'

        results = {
            'corrid': None,
            'read_1': [],
            'read_2': [],
            'read_3': [],
            'failures': 0,
        }

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            prepares = 0

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prepares

                if prepares == 0:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                else:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                prepares += 1


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_read(xpath)
                results['corrid'] = block.add_query_read(xpath)
                results['corrid'] = block.add_query_read(xpath)

                res_iter = yield from block.execute(
                        flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE,
                        now=True)

                # Query only corrid 1
                for i in res_iter.query_result(1):
                    r = yield from i
                    results['read_1'].append(str(r.result))

                # Query only corrid2
                for i in res_iter.query_result(2):
                    r = yield from i
                    results['read_2'].append(str(r.result))

                # Query corrid 3 using *any* flag, Only corrid 3 must be
                # there as we have exhaused corrid 1 & 2's iterator
                for i in res_iter.query_result(0):
                    r = yield from i
                    results['read_3'].append(str(r.result))

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(results['corrid'], 3)
        self.assertEqual(len(results['read_1']), 1)
        self.assertEqual(len(results['read_2']), 1)
        self.assertEqual(len(results['read_3']), 1)
        self.assertEqual(results['read_1'][0], str(ret))
        self.assertEqual(results['read_2'][0], str(ret))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_simple_block_corrid")

    def test_block_corrid_failure(self):
        """
        Tests the following:
        1. Ensure ANY_CORRID fetching by using the BRI direcly
        2. Ensure Read are flowing through even during failures
        3. Ensure only one failure message is triggered per block
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_block_corrid_failure")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_block_corrid_failure'

        results = {
            'corrid': None,
            'read': [],
            'failures': 0,
        }

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            prepares = 0

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prepares

                if prepares == 0:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                else:
                    xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath, ret)
                prepares += 1


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_read(xpath)
                results['corrid'] = block.add_query_read(xpath)

                res_iter = yield from block.execute(
                        flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE,
                        now=True)

                # Query only corrid 1
                for i in res_iter:
                    try:
                        r = yield from i
                        results['read'].append(str(r.result))
                    except rift.tasklets.dts.TransactionFailed:
                        results['failures'] += 1

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(results['corrid'], 2)
        self.assertEqual(len(results['read']), 1)
        self.assertEqual(results['read'][0], str(ret))
        self.assertEqual(results['failures'], 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_block_corrid_failure")

    def test_multiple_block(self):
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_multiple_block")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_multiple_block'

        results = {'failures': 0, 'reads': []}


        @asyncio.coroutine
        def pub():
            prepares = 0
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prepares

                if prepares == 0:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                else:
                    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
                prepares += 1

            def on_precommit(*args):
                return rwdts.MemberRspCode.ACTION_NOT_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare,
                    on_precommit=on_precommit)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            nonlocal results
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create() as xact:
                block = xact.block_create()
                corrid = block._add_query(xpath, rwdts.QueryAction.READ)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    r = yield from i
                    results['reads'].append(r)

                sec_block = xact.block_create()
                corrid = sec_block._add_query(xpath, rwdts.QueryAction.READ)

                res_iter = yield from sec_block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    r = yield from i
                    results['reads'].append(r)

                third_block = xact.block_create()
                corrid = third_block._add_query(xpath, rwdts.QueryAction.CREATE, msg=ret)

                res_iter = yield from third_block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)
                for i in res_iter:
                    r = yield from i

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionAborted:
                    results['failures'] += 1
            qri = yield from transaction.commit()

            # BUG we should here be able to wait for the dts.transaction to actually be fully committed
            # There will have been an xact_state callback with status.xact_done == TRUE, and the actual 
            # status is COMMIT / ABORT.
            #
            # Probably this should be automagically handled in dts.transaction context manager???

            # This currently tends to fire (and end the test) typically before the xact is really truely done.
            # Most visible as a lack of TRACE printout from the client API, as happens at the end of xact
            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set, timeout=10)

        self.assertEqual(results['failures'], 1)
        self.assertEqual(len(results['reads']), 2)
        self.assertEqual(results['reads'][0].result, ret)
        self.assertEqual(results['reads'][1].result, ret)
        self.reg.deregister()

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_multiple_block")

    def test_send_error(self):
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_send_error")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'
        error_xpath = '/rw-dts-toy-tasklet:employee'

        ret = toyyang.AContainer()
        ret.a_string = 'test_send_error'
        results = {
            'corrid': None,
            'read': [],
            'failures': 0,
        }

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            prepares = 0

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prepares

                xact_info.send_error_xpath(rwtypes.RwStatus.NOTFOUND, error_xpath, 'Error: Blah Blah Blah')
                xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)
                prepares += 1


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_read(xpath)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    try:
                        r = yield from i
                        results['read'].append(str(r.result))
                    except rift.tasklets.dts.TransactionFailed:
                        err_cause, errstr = xact.get_error_heuristic(results['corrid'])
                        #print('err_cause=%s, errstr=%s' % (str(err_cause), str(errstr)))
                        results['failures'] += 1

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1
            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        print(results)
        self.assertEqual(results['corrid'], 1)
        self.assertEqual(results['failures'], 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_send_error")

    def test_rpc(self):
        """
        Test that RPC input/output works.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.query_rpc() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_rpc")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = self.id() + 'req'

        resp = toyyang.ToyRpcOutput()
        resp.ret = self.id() + 'resp'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub():
            nonlocal results
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        out_xpath,
                        resp)


            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            rpc_iter = yield from dts.query_rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                results['output'] = str(resp.result)
                results['n_results'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['input'], str(req))
        self.assertEqual(results['output'], str(resp))
        self.assertEqual(results['n_results'], 1)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_rpc")

    def test_rpc_nack(self):
        """
        Test that RPC input/output works; test for NACK.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.query_rpc() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_rpc_nack")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input': '',
            'output': '',
            'n_results': 0,
            'transactions_failed': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = self.id() + 'req'

        resp = toyyang.ToyRpcOutput()
        resp.ret = self.id() + 'resp'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub():
            nonlocal results
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.NACK,
                        out_xpath,
                        resp)


            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            rpc_iter = yield from dts.query_rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                try:
                    resp = yield from i
                    results['output'] = str(resp.result)
                    results['n_results'] += 1
                except rift.tasklets.dts.TransactionFailed:
                    results['transactions_failed'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['input'], str(req))
        self.assertEqual(results['output'], '')
        self.assertEqual(results['n_results'], 0)
        self.assertEqual(results['transactions_failed'], 1)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_rpc_nack")

    def test_rpc_na(self):
        """
        Test that RPC input/output works; test for NA.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.query_rpc() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_rpc_na")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = self.id() + 'req'

        resp = toyyang.ToyRpcOutput()
        resp.ret = self.id() + 'resp'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub():
            nonlocal results
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.NA,
                        out_xpath,
                        resp)

            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            rpc_iter = yield from dts.query_rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                results['output'] = str(resp.result)
                results['n_results'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['input'], str(req))
        self.assertEqual(results['output'], '')
        self.assertEqual(results['n_results'], 0)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_rpc_na")

    def test_rpc_ack_and_nack(self):
        """
        Test that RPC input/output works. Test with one pub sending ACK and
        another one sending NACK.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.query_rpc() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_rpc_ack_and_nack")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = {
            'input1': '',
            'input2': '',
            'output': '',
            'n_results': 0,
            'transactions_failed': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = self.id() + 'req'

        resp = toyyang.ToyRpcOutput()
        resp.ret = self.id() + 'resp'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub1():
            nonlocal results
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input1'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        out_xpath,
                        resp)

            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg1 = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def pub2():
            nonlocal results
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input2'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.NACK,
                        out_xpath,
                        resp)


            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg2 = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            rpc_iter = yield from dts.query_rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                try:
                    resp = yield from i
                    results['output'] = str(resp.result)
                    results['n_results'] += 1
                except rift.tasklets.dts.TransactionFailed:
                    results['transactions_failed'] += 1

            events[2].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[2].is_set)

        self.assertEqual(results['input1'], str(req))
        self.assertEqual(results['input2'], str(req))
        self.assertEqual(results['output'], str(resp))
        self.assertEqual(results['n_results'], 1)
        self.assertEqual(results['transactions_failed'], 1)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_rpc_ack_and_nack")

    def test_rpc_ack_and_na(self):
        """
        Test that RPC input/output works. Test with one pub sending ACK and
        another one sending NA.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.query_rpc() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_rpc_ack_and_na")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = {
            'input1': '',
            'input2': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = self.id() + 'req'

        resp = toyyang.ToyRpcOutput()
        resp.ret = self.id() + 'resp'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub1():
            nonlocal results
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input1'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        out_xpath,
                        resp)

            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg1 = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def pub2():
            nonlocal results
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input2'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.NA,
                        out_xpath,
                        resp)

            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg2 = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            rpc_iter = yield from dts.query_rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                results['output'] = str(resp.result)
                results['n_results'] += 1

            events[2].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[2].is_set)

        self.assertEqual(results['input1'], str(req))
        self.assertEqual(results['input2'], str(req))
        self.assertEqual(results['output'], str(resp))
        self.assertEqual(results['n_results'], 1)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_rpc_ack_and_na")

    def test_multiple_queries(self):
        """
        Test running multiple queries at the same time.

        The test will progress through stages defined by the events list:
            0:  First publisher is up and hit on_ready
            1:  Second publisher is up and hit on_ready
            2:  Subscriber ran through results from dts.query_read() to both publishers.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_multiple_queries")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = {
            'sub1': '',
            'n_sub1': 0,
            'sub2': '',
            'n_sub2': 0,
        }

        xpath1 = 'C,/rw-dts-toy-tasklet:a-container'
        xpath2 = 'C,/rw-dts-toy-tasklet:another-container'

        v1 = toyyang.AContainer()
        v1.an_int = 1
        v1.a_string = self.id() + '1'

        v2 = toyyang.AnotherContainer()
        v2.val = self.id() + '2'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready_v1(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_ready_v2(reg_handle, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare_v1(xact_info, action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath1, v1)

            @asyncio.coroutine
            def on_prepare_v2(xact_info, action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath2, v2)

            handler1 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_v1,
                    on_prepare=on_prepare_v1)
            self.reg1 = yield from dts.register(
                    xpath1,
                    handler1,
                    flags=rwdts.Flag.PUBLISHER)

            handler2 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_v2,
                    on_prepare=on_prepare_v2)
            self.reg2 = yield from dts.register(
                    xpath2,
                    handler2,
                    flags=rwdts.Flag.PUBLISHER)

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def sub1():
                nonlocal results

                iter1 = yield from dts.query_read(xpath1, flags=0)
                for i in iter1:
                    resp = yield from i
                    results['sub1'] = str(resp.result)
                    results['n_sub1'] += 1


            @asyncio.coroutine
            def sub2():
                nonlocal results

                iter2 = yield from dts.query_read(xpath2, flags=0)
                for i in iter2:
                    resp = yield from i
                    results['sub2'] = str(resp.result)
                    results['n_sub2'] += 1

            yield from events[0].wait()
            yield from events[1].wait()
            yield from asyncio.wait(
                    (sub1(), sub2()),
                    loop=self.loop)

            events[2].set()


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[2].is_set)

        self.assertEqual(results['sub1'], str(v1))
        self.assertEqual(results['n_sub1'], 1)
        self.assertEqual(results['sub2'], str(v2))
        self.assertEqual(results['n_sub2'], 1)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_multiple_queries")

    def test_multiple_queries_in_blocks(self):
        """
        Test running multiple queries at the same time.

        The test will progress through stages defined by the events list:
            0:  First publisher is up and hit on_ready
            1:  Second publisher is up and hit on_ready
            2:  Subscriber ran through results from dts.query_read() to both publishers.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_multiple_queries_in_blocks")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = {
            'corrid': 0,
            'read': [],
            'failures': 0,
        }

        xpath1 = 'C,/rw-dts-toy-tasklet:a-container'
        xpath2 = 'C,/rw-dts-toy-tasklet:another-container'

        v1 = toyyang.AContainer()
        v1.an_int = 1
        v1.a_string = self.id() + '1'

        v2 = toyyang.AnotherContainer()
        v2.val = self.id() + '2'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready_v1(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_ready_v2(reg_handle, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare_v1(xact_info, action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath1, v1)

            @asyncio.coroutine
            def on_prepare_v2(xact_info, action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath2, v2)

            handler1 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_v1,
                    on_prepare=on_prepare_v1)
            self.reg1 = yield from dts.register(
                    xpath1,
                    handler1,
                    flags=rwdts.Flag.PUBLISHER)

            handler2 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_v2,
                    on_prepare=on_prepare_v2)
            self.reg2 = yield from dts.register(
                    xpath2,
                    handler2,
                    flags=rwdts.Flag.PUBLISHER)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_read(xpath1, flags=0)
                results['corrid'] = block.add_query_read(xpath2, flags=0)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    try:
                        r = yield from i
                        results['read'].append(str(r))
                    except rift.tasklets.dts.TransactionFailed:
                        results['failures'] += 1

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1

            events[2].set()


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[2].is_set)

        self.assertEqual(len(results['read']), 2)
        self.assertEqual(results['failures'], 0)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_multiple_queries_in_blocks")

    def test_elements_iter(self):
        """
        Verify that the elements iterator is functioning correctly (hits all
        elements and resets when the loop exits early).
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_elements_iter")
        results = {
                'all': [],
                'early': [],
                'after-early': [],
        }
        done = asyncio.Event(loop=self.loop)

        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[name="%s"]' % (emp.name,),
                            emp)

                results['all'].extend(str(element) for element in regh.elements)

                for element in regh.elements:
                    results['early'].append(str(element))
                    break

                results['after-early'].extend(str(element) for element in regh.elements)

                done.set()

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready)
            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)


        asyncio.ensure_future(pub(), loop=self.loop)

        self.run_until(done.is_set)

        self.assertEqual(2, len(results['all']))
        self.assertEqual(1, len(results['early']))
        self.assertEqual(2, len(results['after-early']))
        self.reg1.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_elements_iter")

    def test_multiple_responses(self):
        """
        Verify that returning multiple results from a single xpath works with
        the query iterator

        The test will progress through stages defined by the events list:
            0:  First publisher is up and hit on_ready
            1:  Second publisher is up and hit on_ready
            2:  Subscriber finshed getting all results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_multiple_responses")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = []

        emp1 = toyyang.Employee()
        emp1.name = self.id() + '1'

        emp2 = toyyang.Employee()
        emp2.name = self.id() + '2'

        emp3 = toyyang.Employee()
        emp3.name = self.id() + '3'

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('puba')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.MORE, xpath + '[name="%s"]' % (emp1.name,), emp1)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath + '[name="%s"]' % (emp2.name,), emp2)


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.SHARED,
                    handler=handler)

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pubb')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath + '[name="%s"]' % (emp3.name,), emp3)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.SHARED,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            res_iter = yield from dts.query_read(xpath)

            for i in res_iter:
                r = yield from i
                results.append(str(r.result))

            events[2].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(3, len(results))
        self.assertIn(str(emp1), results)
        self.assertIn(str(emp2), results)
        self.assertIn(str(emp3), results)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_multiple_responses")

    def test_audit_subscriber(self):
        """
        Verify that the audit functionality for subscriber with caching enabled
        is functioning correctly using both the dts handle and the registration
        handle to initiate the audit

        The test will progress through stages defined by the events list:
            0:  publisher is up
            1:  appconf group is up
            2:  audits have finished
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_audit_subscriber")
        results = {
            'audit_handle': None,
            'audit_regh': None,
            'on_prepare_calls': 0,
        }
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                results['on_prepare_calls'] += 1
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[3].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            def on_apply(dts, acg, xact, action, scratch):
                events[1].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                regh = acg.register(xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE, on_prepare=on_prepare)

            yield from events[1].wait()

            results['audit_handle'] = yield from dts.audit(rwdts.AuditAction.RECONCILE)
            results['audit_regh'] = yield from regh.audit(rwdts.AuditAction.RECONCILE)

            events[2].set()
            events[3].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertIsNotNone(results['audit_handle'])
        self.assertIsNone(results['audit_handle'].audit_msg)
        self.assertEqual(results['audit_handle'].audit_status, rwdts.AuditStatus.SUCCESS)

        self.assertIsNotNone(results['audit_regh'])
        self.assertIsNone(results['audit_regh'].audit_msg)
        self.assertEqual(results['audit_regh'].audit_status, rwdts.AuditStatus.SUCCESS)

        self.assertEqual(results['on_prepare_calls'], 3)
        self.run_until(events[3].is_set)
        self.reg1.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_audit_subscriber")


    def test_rpc_blocks(self):
        """
        Test that RPC input/output works. Test with one pub sending ACK and
        another one sending NA.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.query_rpc() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_rpc_blocks")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input1': '',
            'input2': '',
            'output': '',
            'corrid': 0,
            'read': [],
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = self.id() + 'req'

        resp = toyyang.ToyRpcOutput()
        resp.ret = self.id() + 'resp'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub1():
            nonlocal results
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(reg_handle, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, action, ks_path, msg):
                nonlocal results

                results['input1'] = str(msg)
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        out_xpath,
                        resp)

            handlers = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)

            self.reg1 = yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            with transaction.create(flags=rwdts.XactFlag.TRACE) as xact:
                block = xact.block_create()
                results['corrid'] = block.add_query_rpc(in_xpath, flags=0, msg=req)
                results['corrid'] = block.add_query_rpc(in_xpath, flags=0, msg=req)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                for i in res_iter:
                    try:
                        r = yield from i
                        results['read'].append(str(r.result))
                    except rift.tasklets.dts.TransactionFailed:
                        results['failures'] += 1

            """
            rpc_iter = yield from dts.query_rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                results['output'] = str(resp)
                results['n_results'] += 1
                """

            itr = yield from transaction.commit()
            for r in itr:
                try:
                    yield from r
                except rift.tasklets.dts.TransactionFailed:
                    results['failures'] += 1
            events[1].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        print(results)

        #self.assertEqual(results['input1'], str(req))
        #self.assertEqual(results['output'], str(resp))
        #self.assertEqual(results['n_results'], 1)
        self.reg1.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_rpc_blocks")

    def test_sub_initiating_read(self):
        """
        Test which initiates a detached read query from a subscriber in the middle
        of a transaction

        pub1: - a client that creates and updates xpath1
        pub2: - a publisher for xpath2 - this sends more than one responses
                the last response has ACK set

        sub:  - a subscriber for xpath1.
                also, this one generates a subquery to read xpath2 from on_prepare
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_sub_initiating_read")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(6)]

        xpath1 = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        xpath2 = 'D,/rw-dts-toy-tasklet:employee'

        employees = [toyyang.Employee() for _ in range(3)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)


        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            print('------PUB1:create')
            yield from dts.query_create(xpath1, rwdts.XactFlag.ADVISE, ret1)
            events[1].set()
            yield from events[3].wait()
            print('------PUB1:update')
            yield from dts.query_update(xpath1, rwdts.XactFlag.ADVISE, ret2)
            events[2].set()

            yield from events[4].wait()

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    print('------PUB2:on_ready')
                    regh.create_element(
                            xpath2 + '[name="%s"]' % (emp.name,),
                            emp)
                events[5].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                emp1 = None
                for emp in employees:
                    print('------PUB2:on_prepare - %s' % (emp.name,))
                    if emp1 is not None:
                        xact_info.respond_xpath(rwdts.XactRspCode.MORE, xpath2 + '[name="%s"]' % (emp1.name,), emp1)
                    emp1 = emp
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath2 + '[name="%s"]' % (emp1.name,), emp1)

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready, on_prepare=on_prepare)
            self.reg1 = yield from dts.register(
                    xpath2,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[4].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[3].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                res_iter = yield from dts.query_read(
                        xpath2,
                        rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE)

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath1)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                events[5].set()
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg2 = yield from dts.register(
                    xpath1,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[3].wait()
            yield from events[2].wait()
            yield from events[5].wait()

            res_iter = yield from dts.query_read(
                    xpath2,
                    rwdts.XactFlag.MERGE)

            yield from asyncio.sleep(1, loop=self.loop)

            events[4].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set, timeout=40)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_sub_initiating_read")

    def test_sub_initiating_linked_read(self):
        """
        Test which initiates a linked block-read from a subscriber

        pub1: - a client that creates and updates xpath1
        pub2: - a publisher for xpath2 - this sends more than one responses
                the last response has ACK set

        sub:  - a subscriber for xpath1.
                also, this one initiates a linked block-read to xpath2 from
                inside on_prepare()
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_sub_initiating_linked_read")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(6)]

        xpath1 = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        xpath2 = 'D,/rw-dts-toy-tasklet:employee'

        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)


        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            print('------PUB1:create')
            yield from dts.query_create(xpath1, rwdts.XactFlag.ADVISE, ret1)
            events[1].set()
            yield from events[3].wait()
            print('------PUB1:update')
            yield from dts.query_update(xpath1, rwdts.XactFlag.ADVISE, ret2)
            events[2].set()

            yield from events[4].wait()

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------PUB2:on_ready')
                for emp in employees:
                    regh.create_element(
                            xpath2 + '[name="%s"]' % (emp.name,),
                            emp)
                events[5].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                emp1 = None
                for emp in employees:
                    print('------PUB2:on_prepare - %s' % (emp.name,))
                    if emp1 is not None:
                        xact_info.respond_xpath(rwdts.XactRspCode.MORE, xpath2 + '[name="%s"]' % (emp1.name,), emp1)
                    emp1 = emp
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath2 + '[name="%s"]' % (emp1.name,), emp1)

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready, on_prepare=on_prepare)
            #handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready)
            self.reg1 = yield from dts.register(
                    xpath2,
                    #flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[4].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[3].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact = xact_info.xact
                block = xact.block_create()
                corrid = block.add_query_read(xpath2)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath1)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                events[5].set()
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg2 = yield from dts.register(
                    xpath1,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[3].wait()
            yield from events[2].wait()
            yield from events[5].wait()

            yield from asyncio.sleep(1, loop=self.loop)

            events[4].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set, timeout=40)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_sub_initiating_linked_read")

    def test_sub_initiating_linked_create(self):
        """
        Test which initiates a linked block-create from a subscriber

        pub:  - a client that creates and updates xpath1
        sub1: - a subscriber to xpath2 - this sends more than one responses
                the last response has ACK set

        sub2: - a subscriber for xpath1;  also a publisher for xpath2
                in the on_prepare phase this initiates a linked block-create for xpath2
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_sub_initiating_linked_create")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(7)]

        xpath1 = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        xpath2 = 'D,/rw-dts-toy-tasklet:another-container'

        ret3 = toyyang.AnotherContainer()
        ret3.val = self.id() + '3'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            print('------PUB:create')
            yield from dts.query_create(xpath1, rwdts.XactFlag.ADVISE, ret1)
            events[1].set()
            yield from events[3].wait()
            print('------PUB:update')
            yield from dts.query_update(xpath1, rwdts.XactFlag.ADVISE, ret2)
            events[2].set()

            yield from events[4].wait()

        @asyncio.coroutine
        def sub1():
            nonlocal results

            tinfo = self.new_tinfo('sub1')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB1:on_ready', regh)
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB1:on_prepare')
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath2)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)

            self.reg1 = yield from dts.register(
                    xpath2,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

        @asyncio.coroutine
        def sub2():
            nonlocal results

            tinfo = self.new_tinfo('sub2')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready2(regh, status):
                yield from events[0].wait()
                print('------SUB2:on_ready2', regh)
                #yield from dts.create(xpath2, rwdts.XactFlag.ADVISE, ret3)
                events[6].set()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB2:on_ready', regh)
                events[3].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB2:on_prepare', args)

                yield from events[0].wait()
                xact = xact_info.xact
                block = xact.block_create()
                corrid = block.add_query_create(xpath2, ret3, rwdts.XactFlag.ADVISE)
                #corrid = block.add_update(xpath2, ret3, rwdts.XactFlag.ADVISE)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath1)

            def on_precommit(*args):
                print('------SUB2:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB2:on_commit', args)
                events[5].set()
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB2:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg2 = yield from dts.register(
                    xpath1,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready2)
            self.reg3 = yield from dts.register(
                    xpath2,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    #flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[3].wait()
            yield from events[2].wait()
            yield from events[5].wait()
            yield from events[6].wait()

            """
            res_iter = yield from dts.read(
                    xpath2,
                    rwdts.XactFlag.MERGE)
            """
            yield from asyncio.sleep(1, loop=self.loop)

            events[4].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub1(), loop=self.loop)
        asyncio.ensure_future(sub2(), loop=self.loop)

        self.run_until(events[4].is_set, timeout=40)
        self.reg1.deregister()
        self.reg2.deregister()
        self.reg3.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_sub_initiating_linked_create")

    def test_sub_initiating_linked_update_w_no_sub(self):
        """
        Test which initiates a linked block-create from inside a subscriber
        with no one subscribed to the newly created xpath

        pub:  - a client that creates and updates xpath1
        sub:  - a subscriber for xpath1.
                also a publisher for xpath2
                in the on_prepare phase this initiates a linked block-update for xpath2
        There is no one subscribed to xpath2
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_sub_initiating_linked_update_w_no_sub")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(7)]

        xpath1 = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = self.id() + '1'

        ret2 = toyyang.AContainer()
        ret2.a_string = self.id() + '2'

        xpath2 = 'D,/rw-dts-toy-tasklet:another-container'

        ret3 = toyyang.AnotherContainer()
        ret3.val = self.id() + '3'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            print('------PUB:create')
            yield from dts.query_create(xpath1, rwdts.XactFlag.ADVISE, ret1)
            events[1].set()
            yield from events[3].wait()
            print('------PUB:update')
            yield from dts.query_update(xpath1, rwdts.XactFlag.ADVISE, ret2)
            events[2].set()

            yield from events[4].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready2(regh, status):
                print('------SUB:on_ready2', regh)
                yield from dts.query_create(xpath2, rwdts.XactFlag.ADVISE, ret3)
                events[6].set()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[3].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact = xact_info.xact
                block = xact.block_create()
                corrid = block.add_query_update(xpath2, ret3, rwdts.XactFlag.ADVISE)

                res_iter = yield from block.execute(flags=rwdts.XactFlag.MERGE|rwdts.XactFlag.TRACE, now=True)

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath1)

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                events[5].set()
                return rwdts.MemberRspCode.ACTION_OK

            def on_abort(*args):
                print('------SUB:on_abort', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit,
                on_abort=on_abort)

            self.reg1 = yield from dts.register(
                    xpath1,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready2)
            self.reg2 = yield from dts.register(
                    xpath2,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    #flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[3].wait()
            yield from events[2].wait()
            yield from events[5].wait()
            yield from events[6].wait()

            """
            res_iter = yield from dts.read(
                    xpath2,
                    rwdts.XactFlag.MERGE)
            """
            yield from asyncio.sleep(1, loop=self.loop)

            events[4].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set, timeout=40)
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_sub_initiating_linked_update_w_no_sub")

    def test_appdata_safe_key(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_safe_key")
        results = []
        app_create = 0
        app_delete = 0
        app_take = 0
        app_put = 0
        app_getnext = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_create(shard, key, msg, ctx):
                nonlocal app_create
                app_create += 1
                return rwtypes.RwStatus.SUCCESS

            def on_take(shard, key, ctx):
                nonlocal app_take, employees
                if app_take == 2:
                  return rwtypes.RwStatus.FAILURE

                msg = employees[app_take].to_pbcm()
                app_take += 1
                return rwtypes.RwStatus.SUCCESS, msg

            def on_put(shard, key, msg, ctx):
                nonlocal app_put
                app_put += 1
                return rwtypes.RwStatus.SUCCESS

            def on_delete(shard, key, ctx):
                nonlocal app_delete
                app_delete += 1
                return rwtypes.RwStatus.SUCCESS

            def on_getnext(key, ctx):
                nonlocal app_getnext, employees, xpath
                if app_getnext == 2:
                  return rwtypes.RwStatus.FAILURE

                ret_xpath = xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[app_getnext].name)
                app_getnext += 1
                return rwtypes.RwStatus.SUCCESS, dts.convert_to_keyspec(ret_xpath)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_safe_key(getnext=on_getnext, take=on_take, put=on_put, create=on_create, delete_fn=on_delete)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, app_take, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            app_take = 0
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(2, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_safe_key0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

            self.assertEqual(0, len(results))

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_create)
        self.assertEqual(2, app_delete)
        self.assertEqual(2, app_take)
        self.assertEqual(2, app_put)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_safe_key")

    def test_appdata_queue_key(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_queue_key")
        results = []
        app_copy = 0
        app_pbdelta = 0
        app_getnext = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_pbdelta(shard, key, msg, ctx):
                nonlocal app_pbdelta
                app_pbdelta += 1
                return rwtypes.RwStatus.SUCCESS

            def on_copy(shard, key, ctx):
                nonlocal app_copy, employees
                if app_copy == 2:
                   return rwtypes.RwStatus.FAILURE 

                msg = employees[app_copy].to_pbcm() 
                app_copy += 1
                return rwtypes.RwStatus.SUCCESS, msg

            def on_getnext(key, ctx):
                nonlocal app_getnext, employees, xpath
                if app_getnext == 2:
                  return rwtypes.RwStatus.FAILURE

                ret_xpath = xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[app_getnext].name)
                app_getnext += 1
                return rwtypes.RwStatus.SUCCESS, dts.convert_to_keyspec(ret_xpath)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_queue_key(getnext=on_getnext, copy=on_copy, pbdelta=on_pbdelta)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, app_copy, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            app_copy = 0
            query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(2, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_queue_key0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

            self.assertEqual(0, len(results))

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_copy)
        self.assertEqual(4, app_pbdelta)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_queue_key")

    def test_appdata_unsafe_key(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_unsafe_key")
        results = []
        app_create = 0
        app_delete = 0
        app_get = 0
        app_getnext = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_create(shard, key, msg, ctx):
                nonlocal app_create
                app_create += 1
                return rwtypes.RwStatus.SUCCESS

            def on_get(shard, key, ctx):
                nonlocal app_get, employees
                if app_get == 2:
                   return rwtypes.RwStatus.FAILURE

                msg = employees[app_get].to_pbcm()
                app_get += 1
                return rwtypes.RwStatus.SUCCESS, msg

            def on_delete(shard, key, ctx):
                nonlocal app_delete
                app_delete += 1
                return rwtypes.RwStatus.SUCCESS

            def on_getnext(key, ctx):
                nonlocal app_getnext, employees, xpath
                if app_getnext == 2:
                  return rwtypes.RwStatus.FAILURE

                ret_xpath = xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[app_getnext].name)
                app_getnext += 1
                return rwtypes.RwStatus.SUCCESS, dts.convert_to_keyspec(ret_xpath)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_unsafe_key(getnext=on_getnext, get=on_get, create=on_create, delete_fn=on_delete)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, app_get, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            app_get = 0

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(2, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_unsafe_key0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_create)
        self.assertEqual(2, app_delete)
        self.assertEqual(2, app_get)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_unsafe_key")

    def test_appdata_unsafe_pe(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_unsafe_pe")
        results = []
        app_create = 0
        app_delete = 0
        app_get = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_create(shard, key, msg, ctx):
                nonlocal app_create
                app_create += 1
                return rwtypes.RwStatus.SUCCESS

            def on_get(shard, key, ctx):
                nonlocal app_get, employees
                if app_get == 2:
                   return rwtypes.RwStatus.FAILURE

                app_get += 1
                msg = employees[0].to_pbcm()
                return rwtypes.RwStatus.SUCCESS, msg

            def on_delete(shard, key, ctx):
                nonlocal app_delete
                app_delete += 1
                return rwtypes.RwStatus.SUCCESS

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_unsafe_pe(get=on_get, create=on_create, delete_fn=on_delete)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
            res_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(1, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_unsafe_pe0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []
            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_create)
        self.assertEqual(2, app_delete)
        self.assertEqual(2, app_get)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_unsafe_pe")

    def test_appdata_queue_pe(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_queue_pe")
        results = []
        app_copy = 0
        app_pbdelta = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name), 
                            employees[0])

                events[0].set()

            def on_pbdelta(shard, key, msg, ctx):
                nonlocal app_pbdelta
                app_pbdelta += 1
                return rwtypes.RwStatus.SUCCESS

            def on_copy(shard, key, ctx):
                nonlocal app_copy, employees
                if app_copy == 2:
                   return rwtypes.RwStatus.FAILURE

                app_copy += 1
                msg = employees[0].to_pbcm()
                return rwtypes.RwStatus.SUCCESS, msg

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_queue_pe(copy=on_copy, pbdelta=on_pbdelta)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
            res_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(1, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_queue_pe0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_copy)
        self.assertEqual(4, app_pbdelta)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_queue_pe")

    def test_appdata_safe_pe(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_safe_pe")
        results = []
        app_create = 0
        app_delete = 0
        app_take = 0
        app_put = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name), 
                            employees[0])

                events[0].set()

            def on_create(shard, key, msg, ctx):
                nonlocal app_create
                app_create += 1
                return rwtypes.RwStatus.SUCCESS

            def on_take(shard, key, ctx):
                nonlocal app_take, employees
                if app_take == 2:
                  return rwtypes.RwStatus.FAILURE

                msg = employees[app_take].to_pbcm()
                app_take += 1
                return rwtypes.RwStatus.SUCCESS, msg

            def on_put(shard, key, msg, ctx):
                nonlocal app_put
                app_put += 1
                return rwtypes.RwStatus.SUCCESS

            def on_delete(shard, key, ctx):
                nonlocal app_delete
                app_delete += 1
                return rwtypes.RwStatus.SUCCESS

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_safe_pe(take=on_take, put=on_put, create=on_create, delete_fn=on_delete)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, app_take, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            app_take = 0
            query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
            res_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(1, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_safe_pe0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_create)
        self.assertEqual(2, app_delete)
        self.assertEqual(2, app_take)
        self.assertEqual(3, app_put)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_safe_pe")

    def test_appdata_unsafe_minikey(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_unsafe_minikey")
        results = []
        app_create = 0
        app_delete = 0
        app_get = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_create(shard, key, msg, ctx):
                nonlocal app_create
                app_create += 1
                return rwtypes.RwStatus.SUCCESS

            def on_get(shard, key, ctx):
                nonlocal app_get, employees
                if app_get == 2:
                   return rwtypes.RwStatus.FAILURE

                app_get += 1
                msg = employees[0].to_pbcm()
                return rwtypes.RwStatus.SUCCESS, msg

            def on_delete(shard, key, ctx):
                nonlocal app_delete
                app_delete += 1
                return rwtypes.RwStatus.SUCCESS

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_unsafe_minikey(get=on_get, create=on_create, delete_fn=on_delete)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
            res_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(1, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_unsafe_minikey0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()
        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_create)
        self.assertEqual(2, app_delete)
        self.assertEqual(2, app_get)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_unsafe_minikey")

    def test_appdata_safe_minikey(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_safe_minikey")
        results = []
        app_create = 0
        app_delete = 0
        app_take = 0
        app_put = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_create(shard, key, msg, ctx):
                nonlocal app_create
                app_create += 1
                return rwtypes.RwStatus.SUCCESS

            def on_take(shard, key, ctx):
                nonlocal app_take, employees
                if app_take == 2:
                  return rwtypes.RwStatus.FAILURE

                msg = employees[app_take].to_pbcm()
                app_take += 1
                return rwtypes.RwStatus.SUCCESS, msg

            def on_put(shard, key, msg, ctx):
                nonlocal app_put
                app_put += 1
                return rwtypes.RwStatus.SUCCESS

            def on_delete(shard, key, ctx):
               nonlocal app_delete
               app_delete += 1
               return rwtypes.RwStatus.SUCCESS

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_safe_minikey(take=on_take, put=on_put, create=on_create, delete_fn=on_delete)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, app_take, query_xpath

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
  
            app_take = 0
            res_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(1, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_safe_minikey0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()
        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)
        self.assertEqual(2, app_create)
        self.assertEqual(2, app_delete)
        self.assertEqual(1, app_take)
        self.assertEqual(2, app_put)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_safe_minikey")

    def test_appdata_queue_minikey(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appdata_queue_minikey")
        results = []
        app_copy = 0
        app_pbdelta = 0
        app_getnext = 0
        exceptions = 0
        events = [asyncio.Event(loop=self.loop) for _ in range(5)]


        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp)
                regh.update_element(xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name),
                            employees[0])

                events[0].set()

            def on_pbdelta(shard, key, msg, ctx):
                nonlocal app_pbdelta
                app_pbdelta += 1
                return rwtypes.RwStatus.SUCCESS

            def on_copy(shard, key, ctx):
                nonlocal app_copy, employees
                if app_copy == 2:
                   return rwtypes.RwStatus.FAILURE

                msg = employees[app_copy].to_pbcm()
                app_copy += 1
                return rwtypes.RwStatus.SUCCESS, msg

            def on_getnext(key, ctx):
                nonlocal app_getnext, employees, xpath
                if app_getnext == 2:
                  return rwtypes.RwStatus.FAILURE

                ret_xpath = xpath + '[rw-dts-toy-tasklet:name="%s"]' % (employees[app_getnext].name)
                app_getnext += 1
                return rwtypes.RwStatus.SUCCESS, dts.convert_to_keyspec(ret_xpath)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            shard = yield from reg.shard_init(flags=rwdts.Flag.PUBLISHER)
            shard.appdata_register_queue_minikey(getnext=on_getnext, copy=on_copy, pbdelta=on_pbdelta)
            yield from events[1].wait()

            for emp in employees:
                reg.delete_element(
                       xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,))

            events[2].set()

            yield from events[3].wait()

            reg.deregister()

            events[4].set()

        @asyncio.coroutine
        def sub():
            nonlocal results, app_copy, exceptions

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            app_copy = 0
            query_xpath = xpath +  '[rw-dts-toy-tasklet:name="%s"]' % (employees[0].name)
            res_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            self.assertEqual(1, len(results))
            self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_appdata_queue_minikey0')
            events[1].set()

            yield from events[2].wait()

            sec_iter = yield from dts.query_read(query_xpath, rwdts.XactFlag.MERGE)

            results = []

            for i in sec_iter:
                try:
                    result = yield from i
                    results.append(result.result)
                except rift.tasklets.dts.TransactionFailed:
                    exceptions += 1

            self.assertEqual(0, len(results))

            events[3].set()

            yield from events[4].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[4].is_set)
        self.assertEqual(2, app_copy)
        self.assertEqual(4, app_pbdelta)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appdata_queue_minikey")

    def test_appconf_delete_success(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_success")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]
   
        prep_cnt = 0
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)

            yield from events[2].wait()

            yield from dts.query_delete(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE)

            yield from events[3].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)
                if prep_cnt == 2:
                  events[3].set()

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(2, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_success")

    def test_appconf_delete_failure(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_failure")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)

            yield from events[1].wait()

            yield from dts.query_delete(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE)

            events[2].set() 

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                pass

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)
                events[1].set()

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[2].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[2].is_set)
        self.assertEqual(1, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_failure")

    def test_appconf_delete_partial(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_partial")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_inter_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]/rw-dts-toy-tasklet:interface[rw-dts-toy-tasklet:name="interface1"]'
        del_nc_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]'
        del_col_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'
        del_container = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]/rw-dts-toy-tasklet:nc-data'
        del_leaf = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]/rw-dts-toy-tasklet:interface[rw-dts-toy-tasklet:name="interface1"]/rw-dts-toy-tasklet:inter-info'

        pub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from events[2].wait()

            #yield from dts.query_delete(
            #        del_leaf,
            #        flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_container,
                    flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_inter_xpath,
                    flags=rwdts.XactFlag.ADVISE)

           
            yield from dts.query_delete(
                    del_nc_xpath,
                    flags=rwdts.XactFlag.ADVISE)
      
            yield from dts.query_delete(
                    del_col_xpath,
                    flags=rwdts.XactFlag.ADVISE) 

            events[3].set()
 
        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of 
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored. 
                 If a field is deleted, then get the field name that is deleted and manipulate the 
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()
              
                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name()) 

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(sub_xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set, 10) # setting a timeout of 10 instead of 5
        self.assertEqual(5, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_partial")

    def test_appconf_delete_container(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client
        
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_container")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]
        
        prep_cnt = 0
        del_container = 'C,/rw-dts-toy-tasklet:toytasklet-colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info" 
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4] 
        
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
        
            yield from events[0].wait()
            
            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_container,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(sub_xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(6, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_container")

    def test_appconf_delete_wildcard(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_wildcard")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_list_no_key = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_list_no_key,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(sub_xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(6, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_wildcard")

    def test_appconf_delete_reg_deeper_wildcard(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_reg_deeper_wildcard")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_list_no_key = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_list_no_key,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony container")

                fref.goto_proto_name(pb_msg, "colony")

                if fref.is_field_deleted():
                  print("Delete the colony list")

                col_list = msg.get_colony()

                for col in col_list:
                  col_pb = col.to_pbcm()
                  fref.goto_proto_name(col_pb, "name")
                  if fref.is_field_deleted():
                    print("Delete colony name %s" %col.get_name())

                  if fref.is_field_present():
                    print("Colony present name %s" %col.get_name())

                  fref.goto_proto_name(col_pb, "network_context")

                  if fref.is_field_deleted():
                    print("NC list deleted")

                  if fref.is_field_present():
                    print("NC list present")

                  nc_list = col.get_network_context()

                  for nc in nc_list:
                    nc_pb = nc.to_pbcm()
                    fref.goto_proto_name(nc_pb, "name")
                    if fref.is_field_deleted():
                      print("NC deleted name %s" %nc.get_name())

                    if fref.is_field_present():
                      print("NC present name %s" %nc.get_name())

                    fref.goto_proto_name(nc_pb, "nc_data")
                    if fref.is_field_deleted():
                      print("NC-data container deleted name %s" %nc.get_nc_data())

                    if fref.is_field_present():
                      print("NC-data container present name %s" %nc.get_nc_data())

                    fref.goto_proto_name(nc_pb, "interface")
                    if fref.is_field_deleted():
                      print("Interface list deleted")

                    if fref.is_field_present():
                      print("Interface list present")

                    inter_list = nc.get_interface()

                    for inter in inter_list:
                      inter_pb = inter.to_pbcm()
                      fref.goto_proto_name(inter_pb, "name")

                      if fref.is_field_deleted():
                        print("Interface deleted name %s" %inter.get_name())

                      if fref.is_field_present():
                        print("Interface present name %s" %inter.get_name())

                      fref.goto_proto_name(inter_pb, "inter_info")
                      if fref.is_field_deleted():
                        print("Interface info leaf deleted name %s" %inter.get_inter_info())

                      if fref.is_field_present():
                        print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(sub_xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(4, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_reg_deeper_wildcard")

    def test_appconf_delete_key(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_key")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_list_key1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        del_list_key2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        del_list_key3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_list_key1,
                    flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_list_key2,
                    flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_list_key3,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(sub_xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(6, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_key")

    def test_appconf_delete_container_wildcard(self):
        """
        Verify an expection on deinit callback causes a transaction to abort.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_delete_container_wildcard")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_container = 'C,/rw-dts-toy-tasklet:toytasklet-colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_container,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                acg.handle.prepare_complete_ok(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(sub_xpath, rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY, on_prepare=on_prepare)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(2, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_delete_container_wildcard")

    def test_notification(self):
        """
        Verify send_notification method

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_notification")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        prep_cnt = 0
        notif_xpath = 'N,/rw-dts-toy-tasklet:event-route-added'
        note = toyyang.YangNotif_RwDtsToyTasklet_EventRouteAdded()
        note.name = "Test route"
        note.id = 3

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            dts.send_notification(
                    note.to_pbcm())
       
        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt
                prep_cnt += 1
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, notif_xpath)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)

            self.reg = yield from dts.register(
                    notif_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)
        self.assertEqual(1, prep_cnt)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_notification")


    def test_2_pub(self):
        """
        Verify if the READ operation gets the data at the desired keyspec level.

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_2_pub")
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        pub1_xpath = pub1_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'
        pub2_xpath = pub2_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/profile'
        results = []
        q_count = 0

        company = toyyang.Company()
        company.name = "PUB1"
        bd = toyyang.Company().create_board()
        memb = bd._create_member()
        memb.name = "Member1"
        bd.member = [memb]
        company.board = bd

        company_profile = toyyang.Company().create_profile()
        company_profile.revenue = '10000'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready1(regh, status):
                regh.create_element(
                        pub1_xpath,
                        company)
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready1)

            self.reg1 = yield from dts.register(
                    pub1_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready2(regh, status):
                regh.create_element(
                        pub2_xpath,
                        company_profile)
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                nonlocal q_count
                if q_count == 0:
                  xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub2_xpath, None)
                else:
                  xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub2_xpath, company_profile)
                q_count += 1

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready2,
                on_prepare=on_prepare)

            self.reg2 = yield from dts.register(
                    pub2_xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[1].wait()

            res_iter = yield from dts.query_read(pub2_xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)
                print(result)

            self.assertEqual(0, len(results))

            res_iter = yield from dts.query_read(pub2_xpath, rwdts.XactFlag.MERGE)
  
            for i in res_iter:
                result = yield from i
                results.append(result.result)
                print(result)

            self.assertEqual(1, len(results))

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[2].is_set)
        self.reg1.deregister()
        self.reg2.deregister()

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_2_pub")

    def test_xact_create_wo_sub(self):
        """
        Verify that transactional create element operation at the pub goes thru
        fine and the data is committed even though there are no registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_create_wo_sub")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]



        employees = [toyyang.Employee() for _ in range(1)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            transaction = rift.tasklets.DTS.Transaction(dts)
            xact = transaction.create_xact()

            @asyncio.coroutine
            def on_ready(regh, status):
                print("Inside on_ready")
                nonlocal xact
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp,
                            xact)
                    xact.commit()
                    events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from asyncio.gather(events[0].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set, timeout=5)
        self.assertEqual(1, len(results))
        self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_xact_create_wo_sub0')
        self.reg1.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_create_wo_sub")

    def test_xact_create_w_sub(self):
        """
        Verify that transactional create element operation at the pub goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_create_w_sub")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]



        employees = [toyyang.Employee() for _ in range(1)]
        for i, employee in enumerate(employees):
            employee.name = self.id() + str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            transaction = rift.tasklets.DTS.Transaction(dts)
            xact = transaction.create_xact()

            @asyncio.coroutine
            def on_ready(regh, status):
                nonlocal xact
                for emp in employees:
                    regh.create_element(
                            xpath + '[rw-dts-toy-tasklet:name="%s"]' % (emp.name,),
                            emp,
                            xact)
                    xact.commit()
                    events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ|rwdts.Flag.DATASTORE,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(1, len(results))
        self.assertEqual(results[0].result.name, '__main__.DtsTestCase.test_xact_create_w_sub0')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_create_w_sub")

    def test_appconf_prepare_fail(self):
        """
        Verify an exception in init callback triggers a trasaction exception. Also verify if the
        exception message is sent back to the client.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_prepare_fail")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        failures = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, failures
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            try:
                yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                failures += 1
                events[1].set()

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                pass

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_fail(xact_info.handle, rwtypes.RwStatus.FAILURE, expected_exp_msg)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(failures, 1)
        self.assertTrue(expected_exp_msg in exp_msg)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_prepare_fail")

    def test_appconf_prepare_na(self):
        """
        Verify an exception in init callback triggers a trasaction exception. Also verify if the
        exception message is sent back to the client.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_appconf_prepare_na")
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        failures = 0
        expected_exp_msg = "Test Error"
        exp_msg = None
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            nonlocal exp_msg, failures
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            try:
                yield from dts.query_update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)
            except rift.tasklets.dts.TransactionAborted as e:
                exp_msg = str(e)
                failures += 1

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            def on_init(acg, xact, scratch):
                pass

            def on_deinit(acg, xact, scratch):
                events[1].set()

            def on_apply(dts, acg, xact, action, scratch):
                events[0].set()

            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_na(xact_info.handle)

            with dts.appconf_group_create(handler=handlers) as acg:
                acg.register(xpath, rwdts.Flag.SUBSCRIBER, on_prepare=on_prepare)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_appconf_prepare_na")

    def test_3_wildcards_pub_sub_with_cursor(self):
        """
        Verify that the element functions on a registration work

        The test will progress through stages defined by the events list:
            0:  The publisher is up and hit the on_ready() callback
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the cursor
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_3_wildcards_pub_sub_with_cursor")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        xpath = 'D,/rw-dts-toy-tasklet:company'
        reg_xpath = 'D,/rw-dts-toy-tasklet:company/rw-dts-toy-tasklet:profile/rw-dts-toy-tasklet:office-locations/rw-dts-toy-tasklet:printer-details'

        companies = [toyyang.Company() for _ in range(2)]
        for i, company in enumerate(companies):
            company.name = "company" + self.id() + str(i)

        offices = [toyyang.Company_Profile_OfficeLocations() for _ in range(2)]
        for i, office in enumerate(offices):
            office.location_code =  i
            office.location_name =  "office" + self.id() + str(i)

        printers = [toyyang.Company_Profile_OfficeLocations_PrinterDetails() for _ in range(2)]
        for i, printer in enumerate(printers):
            printer.identifier = "printer" + self.id() + str(i)

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                for idx in range(2):
                    regh.create_element(
                            xpath +
                            '[rw-dts-toy-tasklet:name="%s"]' % (companies[idx].name,) +
                            '/profile/office-locations[rw-dts-toy-tasklet:location-code="%d"]' % (offices[idx].location_code) +
                            '/printer-details[rw-dts-toy-tasklet:identifier="%s"]' % (printers[idx].identifier),
                            printers[idx])
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready)
            self.reg1 = yield from dts.register(
                    reg_xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)


        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                results.extend(regh.elements)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    reg_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, 60)

        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0]), str(printers[0]))
        self.assertEqual(str(results[1]), str(printers[1]))
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_3_wildcards_pub_sub_with_cursor")

    def test_wildcards_chain_pub_sub_with_cursor(self):
        """
        Verify that the element functions on a registration work

        The test will progress through stages defined by the events list:
            0:  The pub is up and hit the on_ready() callback
            1:  The sub is up and hit the on_ready() callback
            2:  The sub publisher both wildcarded and hit the on_ready callback
            3:  The pub subscriber both wildcarded and hit the on_ready callback
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_wildcards_chain_pub_sub_with_cursor")
        results_one = []
        results_two = []
        results_three = []
        events = [asyncio.Event(loop=self.loop) for _ in range(8)]

        reg_xpath_one = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="company"]/rw-dts-toy-tasklet:profile/rw-dts-toy-tasklet:office-locations'
        reg_xpath_two = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="company"]/rw-dts-toy-tasklet:profile/rw-dts-toy-tasklet:office-locations/rw-dts-toy-tasklet:printer-details'

        offices = [toyyang.Company_Profile_OfficeLocations() for _ in range(2)]
        for i, office in enumerate(offices):
            office.location_code =  i
            office.location_name =  "office" + self.id() + str(i)

        offices_with_printer = [toyyang.Company_Profile_OfficeLocations() for _ in range(2)]
        for i, office in enumerate(offices_with_printer):
            office.location_code =  i + 2
            office.location_name =  "office_with_printer" + self.id() + str(i)
            office.printer_details = [toyyang.Company_Profile_OfficeLocations_PrinterDetails() for _ in range(2)]
            for i, printer in enumerate(office.printer_details):
                printer.identifier = "printer_in_office" + self.id() + str(i)


        printers = [toyyang.Company_Profile_OfficeLocations_PrinterDetails() for _ in range(2)]
        for i, printer in enumerate(printers):
            printer.identifier = "printer" + self.id() + str(i)

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready_one(regh, status):
                for idx in range(2):
                    regh.create_element(
                            reg_xpath_one +
                            '[rw-dts-toy-tasklet:location-code="%d"]' % (offices[idx].location_code),
                            offices[idx])
                for idx in range(2):
                    regh.create_element(
                            reg_xpath_one +
                            '[rw-dts-toy-tasklet:location-code="%d"]' % (offices_with_printer[idx].location_code),
                            offices_with_printer[idx])
                events[0].set()

            handler_one = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_one)
            self.reg_one = yield from dts.register(
                    reg_xpath_one,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler_one)

            yield from events[2].wait()

            @asyncio.coroutine
            def on_ready_two(regh, status):
                results_two.extend(regh.elements)
                events[3].set()

            handler_two = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_two)

            self.reg_two = yield from dts.register(
                    reg_xpath_two,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler_two)

            yield from events[3].wait()
            events[4].set()

            yield from events[6].wait()
            events[7].set()

        @asyncio.coroutine
        def sub():
            nonlocal results_one
            nonlocal results_two
            nonlocal results_three

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready_one(regh, status):
                results_one.extend(regh.elements)
                events[1].set()

            handler_one = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_one)

            self.sub_reg_one = yield from dts.register(
                    reg_xpath_one,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler_one)

            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready_two(regh, status):
                for idx in range(2):
                    regh.create_element(
                            reg_xpath_one +
                            '[rw-dts-toy-tasklet:location-code="%d"]' % (offices[idx].location_code) +
                            '/printer-details[rw-dts-toy-tasklet:identifier="%s"]' % (printers[idx].identifier),
                            printers[idx])
                events[2].set()

            handler_two = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_two)

            self.sub_reg_two = yield from dts.register(
                    reg_xpath_two,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler_two)

            yield from events[4].wait()
            @asyncio.coroutine
            def on_ready_three(regh, status):
                results_three.extend(regh.elements)
                events[5].set()

            handler_three = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_three)

            self.sub_reg_three = yield from dts.register(
                    reg_xpath_one,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler_three)

            yield from events[5].wait()
            events[6].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[7].is_set, 60)

        self.assertEqual(len(results_one), 4)
        self.assertEqual(str(results_one[0]), str(offices[0]))
        self.assertEqual(str(results_one[1]), str(offices[1]))
        self.assertEqual(str(results_one[2]), str(offices_with_printer[0]))
        self.assertEqual(str(results_one[3]), str(offices_with_printer[1]))
        self.assertEqual(len(results_two), 6)
        self.assertEqual(str(results_two[0]), str(offices_with_printer[0].printer_details[0]))
        self.assertEqual(str(results_two[1]), str(offices_with_printer[0].printer_details[1]))
        self.assertEqual(str(results_two[2]), str(offices_with_printer[1].printer_details[1]))
        self.assertEqual(str(results_two[3]), str(offices_with_printer[1].printer_details[0]))
        self.assertEqual(str(results_two[4]), str(printers[0]))
        self.assertEqual(str(results_two[5]), str(printers[1]))
        offices[0].printer_details = [toyyang.Company_Profile_OfficeLocations_PrinterDetails() for _ in range(1)]
        offices[0].printer_details[0].identifier = "printer" + self.id() + "0"
        offices[1].printer_details = [toyyang.Company_Profile_OfficeLocations_PrinterDetails() for _ in range(1)]
        offices[1].printer_details[0].identifier = "printer" + self.id() + "1"
        self.assertEqual(len(results_three), 4)
        self.assertEqual(str(results_three[0]), str(offices[0]))
        self.assertEqual(str(results_three[1]), str(offices[1]))
        self.assertEqual(str(results_three[2]), str(offices_with_printer[0]))
        self.assertEqual(str(results_three[3]), str(offices_with_printer[1]))
        self.reg_one.deregister()
        self.reg_two.deregister()
        self.sub_reg_one.deregister()
        self.sub_reg_two.deregister()
        self.sub_reg_three.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_wildcards_chain_pub_sub_with_cursor")

    def test_xact_create_shallow_reroot(self):
        """
        Verify that create element operation at the pub done at deeper level than reg goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_create_shallow_reroot")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        company = toyyang.Company()
        company.name = "PUB1"

        employee = toyyang.Company().create_employee()
        employee.id = 2
        employee.name = "Employee1"
        employee.title = "Engineer"
        employee.start_date =  "10-10-1010"

        company.employee = [employee]

        xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'
        pub_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/rw-dts-toy-tasklet:employee[rw-dts-toy-tasklet:id="2"]'
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath,
                                    company)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(1, len(results))
        print(results[0].result.name)
        self.assertEqual(results[0].result.name, 'PUB1')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_create_shallow_reroot")

    def test_xact_create_deep_reroot(self):
        """
        Verify that create element operation at the pub done at shallower level than reg goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_create_deep_reroot")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]


        company = toyyang.Company()
        company.name = "PUB1"

        employee1 = toyyang.Company().create_employee()
        employee1.id = 1
        employee1.name = "Employee1"
        employee1.title = "Engineer"
        employee1.start_date =  "10-10-1010"

        employee2 = toyyang.Company().create_employee()
        employee2.id = 2
        employee2.name = "Employee2"
        employee2.title = "Jr-Engineer"
        employee2.start_date =  "20-20-2020"

        company.employee = [employee1, employee2]

        xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/rw-dts-toy-tasklet:employee'
        pub_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(pub_xpath,
                                    company)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(2, len(results))
        print(results[0].result.name)
        self.assertEqual(results[0].result.name, 'Employee1')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_create_deep_reroot")

    def test_xact_update_deep_reroot(self):
        """
        Verify that update element operation at the pub done at shallower level than reg goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_update_deep_reroot")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]


        company = toyyang.Company()
        company.name = "PUB1"

        employee1 = toyyang.Company().create_employee()
        employee1.id = 1
        employee1.name = "Employee1"
        employee1.title = "Engineer"
        employee1.start_date =  "10-10-1010"

        employee2 = toyyang.Company().create_employee()
        employee2.id = 2
        employee2.name = "Employee2"
        employee2.title = "Jr-Engineer"
        employee2.start_date =  "20-20-2020"

        company.employee = [employee1, employee2]

        xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/rw-dts-toy-tasklet:employee'
        pub_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath + '[rw-dts-toy-tasklet:id="1"]',
                                    employee1)
                regh.update_element(pub_xpath,
                                    company)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(2, len(results))
        print(results[0].result.name)
        self.assertEqual(results[0].result.name, 'Employee1')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_update_deep_reroot")

    def test_xact_update_shallow_reroot(self):
        """
        Verify that update element operation at the pub done at deeper level than reg goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_update_shallow_reroot")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        company = toyyang.Company()
        company.name = "PUB1"

        employee = toyyang.Company().create_employee()
        employee.id = 2
        employee.name = "Employee1"
        employee.title = "Engineer"
        employee.start_date =  "10-10-1010"

        xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'
        pub_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/rw-dts-toy-tasklet:employee[rw-dts-toy-tasklet:id="2"]'
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath,
                                    company)

                regh.update_element(pub_xpath,
                                    employee)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(1, len(results))
        print(results[0].result.name)
        self.assertEqual(results[0].result.name, 'PUB1')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_update_shallow_reroot")

    def test_xact_delete_deep_reroot(self):
        """
        Verify that delete element operation at the pub done at shallower level than reg goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_delete_deep_reroot")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        employee1 = toyyang.Company().create_employee()
        employee1.id = 1
        employee1.name = "Employee1"
        employee1.title = "Engineer"
        employee1.start_date =  "10-10-1010"

        employee2 = toyyang.Company().create_employee()
        employee2.id = 2
        employee2.name = "Employee2"
        employee2.title = "Jr-Engineer"
        employee2.start_date =  "20-20-2020"

        xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/rw-dts-toy-tasklet:employee'
        pub_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath + '[rw-dts-toy-tasklet:id="1"]',
                                    employee1)
                regh.create_element(xpath + '[rw-dts-toy-tasklet:id="2"]',
                                    employee2)
                regh.delete_element(pub_xpath)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(0, len(results))
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_delete_deep_reroot")

    def test_xact_delete_shallow_reroot(self):
        """
        Verify that delete element operation at the pub done at deeper level than reg goes thru
        fine and the data is committed when there are registered subscribers
        for the keyspec.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_xact_delete_shallow_reroot")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        company = toyyang.Company()
        company.name = "PUB1"

        employee1 = toyyang.Company().create_employee()
        employee1.id = 1
        employee1.name = "Employee1"
        employee1.title = "Engineer"
        employee1.start_date =  "10-10-1010"

        employee2 = toyyang.Company().create_employee()
        employee2.id = 2
        employee2.name = "Employee2"
        employee2.title = "Jr-Engineer"
        employee2.start_date =  "20-20-2020"

        company.employee = [employee1, employee2]

        xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]'
        pub_xpath = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="PUB1"]/rw-dts-toy-tasklet:employee[rw-dts-toy-tasklet:id="2"]'
        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath,
                                    company)

                regh.delete_element(pub_xpath)
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg1 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready)

            self.reg2 = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from asyncio.gather(events[1].wait(), asyncio.sleep(2, loop=self.loop), loop=self.loop)
            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)
                print(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set, timeout=5)
        self.assertEqual(1, len(results))
        print(results[0].result.name)
        self.assertEqual(results[0].result.name, 'PUB1')
        self.reg1.deregister()
        self.reg2.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_xact_delete_shallow_reroot")

    def test_container_pub_in_list_pub(self):
        """
        Verify that the element functions on a registration work

        The test will progress through stages defined by the events list:
            0:  The pub is up and hit the on_ready() callback
            1:  The sub is up and hit the on_ready() callback
            2:  The sub publisher for container with list is up and hit the on_ready() callback
            3:  The sub on_prepare completes processing of publish in (2)
            4:  The sub publish deeper to allow update of cache from (3) is up and hit on_ready callback
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_container_pub_in_list_pub")
        results_one = []
        results_two = []
        events = [asyncio.Event(loop=self.loop) for _ in range(6)]

        reg_xpath_one = 'D,/rw-dts-toy-tasklet:company'
        reg_xpath_two = 'D,/rw-dts-toy-tasklet:company/rw-dts-toy-tasklet:profile'
        reg_xpath_three = 'D,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name="company"]/rw-dts-toy-tasklet:profile/rw-dts-toy-tasklet:office-locations/rw-dts-toy-tasklet:printer-details'

        companies = [toyyang.Company() for _ in range(2)]
        for i, company in enumerate(companies):
            company.name =  "company" + self.id() + str(i)
            company.general_info =  "general-info" + self.id() + str(i)

        profiles = [toyyang.Company_Profile() for _ in range(2)]

        for i, profile in enumerate(profiles):
            profile.revenue =  "revenue" + self.id() + str(i)

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready_one(regh, status):
                for idx in range(2):
                    regh.create_element(
                            reg_xpath_one +
                            '[rw-dts-toy-tasklet:name="%s"]' % (companies[idx].name),
                            companies[idx])
                events[0].set()

            handler_one = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_one)

            self.reg_one = yield from dts.register(
                    reg_xpath_one,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler_one)

        @asyncio.coroutine
        def sub():
            nonlocal results_one
            nonlocal results_two

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                self.prepare_count += 1
                xact_info.respond_xpath(rwdts.XactRspCode.ACK)
                if (self.prepare_count == 2):
                    events[3].set()


            @asyncio.coroutine
            def on_ready_one(regh, status):
                results_one.extend(regh.elements)
                events[1].set()

            self.prepare_count = 0
            handler_one = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_one,
                    on_prepare=on_prepare)

            self.sub_reg_one = yield from dts.register(
                    reg_xpath_one,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler_one)

            yield from events[1].wait()

            @asyncio.coroutine
            def on_ready_two(regh, status):
                for idx in range(2):
                    regh.create_element(
                            reg_xpath_one +
                            '[rw-dts-toy-tasklet:name="%s"]' % (companies[idx].name) +
                            '/rw-dts-toy-tasklet:profile',
                            profiles[idx])
                events[2].set()

            handler_two = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_two)

            self.sub_reg_two = yield from dts.register(
                    reg_xpath_two,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler_two)

            yield from events[3].wait()
            @asyncio.coroutine
            def on_ready_three(regh, status):
                events[4].set()

            handler_three = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_three)

            self.sub_reg_three = yield from dts.register(
                    reg_xpath_three,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler_three)

            yield from events[4].wait()
            results_two.extend(self.sub_reg_one.elements)
            events[5].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[5].is_set, 60)

        self.assertEqual(len(results_one), 2)
        self.assertEqual(str(results_one[0]), str(companies[0]))
        self.assertEqual(str(results_one[1]), str(companies[1]))
        companies[0].profile = profiles[0]
        companies[1].profile = profiles[1]
        self.assertEqual(len(results_two), 2)
        self.assertEqual(str(results_two[0]), str(companies[0]))
        self.assertEqual(str(results_two[1]), str(companies[1]))
        self.reg_one.deregister()
        self.sub_reg_one.deregister()
        self.sub_reg_two.deregister()
        self.sub_reg_three.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_container_pub_in_list_pub")

    def test_delete_partial(self):
        """
        Verify partial delete operation succeeds.

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_delete_partial")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_inter_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]/rw-dts-toy-tasklet:interface[rw-dts-toy-tasklet:name="interface1"]'
        del_nc_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]'
        del_col_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'
        del_container = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]/rw-dts-toy-tasklet:nc-data'
        del_leaf = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]/rw-dts-toy-tasklet:network-context[rw-dts-toy-tasklet:name="nc1"]/rw-dts-toy-tasklet:interface[rw-dts-toy-tasklet:name="interface1"]/rw-dts-toy-tasklet:inter-info'

        pub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_container,
                    flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_inter_xpath,
                    flags=rwdts.XactFlag.ADVISE)


            yield from dts.query_delete(
                    del_col_xpath,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1
                if (prep_cnt == 1):
                  events[1].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, sub_xpath)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare,
                    on_ready=on_ready)

            self.reg = yield from dts.register(
                    sub_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set, 10) # setting a timeout of 10 instead of 5
        self.assertEqual(4, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_delete_partial")

    def test_delete_container(self):
        """
        Verify container delete operation succeeds.

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_delete_container")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_container = 'C,/rw-dts-toy-tasklet:toytasklet-colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_container,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1


                if (prep_cnt == 3):
                  events[1].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub_xpath1)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare,
                    on_ready=on_ready)

            self.reg = yield from dts.register(
                    sub_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(6, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_delete_container")

    def test_delete_wildcard(self):
        """
        Verify wildcard delete succeeds.

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_delete_wildcard")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_list_no_key = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_list_no_key,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1


                if (prep_cnt == 3):
                  events[1].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub_xpath1)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare,
                    on_ready=on_ready)

            self.reg = yield from dts.register(
                    sub_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(6, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_delete_wildcard")

    def test_delete_reg_deeper_wildcard(self):
        """
        Verify deeper delete succeeds.

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_delete_reg_deeper_wildcard")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_list_no_key = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_list_no_key,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony container")

                fref.goto_proto_name(pb_msg, "colony")

                if fref.is_field_deleted():
                  print("Delete the colony list")

                col_list = msg.get_colony()

                for col in col_list:
                  col_pb = col.to_pbcm()
                  print(col_pb)
                  fref.goto_proto_name(col_pb, "name")
                  if fref.is_field_deleted():
                    print("Delete colony name %s" %col.get_name())

                  if fref.is_field_present():
                    print("Colony present name %s" %col.get_name())

                  fref.goto_proto_name(col_pb, "network_context")

                  if fref.is_field_deleted():
                    print("NC list deleted")

                  if fref.is_field_present():
                    print("NC list present")

                  nc_list = col.get_network_context()

                  for nc in nc_list:
                    nc_pb = nc.to_pbcm()
                    fref.goto_proto_name(nc_pb, "name")
                    if fref.is_field_deleted():
                      print("NC deleted name %s" %nc.get_name())

                    if fref.is_field_present():
                      print("NC present name %s" %nc.get_name())

                    fref.goto_proto_name(nc_pb, "nc_data")
                    if fref.is_field_deleted():
                      print("NC-data container deleted name %s" %nc.get_nc_data())

                    if fref.is_field_present():
                      print("NC-data container present name %s" %nc.get_nc_data())

                    fref.goto_proto_name(nc_pb, "interface")
                    if fref.is_field_deleted():
                      print("Interface list deleted")

                    if fref.is_field_present():
                      print("Interface list present")

                    inter_list = nc.get_interface()

                    for inter in inter_list:
                      inter_pb = inter.to_pbcm()
                      fref.goto_proto_name(inter_pb, "name")

                      if fref.is_field_deleted():
                        print("Interface deleted name %s" %inter.get_name())

                      if fref.is_field_present():
                        print("Interface present name %s" %inter.get_name())

                      fref.goto_proto_name(inter_pb, "inter_info")
                      if fref.is_field_deleted():
                        print("Interface info leaf deleted name %s" %inter.get_inter_info())

                      if fref.is_field_present():
                        print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1

                if (prep_cnt == 3):
                  events[1].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub_xpath1)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare,
                    on_ready=on_ready)

            self.reg = yield from dts.register(
                    sub_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(4, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_delete_reg_deeper_wildcard")

    def test_delete_key(self):
        """
        Verify all undividual deletes based on key succeeds.

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_delete_key")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_list_key1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        del_list_key2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        del_list_key3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        pub_xpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony2"]'
        pub_xpath3 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony3"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        colony2 = toyyang.ColonyConfig()
        colony2.name = "colony2"
        nc3 = toyyang.NetworkContext()
        nc3.name = "nc3"
        interface3 = toyyang.Interface()
        interface3.name = "interface3"
        nc3.interface = [interface3]
        colony2.network_context = [nc3]

        colony3 = toyyang.ColonyConfig()
        colony3.name = "colony3"
        nc4 = toyyang.NetworkContext()
        nc4.name = "nc4"
        interface4 = toyyang.Interface()
        interface4.name = "interface4"
        nc4.interface = [interface4]
        colony3.network_context = [nc4]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from dts.query_create(
                    pub_xpath2,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony2)

            yield from dts.query_create(
                    pub_xpath3,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony3)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_list_key1,
                    flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_list_key2,
                    flags=rwdts.XactFlag.ADVISE)

            yield from dts.query_delete(
                    del_list_key3,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1

                if (prep_cnt == 3):
                  events[1].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub_xpath1)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare,
                    on_ready=on_ready)

            self.reg = yield from dts.register(
                    sub_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(6, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_delete_key")

    def test_delete_container_wildcard(self):
        """
        Verify container delete succeeds.
        Also verify if the exception message is sent back to the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_delete_container_wildcard")
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        prep_cnt = 0
        del_container = 'C,/rw-dts-toy-tasklet:toytasklet-colony'

        pub_xpath1 = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'
        sub_xpath = 'C,/rw-dts-toy-tasklet:toytasklet-colony/rw-dts-toy-tasklet:colony[rw-dts-toy-tasklet:name="colony1"]'

        colony1 = toyyang.ColonyConfig()
        colony1.name = "colony1"
        nc1 = toyyang.NetworkContext()
        nc1.name = "nc1"
        ndata = nc1.create_nc_data()
        ndata.nc_info = "nc1-info"
        nc1.nc_data = ndata
        interface1 =toyyang.Interface()
        interface1.name = "interface1"
        interface1.inter_info = "inter1-info"
        interface2 = toyyang.Interface()
        interface2.name = "interface2"
        nc1.interface = [interface1, interface2]
        colony1.network_context = [nc1]

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            yield from dts.query_create(
                    pub_xpath1,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=colony1)

            yield from events[2].wait()

            yield from dts.query_delete(
                    del_container,
                    flags=rwdts.XactFlag.ADVISE)

            events[3].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt

                fref = ProtobufC.FieldReference.alloc()
                self.assertTrue(fref)
                pb_msg = msg.to_pbcm()
                """
                 Check for deletion of fields in the message first followed by check for presence of
                 the field. Check for each field in each child for deletion and presence.
                 If a field is present, then get the field data and store it if not already stored.
                 If a field is deleted, then get the field name that is deleted and manipulate the
                 stored data in the application.

                 In places of prints below, application can get the data information (key) and either
                 update it's data store or delete it from data store depending on is_present or is_deleted
                """

                fref.goto_whole_message(pb_msg)

                if fref.is_field_deleted():
                  print("Delete the colony list")

                fref.goto_proto_name(pb_msg, "name")
                if fref.is_field_deleted():
                  print("Delete colony name %s" %msg.get_name())

                if fref.is_field_present():
                  print("Colony present name %s" %msg.get_name())

                fref.goto_proto_name(pb_msg, "network_context")
                if fref.is_field_deleted():
                  print("NC list deleted")

                if fref.is_field_present():
                  print("NC list present")

                nc_list = msg.get_network_context()

                for nc in nc_list:
                  nc_pb = nc.to_pbcm()
                  fref.goto_proto_name(nc_pb, "name")
                  if fref.is_field_deleted():
                    print("NC deleted name %s" %nc.get_name())

                  if fref.is_field_present():
                    print("NC present name %s" %nc.get_name())

                  fref.goto_proto_name(nc_pb, "nc_data")
                  if fref.is_field_deleted():
                    print("NC-data container deleted name %s" %nc.get_nc_data())

                  if fref.is_field_present():
                    print("NC-data container present name %s" %nc.get_nc_data())

                  fref.goto_proto_name(nc_pb, "interface")
                  if fref.is_field_deleted():
                    print("Interface list deleted")

                  if fref.is_field_present():
                    print("Interface list present")

                  inter_list = nc.get_interface()

                  for inter in inter_list:
                    inter_pb = inter.to_pbcm()
                    fref.goto_proto_name(inter_pb, "name")

                    if fref.is_field_deleted():
                      print("Interface deleted name %s" %inter.get_name())

                    if fref.is_field_present():
                      print("Interface present name %s" %inter.get_name())

                    fref.goto_proto_name(inter_pb, "inter_info")
                    if fref.is_field_deleted():
                      print("Interface info leaf deleted name %s" %inter.get_inter_info())

                    if fref.is_field_present():
                      print("Interface info leaf present name %s" %inter.get_inter_info())

                print('\n' * 2)
                prep_cnt += 1

                if (prep_cnt == 1):
                  events[1].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, pub_xpath1)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare,
                    on_ready=on_ready)

            self.reg = yield from dts.register(
                    sub_xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.DELTA_READY|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[3].is_set)
        self.assertEqual(2, prep_cnt)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_delete_container_wildcard")

    def test_return_payload(self):
        """
        Verify that when the RWDTS_XACT_FLAG_RETURN_PAYLOAD flag is
        used with action CREATE, the created object is sent back to
        the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_return_payload")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        prep_cnt = 0 

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        results = []

        @asyncio.coroutine
        def pub():

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt
                my_ret = msg
                if prep_cnt == 0: 
                   my_ret.an_int = 123456
                else:
                   my_ret.an_int = 456789
                prep_cnt = prep_cnt + 1
                xpath = ks_path.to_xpath(toyyang.get_schema())
                self.reg.create_element(xpath, my_ret, xact_info.xact.xact)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                )

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.query_create(xpath, rwdts.XactFlag.RET_PAYLOAD, ret)

            for i in res_iter:
                result = yield from i
                self.assertEqual(result.result.an_int, 123456)
                results.append(result.result)

            sec_iter = yield from dts.query_update(xpath, rwdts.XactFlag.RET_PAYLOAD, ret)

            for i in sec_iter:
                result = yield from i
                self.assertEqual(result.result.an_int, 456789)
                results.append(result.result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(len(results), 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_return_payload")

    def test_return_payload_sub(self):
        """
        Verify that when the RWDTS_XACT_FLAG_RETURN_PAYLOAD flag is
        used with action CREATE, the created object is sent back to
        the client

        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_return_payload_sub")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        prep_cnt = 0

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        results = []

        @asyncio.coroutine
        def pub():

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()

            res_iter = yield from dts.query_create(xpath, rwdts.XactFlag.ADVISE | rwdts.XactFlag.RET_PAYLOAD, ret)

            for i in res_iter:
                result = yield from i
                self.assertEqual(result.result.an_int, 123456)
                results.append(result.result)

            sec_iter = yield from dts.query_update(xpath, rwdts.XactFlag.ADVISE | rwdts.XactFlag.RET_PAYLOAD, ret)

            for i in sec_iter:
                result = yield from i
                self.assertEqual(result.result.an_int, 456789)
                results.append(result.result)

            events[1].set()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                nonlocal prep_cnt
                my_ret = msg
                if prep_cnt == 0:
                  my_ret.an_int = 123456
                else:
                  my_ret.an_int = 456789
                prep_cnt = prep_cnt + 1
                xpath = ks_path.to_xpath(toyyang.get_schema())
                self.reg.create_element(xpath, my_ret, xact_info.xact.xact)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                )

            self.reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER,
                    handler=handler)


        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(len(results), 2)
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_return_payload_sub")

    def test_deregister(self):
        """
        Verify that a deregister actually removes registeration from dts member
        table.
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_deregister")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'
        data = toyyang.AContainer()
        data.a_string = self.id()


        @asyncio.coroutine
        def get_published_xpaths(dts):
            published_xpaths = set()

            res_iter = yield from dts.query_read("D,/rwdts:dts")
            for i in res_iter:
                res = (yield from i).result
                for member in res.member:
                    published_xpaths |= {reg.keyspec for reg in member.state.registration if reg.flags == "publisher"}

            return published_xpaths

        @asyncio.coroutine
        def pub():
            nonlocal data
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)


            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath, data)
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready)
            reg = yield from dts.register(
                xpath,
                flags=rwdts.Flag.PUBLISHER | rwdts.Flag.CACHE,
                handler=handler)

            
            yield from events[1].wait()
            published_xpaths = yield from get_published_xpaths(dts)
            self.assertIn(xpath, published_xpaths)

            reg.deregister()
            yield from asyncio.sleep(1, loop=self.loop)
            published_xpaths = yield from get_published_xpaths(dts)
            self.assertNotIn(xpath, published_xpaths)
            events[2].set()

        @asyncio.coroutine
        def client():
            nonlocal data
            tinfo = self.new_tinfo('client')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from events[0].wait()
            res_iter = yield from dts.query_read(xpath, flags=rwdts.XactFlag.TRACE)

            results = []
            for i in res_iter:
                result = yield from i
                results.append(result.result)

            self.assertEqual(len(results), 1)
            self.assertEqual(str(results[0]), str(data))
            events[1].set()

            yield from events[2].wait()
            res_iter = yield from dts.query_read(xpath, flags=rwdts.XactFlag.TRACE)

            resultsafter = []
            for i in res_iter:
                result = yield from i
                resultsafter.append(result.result)
            self.assertEqual(len(resultsafter), 0)
            events[3].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(client(), loop=self.loop)

        self.run_until(events[3].is_set)

        print("}}}}}}}}}}}}}}}}}}}}DONE - test_deregister")

    def test_sub_read(self):
        """
        Verify that subscriber is queried for READ when query is triggered with
        RWDTS_XACT_FLAG_SUB_READ flag

        The test will progress through stages defined by the events list:
            0:  Subscriber registration hit on_ready()
            1:  Publisher finished iterating through dts.query_read() results
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_sub_read")
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            yield from asyncio.gather(dts.ready.wait(), events[0].wait(), loop=self.loop)

            res_iter = yield from dts.query_read(xpath, rwdts.XactFlag.SUB_READ|rwdts.XactFlag.TRACE)

            for i in res_iter:
                result = yield from i
                results.append(result.result)

            events[1].set()

        @asyncio.coroutine
        def sub():
            nonlocal results
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            @asyncio.coroutine
            def on_ready(*args):
                events[0].set()
            
            @asyncio.coroutine 
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)
            
            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)

            self.reg = yield from dts.register(
                xpath,
                flags=rwdts.Flag.SUBSCRIBER,
                handler=handler)

            yield from events[1].wait()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(1, len(results))
        self.assertEqual(str(results[0]), str(ret))
        self.reg.deregister()
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_sub_read")

    def test_reg_dereg(self):
        """
        Verify registration and immediate deregistration works as epxected
        with no on_prepare callback.

        The test does now have any explicit assert statements, the result status
        is determined by test_done timeout.
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = self.id()

        results = []
        
        @asyncio.coroutine
        def check_reg():
            res_iter = yield from self.dts_mgmt.query_read('/rwdts:dts')
            for i in res_iter:
                info_result = yield from i
                for memb in info_result.result.member:
                    match = [reg for reg in memb.state.registration if reg.keyspec == xpath]
                    if (len(match)):
                        return(match)   
                return(match)

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            @asyncio.coroutine
            def on_ready(regh, status):
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready)
            reg = yield from dts.register(
                xpath,
                flags=rwdts.Flag.PUBLISHER,
                handler=handler)
            yield from events[0].wait()
            matched = yield from check_reg()
            self.assertEqual(1, len(matched))
            reg.deregister()
            matched= yield from check_reg()
            self.assertEqual(0, len(matched))
            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)

        self.run_until(events[1].is_set)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_reg_dereg")

    def test_multireg_wc_dereg(self):
        """
        Verify registration and immediate deregistration works as epxected
        with no on_prepare callback.

        The test does now have any explicit assert statements, the result status
        is determined by test_done timeout.
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]
        xpath1 = 'D,/rw-dts-toy-tasklet:a-company/rw-dts-toy-tasklet:an-employee[rw-dts-toy-tasklet:first-name=\'One\'][rw-dts-toy-tasklet:last-name=\'Last\']'
        xpath2 = 'D,/rw-dts-toy-tasklet:a-company/rw-dts-toy-tasklet:an-employee[rw-dts-toy-tasklet:first-name=\'Two\'][rw-dts-toy-tasklet:last-name=\'Last\']'
        xpath3 = 'D,/rw-dts-toy-tasklet:a-company/rw-dts-toy-tasklet:an-employee[rw-dts-toy-tasklet:first-name=\'One\'][rw-dts-toy-tasklet:last-name=\'Last\'][rw-dts-toy-tasklet:phone=\'1234\']'
        xpath4 = 'D,/rw-dts-toy-tasklet:a-company/rw-dts-toy-tasklet:an-employee[rw-dts-toy-tasklet:first-name=\'Two\'][rw-dts-toy-tasklet:last-name=\'Last\'][rw-dts-toy-tasklet:phone=\'1234\']'

        self.deregistered = False
        employees = [toyyang.AnEmployee() for _ in range(2)]
        employees[0].first_name="One"
        employees[1].first_name="Two"
        for i, employee in enumerate(employees):
            employee.last_name = "Last"
            employee.phone = "1234"
            employee.data = i

        @asyncio.coroutine
        def check_reg(xpath):
            res_iter = yield from self.dts_mgmt.query_read('/rwdts:dts')
            for i in res_iter:
                info_result = yield from i
                for memb in info_result.result.member:
                    match = [reg for reg in memb.state.registration if reg.keyspec == xpath]
                    if (len(match)):
                        return(match)
                return(match)

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            @asyncio.coroutine
            def on_ready1(regh, status):
                print("onready 1 invoked")
                events[0].set()
            @asyncio.coroutine
            def on_ready2(regh, status):
                print("onready 2 invoked")
                events[1].set()

            @asyncio.coroutine
            def on_prepare1(xact_info, *args):
                print("on_prepare1 invoked")
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath3)

            @asyncio.coroutine
            def on_prepare2(xact_info, *args):
                print("on_prepare2 invoked")
                if self.deregistered:
                    events[3].set()
                else:
                    events[2].set()

                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath4)

            handler1 = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready1, on_prepare=on_prepare1)
            reg1 = yield from dts.register(
                xpath1,
                flags=rwdts.Flag.SUBSCRIBER,
                handler=handler1)
            yield from events[0].wait()

            handler2 = rift.tasklets.DTS.RegistrationHandler(on_ready=on_ready2, on_prepare=on_prepare2)
            reg2 = yield from dts.register(
                xpath2,
                flags=rwdts.Flag.SUBSCRIBER,
                handler=handler2)
            yield from events[1].wait()
            
            yield from dts.query_create(xpath3, rwdts.XactFlag.ADVISE, employees[0])
            yield from dts.query_create(xpath4, rwdts.XactFlag.ADVISE, employees[1])

            yield from events[2].wait()
            matched = yield from check_reg(xpath1)
            self.assertEqual(1, len(matched))
            reg1.deregister()
            matched= yield from check_reg(xpath1)
            self.assertEqual(0, len(matched))
            self.deregistered = True
 
            employees[1].data = 2987
            yield from dts.query_update(xpath4, rwdts.XactFlag.ADVISE,  employees[1])


        asyncio.ensure_future(pub(), loop=self.loop)

        self.run_until(events[3].is_set)
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_multireg_wc_dereg")

def main():
    if 'RIFT_NO_SUDO_REAPER' not in os.environ:
        os.environ['RIFT_NO_SUDO_REAPER'] = '1'

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args, _ = parser.parse_known_args()

    DtsTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)


if __name__ == '__main__':
    main()

