
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import subprocess


class CommandFailed(Exception):
    pass


@asyncio.coroutine
def run_command(loop, cmd):
    cmd_proc = yield from asyncio.create_subprocess_shell(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, loop=loop
            )
    stdout, stderr = yield from cmd_proc.communicate()
    if cmd_proc.returncode != 0:
        raise CommandFailed("Starting async command (%s) failed (rc=%s). (stderr: %s)",
                            cmd, cmd_proc.returncode, stderr)

    return stdout.decode(), stderr.decode()

