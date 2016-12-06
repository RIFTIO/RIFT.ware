#!/usr/bin/env python3
import argparse
import asyncio
import logging
import os
import tempfile
import unittest
import xmlrunner

import gi
gi.require_version("RwDts", "1.0")
gi.require_version("RwDtsWrapperTestYang", "1.0")
gi.require_version("RwlogMgmtYang", "1.0")
from gi.repository import (
    RwDts,
    RwDtsWrapperTestYang as yang,
    RwlogMgmtYang,
)

import rift.tasklets
import rift.test.dts

from rift.rwpb import(
    Schema,
    accumulate,
    KeyCategory,
)

import shared

schema = Schema(["rwlog-mgmt","rw-dts-wrapper-test"])

# need to add the current directory to the PLUGINDIR so the sender/receiver can be found
# this has to happen at load time becuase of when this variable is loaded by libpeas
os.environ["PLUGINDIR"] += (":" + os.path.dirname(os.path.realpath(__file__)))

class DtsWrapperTest(rift.test.dts.AbstractDTSTest):

    @classmethod
    def start_logd(cls):
        plugin_dir = os.path.join(os.environ["RIFT_INSTALL"], "usr/lib/rift/plugins")
        cls.rwmain.add_tasklet(
                os.path.join(plugin_dir, 'rwlogd-c'),
                'rwlogd-c'
                )

    @classmethod
    def start_sender(cls):
        cls.rwmain.add_tasklet(
            os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                'sender'
            ),
            'sender'
        )

    @classmethod
    def start_receiver(cls):
        cls.rwmain.add_tasklet(
            os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                'receiver'
            ),
            'receiver'
        )

    @classmethod
    def configure_schema(cls):
        return yang.get_schema()

    @classmethod
    def configure_timeout(cls):
        return 30

    def configure_test(self, loop, test_id):
        self.tinfo = self.new_tinfo(self.id())
        self.dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop)
        
    @asyncio.coroutine
    def turn_on_logging(self):
        self.start_logd()
        yield from self.wait_for_tasklet_running("rwlogd-c")

        yield from asyncio.sleep(.1, loop=self.loop)

        temp_file = tempfile.mktemp()
        print("logging file", temp_file)
        self.log.debug("Created file log sink location: %s", temp_file)
        with open(temp_file, 'w') as file_hdl:
            base_file = os.path.basename(file_hdl.name)
            debug_msg = schema.protobuf.logging()
            debug_msg.from_dict({
                "sink": [{
                    "name": "file_a",
                    "filename": base_file,
                    "filter": {
                        "category": [{
                            "name": "rw-dts-api-log",
                            "severity": "debug"
                        },
                        {
                            "name": "rw-dts-router-log",
                            "severity": "debug"
                        }],
                    }
                }]
            })

            yield from self.dts.query_update(
                schema.protobuf.logging._path,
                RwDts.XactFlag.ADVISE,
                debug_msg,
            )

    @asyncio.coroutine
    def start_tasklets(self):
        if self.log_level == logging.DEBUG:
            yield from self.turn_on_logging()

        self.start_sender()
        self.start_receiver()
        yield from self.wait_for_tasklet_running("sender")
        yield from self.wait_for_tasklet_running("receiver")

    @rift.test.dts.async_test
    def test_python_dts_wrappers(self):
        yield from self.start_tasklets()

        sender = shared.sender
        receiver = shared.receiver

        ##### test container member data

        # test member data list
        
        with accumulate(sender):
            sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("bar", 27)].toggle = False

            x = sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("max", 42)]
            x.toggle = False
            x.key_1 = "max"
            x.key_2 = 42

        yield from asyncio.sleep(.1, loop=self.loop)

        self.assertEqual(len(sender.data.rw_dts_wrapper_test.complex_op_data.mirror.get_protobuf()), 2)
        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.complex_op_data.mirror
        self.assertEqual(receiver.results[key._path], 2)

        # test member data list delete

        with accumulate(sender):
            del sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("max", 42)]
        yield from asyncio.sleep(.1, loop=self.loop)

        self.assertEqual(len(sender.data.rw_dts_wrapper_test.complex_op_data.mirror.get_protobuf()), 1)
        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.complex_op_data.mirror
        self.assertEqual(receiver.results[key._path], 3)

        # test member data list discard

        try:
            with accumulate(sender):
                sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("qwe", 77)].toggle = False
                sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("asd", 44)].toggle = False
                1/0 # force a discard
        except ZeroDivisionError:
            pass

        self.assertEqual(len(sender.data.rw_dts_wrapper_test.complex_op_data.mirror.get_protobuf()), 1)
        # make sure the subscriber didn't see anything
        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.complex_op_data.mirror
        self.assertEqual(receiver.results[key._path], 3)

        # test member data list discard delete

        try:
            with accumulate(sender):
                del sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("bar", 27)]
                1/0 # force a discard
        except ZeroDivisionError:
            pass
        
        yield from asyncio.sleep(.1, loop=self.loop)

        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.complex_op_data.mirror
        self.assertEqual(receiver.results[key._path], 3)
        self.assertEqual(len(sender.data.rw_dts_wrapper_test.complex_op_data.mirror.get_protobuf()), 1)
        self.assertEqual(sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("bar", 27)].get_protobuf().key_1, "bar")
        self.assertEqual(sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("bar", 27)].get_protobuf().key_2, 27)
        self.assertEqual(sender.data.rw_dts_wrapper_test.complex_op_data.mirror[("bar", 27)].get_protobuf().toggle, False)

        ##### test container member data

        # test member data container
        with accumulate(sender):
            sender.data.rw_dts_wrapper_test.simple_op_member_data.toggle = True
        yield from asyncio.sleep(.1, loop=self.loop)

        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.simple_op_member_data
        self.assertEqual(receiver.results[key._path], 1)

        # test member data container discard 

        try:
            with accumulate(sender):
                sender.data.rw_dts_wrapper_test.simple_op_member_data.toggle = False
                1/0 # force a discard
        except ZeroDivisionError:
            pass
        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.simple_op_member_data
        self.assertEqual(receiver.results[key._path], 1)
        self.assertEqual(sender.data.rw_dts_wrapper_test.simple_op_member_data.toggle, True)

        # test member data container delete discard

        try:
            with accumulate(sender):
                del sender.data.rw_dts_wrapper_test.simple_op_member_data
                1/0
        except ZeroDivisionError:
            pass

        yield from asyncio.sleep(.1, loop=self.loop)
        self.assertEqual(receiver.results[key._path], 1)
        self.assertEqual(sender.data.rw_dts_wrapper_test.simple_op_member_data.toggle, True)

        # test member data container delete

        with accumulate(sender):
            del sender.data.rw_dts_wrapper_test.simple_op_member_data
        yield from asyncio.sleep(.1, loop=self.loop)
        self.assertEqual(receiver.results[key._path], 2)


        ##### test @subscribe

        # test config container subscriber

        key = schema.key(KeyCategory.Config).rw_dts_wrapper_test.simple_config_data
        msg = schema.protobuf.rw_dts_wrapper_test.simple_config_data()
        msg.toggle = True

        yield from self.dts.query_create(
            key,
            RwDts.XactFlag.ADVISE,
            msg
        )
        yield from asyncio.sleep(.1, loop=self.loop)

        self.assertEqual(sender.results[key._path], 1)

        # test config list subscriber

        key = schema.key(KeyCategory.Config).rw_dts_wrapper_test.complex_config_data.actual[("foo",12)]
        msg = schema.protobuf.rw_dts_wrapper_test.complex_config_data.actual()
        msg.key_1 = "foo"
        msg.key_2 = 12
        msg.toggle = True

        yield from self.dts.query_create(
            key,
            RwDts.XactFlag.ADVISE,
            msg
        )
        yield from asyncio.sleep(.1, loop=self.loop)

        key = schema.key(KeyCategory.Config).rw_dts_wrapper_test.complex_config_data.actual
        self.assertEqual(sender.results[key._path], 1)

        # test config list subscriber

        key = schema.key(KeyCategory.Config).rw_dts_wrapper_test.complex_config_data.actual[("foo",12)]

        yield from self.dts.query_delete(
            key,
            RwDts.XactFlag.ADVISE,
        )
        yield from asyncio.sleep(.1, loop=self.loop)

        key = schema.key(KeyCategory.Config).rw_dts_wrapper_test.complex_config_data.actual
        self.assertEqual(sender.results[key._path], 2)

        ##### test misc

        # test that accumulate throws exception for when you enter a block with pending changes

        sender.data.rw_dts_wrapper_test.simple_op_member_data.toggle = False

        try:
            with accumulate(sender):
                self.assertEqual(False, True)
        except RuntimeError:
            pass
        finally:
            self.assertEqual(sender.data._rw_dts_wrapper_test._current_element.is_dirty(), True)
            sender.data.rw_dts_wrapper_test.discard()
            key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.simple_op_member_data
            self.assertEqual(receiver.results[key._path], 2)

        with accumulate(sender):
            # make sure we get back into a good state after the discard
            sender.data.rw_dts_wrapper_test.simple_op_member_data.toggle = False
        yield from asyncio.sleep(.1, loop=self.loop)

        key = schema.key(KeyCategory.Data).rw_dts_wrapper_test.simple_op_member_data
        self.assertEqual(receiver.results[key._path], 3)

        ##### caveat
        '''
        the member data implementation is a singleton per registrant, so you 
        cannot hold a reference in the middle
        '''
        x = sender.data.rw_dts_wrapper_test.complex_op_data
        y = sender.data.rw_dts_wrapper_test.simple_op_member_data
        
        self.assertEqual(dir(x)[0], dir(y)[0])


def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args, _ = parser.parse_known_args()

    DtsWrapperTest.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main()

if __name__ == '__main__':
    main()
