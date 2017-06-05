
# STANDARD_RIFT_IO_COPYRIGHT

import os
import logging
import tempfile

from . import core

logger = logging.getLogger(__name__)

class Webserver(core.NativeProcess):
    """
    This class represents a webserver process.
    """

    def __init__(self,
            uid=None,
            name="RW.Webserver",
            ui_dir="./usr/share/rwmgmt-ui",
            confd_host="localhost",
            confd_port="8008",
            uagent_port=None,
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a Webserver object.

        Arguments:
            uid         - a unique identifier
            name        - the name of the process
            ui_dir      - the path to the UI resources
            confd_host  - the host that confd is on
            confd_port  - the port that confd is communicating on
            uagent_port - the port that the uAgent is communicating on
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        self.ui_dir = ui_dir
        self.confd_host = confd_host
        self.confd_port = confd_port
        self.uagent_port = uagent_port

        super(Webserver, self).__init__(
                uid=uid,
                name=name,
                exe="./usr/local/bin/rwmgmt-api-standalone",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )

    @property
    def args(self):
        return ' '.join([
            '--ui_dir {}'.format(self.ui_dir),
            '--server localhost:{}'.format(self.uagent_port),
            '--log-level CRITICAL',
            '--confd admin:admin@{}:{}'.format(self.confd_host, self.confd_port),
            ])

class RedisCluster(core.NativeProcess):
    """
    This class represents a redis cluster process.
    """

    def __init__(self,
            uid=None,
            name="RW.RedisCluster",
            num_nodes=3,
            init_port=3152,
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a RedisCluster object.

        Arguments:
            name      - the name of the process
            uid       - a unique identifier
            num_nodes - the number of nodes in the cluster
            init_port - the nodes in the cluster are assigned sequential ports
                        starting at the value given by 'init_port'.
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        args = './usr/bin/redis_cluster.py -c -n {} -p {}'
        super(RedisCluster, self).__init__(
                uid=uid,
                name=name,
                exe='python',
                args=args.format(num_nodes, init_port),
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )


class RedisServer(core.NativeProcess):
    """
    This class represents a redis server process.
    """

    def __init__(self, uid=None, name="RW.RedisServer", port=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a RedisServer object.

        Arguments:
            name - the name of the process
            uid  - a unique identifier
            port - the port to use for the server
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        # If the port is not specified, use the default for redis (NB: the
        # redis_cluster.py wrapper requires the init port to be specified so
        # something has to be provided).
        if port is None:
            port = '6379'

        super(RedisServer, self).__init__(
                uid=uid,
                name=name,
                exe='python',
                args= './usr/bin/redis_cluster.py -c -n 1 -p {}'.format(port),
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )


class UIServerLauncher(core.NativeProcess):
    """
    This class represents a UI Server Launcher.
    """

    def __init__(self, uid=None, name="RW.MC.UI", config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a UI Server Launcher

        Arguments:
            uid  - a unique identifier
            name - the name of the process
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting 
        """
        super(UIServerLauncher, self).__init__(
                uid=uid,
                name=name,
                exe="./usr/share/rw.ui/skyquake/scripts/launch_ui.sh",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )
    @property
    def args(self):
        return ' '

class RiftCli(core.NativeProcess):
    """
    This class represents a Rift CLI process.
    """

    def __init__(self,
              uid=None,
              name="RW.CLI",
              netconf_host="127.0.0.1",
              netconf_port="2022",
              config_ready=True,
              recovery_action=core.RecoveryType.FAILCRITICAL.value,
              netconf_username="admin",
              netconf_password="admin",
              start=True,
              data_storetype=core.DataStore.NOSTORE.value,
              mode_active=True,
              ):
        """Creates a RiftCli object.

        Arguments:
            manifest_file - the file listing exported yang modules
            uid  - a unique identifier
            name - the name of the process
            netconf_host - IP/Host name where the Netconf server is listening
            netconf_port - Port on which Netconf server is listening
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            netconf_username - the netconf username
            netconf_password - the netconf password
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(RiftCli, self).__init__(
                uid=uid,
                name=name,
                exe="./usr/bin/rwcli",
                interactive=True,
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )
        self.netconf_host = netconf_host
        self.netconf_port = netconf_port
        self.netconf_username = netconf_username
        self.netconf_password = netconf_password
        self.use_netconf = True

    @property
    def args(self):
        args = ""
        if self.netconf_username is not None:
            args += " --username %s " % self.netconf_username
        if self.netconf_password is not None:
            args += " --passwd %s " % self.netconf_password

        if self.use_netconf:
            args += " --netconf_host %s" % self.netconf_host
            args += " --netconf_port %s" % self.netconf_port
        else:
            args += " --rwmsg"

        return args 

class CrossbarServer(core.NativeProcess):
    """
    This class represents a Crossbar process used for DTS mock.
    """

    def __init__(self, uid=None, name="RW.Crossbar", config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a CrossbarServer object.

        Arguments:
            uid  - a unique identifier
            name - the name of the process
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(CrossbarServer, self).__init__(
                uid=uid,
                name=name,
                exe="/usr/bin/crossbar",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )

    @property
    def args(self):
        return ' '.join([
            "start", "--cbdir", "etc/crossbar/config", "--loglevel", "debug", "--logtofile",
            ])

