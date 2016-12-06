#!/usr/bin/env python3
import argparse
import asyncio
import logging
import os
import tempfile
import unittest
import xmlrunner

# Add the current directory to the PLUGINDIR so we can use the plugin
# file added here.
os.environ["PLUGINDIR"] += (":" + os.path.dirname(os.path.realpath(__file__)))
import gi
gi.require_version("RwDts", "1.0")
gi.require_version("RwlogMgmtYang", "1.0")
from gi.repository import (
    RwDts,
    RwlogMgmtYang,
)

import rift.tasklets
import rift.test.dts

class RwLogTestCase(rift.test.dts.AbstractDTSTest):
    # Disable the log_utest_mode so that log messages actually get logged
    # using the rwlog handler since that is what we are testing here.
    log_utest_mode = False

    @classmethod
    def configure_suite(cls, rwmain):
        plugin_dir = os.path.join(os.environ["RIFT_INSTALL"], "usr/lib/rift/plugins")
        rwmain.add_tasklet(
                os.path.join(plugin_dir, 'rwlogd-c'),
                'rwlogd-c'
                )

    @classmethod
    def start_log_test_tasklet(cls):
        cls.rwmain.add_tasklet(
                os.path.join(
                    os.path.dirname(os.path.realpath(__file__)),
                    'logtesttasklet-python'
                    ),
                'logtesttasklet-python'
                )

    @classmethod
    def configure_schema(cls):
        return RwlogMgmtYang.get_schema()

    @classmethod
    def configure_timeout(cls):
        return 240

    def configure_test(self, loop, test_id):
        self.log.debug("STARTING - %s", self.id())
        self.tinfo = self.new_tinfo(self.id())
        self.dts = rift.tasklets.DTS(self.tinfo, self.schema, self.loop)

    @rift.test.dts.async_test
    def test_tasklet_logging(self):
        yield from self.wait_for_tasklet_running("rwlogd-c")
        #yield from asyncio.sleep(2, loop=self.loop)

        temp_file = tempfile.mktemp()
        self.log.debug("Created file log sink location: %s", temp_file)
        try:
            with open(temp_file, 'w') as file_hdl:
                base_file = os.path.basename(file_hdl.name)
                debug_msg = RwlogMgmtYang.Logging.from_dict({
                    "sink": [{
                        "name": "file_a",
                        "filename": base_file,
                        "filter": {
                            "category": [{
                                    "name": "rw-logtest-log",
                                    "severity": "debug"
                                    }],
                            }
                        }]
                    })

                yield from self.dts.query_update(
                        "C,/rwlog-mgmt:logging",
                        RwDts.XactFlag.ADVISE | RwDts.XactFlag.TRACE,
                        debug_msg,
                        )

                self.start_log_test_tasklet()

                for i in range(10):
                    self.log.debug("rwmain_debug")
                    yield from asyncio.sleep(.1, loop=self.loop)

                # The logtesttasklet signals being done, by moving into DTS Running state
                yield from self.wait_for_tasklet_running("logtesttasklet-python")

            with open(temp_file, 'r') as file_hdl:
                file_hdl.seek(0)
                output_str = file_hdl.read()
                lines = output_str.split("\n")

                assert len(lines) > 1

                assert "tasklet_debug" in output_str

                library_log_count = len([line for line in lines if "library_debug" in line])
                assert library_log_count == 10

                # The log message here should only show up in rw-generic-log
                # and not have been part of the filtered messages
                assert "rwmain_debug" not in output_str

                threaded_library_log_count = len([line for line in lines if "threaded_library" in line])
                self.log.debug("Got threaded library log count of: %s", threaded_library_log_count)
                assert threaded_library_log_count == 10

                assert "threaded_nested_library" not in output_str

        finally:
            pass
            #os.remove(temp_file)

def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args, _ = parser.parse_known_args()

    RwLogTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()
