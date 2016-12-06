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
# @file mano_vms.py
# @author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
# @date 02/29/2016
# @brief This module contains classes representing VMs used in MANO System
#
import rift.vcs
import rift.app.strongswan.tasklet

class StandardLeadVM(rift.vcs.VirtualMachine):
    """
    This class represents a master VM with all infrastructure components

    This is a merge of both the CliVM and MgmtVM components
    """

    def __init__(self, name=None, *args, **kwargs):
        """Creates a StandardLeadVM object.

        Arguments:
            name          - the name of the VM

        """
        name = "RW.VDU.LEAD.STND" if name is None else name
        super().__init__(name=name, *args, **kwargs)
        self.config_ready = self.config_ready and self.start

        self.add_proc(rift.vcs.uAgentTasklet())
        self.add_proc(rift.vcs.RiftCli())
        self.add_proc(rift.vcs.MsgBrokerTasklet())
        self.add_proc(rift.vcs.RestconfTasklet(rest_port="8008"))
        self.add_proc(rift.vcs.RestPortForwardTasklet())

        self.add_proc(rift.vcs.DtsRouterTasklet())
        self.add_proc(rift.vcs.UIServerLauncher())


class StandardVM(rift.vcs.VirtualMachine):
    """
    This class represents a master VM with all infrastructure components
    """
    def __init__(self, name=None, *args, **kwargs):
        """Creates a StandardVM object.

        Arguments:
            name          - the name of the VM

        """
        name = "RW.VDU.MEMB.STND" if name is None else name
        super().__init__(name=name, *args, **kwargs)
        self.config_ready = self.config_ready and self.start

class LeadFastpathVM(rift.vcs.VirtualMachine):
    """
    This class represents the lead virtual fabric VM composed of process components
    necessary for a base fast path lead VM
    """

    def __init__(self, name=None, *args, **kwargs):
        """Creates a LeadFastpathVM object.

        Arguments:
            name          - the name of the VM

        """
        name = "RW.VDU.MEMB.DDPL" if name is None else name
        super().__init__(name=name, *args, **kwargs)
        self.config_ready = self.config_ready and self.start

        fpctrl_tasklets=[rift.vcs.Controller(), rift.vcs.SfMgr(),
                  rift.vcs.SffMgr()]

        for tl in fpctrl_tasklets:
            tl.config_ready = self.config_ready

        self.subcomponents = [
                rift.vcs.Proc(
                    name="rw.proc.fpath",
                    run_as="root",
                    config_ready=self.config_ready,
                    tasklets=[rift.vcs.Fastpath(name='RW.Fpath',
                                                config_ready=self.config_ready)],
                    ),
                rift.vcs.Proc(
                    name="rw.proc.ncmgr",
                    run_as="root",
                    config_ready=self.config_ready,
                    tasklets=[rift.vcs.NetworkContextManager(config_ready=self.config_ready)],
                    ),
                rift.vcs.Proc(
                    name="rw.proc.fpctrl",
                    config_ready=self.config_ready,
                    tasklets=fpctrl_tasklets,
                    ),
                rift.vcs.Proc(
                    name="rw.proc.ifmgr",
                    config_ready=self.config_ready,
                    tasklets=[rift.vcs.InterfaceManager(config_ready=self.config_ready)],
                    ),
                ]


class LeadFastpathIPSECVM(LeadFastpathVM):
    """
    This class represents the Strongswan VM.
    """
    def __init__(self, name=None, *args, **kwargs):
        """Creates a StrongswanLeadVM object.
         Arguments:
             ip - ip address of the VM
             name - name of the VM
        """
        name = "RW.VDU.MEMB.DDPL-IPSEC" if name is None else name
        super().__init__(name=name, *args, **kwargs)

        self.subcomponents.extend([
                rift.vcs.Proc(
                    name="rw.proc.strongswan",
                    run_as="root",
                    config_ready=self.config_ready,
                    tasklets=[rift.app.strongswan.tasklet.StrongswanTasklet(config_ready=self.config_ready)],
                    ),
                ])


class FastpathVM(rift.vcs.VirtualMachine):
    """
    This class is used for VM running applications that require a fastpath tasklets.
    """

    def __init__(self, name=None, *args, **kwargs):
        """Creates a FastpathVM object.

        Arguments:
            name          - the name of the VM

        """
        name = "RW.VDU.MEMB.DDPM" if name is None else name
        super().__init__(name=name, *args, **kwargs)
        self.config_ready = self.config_ready and self.start

        self.subcomponents = [
                rift.vcs.Proc(
                    name="rw.proc.fpath",
                    run_as="root",
                    config_ready=self.config_ready,
                    tasklets=[rift.vcs.Fastpath(name='RW.Fpath', config_ready=self.config_ready)],
                    ),
                ]
