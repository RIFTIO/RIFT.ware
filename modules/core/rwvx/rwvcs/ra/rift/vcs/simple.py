#!/usr/bin/env python3

import logging
import os
import pprint
import sys

import rift.vcs.api
import rift.vcs.compiler
import rift.vcs.manifest
import rift.vcs.vms

gdb_enabled = False
class RedisServer(rift.vcs.NativeProcess):
    def __init__(self, name="RW.Redis.Server",
                 config_ready=True,
                 recovery_action=rift.vcs.core.RecoveryType.FAILCRITICAL.value,
                 data_storetype=rift.vcs.core.DataStore.NOSTORE.value,
                 mode_active=True,
                 ):
        super(RedisServer, self).__init__(
                name=name,
                exe="./usr/bin/redis-server",
                config_ready=config_ready,
                recovery_action=recovery_action,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )

    @property
    def args(self):
        return "./usr/bin/active_redis.conf --port 9999"

class CliVM(rift.vcs.VirtualMachine):
    """
    This class represents a CLI VM.
    """

    def __init__(self, name=None, start=True, *args, **kwargs):
        """Creates a CliVM object.

        Arguments:
            name - the name of the tasklet

        """
        name = "VM_TEMPL_2" if name is None else name
        super(CliVM, self).__init__(name=name, start=start, *args, **kwargs)

        self.add_proc(rift.vcs.DtsPerfTasklet(), mode_active=False)
        self.add_proc(RedisServer(), mode_active=False) 
        if not start:
            self.add_tasklet(rift.vcs.uAgentTasklet(), mode_active=False)



class MgmtVM(rift.vcs.VirtualMachine):
    """
    This class represents a management VM.
    """

    def __init__(self, name=None, start=True, *args, **kwargs):
        """Creates a MgmtVM object.

        Arguments:
            name          - the name of the tasklet

        """
        name = "VM_TEMPL_1" if name is None else name
        super(MgmtVM, self).__init__(name=name, start=start, *args, **kwargs)

        #self.add_proc(rift.vcs.MsgBrokerTasklet())
        self.add_proc(rift.vcs.DtsRouterTasklet())
        self.add_proc(rift.vcs.DtsPerfTasklet())
        #self.add_proc(rift.vcs.LogdTasklet())

        self.add_proc(rift.vcs.procs.RiftCli());

        #Confd would need RestConf present
        self.add_proc(rift.vcs.uAgentTasklet())
        #self.add_proc(rift.vcs.Confd())
        self.add_proc(rift.vcs.RestconfTasklet())
        self.add_proc(rift.vcs.Watchdog())
        self.add_proc(RedisServer())

        #self.add_proc(rift.vcs.Webserver())
        #self.add_proc(rift.vcs.RedisCluster())

class GenVM(rift.vcs.VirtualMachine):
    """
    This class represents a generic VM.
    """

    def __init__(self, name=None, *args, **kwargs):
        """Creates a GenVM object.

        Arguments:
            name          - the name of the tasklet

        """
        name = "RW_VM_GEN" if name is None else name
        super(GenVM, self).__init__(name=name, *args, **kwargs)


        #self.add_proc(rift.vcs.DtsPerfTasklet())
        #self.add_proc(rift.vcs.LogdTasklet())

class Gen2VM(rift.vcs.VirtualMachine):
    """
    This class represents a generic VM.
    """

    def __init__(self, name=None, *args, **kwargs):
        """Creates a Gen2VM object.

        Arguments:
            name          - the name of the tasklet

        """
        name = "RW_VM_GEN2" if name is None else name
        super(Gen2VM, self).__init__(name=name, *args, **kwargs)

        #self.add_proc(rift.vcs.DtsPerfTasklet())
        #self.add_proc(rift.vcs.LogdTasklet())


def main():
    MASTER="10.0.202.246"
    VM1="10.0.202.26"
    VM2="10.0.106.26"
    VM3="10.0.106.27"
    collapsed = False
    mgmt_ip_list=[VM2, VM3]
    mgmt_ip_list=[MASTER, VM1]
    start=False

    if '-l' in sys.argv:
        MASTER='127.0.0.1'
        VM1='127.0.0.1'
        VM2='127.0.0.1'
        VM3='127.0.0.2'
        collapsed = True
        start=True
        mgmt_ip_list=[MASTER]

    if '-g' in sys.argv:
        gdb_enabled = True

    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')
    logging.getLogger().setLevel(logging.DEBUG)

    colony = rift.vcs.core.Colony(name='top', uid=1)

    #gen = rift.vcs.core.Cluster(name='gen')
    #colony.add_cluster(gen)
    #vm3 = GenVM(ip=VM2)
    #vm3.leader = True
    #gen.add_virtual_machine(vm3)

    #vm4 = Gen2VM(ip=VM3)
    #gen.add_virtual_machine(vm4)


    #mgmt = rift.vcs.core.Cluster(name='mgmt')
    #colony.add_cluster(mgmt)

    vm2 = MgmtVM(ip=MASTER,start=start)
    vm2.leader = True
    #mgmt.add_virtual_machine(vm2)
    colony.append(vm2)

    lead = CliVM(ip=VM1, start=start)
    colony.append(lead)

    multid=False
    if '-m' in sys.argv:
      multid=True

    # Construct the system
    sysinfo = rift.vcs.core.SystemInfo(
            mode='ethsim',
            collapsed=collapsed,
            zookeeper=rift.vcs.manifest.RaZookeeper(zake=collapsed, master_ip=MASTER),
            colonies=[colony],
            multi_broker=True,
            multi_dtsrouter=True,
            mgmt_ip_list=mgmt_ip_list,

    )

    # Compile the manifest
    compiler = rift.vcs.compiler.LegacyManifestCompiler()
    try:
        sysinfo_ret, manifest = compiler.compile(sysinfo)
    except:
        sysinfo_ret, manifest = compiler.compile(sysinfo, "cli_rwfpath_schema_listing.txt")
        pass

    pwd = os.getcwd()
    os.chdir(os.environ['RIFT_INSTALL'])

    manifest_file = os.path.join(pwd, "manifest.xml")

    vcs = rift.vcs.api.RaVcsApi()
    vcs.manifest_generate_xml(manifest, manifest_file)


if __name__ == "__main__":
    main()

