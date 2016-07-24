#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import logging
import os

import rift.rwcal.cloudsim.lxc as lxc
import rift.rwcal.cloudsim.lvm as lvm


logger = logging.getLogger('rwcal-test')


def main():
    template = os.path.realpath("../rift/cal/lxc-fedora-rift.lxctemplate")
    tarfile = "/net/strange/localdisk/jdowner/lxc.tar.gz"
    volume = 'rift-test'

    lvm.create(volume, '/lvm/rift-test.img')

    master = lxc.create_container('test-master', template, volume, tarfile)

    snapshots = []
    for index in range(5):
        snapshots.append(master.snapshot('test-snap-{}'.format(index + 1)))

    for snapshot in snapshots:
        snapshot.destroy()

    master.destroy()

    lvm.destroy(volume)



if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
