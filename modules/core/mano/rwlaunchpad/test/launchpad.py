#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import logging
import os
import resource
import socket
import sys
import subprocess
import shlex
import shutil
import netifaces

from rift.rwlib.util import certs
import rift.rwcal.cloudsim
import rift.rwcal.cloudsim.net
import rift.vcs
import rift.vcs.core as core
import rift.vcs.demo
import rift.vcs.vms

import rift.rwcal.cloudsim
import rift.rwcal.cloudsim.net

from rift.vcs.ext import ClassProperty

logger = logging.getLogger(__name__)


class NsmTasklet(rift.vcs.core.Tasklet):
    """
    This class represents a network services manager tasklet.
    """

    def __init__(self, name='network-services-manager', uid=None,
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        """
        Creates a NsmTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
        """
        super(NsmTasklet, self).__init__(name=name, uid=uid,
                                         config_ready=config_ready,
                                         recovery_action=recovery_action,
                                         data_storetype=data_storetype,
                                        )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwnsmtasklet')
    plugin_name = ClassProperty('rwnsmtasklet')


class VnsTasklet(rift.vcs.core.Tasklet):
    """
    This class represents a network services manager tasklet.
    """

    def __init__(self, name='virtual-network-service', uid=None,
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        """
        Creates a VnsTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
        """
        super(VnsTasklet, self).__init__(name=name, uid=uid,
                                         config_ready=config_ready,
                                         recovery_action=recovery_action,
                                         data_storetype=data_storetype,
                                        )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwvnstasklet')
    plugin_name = ClassProperty('rwvnstasklet')


class VnfmTasklet(rift.vcs.core.Tasklet):
    """
    This class represents a virtual network function manager tasklet.
    """

    def __init__(self, name='virtual-network-function-manager', uid=None,
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        """
        Creates a VnfmTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
        """
        super(VnfmTasklet, self).__init__(name=name, uid=uid,
                                          config_ready=config_ready,
                                          recovery_action=recovery_action,
                                          data_storetype=data_storetype,
                                         )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwvnfmtasklet')
    plugin_name = ClassProperty('rwvnfmtasklet')


class ResMgrTasklet(rift.vcs.core.Tasklet):
    """
    This class represents a Resource Manager tasklet.
    """

    def __init__(self, name='Resource-Manager', uid=None,
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        """
        Creates a ResMgrTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
        """
        super(ResMgrTasklet, self).__init__(name=name, uid=uid,
                                            config_ready=config_ready,
                                            recovery_action=recovery_action,
                                            data_storetype=data_storetype,
                                           )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwresmgrtasklet')
    plugin_name = ClassProperty('rwresmgrtasklet')


class MonitorTasklet(rift.vcs.core.Tasklet):
    """
    This class represents a tasklet that is used to monitor NFVI metrics.
    """

    def __init__(self, name='nfvi-metrics-monitor', uid=None,
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        """
        Creates a MonitorTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier

        """
        super(MonitorTasklet, self).__init__(name=name, uid=uid,
                                             config_ready=config_ready,
                                             recovery_action=recovery_action,
                                             data_storetype=data_storetype,
                                            )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwmonitor')
    plugin_name = ClassProperty('rwmonitor')


def get_ui_ssl_args():
    """Returns the SSL parameter string for launchpad UI processes"""

    try:
        use_ssl, certfile_path, keyfile_path = certs.get_bootstrap_cert_and_key()
    except certs.BootstrapSslMissingException:
        logger.error('No bootstrap certificates found.  Disabling UI SSL')
        use_ssl = False

    # If we're not using SSL, no SSL arguments are necessary
    if not use_ssl:
        return ""

    return "--enable-https --keyfile-path=%s --certfile-path=%s" % (keyfile_path, certfile_path)


class UIServer(rift.vcs.NativeProcess):
    def __init__(self, name="RW.MC.UI",
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        super(UIServer, self).__init__(
                name=name,
                exe="./usr/share/rw.ui/skyquake/scripts/launch_ui.sh",
                config_ready=config_ready,
                recovery_action=recovery_action,
                data_storetype=data_storetype,
                )

    @property
    def args(self):
        return get_ui_ssl_args()

class ConfigManagerTasklet(rift.vcs.core.Tasklet):
    """
    This class represents a Resource Manager tasklet.
    """

    def __init__(self, name='Configuration-Manager', uid=None,
                 config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=core.DataStore.NOSTORE.value,
                 ):
        """
        Creates a ConfigManagerTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
        """
        super(ConfigManagerTasklet, self).__init__(name=name, uid=uid,
                                                   config_ready=config_ready,
                                                   recovery_action=recovery_action,
                                                   data_storetype=data_storetype,
                                                  )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwconmantasklet')
    plugin_name = ClassProperty('rwconmantasklet')


class Demo(rift.vcs.demo.Demo):
    def __init__(self):

        procs = [
            ConfigManagerTasklet(),
            UIServer(),
            rift.vcs.DtsRouterTasklet(),
            rift.vcs.MsgBrokerTasklet(),
            rift.vcs.RestPortForwardTasklet(),
            rift.vcs.RestconfTasklet(),
            rift.vcs.RiftCli(),
            rift.vcs.uAgentTasklet(),
            rift.vcs.Launchpad(),
            ]

        restart_procs = [
            VnfmTasklet(recovery_action=core.RecoveryType.RESTART.value, data_storetype=core.DataStore.BDB.value),
            VnsTasklet(recovery_action=core.RecoveryType.RESTART.value, data_storetype=core.DataStore.BDB.value),
            MonitorTasklet(recovery_action=core.RecoveryType.RESTART.value, data_storetype=core.DataStore.BDB.value),
            NsmTasklet(recovery_action=core.RecoveryType.RESTART.value, data_storetype=core.DataStore.BDB.value),
            ResMgrTasklet(recovery_action=core.RecoveryType.RESTART.value, data_storetype=core.DataStore.BDB.value),
            ]
        super(Demo, self).__init__(
            # Construct the system. This system consists of 1 cluster in 1
            # colony. The master cluster houses CLI and management VMs
            sysinfo = rift.vcs.SystemInfo(
                    colonies=[
                            rift.vcs.Colony(
                                name='master',
                                clusters=[
                                    rift.vcs.VirtualMachine(
                                        name='vm-launchpad',
                                        ip='127.0.0.1',
                                        procs=procs,
                                        restart_procs=restart_procs,
                                        ),
                                    ]
                                )
                            ]
                        ),

            # Define the generic portmap.
            port_map = {},

            # Define a mapping from the placeholder logical names to the real
            # port names for each of the different modes supported by this demo.
            port_names = {
                'ethsim': {
                },
                'pci': {
                }
            },

            # Define the connectivity between logical port names.
            port_groups = {},
        )


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')

    # Create a parser which includes all generic demo arguments
    parser = rift.vcs.demo.DemoArgParser()

    args = parser.parse_args(argv)

    # Disable loading any kernel modules for the launchpad VM
    # since it doesn't need it and it will fail within containers
    os.environ["NO_KERNEL_MODS"] = "1"

    # Remove the persistant DTS recovery files 
    for f in os.listdir(os.environ["INSTALLDIR"]):
        if f.endswith(".db"):
            os.remove(os.path.join(os.environ["INSTALLDIR"], f))
    try:
        shutil.rmtree(os.path.join(os.environ["INSTALLDIR"], "zk/server-1"))
    except:
        pass

    #load demo info and create Demo object
    demo = Demo()

    # Create the prepared system from the demo
    system = rift.vcs.demo.prepared_system_from_demo_and_args(demo, args, 
              northbound_listing="cli_rwmc_schema_listing.txt",
              netconf_trace_override=True)

    confd_ip = socket.gethostbyname(socket.gethostname())
    intf = netifaces.ifaddresses('eth0')
    if intf and netifaces.AF_INET in intf and len(intf[netifaces.AF_INET]):
       confd_ip = intf[netifaces.AF_INET][0]['addr']
    rift.vcs.logger.configure_sink(config_file=None, confd_ip=confd_ip)

    # Start the prepared system
    system.start()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CORE, (resource.RLIM_INFINITY, resource.RLIM_INFINITY) )
    try:
        main()
    except rift.vcs.demo.ReservationError:
        print("ERROR: unable to retrieve a list of IP addresses from the reservation system")
        sys.exit(1)
    except rift.vcs.demo.MissingModeError:
        print("ERROR: you need to provide a mode to run the script")
        sys.exit(1)
    finally:
        os.system("stty sane")
