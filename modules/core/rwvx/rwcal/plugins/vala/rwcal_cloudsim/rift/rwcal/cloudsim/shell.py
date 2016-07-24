
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import logging
import subprocess


logger = logging.getLogger(__name__)


class ProcessError(Exception):
    pass


def command(cmd):
    logger.debug('executing: {}'.format(cmd))

    process = subprocess.Popen(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            )

    stdout, stderr = process.communicate()
    process.wait()

    if process.returncode != 0:
        raise ProcessError(stderr.decode())

    return stdout.decode().splitlines()

