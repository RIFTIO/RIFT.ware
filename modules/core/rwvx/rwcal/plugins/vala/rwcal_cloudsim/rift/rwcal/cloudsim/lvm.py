
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import logging
import os
import re

from . import shell


logger = logging.getLogger(__name__)


class PhysicalVolume(
        collections.namedtuple(
            "PhysicalVolume", [
                "pv",
                "vg",
                "fmt",
                "attr",
                "psize",
                "pfree",
                ]
            )
        ):
    pass


class VolumeGroup(
        collections.namedtuple(
            "VolumeGroup", [
                "vg",
                "num_pv",
                "num_lv",
                "num_sn",
                "attr",
                "vsize",
                "vfree",
                ]
            )
        ):
    pass


class LoopbackVolumeGroup(object):
    def __init__(self, name):
        self._name = name

    def __repr__(self):
        return repr({
            "name": self.name,
            "filepath": self.filepath,
            "loopback": self.loopback,
            "exists": self.exists,
            "volume_group": self.volume_group,
            })

    @property
    def exists(self):
        return any(v.vg == self.name for v in volume_groups())

    @property
    def name(self):
        return self._name

    @property
    def filepath(self):
        return find_backing_file(self.name)

    @property
    def loopback(self):
        return find_loop_device(self.name)

    @property
    def volume_group(self):
        for vgroup in volume_groups():
            if vgroup.vg == self.name:
                return vgroup

    @property
    def physical_volume(self):
        for pvolume in physical_volumes():
            if pvolume.vg == self.name:
                return pvolume

    @property
    def size(self):
        return os.path.getsize(self.filepath)

    def extend_mbytes(self, num_mbytes):
        """ Extend the size of the Loopback volume group

        Arguments:
            num_bytes - Number of megabytes to extend by
        """

        # Extend the size of the backing store
        shell.command('truncate -c -s +{}M {}'.format(
            num_mbytes, self.filepath)
            )

        # Notify loopback driver of the resized backing store
        shell.command('losetup -c {}'.format(self.loopback))

        # Expand the physical volume to match new size
        shell.command('pvresize {}'.format(self.physical_volume.pv))


def find_loop_device(volume):
    pvolumes = physical_volumes()
    for pvolume in pvolumes:
        if pvolume.vg == volume:
            return pvolume.pv

    return None


def find_backing_file(volume):
    """
    /dev/loop0: [64513]:414503 (/lvm/rift.img)

    """
    loop = find_loop_device(volume)
    if loop is None:
        return None

    output = shell.command("losetup {}".format(loop))[0]
    return re.search('.*\(([^)]*)\).*', output).group(1)


def create(volume="rift", filepath="/lvm/rift.img"):
    """
    First, we create a loopback device using a file that we put in the file
    system where running this from. Second, we create an LVM volume group onto
    the loop device that was just created
    """
    pvolumes = physical_volumes()
    for pvolume in pvolumes:
        if pvolume.vg == volume:
            raise ValueError("VolumeGroup %s already exists" % volume)

    # Delete the existing backing file if it exists
    if os.path.exists(filepath):
        os.remove(filepath)

    # Create the file that will be used as the backing store
    if not os.path.exists(os.path.dirname(filepath)):
        os.makedirs(os.path.dirname(filepath))

    # Create a minimal file to hold any LVM physical volume metadata
    shell.command('truncate -s 50M {}'.format(filepath))

    # Acquire the next available loopback device
    loopback = shell.command('losetup -f --show {}'.format(filepath))[0]

    # Create a physical volume
    shell.command('pvcreate {}'.format(loopback))

    # Create a volume group
    shell.command('vgcreate {} {}'.format(volume, loopback))

    return LoopbackVolumeGroup(volume)


def get(volume="rift"):
    pvolumes = physical_volumes()
    for pvolume in pvolumes:
        if pvolume.vg == volume:
            return LoopbackVolumeGroup(pvolume.vg)


def destroy(volume="rift"):
    pvolumes = physical_volumes()
    for pvolume in pvolumes:
        if pvolume.vg == volume:
            break
    else:
        return

    # Cache the backing file path
    filepath = find_backing_file(volume)

    # Remove the volume group
    shell.command('vgremove -f {}'.format(pvolume.vg))

    # Remove the physical volume
    shell.command('pvremove -y {}'.format(pvolume.pv))

    # Release the loopback device
    shell.command('losetup -d {}'.format(pvolume.pv))

    # Remove the backing file
    os.remove(filepath)


def physical_volumes():
    """Returns a list of physical volumes"""
    cmd = 'pvs --separator "," --rows'
    lines = [line.strip().split(',') for line in shell.command(cmd)]
    if not lines:
        return []

    mapping = {
            "PV": "pv",
            "VG": "vg",
            "Fmt": "fmt",
            "Attr": "attr",
            "PSize": "psize",
            "PFree": "pfree",
            }

    # Transpose the data so that the first element of the list is a list of
    # keys.
    transpose = list(map(list, zip(*lines)))

    # Extract keys
    keys = transpose[0]

    # Iterate over the remaining data and create the physical volume objects
    volumes = []
    for values in transpose[1:]:
        volume = {}
        for k, v in zip(keys, values):
            volume[mapping[k]] = v

        volumes.append(PhysicalVolume(**volume))

    return volumes


def volume_groups():
    """Returns a list of volume groups"""
    cmd = 'vgs --separator "," --rows'
    lines = [line.strip().split(',') for line in shell.command(cmd)]
    if not lines:
        return []

    mapping = {
            "VG": "vg",
            "#PV": "num_pv",
            "#LV": "num_lv",
            "#SN": "num_sn",
            "Attr": "attr",
            "VSize": "vsize",
            "VFree": "vfree",
            }

    # Transpose the data so that the first element of the list is a list of
    # keys.
    transpose = list(map(list, zip(*lines)))

    # Extract keys
    keys = transpose[0]

    # Iterate over the remaining data and create the volume groups
    groups = []
    for values in transpose[1:]:
        group = {}
        for k, v in zip(keys, values):
            group[mapping[k]] = v

        groups.append(VolumeGroup(**group))

    return groups

