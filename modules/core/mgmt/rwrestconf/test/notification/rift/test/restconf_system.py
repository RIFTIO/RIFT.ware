#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Balaji Rajappa, Varun Prasad
# Creation Date: 02/19/2016
# 

"""
The Test setup consists of a VM containing the RESTCONF tasklet and its
dependencies. The Test setup is launched as a subprocess and the tests are
executed from this script.

The following tests are performed
 - Testing the restconf-state to get the supported streams and its attributes
 - Testing both HTTP based event streaming and Websocket based event streaming
"""


import asyncio
import logging
import os

import gi
gi.require_version('IetfRestconfMonitoringYang', '1.0')
gi.require_version('RwlogMgmtYang', '1.0')

import rift.auto.session
import rift.vcs.api
import rift.vcs.compiler
import rift.vcs.manifest
import rift.vcs.vms

import time

logger = logging.getLogger(__name__)

class TestSystem(object):
    class TestVM(rift.vcs.VirtualMachine):
        """
        A Basic VM containing the following tasklets.
            - DtsRouter
            - uAgent (confd will be launched by this)
            - RestconfTasklet
            - RiftCli (not really required, but to satisfy certain constraints
        """
        def __init__(self, *args, **kwargs):
            super().__init__(name="TestVM", *args, **kwargs)
            self.add_proc(rift.vcs.DtsRouterTasklet())
            self.add_tasklet(rift.vcs.uAgentTasklet())
            self.add_proc(rift.vcs.procs.RiftCli(
                    netconf_username="admin",
                    netconf_password="admin"));

            self.add_proc(rift.vcs.RestconfTasklet())

    def __init__(self):
        self.rift_install = os.environ['RIFT_INSTALL'] 
        self.manifest_path = os.path.join(self.rift_install, "rest_test_manifest.xml")
        self.rwmain_path = os.path.join(self.rift_install, "usr/bin/rwmain")
        self.loop = asyncio.get_event_loop()
        self.rwmain_proc = None

    def generate_manifest(self):
        test_vm = TestSystem.TestVM(ip="127.0.0.1")
        test_vm.lead = True

        colony = rift.vcs.core.Colony(name='top', uid=1)
        colony.append(test_vm)

        sysinfo = rift.vcs.core.SystemInfo(
            mode='ethsim',
            collapsed=True,
            zookeeper=rift.vcs.manifest.RaZookeeper(zake=True, master_ip="127.0.0.1"),
            colonies=[colony],
            multi_broker=True,
            multi_dtsrouter=False,
        )

        # Compile the manifest
        compiler = rift.vcs.compiler.LegacyManifestCompiler()
        _, manifest = compiler.compile(sysinfo)

        vcs = rift.vcs.api.RaVcsApi()
        vcs.manifest_generate_xml(manifest, self.manifest_path)

    def start_testware(self):
        @asyncio.coroutine
        def start():
            logger.info("Starting rwmain")
            rwmain_cmd = self.rwmain_path + " -m " + self.manifest_path

            # Launch rwmain as a session leader, so that this script is not
            # killed when rwmain terminates (different process group)
            self.rwmain_proc = yield from asyncio.subprocess.create_subprocess_shell(
                                    rwmain_cmd, loop=self.loop,
                                    preexec_fn=os.setsid,
                               )

            # wait till RESTCONF is ready 
            while True:
                yield from asyncio.sleep(3, loop=self.loop)
                try:
                    yield from self.loop.create_connection(
                            lambda: asyncio.Protocol(),
                            "127.0.0.1",
                            "8888",
                            )
                    break
                except OSError as e:
                    logger.warning("Connection failed: %s", str(e))

        pwd = os.getcwd()
        os.chdir(self.rift_install)
        # ATTN: Are these really needed?
        os.system("rm -rf ./persist*")
        self.generate_manifest()
        os.environ['RIFT_NO_SUDO_REAPER'] = '1'
        self.loop.run_until_complete(
                    asyncio.wait_for(start(), timeout=300, loop=self.loop))

        # wait till the system is ready 
        mgmt_session = rift.auto.session.NetconfSession(host="127.0.0.1")
        mgmt_session.connect()
        rift.vcs.vcs.wait_until_system_started(mgmt_session)

        # wait a little more (this was added to have rwlog ready). The tests set
        # a logging configuration to trigger NETCONF notifications.
        time.sleep(5)

        logger.info("Testware started and running.")
        os.chdir(pwd)

    def stop_testware(self):
        @asyncio.coroutine
        def stop():
            try:
                self.rwmain_proc.terminate()
                yield from self.rwmain_proc.wait()
            except ProcessLookupError:
                pass

        if self.rwmain_proc is not None:
            self.loop.run_until_complete(stop())
            self.rwmain_proc = None
        logger.info("testware terminated.")

