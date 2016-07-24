
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import contextlib
import functools
import logging
import os
import shutil
import uuid

from . import shell
from . import image
from . import lvm


logger = logging.getLogger(__name__)


class ValidationError(Exception):
    pass


@contextlib.contextmanager
def mount(mountpoint, path):
    """Mounts a device and unmounts it upon exit"""
    shell.command('mount {} {}'.format(mountpoint, path))
    logger.debug('mount {} {}'.format(mountpoint, path))
    yield
    shell.command('umount {}'.format(path))
    logger.debug('umount {}'.format(path))


def create_container(name, template_path, volume, rootfs_qcow2file):
    """Create a new container

    Arguments:
        name          - the name of the new container
        template_path - the template defines the type of container to create
        volume        - the volume group that the container will be in
        roots_tarfile - a path to a tarfile that contains the rootfs

    Returns:
        A Container object for the new snapshot

    """
    cmd = 'lxc-create -t {} -n {} -B lvm --fssize {}M --vgname {}'
    cmd += " -- --rootfs-qcow2file {}".format(rootfs_qcow2file)
    cmd += " 2>&1 | tee -a /var/log/rift_lxc.log"
    virtual_size_mbytes = image.qcow2_virtual_size_mbytes(rootfs_qcow2file)

    loop_volume = lvm.get(volume)
    loop_volume.extend_mbytes(virtual_size_mbytes)

    shell.command(cmd.format(
        template_path, name, virtual_size_mbytes, volume
        ))

    return Container(name, volume=volume, size_mbytes=virtual_size_mbytes)


def create_snapshot(base, name, volume, size_mbytes):
    """Create a clone of an existing container

    Arguments:
        base     - the name of the existing container
        name     - the name to give to the clone
        volume   - the volume group that the container will be in

    Returns:
        A Container object for the new snapshot

    """
    cmd = '/bin/bash lxc-clone -o {} -n {} --vgname {} --snapshot'

    loop_volume = lvm.get(volume)
    loop_volume.extend_mbytes(size_mbytes)

    try:
        shell.command(cmd.format(base, name, volume))

    except shell.ProcessError as e:
        # Skip the error that occurs here. It is corrected during configuration
        # and results from a bug in the lxc script.

        # In lxc-clone, when cloning multiple times from the same container
        # it is possible that the lvrename operation fails to rename the
        # file in /dev/rift (but the logical volume is renamed).
        # This logic below resolves this particular scenario.
        if "lxc-clone: failed to mount new rootfs" in str(e):
            os.rmdir("/dev/rift/{name}".format(name=name))
            shutil.move("/dev/rift/{name}_snapshot".format(name=name),
                        "/dev/rift/{name}".format(name=name)
                        )

        elif "mkdir: cannot create directory" not in str(e):
            raise

    return Container(name, volume=volume, size_mbytes=size_mbytes)


def purge_cache():
    """Removes any cached templates"""
    shell.command('rm -rf /var/cache/lxc/*')


def containers():
    """Returns a list of containers"""
    return [c for c in shell.command('lxc-ls') if c]


def destroy(name):
    """Destroys a container

    Arguments:
        name - the name of the container to destroy

    """
    shell.command('lxc-destroy -n {}'.format(name))


def start(name):
    """Starts a container

    Arguments:
        name - the name of the container to start

    """
    shell.command('lxc-start -d -n {} -l DEBUG'.format(name))


def stop(name):
    """Stops a container

    Arguments
        name - the name of the container to start

    """
    shell.command('lxc-stop -n {}'.format(name))


def state(name):
    """Returns the current state of a container

    Arguments:
        name - the name of the container whose state is retuned

    Returns:
        A string describing the state of the container

    """
    _, state = shell.command('lxc-info -s -n {}'.format(name))[0].split()
    return state


def ls():
    """Prints the output from 'lxc-ls --fancy'"""
    print('\n'.join(shell.command('lxc-ls --fancy')))


def validate(f):
    """
    This decorator is used to check that a given container exists. If the
    container does not exist, a ValidationError is raised.

    """
    @functools.wraps(f)
    def impl(self, *args, **kwargs):
        if self.name not in containers():
            msg = 'container ({}) does not exist'.format(self.name)
            raise ValidationError(msg)

        return f(self, *args, **kwargs)

    return impl


class Container(object):
    """
    This class provides an interface to an existing container on the system.
    """

    def __init__(self, name, size_mbytes=4096, volume="rift", hostname=None):
        self._name = name
        self._size_mbytes = size_mbytes
        self._volume = volume
        self.hostname = name if hostname is None else hostname

    @property
    def name(self):
        """The name of the container"""
        return self._name

    @property
    def size(self):
        """The virtual size of the container"""
        return self._size_mbytes

    @property
    def volume(self):
        """The volume that the container is a part of"""
        return self._volume

    @property
    def loopback_volume(self):
        """ Instance of lvm.LoopbackVolumeGroup """
        return lvm.get(self.volume)

    @property
    @validate
    def state(self):
        """The current state of the container"""
        return state(self.name)

    @validate
    def start(self):
        """Starts the container"""
        start(self.name)

    @validate
    def stop(self):
        """Stops the container"""
        stop(self.name)

    @validate
    def destroy(self):
        """Destroys the container"""
        destroy(self.name)

    @validate
    def info(self):
        """Returns info about the container"""
        return shell.command('lxc-info -n {}'.format(self.name))

    @validate
    def snapshot(self, name):
        """Create a snapshot of this container

        Arguments:
            name - the name of the snapshot

        Returns:
            A Container representing the new snapshot

        """
        return create_snapshot(self.name, name, self.volume, self.size)

    @validate
    def configure(self, config, volume='rift', userdata=None):
        """Configures the container

        Arguments:
            config   - a container configuration object
            volume   - the volume group that the container will belong to
            userdata - a string containing userdata that will be passed to
                       cloud-init for execution

        """
        # Create the LXC config file
        with open("/var/lib/lxc/{}/config".format(self.name), "w") as fp:
            fp.write(str(config))
            logger.debug('created /var/lib/lxc/{}/config'.format(self.name))

        # Mount the rootfs of the container and configure the hosts and
        # hostname files of the container.
        rootfs = '/var/lib/lxc/{}/rootfs'.format(self.name)
        os.makedirs(rootfs, exist_ok=True)

        with mount('/dev/rift/{}'.format(self.name), rootfs):

            # Create /etc/hostname
            with open(os.path.join(rootfs, 'etc/hostname'), 'w') as fp:
                fp.write(self.hostname + '\n')
                logger.debug('created /etc/hostname')

            # Create /etc/hostnames
            with open(os.path.join(rootfs, 'etc/hosts'), 'w') as fp:
                fp.write("127.0.0.1 localhost {}\n".format(self.hostname))
                fp.write("::1 localhost {}\n".format(self.hostname))
                logger.debug('created /etc/hosts')

            # Disable autofs (conflicts with lxc workspace mount bind)
            autofs_service_file = os.path.join(
                    rootfs,
                    "etc/systemd/system/multi-user.target.wants/autofs.service",
                    )
            if os.path.exists(autofs_service_file):
                os.remove(autofs_service_file)

            # Setup the mount points
            for mount_point in config.mount_points:
                mount_point_path = os.path.join(rootfs, mount_point.remote)
                os.makedirs(mount_point_path, exist_ok=True)

            # Copy the cloud-init script into the nocloud seed directory
            if userdata is not None:
                try:
                    userdata_dst = os.path.join(rootfs, 'var/lib/cloud/seed/nocloud/user-data')
                    os.makedirs(os.path.dirname(userdata_dst))
                except FileExistsError:
                    pass

                try:
                    with open(userdata_dst, 'w') as fp:
                        fp.write(userdata)
                except Exception as e:
                    logger.exception(e)

                # Cloud init requires a meta-data file in the seed location
                metadata = "instance_id: {}\n".format(str(uuid.uuid4()))
                metadata += "local-hostname: {}\n".format(self.hostname)

                try:
                    metadata_dst = os.path.join(rootfs, 'var/lib/cloud/seed/nocloud/meta-data')
                    with open(metadata_dst, 'w') as fp:
                        fp.write(metadata)

                except Exception as e:
                    logger.exception(e)


class ContainerConfig(object):
    """
    This class represents the config file that is used to define the interfaces
    on a container.
    """

    def __init__(self, name, volume='rift'):
        self.name = name
        self.volume = volume
        self.networks = []
        self.mount_points = []
        self.cgroups = ControlGroupsConfig()

    def add_network_config(self, network_config):
        """Add a network config object

        Arguments:
            network_config - the network config object to add

        """
        self.networks.append(network_config)

    def add_mount_point_config(self, mount_point_config):
        """Add a mount point to the configuration

        Arguments,
            mount_point_config - a MountPointConfig object

        """
        self.mount_points.append(mount_point_config)

    def __repr__(self):
        fields = """
            lxc.rootfs = /dev/{volume}/{name}
            lxc.utsname = {utsname}
            lxc.tty = 4
            lxc.pts = 1024
            lxc.mount = /var/lib/lxc/{name}/fstab
            lxc.cap.drop = sys_module mac_admin mac_override sys_time
            lxc.kmsg = 0
            lxc.autodev = 1
            lxc.kmsg = 0
            """.format(volume=self.volume, name=self.name, utsname=self.name)

        fields = '\n'.join(n.strip() for n in fields.splitlines())
        cgroups = '\n'.join(n.strip() for n in str(self.cgroups).splitlines())
        networks = '\n'.join(str(n) for n in self.networks)
        mount_points = '\n'.join(str(n) for n in self.mount_points)

        return '\n'.join((fields, cgroups, networks, mount_points))


class ControlGroupsConfig(object):
    """
    This class represents the control group configuration for a container
    """

    def __repr__(self):
        return """
            #cgroups
            lxc.cgroup.devices.deny = a

            # /dev/null and zero
            lxc.cgroup.devices.allow = c 1:3 rwm
            lxc.cgroup.devices.allow = c 1:5 rwm

            # consoles
            lxc.cgroup.devices.allow = c 5:1 rwm
            lxc.cgroup.devices.allow = c 5:0 rwm
            lxc.cgroup.devices.allow = c 4:0 rwm
            lxc.cgroup.devices.allow = c 4:1 rwm

            # /dev/{,u}random
            lxc.cgroup.devices.allow = c 1:9 rwm
            lxc.cgroup.devices.allow = c 1:8 rwm
            lxc.cgroup.devices.allow = c 136:* rwm
            lxc.cgroup.devices.allow = c 5:2 rwm

            # rtc
            lxc.cgroup.devices.allow = c 254:0 rm
            """


class NetworkConfig(collections.namedtuple(
    "NetworkConfig", [
        "type",
        "link",
        "flags",
        "name",
        "veth_pair",
        "ipv4",
        "ipv4_gateway",
        ]
    )):
    """
    This class represents a network interface configuration for a container.
    """

    def __new__(cls,
            type,
            link,
            name,
            flags='up',
            veth_pair=None,
            ipv4=None,
            ipv4_gateway=None,
            ):
        return super(NetworkConfig, cls).__new__(
                cls,
                type,
                link,
                flags,
                name,
                veth_pair,
                ipv4,
                ipv4_gateway,
                )

    def __repr__(self):
        fields = [
                "lxc.network.type = {}".format(self.type),
                "lxc.network.link = {}".format(self.link),
                "lxc.network.flags = {}".format(self.flags),
                "lxc.network.name = {}".format(self.name),
                ]

        if self.veth_pair is not None:
            fields.append("lxc.network.veth.pair = {}".format(self.veth_pair))

        if self.ipv4 is not None:
            fields.append("lxc.network.ipv4 = {}/24".format(self.ipv4))

        if self.ipv4_gateway is not None:
            fields.append("lxc.network.ipv4.gateway = {}".format(self.ipv4_gateway))

        header = ["# Start {} configuration".format(self.name)]
        footer = ["# End {} configuration\n".format(self.name)]

        return '\n'.join(header + fields + footer)


class MountConfig(collections.namedtuple(
    "ContainerMountConfig", [
        "local",
        "remote",
        "read_only",
        ]
    )):
    """
    This class represents a mount point configuration for a container.
    """

    def __new__(cls, local, remote, read_only=True):
        return super(MountConfig, cls).__new__(
                cls,
                local,
                remote,
                read_only,
                )

    def __repr__(self):
        return "lxc.mount.entry = {} {} none {}bind 0 0\n".format(
                self.local,
                self.remote,
                "" if not self.read_only else "ro,"
                )
