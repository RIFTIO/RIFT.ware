
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import math
import re

from . import shell


class ImageInfoError(Exception):
    pass


def qcow2_virtual_size_mbytes(qcow2_filepath):
    info_output = shell.command("qemu-img info {}".format(qcow2_filepath))
    for line in info_output:
        if line.startswith("virtual size"):
            match = re.search("\(([0-9]*) bytes\)", line)
            if match is None:
                raise ImageInfoError("Could not parse image size")

            num_bytes = int(match.group(1))
            num_mbytes = num_bytes / 1024 / 1024
            return math.ceil(num_mbytes)

    raise ImageInfoError("Could not image virtual size field in output")
