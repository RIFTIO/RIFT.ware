#!/usr/bin/env python3

# STANDARD_RIFT_IO_COPYRIGHT


import asyncio
import logging
import os
import sys
import types
import unittest
import xmlrunner

import gi.repository.CF as cf
import gi.repository.RwBaseYang as rwbase
import gi.repository.RwDts as rwdts
import gi.repository.RwDtsToyTaskletYang as toyyang
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.RwTypes as rwtypes
import gi.repository.RwvnfdYang as rwvnfd

import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


@unittest.skipUnless('FORCE' in os.environ or True, 'Useless until DTS can actually be deallocated, RIFT-7085')
class DtsTestCase(unittest.TestCase):

    def test_separate_pub_sub(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are using
        different dts api handles (and therefore going through the router).

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.read() results
        """
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

        xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_separate_pub_sub'

        @asyncio.coroutine
        def pub():
            @asyncio.coroutine
            def on_ready(*args):
                events[0].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(1, len(results))
        self.assertEqual(str(results[0]), str(ret))

    def test_na_on_prepare(self):
        """
        Verify that the publisher can issue a response with the NA return code.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.read() results
        """
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
            dts = rift.tasklets.DTS(tinfo, self.loop)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(0, len(results))

    def test_nack_on_prepare(self):
        """
        Verify that the publisher can issue a response with the NACK return code.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
            1:  Subscriber finished iterating through dts.read() results
        """
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
                xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
            yield from events[0].wait()

            res_iter = yield from dts.read(xpath, rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[1].is_set)

        self.assertEqual(0, len(results))


    def test_registration_element(self):
        """
        Verify that the element functions on a registration work

        The test will progress through stages defined by the events list:
            0:  The publisher is up and hit the on_ready() callback
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.read() iterator
        """
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_registration_element'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                regh.create_element(xpath, ret)
                events[0].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.CACHE|rwdts.Flag.NO_PREP_READ,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                results.append(regh.get_element(xpath))
                events[1].set()

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            res_iter = yield from dts.read(
                    xpath,
                    rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0]), str(ret))
        self.assertEqual(str(results[1]), str(ret))

    def test_precommit(self):
        """
        Verify that precommit/commit works; the update() triggers a precommit/commit

        The test will progress through stages defined by the events list:
            0:  The publisher is up and hit the on_ready() callback
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.read() iterator
            3:  The publisher finished with the update(), which causes precommit/commit
        """
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = 'test_registration_element'

        ret2 = toyyang.AContainer()
        ret2.a_string = 'UPDATED test_registration_element'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from dts.create(xpath, rwdts.XactFlag.ADVISE, ret1)
            events[0].set()
            yield from events[1].wait()
            yield from dts.update(xpath, rwdts.XactFlag.ADVISE, ret2)
            events[3].set()

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
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

            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[3].wait()
            results.append(reg.get_element(xpath))
            res_iter = yield from dts.read(
                    xpath,
                    rwdts.XactFlag.MERGE)

            for i in res_iter:
                result = yield from i
                results.append(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 1)
        self.assertEqual(str(results[0]), str(ret2))

    def test_both_commit_and_read(self):
        """
        Verify that precommit/commit works as well as read - via PUB works;
        The update() triggers a precommit/commit - this sends ret2
        The read() is creating a transaction to the PUB - this returns ret1

        The test will progress through stages defined by the events list:
            0:  The publisher created the xpath element
            1:  The subscriber is up and hit the on_ready() callback
            2:  The subscriber finished running throught the dts.read() iterator
            3:  The publisher is up and hit the on_ready() callback
        """
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret1 = toyyang.AContainer()
        ret1.a_string = 'test_registration_element'

        ret2 = toyyang.AContainer()
        ret2.a_string = 'UPDATED test_registration_element'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            #yield from dts.create(xpath, rwdts.XactFlag.ADVISE|rwdts.XactFlag.TRACE, ret1)
            yield from dts.create(xpath, rwdts.XactFlag.ADVISE, ret1)
            events[0].set()
            yield from events[1].wait()
            #regh.update_element(xpath, ret2)
            #yield from dts.update(xpath, rwdts.XactFlag.ADVISE|rwdts.XactFlag.TRACE, ret2)
            yield from dts.update(xpath, rwdts.XactFlag.ADVISE, ret2)

            @asyncio.coroutine
            def on_ready(regh, status):
                print('======PUB:on_ready', status)
                events[3].set()
            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('======PUB:on_prepare', args)
                #xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret1)


            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
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

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit)

            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[3].wait()
            results.append(reg.get_element(xpath))
            res_iter = yield from dts.read(
                    xpath,
                    rwdts.XactFlag.MERGE)

            print ('------SUB:res_iter:', res_iter)
            for i in res_iter:
                result = yield from i
                print ('------SUB:result:', result)
                results.append(result)

            events[2].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(len(results), 2)
        self.assertEqual(str(results[0]), str(ret2))
        self.assertEqual(str(results[1]), str(ret1))

    def test_abort(self):
        """
        Verify that abort exception coming back on dts.update()
        when the subscriber is sending a NACK in _prepare()

        The test will progress through stages defined by the events list:
            0:  The publisher created the xpath element
            1:  The subscriber is up and hit the on_ready() callback
            2:  The publisher finished running through
            3:  The subscriber finished running through
        """
        results = []
        events = [asyncio.Event(loop=self.loop) for _ in range(4)]

        xpath = 'D,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'test_abort'

        abort_exception = False
        abort_callback = False

        @asyncio.coroutine
        def pub():
            nonlocal abort_exception

            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            #yield from dts.create(xpath, rwdts.XactFlag.ADVISE|rwdts.XactFlag.TRACE, ret)
            yield from dts.create(xpath, rwdts.XactFlag.ADVISE, ret)
            events[0].set()
            yield from events[1].wait()

            try:
                #yield from dts.update(xpath, rwdts.XactFlag.ADVISE|rwdts.XactFlag.TRACE, ret)
                yield from dts.update(xpath, rwdts.XactFlag.ADVISE, ret)
            except rift.tasklets.dts.TransactionAborted:
                abort_exception = True
            events[2].set()

            yield from events[3].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
            yield from events[0].wait()

            @asyncio.coroutine
            def on_ready(regh, status):
                print('------SUB:on_ready', regh)
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, *args):
                print('------SUB:on_prepare', args)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

            def on_abort(*args):
                nonlocal abort_callback
                print('------SUB:on_abort', args)
                abort_callback = True
                #events[5].set()
                return rwdts.MemberRspCode.ACTION_OK

            def on_precommit(*args):
                print('------SUB:on_precommit', args)
                return rwdts.MemberRspCode.ACTION_NOT_OK

            def on_commit(*args):
                print('------SUB:on_commit', args)
                return rwdts.MemberRspCode.ACTION_OK

            handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_abort=on_abort,
                on_prepare=on_prepare,
                on_precommit=on_precommit,
                on_commit=on_commit
                )

            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE,
                    handler=handler)

            yield from events[1].wait()
            yield from events[2].wait()
            #yield from events[5].wait()

            events[3].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[3].is_set)

        self.assertTrue(abort_exception)
        self.assertTrue(abort_callback)
        self.assertEqual(len(results), 0)

    def test_appconf_group(self):
        """
        Verify that we hit all stages in an appconf group with a single
        registration, that the callback arguments are all wrapped correctly and
        that we get the correct configuration.

        The test will progress through stages defined by the events list:
            0:  Subscriber has created the appconf group and hit the first
                on_apply callback.
            1:  Publisher has finished dts.update()
            2:  Subscriber finished handling the dts.update()
        """
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
        }
        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.an_int = 10
        ret.a_string = 'dead beef'


        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()

            yield from dts.update(
                    xpath,
                    flags=rwdts.XactFlag.ADVISE,
                    msg=ret)

            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            def on_init(acg, xact):
                results['init'] = '%s' % (type(acg),)

            def on_deinit(acg, xact):
                results['deinit'] = '%s' % (type(acg),)

                for reg in acg.registrations:
                    for e in reg.elements:
                        results['elements_iter'].append(str(e))

                events[1].set()

            def on_validate(dts, acg, xact):
                results['validate'] = '%s %s' % (type(dts), type(acg))

            def on_apply(dts, acg, xact, action):
                if not events[0].is_set():
                    # This will be called once the appconf group is actually subscribed
                    # with the router.
                    results['apply_action_1'] = action
                    events[0].set()
                else:
                    results['apply'] = '%s %s' % (type(dts), type(acg))
                    results['apply_action_2'] = action


            handlers = rift.tasklets.AppConfGroup.Handler(
                    on_init=on_init,
                    on_deinit=on_deinit,
                    on_validate=on_validate,
                    on_apply=on_apply)

            @asyncio.coroutine
            def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
                acg.handle.prepare_complete_ok(xact_info.handle)
                results['on_prepare'] = str(msg)

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


    def test_rpc(self):
        """
        Test that RPC input/output works.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.rpc() results
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = 'req val'

        resp = toyyang.ToyRpcOutput()
        resp.ret = 'resp val'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub():
            nonlocal results
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()

            rpc_iter = yield from dts.rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                #print (i, "resp:%u %s" %(results['n_results'],resp))
                results['output'] = str(resp)
                results['n_results'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['input'], str(req))
        self.assertEqual(results['output'], str(resp))
        self.assertEqual(results['n_results'], 1)

    def test_rpc_nack(self):
        """
        Test that RPC input/output works; test for NACK.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.rpc() results
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = 'req val'

        resp = toyyang.ToyRpcOutput()
        resp.ret = 'resp val'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub():
            nonlocal results
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()

            rpc_iter = yield from dts.rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                results['output'] = str(resp)
                results['n_results'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['input'], str(req))
        self.assertEqual(results['output'], '')
        self.assertEqual(results['n_results'], 0)

    def test_rpc_na(self):
        """
        Test that RPC input/output works; test for NA.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.rpc() results
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        results = {
            'input': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = 'req val'

        resp = toyyang.ToyRpcOutput()
        resp.ret = 'resp val'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub():
            nonlocal results
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[1].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()

            rpc_iter = yield from dts.rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                #print (i, "resp:%u %s" %(results['n_results'],resp))
                results['output'] = str(resp)
                results['n_results'] += 1

            events[1].set()

        asyncio.ensure_future(pub(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set)

        self.assertEqual(results['input'], str(req))
        self.assertEqual(results['output'], '')
        self.assertEqual(results['n_results'], 0)

    def test_rpc_ack_and_nack(self):
        """
        Test that RPC input/output works. Test with one pub sending ACK and
        another one sending NACK.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.rpc() results
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = {
            'input1': '',
            'input2': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = 'req val'

        resp = toyyang.ToyRpcOutput()
        resp.ret = 'resp val'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub1():
            nonlocal results
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def pub2():
            nonlocal results
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            rpc_iter = yield from dts.rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                results['output'] = str(resp)
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

    def test_rpc_ack_and_na(self):
        """
        Test that RPC input/output works. Test with one pub sending ACK and
        another one sending NA.

        The test will progress through stages defined by the events list:
            0:  Publisher is up and has hit the on_ready registration callback
            1:  Subscriber finished iterating through dts.rpc() results
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = {
            'input1': '',
            'input2': '',
            'output': '',
            'n_results': 0,
        }

        req = toyyang.ToyRpcInput()
        req.val = 'req val'

        resp = toyyang.ToyRpcOutput()
        resp.ret = 'resp val'

        in_xpath = 'I,/rw-dts-toy-tasklet:toy-rpc'
        out_xpath = 'O,/rw-dts-toy-tasklet:toy-rpc'

        @asyncio.coroutine
        def pub1():
            nonlocal results
            tinfo = self.new_tinfo('pub1')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def pub2():
            nonlocal results
            tinfo = self.new_tinfo('pub2')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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

            yield from dts.register(
                    in_xpath,
                    handlers,
                    flags=rwdts.Flag.PUBLISHER)
            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            nonlocal results

            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            rpc_iter = yield from dts.rpc(in_xpath, flags=0, msg=req)

            for i in rpc_iter:
                resp = yield from i
                #print (i, "resp:%u %s" %(results['n_results'],resp))
                results['output'] = str(resp)
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

    def test_multiple_queries(self):
        """
        Test running multiple queries at the same time.

        The test will progress through stages defined by the events list:
            0:  First publisher is up and hit on_ready
            1:  Second publisher is up and hit on_ready
            2:  Subscriber ran through results from dts.read() to both publishers.
        """
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
        v1.a_string = 'test_multiple_queries'

        v2 = toyyang.AnotherContainer()
        v2.val = 'test_multiple_queries'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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
            yield from dts.register(
                    xpath1,
                    handler1,
                    flags=rwdts.Flag.PUBLISHER)

            handler2 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready_v2,
                    on_prepare=on_prepare_v2)
            yield from dts.register(
                    xpath2,
                    handler2,
                    flags=rwdts.Flag.PUBLISHER)

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            @asyncio.coroutine
            def sub1():
                nonlocal results

                iter1 = yield from dts.read(xpath1, flags=0)
                for i in iter1:
                    resp = yield from i
                    results['sub1'] = str(resp)
                    results['n_sub1'] += 1


            @asyncio.coroutine
            def sub2():
                nonlocal results

                iter2 = yield from dts.read(xpath2, flags=0)
                for i in iter2:
                    resp = yield from i
                    results['sub2'] = str(resp)
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

    def test_elements_iter(self):
        """
        Verify that the elements iterator is functioning correctly (hits all
        elements and resets when the loop exits early).
        """
        results = {
                'all': [],
                'early': [],
                'after-early': [],
        }
        done = asyncio.Event(loop=self.loop)

        employees = [toyyang.Employee() for _ in range(2)]
        for i, employee in enumerate(employees):
            employee.name = str(i)

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)


        asyncio.ensure_future(pub(), loop=self.loop)

        self.run_until(done.is_set)

        self.assertEqual(2, len(results['all']))
        self.assertEqual(1, len(results['early']))
        self.assertEqual(2, len(results['after-early']))

    def test_multiple_responses(self):
        """
        Verify that returning multiple results from a single xpath works with
        the query iterator

        The test will progress through stages defined by the events list:
            0:  First publisher is up and hit on_ready
            1:  Second publisher is up and hit on_ready
            2:  Subscriber finshed getting all results
        """
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]
        results = []

        emp1 = toyyang.Employee()
        emp1.name = '1'

        emp2 = toyyang.Employee()
        emp2.name = '2'

        emp3 = toyyang.Employee()
        emp3.name = '3'

        xpath = 'D,/rw-dts-toy-tasklet:employee'

        @asyncio.coroutine
        def pub1():
            tinfo = self.new_tinfo('puba')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.SHARED,
                    handler=handler)

        @asyncio.coroutine
        def pub2():
            tinfo = self.new_tinfo('pubb')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            @asyncio.coroutine
            def on_ready(regh, status):
                events[1].set()

            @asyncio.coroutine
            def on_prepare(xact_info, query_action, ks_path, msg):
                xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath + '[name="%s"]' % (emp3.name,), emp3)

            handler = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready,
                    on_prepare=on_prepare)
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER|rwdts.Flag.SHARED,
                    handler=handler)

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

            yield from events[0].wait()
            yield from events[1].wait()

            res_iter = yield from dts.read(xpath)

            for i in res_iter:
                r = yield from i
                results.append(str(r))

            events[2].set()

        asyncio.ensure_future(pub1(), loop=self.loop)
        asyncio.ensure_future(pub2(), loop=self.loop)
        asyncio.ensure_future(sub(), loop=self.loop)

        self.run_until(events[2].is_set)

        self.assertEqual(3, len(results))
        self.assertIn(str(emp1), results)
        self.assertIn(str(emp2), results)
        self.assertIn(str(emp3), results)

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
        results = {
            'audit_handle': None,
            'audit_regh': None,
            'on_prepare_calls': 0,
        }
        events = [asyncio.Event(loop=self.loop) for _ in range(3)]

        xpath = 'C,/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'audit_subscriber'

        @asyncio.coroutine
        def pub():
            tinfo = self.new_tinfo('pub')
            dts = rift.tasklets.DTS(tinfo, self.loop)

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
            reg = yield from dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)

            yield from events[2].wait()

        @asyncio.coroutine
        def sub():
            tinfo = self.new_tinfo('sub')
            dts = rift.tasklets.DTS(tinfo, self.loop)
            yield from events[0].wait()

            def on_apply(dts, acg, xact, action):
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


def main():
    plugin_dir = os.path.join(os.environ["RIFT_INSTALL"], "usr/lib/rift/plugins")

    if 'MESSAGE_BROKER_DIR' not in os.environ:
        os.environ['MESSAGE_BROKER_DIR'] = os.path.join(plugin_dir, 'rwmsgbroker-c')

    if 'ROUTER_DIR' not in os.environ:
        os.environ['ROUTER_DIR'] = os.path.join(plugin_dir, 'rwdtsrouter-c')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()


