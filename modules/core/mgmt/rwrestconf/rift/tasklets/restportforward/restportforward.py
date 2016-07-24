# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 8/29/2015
# 

import asyncio
import logging
import gi

from subprocess import (
    Popen,
    PIPE,
    DEVNULL,
)
import sys

import tornado

gi.require_version('RwRestportforwardYang', '1.0')
from gi.repository import (
    RwDts,
    RwRestportforwardYang,
    RwYang,
)

import rift.tasklets


if sys.version_info < (3,4,4):
    asyncio.ensure_future = asyncio.async

def _execute_shell_command(command):
    with Popen(command, shell=True, stdout=PIPE, stderr=PIPE, universal_newlines=True) as proc:
        output = proc.stdout.read()
        error = proc.stderr.read()

    if len(error) == 0:
        return output.splitlines()
    else:
        if "Index of deletion too big." in error:
            return
        else:
            raise ValueError(error)

def _remove_forwarding_rules():
    def remove_rule(chain, rule_num):
        command = "sudo iptables -t nat -D %s %s" % (chain, rule_num)
        _execute_shell_command(command)

    def list_rule_lines(chain):
        command = "sudo iptables -t nat -L %s --line-numbers | grep ^[0-9]" % chain
        lines = _execute_shell_command(command)
        return lines

    def remove_tcp_dport_rule_from_chain(chain, dport):
        lines = list_rule_lines(chain)
        for rule_num, line in enumerate(lines, 1):
            if "tcp dpt:%s" % dport in line:
                remove_rule(chain, rule_num)

    remove_tcp_dport_rule_from_chain("PREROUTING", "8008")
    remove_tcp_dport_rule_from_chain("OUTPUT", "8008")

def _set_port_forward(new_port_number):
    eth0_command = "sudo iptables -t nat -I PREROUTING -i eth0 -p tcp --dport 8008 -j REDIRECT --to-port %d" % new_port_number
    lo_command = "sudo iptables -t nat -I OUTPUT -o lo -p tcp --dport 8008 -j REDIRECT --to-port %d" % new_port_number

    _execute_shell_command(eth0_command)
    _execute_shell_command(lo_command)    


class RestPortForwardTasklet(rift.tasklets.Tasklet):

    def start(self):
        """Tasklet entry point"""
        super(RestPortForwardTasklet, self).start()

        self._manifest = self.tasklet_info.get_pb_manifest()

        self._dts = rift.tasklets.DTS(
            self.tasklet_info,
            RwRestportforwardYang.get_schema(),
            self.loop,
            self.on_dts_state_change)

        # set default to rwrestconf
        try:
            _remove_forwarding_rules()
        except ValueError as e:
            self.log.error(e)
        _set_port_forward(8888)

    def stop(self):
      try:
         self._dts.deinit()
      except Exception:
         print("Caught Exception in LP stop:", sys.exc_info()[0])
         raise

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application in to the corresponding application state
        """
        switch = {
            RwDts.State.INIT: RwDts.State.RUN,
        }

        handlers = {
            RwDts.State.INIT: self.init,
            RwDts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers[state]
        yield from handler()

        try:
            # Transition dts to next state
            next_state = switch[state]
        except KeyError:
            # we don't handle all state changes
            return

        self._dts.handle.set_state(next_state)

    @asyncio.coroutine
    def init(self):
        self._messages = {}

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ksp, msg, scratch):
            self._messages[xact.id] = msg
            acg.handle.prepare_complete_ok(xact_info.handle)

        def on_apply(dts, acg, xact, action, scratch):
            if action == RwDts.AppconfAction.INSTALL and xact.id is None:
                return

            if xact.id not in self._messages:
                raise KeyError("No stashed configuration found with transaction id [{}]".format(xact.id))

            toggle = self._messages[xact.id]
            del self._messages[xact.id]

            default_server = toggle.primary_rest_server

            if default_server == "confd":
                _set_port_forward(8889)
            else:
                _set_port_forward(8888)

            
        with self._dts.appconf_group_create(
                handler=rift.tasklets.AppConfGroup.Handler(
                    on_apply=on_apply)) as acg:
            acg.register(
                xpath="C,/rw-restportforward:rwrestportforward-configuration",
                flags=RwDts.Flag.SUBSCRIBER,
                on_prepare=on_prepare)


    @asyncio.coroutine
    def run(self):
        pass



