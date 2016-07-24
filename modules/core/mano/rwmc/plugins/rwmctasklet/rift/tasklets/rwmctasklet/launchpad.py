
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import ncclient
import ncclient.asyncio_manager
import os
import time
from datetime import timedelta

from . import salt

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwCloudYang', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwConmanYang', '1.0')
gi.require_version('RwNsmYang', '1.0')
from gi.repository import  RwYang, RwCloudYang, RwBaseYang, RwResourceMgrYang, RwConmanYang, RwNsmYang, RwLaunchpadYang


class JobNotStarted(Exception):
    pass


class LaunchpadStartError(Exception):
    pass


class LaunchpadConfigError(Exception):
    pass


class Launchpad(object):
    def __init__(self, mgmt_domain_name, node_id=None, ip_address=None):
        self._mgmt_domain_name = mgmt_domain_name
        self._node_id = node_id
        self._ip_address = ip_address

    def __repr__(self):
        return "Launchpad(mgmt_domain_name={}, node_id={}, ip_address={})".format(
                self._mgmt_domain_name, self._node_id, self._ip_address
                )

    @property
    def ip_address(self):
        return self._ip_address

    @ip_address.setter
    def ip_address(self, ip_address):
        self._ip_address = ip_address

    @property
    def node_id(self):
        return self._node_id

    @node_id.setter
    def node_id(self, node_id):
        self._node_id = node_id

    @property
    def mgmt_domain_name(self):
        return self._mgmt_domain_name

    @property
    def exe_path(self):
        return "{}/demos/launchpad.py".format(os.environ["RIFT_INSTALL"])

    @property
    def args(self):
        return "-m ethsim --ip-list=\"{}\"".format(self.ip_address)


class LaunchpadConfigurer(object):
    NETCONF_PORT=2022
    NETCONF_USER="admin"
    NETCONF_PW="admin"

    def __init__(self, log, loop, launchpad, vm_pool_mgr, network_pool_mgr):
        self._log = log
        self._loop = loop
        self._launchpad = launchpad
        self._vm_pool_mgr = vm_pool_mgr
        self._network_pool_mgr = network_pool_mgr

        self._manager = None

        self._model = RwYang.Model.create_libncx()
        self._model.load_schema_ypbc(RwCloudYang.get_schema())
        self._model.load_schema_ypbc(RwBaseYang.get_schema())
        self._model.load_schema_ypbc(RwResourceMgrYang.get_schema())
        self._model.load_schema_ypbc(RwNsmYang.get_schema())
        self._model.load_schema_ypbc(RwConmanYang.get_schema())
        self._model.load_schema_ypbc(RwLaunchpadYang.get_schema())
        self._cloud_account = None

    @staticmethod
    def wrap_netconf_config_xml(xml):
        xml = '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">{}</config>'.format(xml)
        return xml

    @asyncio.coroutine
    def _connect(self, timeout_secs=240):

        start_time = time.time()
        while (time.time() - start_time) < timeout_secs:

            try:
                self._log.debug("Attemping Launchpad netconf connection.")

                manager = yield from ncclient.asyncio_manager.asyncio_connect(
                        loop=self._loop,
                        host=self._launchpad.ip_address,
                        port=LaunchpadConfigurer.NETCONF_PORT,
                        username=LaunchpadConfigurer.NETCONF_USER,
                        password=LaunchpadConfigurer.NETCONF_PW,
                        allow_agent=False,
                        look_for_keys=False,
                        hostkey_verify=False,
                        )

                return manager

            except ncclient.transport.errors.SSHError as e:
                self._log.warning("Netconf connection to launchpad %s failed: %s",
                                  self._launchpad, str(e))

            yield from asyncio.sleep(5, loop=self._loop)

        raise LaunchpadConfigError("Failed to connect to Launchpad within %s seconds" %
                                   timeout_secs)

    @asyncio.coroutine
    def _configure_launchpad_mode(self):
        """ configure launchpad mode """
        cfg = RwLaunchpadYang.YangData_RwLaunchpad_LaunchpadConfig.from_dict({'operational_mode': 'MC_MANAGED'})
        xml = cfg.to_xml_v2(self._model)
        netconf_xml = self.wrap_netconf_config_xml(xml)

        self._log.debug("Sending launchpad mode config xml to %s: %s",
                        netconf_xml, self._launchpad.ip_address)

        response = yield from self._manager.edit_config(target="running",
                                                        config=netconf_xml,)

        self._log.debug("Received edit config response: %s", str(response))

    @asyncio.coroutine
    def _configure_service_orchestrator(self):
        @asyncio.coroutine
        def configure_service_orchestrator_endpoint():
            """ Configure Service Orchestrator Information to NSM Tasklet"""
            cfg = RwNsmYang.SoEndpoint.from_dict({'cm_ip_address': '127.0.0.1',
                                                  'cm_port'      :  2022,
                                                  'cm_username'  : 'admin',
                                                  'cm_password'  : 'admin'})

            xml = cfg.to_xml_v2(self._model)
            netconf_xml = self.wrap_netconf_config_xml(xml)

            self._log.debug("Sending cm-endpoint config xml to %s: %s",
                            netconf_xml, self._launchpad.ip_address)

            response = yield from self._manager.edit_config(target="running",
                                                            config=netconf_xml,)
            self._log.debug("Received edit config response: %s", str(response))

        @asyncio.coroutine
        def configure_resource_orchestrator_endpoint():
            """ Configure Resource Orchestrator Information to SO Tasklet"""
            cfg = RwConmanYang.RoEndpoint.from_dict({'ro_ip_address': '127.0.0.1',
                                                    'ro_port'      :  2022,
                                                    'ro_username'  : 'admin',
                                                    'ro_password'  : 'admin'})
            xml = cfg.to_xml_v2(self._model)
            netconf_xml = self.wrap_netconf_config_xml(xml)

            self._log.debug("Sending ro-endpoint config xml to %s: %s",
                            netconf_xml, self._launchpad.ip_address)

            response = yield from self._manager.edit_config(target="running",
                                                            config=netconf_xml,)
            self._log.debug("Received edit config response: %s", str(response))

        yield from configure_service_orchestrator_endpoint()
        yield from configure_resource_orchestrator_endpoint()


    @asyncio.coroutine
    def _configure_cloud_account(self, cloud_account):
        self._log.debug("Configuring launchpad %s cloud account: %s",
                        self._launchpad, cloud_account)

        cloud_account_cfg = RwCloudYang.CloudAccount.from_dict(
                cloud_account.account.as_dict()
                )

        xml = cloud_account_cfg.to_xml_v2(self._model)
        netconf_xml = self.wrap_netconf_config_xml(xml)

        self._log.debug("Sending configure cloud account xml to %s: %s",
                        netconf_xml, self._launchpad.ip_address)

        response = yield from self._manager.edit_config(
                target="running",
                config=netconf_xml,
                )

        self._log.debug("Received edit config response: %s", str(response))

    @asyncio.coroutine
    def _wait_until_system_ready(self, timeout_secs=60):
        self._log.debug("Waiting for all tasklets in launchpad %s to be ready", self._launchpad)

        start_time = time.time()
        while (time.time() - start_time) < timeout_secs:
            yield from asyncio.sleep(1, loop=self._loop)

            if self._manager is None:
                self._log.info("Reconnecting to launchpad")
                self._manager = yield from self._connect()

            try:
                response = yield from self._manager.get(('xpath', '/vcs/info'))
            except (ncclient.NCClientError, ncclient.operations.errors.TimeoutExpiredError) as e:
                self._log.error("Caught error when requesting tasklet info: %s", str(e))
                self._manager = None
                continue

            try:
                response_xml = response.data_xml.decode()
            except Exception as e:
                self._log.error("ncclient_manager failed to decode xml: %s", str(e))
                self._log.error("raw ncclient response: %s", response.xml)
                continue

            response_xml = response_xml[response_xml.index('<data'):]

            vcs_info = RwBaseYang.VcsInfo()
            try:
                vcs_info.from_xml_v2(self._model, response_xml)
            except Exception as e:
                self._log.error("Could not convert tasklet info XML response: {}".format(str(e)))
                continue

            components = vcs_info.components.component_info
            pending_components = [component for component in components if component.state not in ["RUNNING"]]
            if not pending_components:
                self._log.info("All components have entered RUNNING state")
                break

            self._log.info("All components have NOT YET entered RUNNING state")

        vcs_info = RwBaseYang.VcsInfo()

    @asyncio.coroutine
    def configure(self, cloud_account):
        self._log.debug("Beginning configuration of launchpad: %s", self._launchpad)

        # Sleep for a bit to prevent making too many unnecessary calls to confd
        # while the launchpad is coming up.
        yield from asyncio.sleep(30, loop=self._loop)

        self._manager = yield from self._connect()
        yield from self._wait_until_system_ready()
        yield from self._configure_launchpad_mode()
        yield from self._configure_cloud_account(cloud_account)
        yield from self._configure_service_orchestrator()

@asyncio.coroutine
def get_previous_lp(log, loop):
    log.debug("Finding previous running Launchpad minions")
    try:
        lp_minions = yield from salt.find_running_minions(log, loop)
        return lp_minions
    except salt.SaltCommandFailed as e:
        # Salt minion command failing is not a reliable indication that the process
        # actually died.
        log.warning("Ignoring find salt minion, error: %s", str(e))
        return {}


class SaltJob(object):
    def __init__(self, log, loop, node, command):
        self._log = log
        self._loop = loop
        self._node = node
        self._command = command

        self._proc = salt.SaltAsyncCommand(
                self._log,
                self._loop,
                self._node,
                None
                )
        self._callbacks = []

    @property
    def command(self):
        return "exec {} {}".format(
                    os.path.join(
                        os.environ['RIFT_ROOT'],
                        "scripts/env/ppid_monitor.sh"
                        ),
                    self._command
                    )

    @asyncio.coroutine
    def _job_monitor(self):
        self._log.debug("Starting salt job monitor")

        # Wait a couple seconds to give salt a chance to start the job
        yield from asyncio.sleep(2, loop=self._loop)

        while (yield from self.is_running()):
            yield from asyncio.sleep(1, loop=self._loop)

        self._log.debug("SaltJob %s no longer running")

        # Call back any callbacks interested in this job
        for cb in self._callbacks:
            self._loop.call_soon(cb, self)

    @asyncio.coroutine
    def start(self, lp_job_id):
        self._log.debug("Starting Launchpad salt job using command: %s", self.command)

        if lp_job_id is not None:
            # If a valid LP job_id is passed, simply assign this as the job ID
            # for the SaltAsyncCommand, we do not need to create a new salt job
            self._proc._set_job_id(lp_job_id)
        else:
            # If LP job_id is None, then we start the SaltAsyncCommand by
            # setting the command to start salt with
            self._proc._set_command(self.command)
            yield from self._proc.start()

        self._loop.create_task(self._job_monitor())

    @asyncio.coroutine
    def is_running(self):
        try:
            return (yield from self._proc.is_running())
        except salt.SaltCommandFailed as e:
            self._log.warning("Could not get is_running state.  Assuming up: (Error: %s)", str(e))
            return True

    @asyncio.coroutine
    def stop(self):
        if self._proc is None:
            raise JobNotStarted("Cannot stop a non-started job")

        yield from self._proc.stop()

        timeout_secs = 5
        try:
            yield from asyncio.wait_for(self._proc.wait(), timeout_secs, loop=self._loop)
        except asyncio.TimeoutError:
            self._log.error("Timed out (%s seconds) waiting for Launchpad proc to stop. Killing.",
                            timeout_secs)
            yield from self._proc.kill()

    def add_callback(self, callback):
        if callback not in self._callbacks:
            self._callbacks.append(callback)


class LaunchpadSaltLifetimeManager(object):
    """ Start/Stop Launchpad using Salt """

    STATES=("pending", "stopping", "stopped", "started", "starting", "configuring", "crashed")

    def __init__(self, loop, log, launchpad):
        self._loop = loop
        self._log = log
        self._launchpad = launchpad

        self._state = "pending"
        self._state_details = "Waiting for Pools"

        self._job = None
        self._start_time = None

    def _set_state(self, new_state, state_details = None):
        assert new_state in LaunchpadSaltLifetimeManager.STATES
        self._log.debug("Setting Launchpad %s state to %s (Details: %s)",
                        self._launchpad, new_state, state_details
                        )
        self._state = new_state
        self._state_details = state_details

    @property
    def state(self):
        return self._state

    @property
    def state_details(self):
        return self._state_details

    @property
    def start_time(self):
        return self._start_time

    def uptime(self):
        if self.state not in ["started"]:
            return "Launchpad not running"

        if self.start_time is None:
            return "Launchpad start time not available!"

        uptime_secs = float(time.time() - self.start_time)
        return str(timedelta(seconds = uptime_secs))

    def on_job_stopped(self, job):
        if job is not self._job:
            self._log.error("Stopped job %s did not match known job %s", job, self._job)
        elif self.state in ["stopping", "stopped"]:
            self._set_state("stopped", "Received message to Stop Launchpad")
        else:
            self._log.error("Job %s stopped unexpectedly", job)
            self._set_state("crashed", "Launchpad job stopped unexpectedly")

    @asyncio.coroutine
    def start(self, lp_job_id):
        if self.state not in ["pending", "stopped", "crashed"]:
            raise LaunchpadStartError("Cannot start a Launchpad in %s state" % self.state)

        if self._launchpad.ip_address is None:
            raise LaunchpadStartError("Launchpad %s ip address is not set" % self._launchpad)

        launchpad_cmd_fmt = '{rift_root}/rift-shell -e -r -- {command} >{log_file} 2>&1'
        launchpad_cmd = launchpad_cmd_fmt.format(
                rift_root=os.environ['RIFT_ROOT'],
                command=" ".join([self._launchpad.exe_path, self._launchpad.args]),
                log_file="/var/log/launchpad_console.log"
                )

        self._set_state("starting", "Allocating resources to start Launchpad")
        self._job = SaltJob(
                self._log,
                self._loop,
                self._launchpad.node_id,
                launchpad_cmd
                )
        self._job.add_callback(self.on_job_stopped)

        yield from self._job.start(lp_job_id)

        # Even though we save the launchpad start time right now,
        # we mark the laucnhpad as 'started' only when laucnhpad
        # has finished configuring.
        self._start_time = time.time()

    @asyncio.coroutine
    def stop(self):
        if self.state not in ["started", "starting", "configuring"]:
            self._log.info("Launchpad not in 'started', 'starting' or 'configuring' state, aborting 'stop'")
            return

        self._set_state("stopping", "Releasing resources allocated for Launchpad")
        yield from self._job.stop()
        self._start_time = None
        self._set_state("stopped", "Received message to Stop Launchpad")

    def delete_confd_reload_dir(self):
        if self.state not in ["stopped"]:
            self._log.error("Launchpad is still running, cannot delete launchpad confd dir")
            return

        cmd = "{rift_root}/rift-shell -e -r -- {command}".format(
                rift_root=os.environ['RIFT_ROOT'],
                command=" ".join(["rwyangutil", "--rm-persist-mgmt-ws"])
                )
        try:
            stdout = salt.execute_salt_cmd(self._log, self._launchpad.node_id, cmd)
        except Exception as e:
            raise Exception("Failed to remove persisit confd dir: %s" %str(e))

