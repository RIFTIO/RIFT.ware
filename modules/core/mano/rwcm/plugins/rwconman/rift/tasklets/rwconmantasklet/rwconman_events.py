
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import ncclient
import ncclient.asyncio_manager
import tornado.httpclient as tornadoh
import asyncio.subprocess
import asyncio
import time
import sys
import os, stat

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwYang', '1.0')
gi.require_version('RwConmanYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
gi.require_version('RwVnfrYang', '1.0')

from gi.repository import (
    RwDts as rwdts,
    RwYang,
    RwConmanYang as conmanY,
    RwNsrYang as nsrY,
    RwVnfrYang as vnfrY,
)

import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async

def log_this_vnf(vnf_cfg):
    log_vnf = ""
    used_item_list = ['nsr_name', 'vnfr_name', 'member_vnf_index', 'mgmt_ip_address']
    for item in used_item_list:
        if item in vnf_cfg:
            if item == 'mgmt_ip_address':
                log_vnf += "({})".format(vnf_cfg[item])
            else:
                log_vnf += "{}/".format(vnf_cfg[item])
    return log_vnf
        
class ConfigManagerROifConnectionError(Exception):
    pass
class ScriptError(Exception):
    pass


class ConfigManagerEvents(object):
    def __init__(self, dts, log, loop, parent):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._parent = parent
        self._nsr_xpath = "/cm-state/cm-nsr"

    @asyncio.coroutine
    def register(self):
        pass

    @asyncio.coroutine
    def update_vnf_state(self, vnf_cfg, state):
        nsr_obj = vnf_cfg['nsr_obj']
        yield from nsr_obj.update_vnf_cm_state(vnf_cfg['vnfr'], state)
        
    @asyncio.coroutine
    def apply_vnf_config(self, vnf_cfg):
        self._log.debug("apply_vnf_config VNF:{}"
                        .format(log_this_vnf(vnf_cfg)))
        
        if vnf_cfg['config_delay']:
            yield from self.update_vnf_state(vnf_cfg, conmanY.RecordState.CFG_DELAY)
            yield from asyncio.sleep(vnf_cfg['config_delay'], loop=self._loop)
            
        # See if we are still alive!
        if vnf_cfg['nsr_obj'].being_deleted:
            # Don't do anything, just return
            self._log.info("VNF : %s is being deleted, skipping configuration!",
                           log_this_vnf(vnf_cfg))
            return True
            
        yield from self.update_vnf_state(vnf_cfg, conmanY.RecordState.CFG_SEND)
        try:
            if vnf_cfg['config_method'] == 'netconf':
                self._log.info("Creating ncc handle for VNF cfg = %s!", vnf_cfg)
                self.ncc = ConfigManagerVNFnetconf(self._log, self._loop, self, vnf_cfg)
                if vnf_cfg['protocol'] == 'ssh':
                    yield from self.ncc.connect_ssh()
                else:
                    yield from self.ncc.connect()
                yield from self.ncc.apply_edit_cfg()
            elif vnf_cfg['config_method'] == 'rest':
                if self.rcc is None:
                    self._log.info("Creating rcc handle for VNF cfg = %s!", vnf_cfg)
                    self.rcc = ConfigManagerVNFrestconf(self._log, self._loop, self, vnf_cfg)
                self.ncc.apply_edit_cfg()
            elif vnf_cfg['config_method'] == 'script':
                self._log.info("Executing script for VNF cfg = %s!", vnf_cfg)
                scriptc = ConfigManagerVNFscriptconf(self._log, self._loop, self, vnf_cfg)
                yield from scriptc.apply_edit_cfg()
            elif vnf_cfg['config_method'] == 'juju':
                self._log.info("Executing juju config for VNF cfg = %s!", vnf_cfg)
                jujuc = ConfigManagerVNFjujuconf(self._log, self._loop, self._parent, vnf_cfg)
                yield from jujuc.apply_edit_cfg()
            else:
                self._log.error("Unknown configuration method(%s) received for %s",
                                vnf_cfg['config_method'], vnf_cfg['vnf_unique_name'])
                yield from self.update_vnf_state(vnf_cfg, conmanY.RecordState.CFG_FAILED)
                return True

            #Update VNF state
            yield from self.update_vnf_state(vnf_cfg, conmanY.RecordState.READY)
            self._log.info("Successfully applied configuration to VNF: %s",
                               log_this_vnf(vnf_cfg))
        except Exception as e:
            self._log.error("Applying configuration(%s) file(%s) to VNF: %s failed as: %s",
                            vnf_cfg['config_method'],
                            vnf_cfg['cfg_file'],
                            log_this_vnf(vnf_cfg),
                            str(e))
            #raise
            return False

        return True
        
class ConfigManagerVNFscriptconf(object):

    def __init__(self, log, loop, parent, vnf_cfg):
        self._log = log
        self._loop = loop
        self._parent = parent
        self._manager = None
        self._vnf_cfg = vnf_cfg

    #@asyncio.coroutine
    def apply_edit_cfg(self):
        vnf_cfg = self._vnf_cfg
        self._log.debug("Attempting to apply scriptconf to VNF: %s", log_this_vnf(vnf_cfg))
        try:
            st = os.stat(vnf_cfg['cfg_file'])
            os.chmod(vnf_cfg['cfg_file'], st.st_mode | stat.S_IEXEC)
            #script_msg = subprocess.check_output(vnf_cfg['cfg_file'], shell=True).decode('utf-8')

            proc = yield from asyncio.create_subprocess_exec(
                vnf_cfg['script_type'], vnf_cfg['cfg_file'],
                stdout=asyncio.subprocess.PIPE)
            script_msg = yield from proc.stdout.read()
            rc = yield from proc.wait()

            if rc != 0:
                raise ScriptError(
                    "script config returned error code : %s" % rc
                    )

            self._log.debug("config script output (%s)", script_msg)
        except Exception as e:
            self._log.error("Error (%s) while executing script config for VNF: %s",
                            str(e), log_this_vnf(vnf_cfg))
            raise

class ConfigManagerVNFrestconf(object):

    def __init__(self, log, loop, parent, vnf_cfg):
        self._log = log
        self._loop = loop
        self._parent = parent
        self._manager = None
        self._vnf_cfg = vnf_cfg

    def fetch_handle(self, response):
        if response.error:
            self._log.error("Failed to send HTTP config request - %s", response.error)
        else:
            self._log.debug("Sent HTTP config request - %s", response.body)

    @asyncio.coroutine
    def apply_edit_cfg(self):
        vnf_cfg = self._vnf_cfg
        self._log.debug("Attempting to apply restconf to VNF: %s", log_this_vnf(vnf_cfg))
        try:
            http_c = tornadoh.AsyncHTTPClient()
            # TBD
            # Read the config entity from file?
            # Convert connectoin-point?
            http_c.fetch("http://", self.fetch_handle)
        except Exception as e:
            self._log.error("Error (%s) while applying HTTP config", str(e))

class ConfigManagerVNFnetconf(object):

    def __init__(self, log, loop, parent, vnf_cfg):
        self._log = log
        self._loop = loop
        self._parent = parent
        self._manager = None
        self._vnf_cfg = vnf_cfg

        self._model = RwYang.Model.create_libncx()
        self._model.load_schema_ypbc(conmanY.get_schema())

    @asyncio.coroutine
    def connect(self, timeout_secs=120):
        vnf_cfg = self._vnf_cfg
        start_time = time.time()
        self._log.debug("connecting netconf .... %s", vnf_cfg)
        while (time.time() - start_time) < timeout_secs:

            try:
                self._log.info("Attemping netconf connection to VNF: %s", log_this_vnf(vnf_cfg))

                self._manager = yield from ncclient.asyncio_manager.asyncio_connect(
                    loop=self._loop,
                    host=vnf_cfg['mgmt_ip_address'],
                    port=vnf_cfg['port'],
                    username=vnf_cfg['username'],
                    password=vnf_cfg['password'],
                    allow_agent=False,
                    look_for_keys=False,
                    hostkey_verify=False,
                )

                self._log.info("Netconf connected to VNF: %s", log_this_vnf(vnf_cfg))
                return

            except ncclient.transport.errors.SSHError as e:
                yield from self._parent.update_vnf_state(vnf_cfg, conmanY.RecordState.FAILED_CONNECTION)
                self._log.error("Netconf connection to VNF: %s, failed: %s",
                                log_this_vnf(vnf_cfg), str(e))

            yield from asyncio.sleep(2, loop=self._loop)

        raise ConfigManagerROifConnectionError(
            "Failed to connect to VNF: %s within %s seconds" %
            (log_this_vnf(vnf_cfg), timeout_secs)
        )

    @asyncio.coroutine
    def connect_ssh(self, timeout_secs=120):
        vnf_cfg = self._vnf_cfg
        start_time = time.time()

        if (self._manager != None and self._manager.connected == True):
            self._log.debug("Disconnecting previous session")
            self._manager.close_session

        self._log.debug("connecting netconf via SSH .... %s", vnf_cfg)
        while (time.time() - start_time) < timeout_secs:

            try:
                yield from self._parent.update_vnf_state(vnf_cfg, conmanY.RecordState.CONNECTING)
                self._log.debug("Attemping netconf connection to VNF: %s", log_this_vnf(vnf_cfg))

                self._manager = ncclient.asyncio_manager.manager.connect_ssh(
                    host=vnf_cfg['mgmt_ip_address'],
                    port=vnf_cfg['port'],
                    username=vnf_cfg['username'],
                    password=vnf_cfg['password'],
                    allow_agent=False,
                    look_for_keys=False,
                    hostkey_verify=False,
                )

                yield from self._parent.update_vnf_state(vnf_cfg, conmanY.RecordState.NETCONF_SSH_CONNECTED)
                self._log.debug("netconf over SSH connected to VNF: %s", log_this_vnf(vnf_cfg))
                return

            except ncclient.transport.errors.SSHError as e:
                yield from self._parent.update_vnf_state(vnf_cfg, conmanY.RecordState.FAILED_CONNECTION)
                self._log.error("Netconf connection to VNF: %s, failed: %s",
                                log_this_vnf(vnf_cfg), str(e))

            yield from asyncio.sleep(2, loop=self._loop)

        raise ConfigManagerROifConnectionError(
            "Failed to connect to VNF: %s within %s seconds" %
            (log_this_vnf(vnf_cfg), timeout_secs)
        )

    @asyncio.coroutine
    def apply_edit_cfg(self):
        vnf_cfg = self._vnf_cfg
        self._log.debug("Attempting to apply netconf to VNF: %s", log_this_vnf(vnf_cfg))

        if self._manager is None:
            self._log.error("Netconf is not connected to VNF: %s, aborting!", log_this_vnf(vnf_cfg))
            return

        # Get config file contents
        try:
            with open(vnf_cfg['cfg_file']) as f:
                configuration = f.read()
        except Exception as e:
            self._log.error("Reading contents of the configuration file(%s) failed: %s", vnf_cfg['cfg_file'], str(e))
            return

        try:
            self._log.debug("apply_edit_cfg to VNF: %s", log_this_vnf(vnf_cfg))
            xml = '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">{}</config>'.format(configuration)
            response = yield from self._manager.edit_config(xml, target='running')
            if hasattr(response, 'xml'):
                response_xml = response.xml
            else:
                response_xml = response.data_xml.decode()

            self._log.debug("apply_edit_cfg response: %s", response_xml)
            if '<rpc-error>' in response_xml:
                raise ConfigManagerROifConnectionError("apply_edit_cfg response has rpc-error : %s",
                                                       response_xml)

            self._log.debug("apply_edit_cfg Successfully applied configuration {%s}", xml)
        except:
            raise

class ConfigManagerVNFjujuconf(object):

    def __init__(self, log, loop, parent, vnf_cfg):
        self._log = log
        self._loop = loop
        self._parent = parent
        self._manager = None
        self._vnf_cfg = vnf_cfg

    #@asyncio.coroutine
    def apply_edit_cfg(self):
        vnf_cfg = self._vnf_cfg
        self._log.debug("Attempting to apply juju conf to VNF: %s", log_this_vnf(vnf_cfg))
        try:
            args = ['python3',
                vnf_cfg['juju_script'],
                '--server', vnf_cfg['mgmt_ip_address'],
                '--user',  vnf_cfg['user'],
                '--password', vnf_cfg['secret'],
                '--port', str(vnf_cfg['port']),
                vnf_cfg['cfg_file']]
            self._log.error("juju script command (%s)", args)

            proc = yield from asyncio.create_subprocess_exec(
                *args,
                stdout=asyncio.subprocess.PIPE)
            juju_msg = yield from proc.stdout.read()
            rc = yield from proc.wait()

            if rc != 0:
                raise ScriptError(
                    "Juju config returned error code : %s" % rc
                    )

            self._log.debug("Juju config output (%s)", juju_msg)
        except Exception as e:
            self._log.error("Error (%s) while executing juju config", str(e))
            raise
