
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

import rift.vcs

class LeadVM(rift.vcs.VirtualMachine):
    """
    This class represents the lead virtual fabric VM.
    """

    def __init__(self, *args, **kwargs):
        """Creates a LeadVM object."""
        super(LeadVM, self).__init__(*args, **kwargs)
        self.subcomponents = [
                rift.vcs.Proc(
                    name="rw.proc.fpath",
                    run_as="root",
                    tasklets=[rift.vcs.Fastpath(name='RW.Fpath')],
                    ),
                # app manager is not required for trafgen applications
                rift.vcs.Proc(
                    name="rw.proc.appmgr",
                    run_as="root",
                    tasklets=[rift.vcs.ApplicationManager()],
                    ),
                rift.vcs.Proc(
                    name="rw.proc.ncmgr",
                    run_as="root",
                    tasklets=[rift.vcs.NetworkContextManager()],
                    ),
                rift.vcs.Proc(
                    name="rw.proc.fpctrl",
                    tasklets=[
                        rift.vcs.Controller(),
                        rift.vcs.NNLatencyTasklet(),
                        rift.vcs.SfMgr(),
                        rift.vcs.SffMgr()
                        ],
                    ),
                rift.vcs.Proc(
                    name="rw.proc.ifmgr",
                    tasklets=[rift.vcs.InterfaceManager()],
                    ),
                ]


class CliVM(rift.vcs.VirtualMachine):
    """
    This class represents a CLI VM.
    """

    def __init__(self, netconf_username="admin", netconf_password="admin", name=None, *args, **kwargs):
        """Creates a CliVM object.

        Arguments:
            netconf_username - the netconf username
            netconf_password - the netconf password
            name - the name of the tasklet

        """
        name = "RW_VM_CLI" if name is None else name
        super(CliVM, self).__init__(name=name, *args, **kwargs)

        self.add_proc(rift.vcs.RiftCli(netconf_username, netconf_password))


class MgmtVM(rift.vcs.VirtualMachine):
    """
    This class represents a management VM.
    """

    def __init__(self, name=None, *args, **kwargs):
        """Creates a MgmtVM object.

        Arguments:
            name          - the name of the VM

        """
        name = "RW_VM_MGMT" if name is None else name
        super(MgmtVM, self).__init__(name=name, *args, **kwargs)

        self.add_proc(rift.vcs.MsgBrokerTasklet())
        self.add_tasklet(rift.vcs.uAgentTasklet())
        self.add_proc(rift.vcs.RestconfTasklet(rest_port="8008"))
        self.add_proc(rift.vcs.RestPortForwardTasklet())
        # Redis is not currently used by DTS
        # Disable but leave ready for easy enablement
        # self.add_proc(rift.vcs.RedisCluster())
        self.add_proc(rift.vcs.DtsRouterTasklet())
        self.add_proc(rift.vcs.UIServerLauncher())


class MasterVM(rift.vcs.VirtualMachine):
    """
    This class represents a master VM with all infrastructure components

    This is a merge of both the CliVM and MgmtVM components
    """

    def __init__(self, netconf_username="admin", netconf_password="admin", name=None, *args, **kwargs):
        """Creates a MasterVM object.

        Arguments:
            netconf_username - the netconf username
            netconf_password - the netconf password
            name          - the name of the VM

        """
        name = "RW_VM_MASTER" if name is None else name
        super(MasterVM, self).__init__(name=name, *args, **kwargs)

        self.add_proc(rift.vcs.uAgentTasklet())
        self.add_proc(rift.vcs.RiftCli(netconf_username=netconf_username, netconf_password=netconf_password))
        self.add_proc(rift.vcs.MsgBrokerTasklet())
        self.add_proc(rift.vcs.RestconfTasklet(rest_port="8008"))
        self.add_proc(rift.vcs.RestPortForwardTasklet())

        # Redis is not currently used by DTS
        # Disable but leave ready for easy enablement
        # self.add_proc(rift.vcs.RedisCluster())
        self.add_proc(rift.vcs.DtsRouterTasklet())
        self.add_proc(rift.vcs.UIServerLauncher())

class SingeVnfVm(MasterVM):
    """
    This class represents VM with master components and
    LeadVM components combined into a single VM
    """
    def __init__(self, *args, **kwargs):
        super(SingeVnfVm, self).__init__(*args, **kwargs)
        self.subcomponents.extend(LeadVM().subcomponents)


class FastpathVM(rift.vcs.VirtualMachine):
    """
    This class is used for VM running applications that require a fastpath tasklets.
    """

    def __init__(self, *args, **kwargs):
        super(FastpathVM, self).__init__(*args, **kwargs)

        self.subcomponents = [
                rift.vcs.Proc(
                    name="rw.proc.fpath",
                    run_as="root",
                    tasklets=[rift.vcs.Fastpath(name='RW.Fpath')],
                    ),
                ]


class FastpathApplicationVM(rift.vcs.VirtualMachine):
    """
    This class is used for VM running applications that require a fastpath and
    application manager tasklets.
    """

    def __init__(self, *args, **kwargs):
        super(FastpathApplicationVM, self).__init__(*args, **kwargs)

        self.subcomponents = [
                rift.vcs.Proc(
                    name="rw.proc.fpath",
                    run_as="root",
                    tasklets=[rift.vcs.Fastpath(name='RW.Fpath')],
                    ),
                rift.vcs.Proc(
                    name="rw.proc.appmgr",
                    run_as="root",
                    tasklets=[rift.vcs.ApplicationManager()],
                    ),
                ]
