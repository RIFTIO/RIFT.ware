
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import argparse
import asyncio
import json
import logging
import shlex
import subprocess

from . import util


class SaltCommandFailed(util.CommandFailed):
    pass


class SaltCommandNotStarted(Exception):
    pass


class MinionConnectionNotFound(Exception):
    pass


def execute_salt_cmd(log, target, cmd):
    saltcmd = "salt {target} cmd.run '{cmd}' --out txt".format(
            target=target,
            cmd=cmd
            )
    log.info("Executing command: %s", saltcmd)

    try:
        stdout = subprocess.check_output(
                shlex.split(saltcmd),
                universal_newlines=True,
                )
    except subprocess.CalledProcessError as e:
        log.error("Failed to execute subprocess command %s (exception %s)", cmd, str(e))
        raise

    return stdout

def get_launchpad_hostname(log, node_id):
    '''
    Find the hostname for launchpad VM
    '''
    cmd = "hostnamectl --static"

    try:
        stdout = execute_salt_cmd(log, node_id, cmd)
    except Exception as e:
        log.error("Failed to get Launchpad hostname (exception %s)", str(e))

    for line in stdout.split("\n"):
        (nodeid, hostname) = line.split(": ")
        if nodeid is None:
            raise SaltCommandFailed("Salt did not return proper node id (expected: %s) (received: %s)",
                    node_id, stdout)

        log.info("command (%s) returned result (%s) (id: %s)", cmd, hostname, nodeid)
        return hostname

    raise SaltCommandFailed("Salt command did not return any output")

@asyncio.coroutine
def is_node_connected(log, loop, node_id):
    try:
        stdout, _ = yield from util.run_command(
                loop, 'salt %s test.ping' % node_id
                )
    except subprocess.CalledProcessError:
        log.warning("test.ping command failed against node_id: %s", node_id)
        return True

    up_minions = stdout.splitlines()
    for line in up_minions:
        if "True" in line:
            return True

    return False


@asyncio.coroutine
def find_job(log, loop, node_id, job_id):
    cmd = "salt -t 60 {node_id} saltutil.find_job {job_id} --output json".format(
            node_id=node_id, job_id=job_id)

    try:
        output, _ = yield from util.run_command(loop, cmd)
    except util.CommandFailed as e:
        raise SaltCommandFailed("Salt command failed: %s" % str(e))

    if not output:
        raise SaltCommandFailed("Empty response from command: %s" % cmd)

    try:
        resp = json.loads(output)
    except ValueError:
        raise SaltCommandFailed("Failed to parse find_job output: %s" % output)

    if node_id not in resp:
        raise SaltCommandFailed("Expected %s in find_job response" % node_id)

    if "jid" in resp[node_id]:
        return resp[node_id]

    return None

@asyncio.coroutine
def find_running_minions(log, loop):
    '''
    Queries Salt for running jobs, and creates a dict with node_id & job_id
    Returns the node_id and job_id
    '''
    cmd = "salt -t 60 '*' saltutil.running --output json --out-indent -1"

    try:
        output, _ = yield from util.run_command(loop, cmd)
    except util.CommandFailed as e:
        raise SaltCommandFailed("Salt command failed: %s" % str(e))

    if not output:
        raise SaltCommandFailed("Empty response from command: %s" % cmd)

    minions = {}
    for line in output.split("\n"):
        # Interested in only those minions which have a "tgt" attribute in the result,
        # as this points to a running target id minion
        if "tgt" in line:
            try:
                resp = json.loads(line)
            except ValueError:
                raise SaltCommandFailed("Failed to parse find_minion output: %s" % output)

            # Get the job id ('jid') from the minion response and populate the dict,
            # using node_id as key and job_id as value.
            for key in resp:
                minions[key] = resp[key][0]['jid']

    log.info("Salt minions found: %s", minions)
    return minions

class SaltAsyncCommand(object):
    def __init__(self, log, loop, target, command):
        self._log = log
        self._loop = loop
        self._target = target
        self._command = command

        self._job_id = None

    def _set_command(self, command):
        self._command = command

    def _set_job_id(self, job_id):
        self._job_id = job_id

    @asyncio.coroutine
    def start(self):
        cmd = "salt --async {target} cmd.run '{cmd}'".format(
                target=self._target,
                cmd=self._command,
                )

        stdout, stderr = yield from util.run_command(self._loop, cmd)

        for line in stdout.split("\n"):
            if "job ID:" in line:
                job_id = line.split(" ")[-1]
                if job_id == "0":
                    raise SaltCommandFailed("Did not create a job id for async command: %s", stdout)

                self._job_id = job_id

                self._log.debug("Salt command (%s) started on node (%s) (jid: %s)",
                                cmd, self._target, self._job_id)
                return

        raise SaltCommandFailed("Did not find async job id in output")

    @asyncio.coroutine
    def is_running(self):
        if not self._job_id:
            raise SaltCommandNotStarted()

        @asyncio.coroutine
        def job_exists():
            try:
                job = yield from find_job(self._log, self._loop, self._target, self._job_id)
            except SaltCommandFailed as e:
                # Salt minion command failing is not a reliable indication that the process
                # actually died.
                self._log.warning("Ignoring find salt job %s error: %s", self._job_id, str(e))
                return True

            return job is not None

        for _ in range(3):
            if (yield from job_exists()):
                return True

        return False

    @asyncio.coroutine
    def wait(self):
        while True:
            is_running = yield from self.is_running()
            if not is_running:
                return

            asyncio.sleep(.25)

    @asyncio.coroutine
    def stop(self):
        if not self._job_id:
            raise SaltCommandNotStarted()

        cmd = "salt {target} saltutil.term_job {job_id}".format(
                target=self._target,
                job_id=self._job_id,
                )

        yield from util.run_command(self._loop, cmd)

    @asyncio.coroutine
    def kill(self):
        if not self._job_id:
            raise SaltCommandNotStarted()

        cmd = "salt {target} saltutil.kill_job {job_id}".format(
                target=self._target,
                job_id=self._job_id,
                )

        yield from util.run_command(self._loop, cmd)


@asyncio.coroutine
def test_salt(loop, node):
    logger.debug("Checking if node is connected")
    assert (yield from is_node_connected(logger, loop, node))

    logger.debug("Running sleep 10 command")
    async_cmd = SaltAsyncCommand(logger, loop, node, "sleep 10")
    yield from async_cmd.start()

    logger.debug("Check if sleep command is running")
    is_running = yield from async_cmd.is_running()
    assert is_running

    logger.debug("Stop the sleep command")
    yield from async_cmd.stop()

    logger.debug("Check if sleep command is no longer running")
    is_running = yield from async_cmd.is_running()
    assert not is_running


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    logger = logging.getLogger("salt-test")
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--node", required=True, help="A connected minion")
    args = parser.parse_args()

    loop = asyncio.get_event_loop()
    loop.run_until_complete(test_salt(loop, args.node))


