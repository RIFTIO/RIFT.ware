
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

import copy
import logging
import os
import re
import uuid
import subprocess

import ndl

from . import core

import gi
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwYang', '1.0')
from gi.repository import RwManifestYang, RwYang
from rift.rwlib.util import certs
import rift.vcs.mgmt

logger = logging.getLogger(__name__)


class ManifestError(Exception):
    pass


class RaManifestObject(object):
    '''
    Do not use. Virtual class

    However most of these methods are used as is by the derived classes
    '''

    def __init__(self, name, subcomponents=None, instance_id=None, config_ready=False,
                 recovery_action="FAILCRITICAL", start=True,
                 data_storetype="NOSTORE",
                ):
        self.name = name
        self.subcomponents = []
        self.instance_id = instance_id
        self.colony_id = self.instance_id
        self.python_vars = []
        self.startups = []
        self.parent = None
        self.config_ready = config_ready
        self.recovery_action = recovery_action
        self.start = start
        self.data_storetype = data_storetype

        if subcomponents is not None:
            for subcomponent in subcomponents:
                self.add_component(subcomponent)

    def _set_ips(self, ips):
        '''
        most classes do not need or use an IP so they skip this step
        RaVm does grab one
        '''
        for c in self.subcomponents:
            ips = c._set_ips(ips)
        return ips

    def _get_parent_by_class(self, cls):
        '''
        walk up the component tree until you find an obect of the right class and return it
        '''
        cl = self
        while not isinstance(cl, cls):
            #logger.debug("%s is not a %s it is a %s" %  ( cl.name, cls.__name__, cl.__class__.__name__ ))
            cl = cl.parent
            if cl is None:
                raise Exception("WARNING unable to locate a %s object that is a parent of %s" % ( cls.__name__, self.name ))
        return cl

    def _find_children_by_class(self, cls):
        '''
        returns a list of all children of that class (or subclasses thereof)
        '''
        res = []
        if isinstance(self, cls):
            res.append(self)
        for c in self.subcomponents:
            res.extend(c._find_children_by_class(cls))
        return res

    def add_startup(self,c):
        '''helper function to append an object to the list of components that should be started by rwmain '''
        if c.start:
           self.startups.append(c)

    def add_component(self, component, startup=False):
        '''
        add a component as a subcomponent of this component
        component trees can be as tall or as wide as you want
        '''
        if not isinstance(component, RaManifestObject):
            raise Exception("add_component: {} is not a RaManifestObject".format(component))
        c = component #.copy()
        self.subcomponents.append(c)
        if startup:
            self.add_startup(c)
        c.parent = self

    def descendents(self):
        '''
        list generated via depth-first list of the object's decendents
        '''
        ret = []
        for sc in self.subcomponents:
            ret.extend(sc.descendents())
        ret.append(self)

        return ret

    def _prepare(self):
        '''
        prepare this object for writing to a protobuf. This allows a child to be ready
        when the parent tries to create the start or annex portion of his entry

        This method should be used to completely configure all objects in the component tree that do not rely on
        settings in other components

        default is to do nothing which is right for many classes
        '''
        for s in self.subcomponents:
            s._prepare()

    def _add_to_inventory(self, parent, manifest, known):
        raise NotImplementedError()


class RaTasklet(RaManifestObject):
    '''
    Base class for all tasklets. RaToyTasklet is a full defined
    tasklet. You should only have to instantiate an instance
    and then pass it to component_add()
    '''
    def __init__(self, name, plugin_directory, plugin_name, instance_id=None, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        '''
        create a tasklet definition
        see RaToyTasklet for an example
        '''

        super(RaTasklet,self).__init__(name, instance_id=instance_id, config_ready=config_ready,
                                       recovery_action=recovery_action, start=start,
                                       data_storetype=data_storetype,
                                       )
        self.plugin_directory = plugin_directory
        self.plugin_name = plugin_name
        self.mode_active = mode_active

    def _add_to_inventory(self, parent, manifest, known):
        '''
        internal function used by as_pb()
        '''
        if self.name not in known:
            known.add(self.name)
            c = parent.add()
            c.component_name = "%s" % self.name
            c.component_type = "RWTASKLET"
            c.rwtasklet.plugin_directory = self.plugin_directory
            c.rwtasklet.plugin_name = self.plugin_name


class RaToyTasklet(RaTasklet):
    '''
    This is a fully defined tasklet. You should only have to
    instantiate an instance and use it.
    If you need to customize it, you should consider basing your version on
    '''
    def __init__(self,name="RW.toytasklet", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active = True,
                ):
        super(RaToyTasklet,self).__init__(name, "./usr/lib/rift/plugins/rwtoytasklet-c", "rwtoytasklet-c",
                                          config_ready=config_ready,
                                          recovery_action=recovery_action,
                                          start=start,
                                          data_storetype=data_storetype,
                                          mode_active=mode_active,
                                         )
        self.python_vars.append('echo_client = -1')


class RaToyTaskletPython(RaTasklet):
    '''
    This is a fully defined tasklet. You should only have to
    instantiate an instance and use it.
    If you need to customize it, you should consider basing your version on
    '''
    def __init__(self,name="RW.toytasklet.python", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active= True,
                ):
        super(RaToyTaskletPython,self).__init__(
                name,
                "./usr/lib/rift/plugins/pytoytasklet",
                "pytoytasklet",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )


class RaDtsTaskletPython(RaTasklet):
    '''
    This is a fully defined tasklet. You should only have to
    instantiate an instance and use it.
    If you need to customize it, you should consider basing your version on
    '''
    def __init__(self,name="RW.dtstasklet.python", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaDtsTaskletPython,self).__init__(name, "./usr/lib/rift/plugins/rwdtstasklet", "rwdtstasklet",
                                                config_ready=config_ready,
                                                recovery_action=recovery_action,
                                                start=start,
                                                data_storetype=data_storetype,
                                                mode_active=mode_active,
                                                )

class RaDtsPerfMgrTasklet(RaTasklet):
    '''The DTS Performance Manager Tasklet. This is a fully defined tasklet. You
    should only have to instantiate an instance and use it. If you need to
    customize it, you should consider creating a new tasklet and basing your
    version on it.
    '''
    def __init__(self,name="RW.dtsperfmgrtasklet.python", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaDtsPerfMgrTasklet,self).__init__(
                name,
                "./usr/lib/rift/plugins/rwdtsperfmgrtasklet",
                "rwdtsperfmgrtasklet",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )

class RaNativeProcess(RaManifestObject):
    '''
    base class for any native executable

    note that it is derived from a tasklet even though it is by all definitions a proc
    '''
    def __init__(self, name, exe, args=None, run_as=None, valgrind=None, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaNativeProcess, self).__init__(name, "", "", config_ready=config_ready,
                                              recovery_action=recovery_action,
                                              start=start,
                                              data_storetype=data_storetype,
                                              )
        self.native_exe = exe
        self.native_args = args
        self.run_as = run_as
        self.valgrind = valgrind
        self.interactive = False
        self.mode_active = mode_active

    def _add_to_inventory(self, parent, manifest, known):
        '''
        internal function used by as_pb()
        '''
        if self.name not in known:
            known.add(self.name)
            c = parent.add()
            c.component_name = "%s" % self.name
            c.component_type = "PROC"
            c.native_proc.exe_path = self.native_exe

            if self.native_args is not None:
                c.native_proc.args = self.native_args

            if self.run_as is not None:
                c.native_proc.run_as = self.run_as

            if self.valgrind:
                c.native_proc.valgrind.enable = True
                log_dir = os.environ['RIFT_ARTIFACTS']
                log_file = 'valgrind_{}.{}.out'.format(c.component_name, str(uuid.uuid4()))
                c.native_proc.valgrind.opts.append('--log-file={}/{}'.format(log_dir, log_file))
                c.native_proc.valgrind.opts.append('--leak-check=full')

            if self.interactive:
                c.native_proc.interactive = self.interactive

class RaWebServer(RaNativeProcess):
    """
    The RaWebServer class represents the rift web UI.
    """
    pass


class RaRedisCluster(RaNativeProcess):
    """
    The RaRedisCluster class represents a cluster of redis nodes.
    """
    pass


class RaRedisServer(RaNativeProcess):
    """
    The RaRedisServer class represents a single redis node running as a native
    process.
    """
    pass

class RaCliProc(RaNativeProcess):
    """
    The RaCliProc class represents the Rift Cli running as native process
    """
    def __init__(self, name, exe=None, args=None, run_as=None, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaCliProc, self).__init__(name, exe=exe, args=args, run_as=run_as,
                                        config_ready=config_ready, recovery_action=recovery_action,
                                        start=start, data_storetype=data_storetype,
                                        mode_active=mode_active,
                                        )

class RaCli(RaTasklet):
    '''
    This is a fully defined tasklet. You should only have to
    instantiate an instance and use it.
    If you need to customize it, you should consider basing your version on
    '''
    def __init__(self, name, manifest_file="cli_rwfpath.xml", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaCli, self).__init__(name, "./usr/lib/rift/plugins/rwcli-c", "rwcli-c",
                                    config_ready=config_ready,
                                    recovery_action=recovery_action,
                                    start=start, data_storetype=data_storetype,
                                    mode_active=True,
                                    )
        self.python_vars.append("""cmdargs_str = '"load cli manifest {}"'""".format(
            manifest_file
            ))

class RaMockCli(RaCli):
    '''
    This is a fully defined tasklet. You should only have to
    instantiate an instance and use it.
    If you need to customize it, you should consider basing your version on
    '''
    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaCli, self).__init__(
                name,
                "./usr/lib/rift/plugins/pytoytasklet",
                "pytoytasklet",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start, data_storetype=data_storetype,
                mode_active=True,
                )

class RaFastpath(RaTasklet):
    def __init__(self,
            name,
            cmdargs=None,
            port_map_expr=None,
            lcore_workers=None,
            hashbin_credit=None,
            memory_model=None,
            config_ready=False,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        super(RaFastpath,self).__init__(name, "./usr/lib/rift/plugins/rwfpath-c", "rwfpath-c",
                                        config_ready=config_ready,
                                        recovery_action=recovery_action,
                                        start=start,
                                        data_storetype=data_storetype,
                                        mode_active=mode_active,
                                        )
        self.cmdargs = None if cmdargs is None else copy.copy(cmdargs)

        if lcore_workers is not None:
            if self.cmdargs is None:
                raise ValueError('ERROR: setting lcore workers without providing cmdargs')
            self.cmdargs.lcore_workers = lcore_workers

        if memory_model is not None:
            if self.cmdargs is None:
                raise ValueError('ERROR: setting memory model without providing cmdargs')
            self.cmdargs.mem_model = memory_model

        if hashbin_credit is not None:
            self.python_vars.append("hashbin_credit = %d" % hashbin_credit)

        self.port_map_expr = port_map_expr

    def _prepare(self):
        '''
        this class has a lot of custom config because it is more sensitive than most to the
        configuration of the system
        '''
        cluster = self._get_parent_by_class(RaCluster)
        self.python_vars.append("colony_name = '%s'" % cluster.name)
        self.python_vars.append("colony_id = %d" % cluster.colony_id)

        if self.port_map_expr:
            port_map = "port_map = '" + self.port_map_expr + "'"
            self.python_vars.append(port_map)

        # Define the fastpath command arguments string
        if self.cmdargs is None:
            cmdargs_str = "cmdargs_str = ''"
        else:
            template = self.cmdargs.arguments()
            cmdargs_str = "cmdargs_str = " + template.format(id=self.colony_id)

        self.python_vars.append(cmdargs_str)


class RaNcMgr(RaTasklet):
    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaNcMgr, self).__init__(name, "./usr/lib/rift/plugins/rwncmgr-c", "rwncmgr-c",
                                      config_ready=config_ready,
                                      recovery_action=recovery_action,
                                      start=start,
                                      data_storetype=data_storetype,
                                      mode_active=mode_active,
                                      )

    def _prepare(self):
        '''
        @type vm: RaVm

        we need to locate the FastPath instance that is part of this VM
        '''
        vm = self._get_parent_by_class(RaVm)
        fp = vm._find_children_by_class(RaFastpath)
        if len(fp) == 0:
            raise Exception("could not locate an instance of RaFastpath as a sibling")
        if len(fp) > 1:
            logger.warning("found more than 1 fastpath as a sibling")
        cmdargs_str = "cmdargs_str = ''"
        self.python_vars.append(cmdargs_str)

        cluster = self._get_parent_by_class(RaCluster)
        self.python_vars.append("colony_name = '%s'" % cluster.name)
        self.python_vars.append("colony_id = %d" % cluster.colony_id)


class RaIfMgr(RaTasklet):
    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaIfMgr, self).__init__(name, "./usr/lib/rift/plugins/rwifmgr-c", "rwifmgr-c",
                                      config_ready=config_ready,
                                      recovery_action=recovery_action,
                                      start=start,
                                      data_storetype=data_storetype,
                                      mode_active=mode_active,
                                      )
        self.python_vars.append("cmdargs_str = ''")

    def _prepare(self):
        '''
        @type vm: RaVm

        we need to locate the FastPath instance that is part of this VM
        '''
        cluster = self._get_parent_by_class(RaCluster)
        self.python_vars.append("colony_name = '%s'" % cluster.name)
        self.python_vars.append("colony_id = %d" % cluster.colony_id)


class RaFpCtrl(RaTasklet):
    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaFpCtrl, self).__init__(name, "./usr/lib/rift/plugins/rwfpctrl-c", "rwfpctrl-c",
                                       config_ready=config_ready,
                                       recovery_action=recovery_action,
                                       start=start,
                                       data_storetype=data_storetype,
                                       mode_active=mode_active,
                                       )
        self.python_vars.append("cmdargs_str = ''")

    def _prepare(self):
        cluster = self._get_parent_by_class(RaCluster)
        self.python_vars.append("colony_name = '%s'" % cluster.name)
        self.python_vars.append("colony_id = %d" % cluster.colony_id)


class RaAppMgr(RaTasklet):
    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaAppMgr, self).__init__(name,"./usr/lib/rift/plugins/rwappmgr-c", "rwappmgr-c",
                                       config_ready=config_ready,
                                       recovery_action=recovery_action,
                                       start=start,
                                       data_storetype=data_storetype,
                                       mode_active=mode_active,
                                       )
        self.python_vars.append("cmdargs_str = ''")

    def _prepare(self):
        vm = self._get_parent_by_class(RaVm)
        fp = vm._find_children_by_class(RaFastpath)
        if len(fp) == 0:
            raise Exception("could not locate an instance of RaFastpath as a sibling")
        if len(fp) > 1:
            logger.warning("found more than 1 fastpath as a sibling")

        cluster = self._get_parent_by_class(RaCluster)
        self.python_vars.append("fpath_instance = '%s'" %fp[0].colony_id)
        self.python_vars.append("colony_name = '%s'" % cluster.name)
        self.python_vars.append("colony_id = %d" % cluster.colony_id)


class RaBroker(RaTasklet):
    def __init__(self, name, port=2222, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaBroker, self).__init__(name, "./usr/lib/rift/plugins/rwmsgbroker-c", "rwmsgbroker-c",
                                       config_ready=config_ready,
                                       recovery_action=recovery_action,
                                       start=start,
                                       data_storetype=data_storetype,
                                       mode_active=mode_active,
                                       )
        self.port = port


class RaUagent(RaTasklet):
    def __init__(self, name, port, confd_ip=None, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaUagent, self).__init__(name, "./usr/lib/rift/plugins/rwuagent-c", "rwuagent-c",
                                       config_ready=config_ready,
                                       recovery_action=recovery_action,
                                       start=start,
                                       data_storetype=data_storetype,
                                       mode_active=mode_active,
                                       )
        self.port = port

        self.host = "127.0.0.1" if confd_ip is None else confd_ip

        host = "--confd-proto AF_INET --confd-ip {0}".format(self.host)
        cli_args = host

        self.python_vars.append("cmdargs_str = '{}'".format(cli_args))


class RaDtsRouter(RaTasklet):
    '''
    This class represents the DTS router tasklet.
    '''

    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        '''Creates an RaDtsRouter object

        Arguments:
            name - the name of the tasklet
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        '''
        super(RaDtsRouter, self).__init__(
                name,
                plugin_directory="./usr/lib/rift/plugins/rwdtsrouter-c",
                plugin_name="rwdtsrouter-c",
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )



class Logd(RaTasklet):
    '''
    This class represents the Logd tasklet.
    '''
    def __init__(self, name, config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        '''Creates a Logd object

        Arguments:
            name   - the name of the tasklet
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        '''
        super(Logd, self).__init__(
                name,
                plugin_directory='./usr/lib/rift/plugins/rwlogd-c',
                plugin_name='rwlogd-c',
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                mode_active=mode_active,
                )

class RaRwInit(RaTasklet):
    '''
    the root of all procs
    '''
    def __init__(self, name="RW.Init", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(RaRwInit,self).__init__(name, "./usr/lib/rift/plugins/rwinit-c", "rwinit-c",
                                      config_ready=config_ready,
                                      recovery_action=recovery_action,
                                      start=start,
                                      data_storetype=data_storetype,
                                      mode_active=mode_active,
                                      )


class RaZookeeper(object):
    '''
    This class represents the zookeeper information required by the manifest.
    '''

    def __init__(self, master_ip="127.0.0.1", unique_ports=False, zake=False):
        '''
        Creates an instance of RaZookeeper.

        Arguments:
            master_ip       - this is the IP address of the CLI node
            unique_ports    - use non-default zookeeper ports based on UID
            zake            - boolean indicating whether to use zake (the fake
                              zookeeper) or the real zookeeper

        '''
        self.unique_ports = unique_ports
        self.master_ip = master_ip
        self.zake = zake

    def __repr__(self):
        '''
        Returns a representation of the RaZookeeper object.

        '''
        return repr(dict(
            unique_ports=self.unique_ports,
            master_ip=self.master_ip,
            zake=self.zake,
            ))

class RaProc(RaManifestObject):
    def __init__(self, name="rwproc", run_as=None, subcomponents=None, valgrind=None,
                 config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                ):
        super(RaProc, self).__init__(name, subcomponents=subcomponents,
                                     config_ready=config_ready,
                                     recovery_action=recovery_action,
                                     start=start,
                                     data_storetype=data_storetype,
                                     )
        self.run_as = run_as
        self.valgrind = valgrind

    def _add_to_inventory(self, parent, manifest, known):
        '''
        internal function used by manifest.as_pb()
        '''
        if self.name in known:
            return

        known.add(self.name)

        c = parent.add()
        c.component_name = self.name
        c.component_type = "RWPROC"
        if self.run_as is not None:
            c.rwproc.run_as = self.run_as

        if self.valgrind:
            c.rwproc.valgrind.enable = True
            log_dir = os.environ['RIFT_ARTIFACTS']
            log_file = 'valgrind_{}.{}.out'.format(c.component_name, str(uuid.uuid4()))
            c.rwproc.valgrind.opts.append('--log-file={}/{}'.format(log_dir, log_file))
            c.rwproc.valgrind.opts.append('--leak-check=full')

        # add your direct descendents with instances
        for s in self.subcomponents:
            a = c.rwproc.tasklet.add()
            a.name = "Start %s for %s" % ( s.name, self.name )
            a.component_name = s.name
            a.config_ready = s.config_ready
            a.recovery_action = core.RecoveryType(s.recovery_action).name
            a.data_storetype = core.DataStore(s.data_storetype).name
            if hasattr(s, 'mode_active'):
                a.mode_active = s.mode_active
            # instance id added for only compoents with config_ready, False
            if s.instance_id is not None and s.config_ready:
                a.instance_id = s.instance_id
            s._add_to_inventory(parent, manifest, known)

            # Python variables for some tasklets are set after the call
            # to _add_to_inventory. Must call _add_to_inventory before this.
            for v in s.python_vars:
                a.python_variable.append(v)


class RaVm(RaManifestObject):
    def __init__(self,
            name="rwvm",
            subcomponents=None,
            ipaddress=None,
            leader=False,
            valgrind=None,
            config_ready=False,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            ):

        super(RaVm,self).__init__(name, subcomponents=subcomponents,
                                  config_ready=config_ready,
                                  recovery_action=recovery_action,
                                  start=start,
                                  data_storetype=data_storetype,
                                  )
        self.ipaddress = ipaddress
        self.leader = leader
        self.valgrind = valgrind
        self.data_storetype=data_storetype

    def _prepare(self):
        super(RaVm, self)._prepare()
        if self.ipaddress is not None:
            self.python_vars.append("vm_ip_address = '%s'" % self.ipaddress)

    def _set_ips(self, ips):
        '''
        pop IPs off the list as needed
        and then return the remainders

        since RaManifest.set_ips calculates and requests the exact number
        of VMs needed, then this should never fail
        '''
        if self.ipaddress is None:
            if len(ips) == 0:
                raise Exception("Out of IP ADDRESSES")
            self.ipaddress = ips[0]
            ips = ips[1:]
        logger.debug("{} is using {}".format(self.name, self.ipaddress))
        return super(RaVm,self)._set_ips(ips)

    def _add_to_inventory(self, parent, manifest, known):
        '''
        internal function used by manifest.as_pb()
        @type m: RaManifest
        '''
        if self.name in known:
            return

        known.add(self.name)

        c = parent.add()
        c.component_name = self.name
        c.component_type = "RWVM"
        if manifest.use_pool and self.ipaddress is None:
            c.rwvm.pool_name = manifest.pool_name

        if self.leader:
            c.rwvm.leader = self.leader

        if self.valgrind:
            c.rwvm.valgrind.enable = True
            log_dir = os.environ['RIFT_ARTIFACTS']
            log_file = 'valgrind_{}.{}.out'.format(c.component_name, str(uuid.uuid4()))
            c.rwvm.valgrind.opts.append('--log-file={}/{}'.format(log_dir, log_file))
            c.rwvm.valgrind.opts.append('--leak-check=full')

        event = c.rwvm.event_list.event.add()
        event.name = 'onentry'

        for component in self.startups:
            action = event.action.add()
            action.name = "Start the {}".format(component.name)
            action.start.component_name = component.name
            action.start.config_ready = component.config_ready
            action.start.recovery_action = core.RecoveryType(component.recovery_action).name
            action.start.data_storetype = core.DataStore(component.data_storetype).name
            if hasattr(component, 'mode_active'):
                action.start.mode_active = component.mode_active
            if hasattr(component, 'ipaddress'):
                action.start.ip_address = component.ipaddress
            action.start.python_variable.extend(component.python_vars)

            if component.instance_id is not None and component.config_ready:
                 action.start.instance_id = str(component.instance_id)

        for component in self.subcomponents:
            component._add_to_inventory(parent, manifest, known)


class RaCollection(RaManifestObject):
    """
    This class represents a generic riftware collection.
    """

    def __init__(self, name, tag, subcomponents=None, instance_id=None,
                 config_ready=False, recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, data_storetype=core.DataStore.NOSTORE.value,
                ):
        """Creates an RaCollection object

        Arguments:
            name          - the name of the component instance
            tag           - the component type name as it is represented in the
                            actual manifest and protobuf objects
            subcomponents - (optional) a list of components contained within
                            the collection
            instance_id   - (optional) an identifier associated with this
                            particular collection
            config_ready  - (optional) config readiness check enable
            recovery_action  - (optional) recovery action mode
            start         - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA

        """
        super(RaCollection, self).__init__(
                name,
                subcomponents=subcomponents,
                instance_id=instance_id,
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                )
        self.manifest_tag = tag

    def _add_to_inventory(self, parent, manifest, known):
        """Add this collection to a parent component

        Arguments:
            parent   - the component that will become the parent of this
                       collection
            manifest - an RaManifest object that this collection will be a part
                       of.

        """
        if self.name in known:
            return

        known.add(self.name)

        component = parent.add()
        component.component_name = self.name
        component.component_type = "RWCOLLECTION"

        node = component.rwcollection
        node.collection_type = self.manifest_tag
        event = node.event_list.event.add()
        event.name = 'onentry'

        # A system is assumed to be a Multi-VM VNF if any of its cluster's
        # config_ready attribute is False

        for subcomponent in self.subcomponents:

            # Do not add action element to components with manifest tag 'rwclony'
            # if the element is a cluster or with config_ready, False
            if not subcomponent.start:
                continue

            action = event.action.add()
            action.name = "Start %s for %s" % (subcomponent.name, self.name)
            action.start.component_name = subcomponent.name
            action.start.python_variable.extend(subcomponent.python_vars)
            action.start.config_ready = subcomponent.config_ready
            action.start.recovery_action = core.RecoveryType(subcomponent.recovery_action).name
            action.start.data_storetype = core.DataStore(subcomponent.data_storetype).name
            if hasattr(subcomponent, 'mode_active'):
                action.start.mode_active = subcomponent.mode_active
            if hasattr(subcomponent, 'ipaddress'):
                action.start.ip_address = subcomponent.ipaddress
            if subcomponent.instance_id is not None and subcomponent.config_ready:
               action.start.instance_id = str(subcomponent.instance_id)

        for subcomponent in self.subcomponents:
            subcomponent._add_to_inventory(parent, manifest, known)


class RaCluster(RaCollection):
    """
    This class is a collection that represents a cluster.

    The notion of a cluster is a legacy concept so please do not extend this
    class. All functionality should stay in the generic RaCollection class.
    """

    def __init__(self,
            name="rwcluster",
            subcomponents=None,
            instance_id=None,
            config_ready=False,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            ):
        super(RaCluster,self).__init__(
                name,
                tag="rwcluster",
                subcomponents=subcomponents,
                instance_id=instance_id,
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                )


class RaColony(RaCollection):
    """
    This class is a collection that represents a colony.

    The notion of a colony is a legacy concept so please do not extend this
    class. All functionality should stay in the generic RaCollection class.
    """

    def __init__(self,
            name="rwcolony",
            subcomponents=None,
            instance_id=None,
            config_ready=False,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            ):
        super(RaColony,self).__init__(
                name,
                tag="rwcolony",
                subcomponents=subcomponents,
                instance_id=instance_id,
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                )


class RaManifest(object):
    def __init__(self, northbound_listing, persist_dir_name,
                 netconf_trace=None,
                 zookeeper=None,
                 test_name=None,
                 agent_mode="AUTOMODE",
                 rift_var_root=None,
                 ):
        self.components = []
        self.bootstrap = RaRwInit()
        self.init = None
        self.rwtraceEnable=True
        self.traceLevel = 5
        self.bootstrapLogPeriod = 30
        self.bootstrapLogSeverity = 6      # max 7 (debug)
        self.consoleLogSeverity = 3
        self.zookeeper = zookeeper
        self.collapse_vm = True
        self.collapse_proc = True
        self.ipaddresses = []
        self.use_pool = True
        self.pool_name = "vmpool"
        self.broker_vm = None
        self.dtsrouter_vm = None
        self.serf = None
        self.multi_broker = False
        self.multi_dtsrouter = False
        self.vm_bootstrap = []
        self.proc_bootstrap = []
        self.northbound_listing = northbound_listing
        self.agent_mode = agent_mode
        self.persist_dir_name = persist_dir_name
        self.test_name = test_name
        self.rift_var_root = rift_var_root

        if netconf_trace is None:
            self.netconf_trace = "AUTO"
        else:
            self.netconf_trace = netconf_trace.value_nick.upper()

    def __iter__(self):
        """A depth-first generator over the manifest components"""
        for component in self.components:
            for subcomponent in component.descendents():
                yield subcomponent

    def set_ips(self, username=None):
        '''
        retrieve the VMs assigned to the current user (or the specified user)
        if username is None then the logged in user will be used
        '''
        num_vms = len(self.list_by_class(RaVm))
        self.ipaddresses = ndl.get_vms(user=username,count=num_vms)
        logger.debug("got {} ips from NDL".format(len(self.ipaddresses)))

    def _set_ips(self):
        ''' set the ips for all components in the tree'''
        if not self.use_pool:
            ips = self.ipaddresses
            for component in self.components:
                ips = component._set_ips(ips)

    def add_component(self, component):
        '''
        add a component to the manifest
        '''
        logger.debug("adding component %s" % (component.name,))

        self.components.append(component)
        component.parent = self

    def remove_component(self, target):
        """Remove the specified compoment from the manifest

        Arguments:
            target - the compoment to remove

        """
        # Search subcomponents for the target component and remove it if found.
        for component in self:
            try:
                component.subcomponents.remove(target)
                break
            except (ValueError, AttributeError):
                pass

        # Search the startups for the target component and remove it if found.
        for component in self:
            try:
                component.startups.remove(target)
                break
            except (ValueError, AttributeError):
                pass


    def remove_empty_procs(self):
        """Iterates over the components are removes any procs that are empty"""
        # Construct a list of procs without any subcomponents
        empty = []
        for component in self:
            if isinstance(component, RaProc) and not component.subcomponents:
                empty.append(component)

        # This is not efficient, but empty procs should be rare
        for component in empty:
            self.remove_component(component)
            logger.warning('removing an empty proc: {}'.format(component))

    def assign_leading_vm(self):
        """Set the leading VM for each colony

        Every colony is required to have a leading VM. This function iterates
        over all of the components looking for VMs containing CLI tasklets.
        These VMs are going to be the lead VMs for the colony that contains
        them because there is only ever one CLI tasklet in a colony.

        """

        def reparent(child, new_parent):
            for component in self:
                if child in component.subcomponents:
                    # Remove the child from the original parent and append it
                    # to the new parent.
                    component.subcomponents.remove(child)
                    new_parent.subcomponents.append(child)

                    # If the child is also a startup item, make sure that it is
                    # removed from the original parent and added as a startup
                    # item in the new parent.
                    if child in component.startups:
                        component.startups.remove(child)
                        new_parent.startups.add(child)

                    break
            else:
                msg = 'unable to find parent of {}'.format(child.name)
                raise ValueError(msg)


        # Iterate until a CLI is found
        components = iter(self)
        for component in components:
            if isinstance(component, RaCliProc):
                break
        else:
            logger.warning('no CLI found to act as a lead')

        # The next VM encountered will be the VM that contains the CLI
        for component in components:
            if isinstance(component, RaVm):
                leading_vm = component
                break

        # The next colony encounter will contain the leading VM
        for component in components:
            if isinstance(component, RaColony):
                leading_vm.leader = True
                reparent(leading_vm, component)

        # Finally, find any clusters/colonies that do not have a leader and inject one.
        for component in self:
            if isinstance(component, RaColony) or isinstance(component, RaCluster):
                for child in component.subcomponents:
                    if hasattr(child, 'leader') and child.leader:
                        break
                else:
                    for child in component.subcomponents:
                        if isinstance(child, RaVm):
                            child.leader = True
                            break
                    else:
                        if len(component.subcomponents) > 0:
                            leader = RaVm(name='%s-leader' % (component.name), leader=True)
                            component.subcomponents.insert(0, leader)
                            leader.parent = component
                        else:
                            # If a colony/cluster has no sub-components then
                            # remove it. This can happen when we apply
                            # 'reparent' on a colony/cluster which contains
                            # only the master VM as it's subcomponent.
                            parent = component.parent
                            parent.subcomponents.remove(component)


    def as_xml(self, extension):
        '''
        render the manifest as an XML string
        '''
        libncx_model = RwYang.model_create_libncx()
        libncx_model.load_schema_ypbc(RwManifestYang.get_schema())
        xml = self.as_pb().to_xml_v2(libncx_model)
        xml = xml.replace('rw-manifest:', '')
        xml = xml.replace(
                '<manifest xmlns:rw-manifest="http://riftio.com/ns/riftware-1.0/rw-manifest">',
                '<manifest xmlns="http://riftio.com/ns/riftware-1.0/rw-manifest" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://riftio.com/ns/riftware-1.0/rw-manifest ./rw-manifest.xsd">'
                )

        return xml

    def _prepare(self):
        self.assign_leading_vm()
        self._set_ips()

        for component in self.components:
            component._prepare()

        # Remove any empty procs from the manifest.
        self.remove_empty_procs()

    def as_pb(self):
        '''
        render the manifest as a protobuf
        '''
        self._prepare()

        convert = ProtobufManifestConverter()
        return convert.manifest_to_protobuf(self)

    def list_by_class(self, component_class):
        """Return a list of component with the specified class

        Arguments:
            component_class - the class of components to select

        Returns:
            A list of components

        """
        return [c for c in self if isinstance(c, component_class)]

    def find_by_class(self, component_class):
        """Returns a component of the specified class

        This function iterates through the components in the manifest and
        returns the first instance that is of the specified class.

        Arguments:
            component_class - the class of the component to find

        Returns:
            A component of the specified class or None if no instance of the
            class can be found.

        """
        for component in self:
            if isinstance(component, component_class):
                return component

        return None

    def find_ancestor_by_class(self, cls, child):
        """Return the childs ancestor specified by class

        This function iterates through the components in the manifest and
        returns the first instance that is of the specified class that contains
        the child component.

        Arguments:
            cls   - the class type of the ancestor to find
            child - a component

        Raises:
            A ManifestError will be raised if the child component cannot be
            found.

        Returns:
            A component of the ancestor class will be return or None if no such
            component exists.

        """
        components = iter(self)

        # Iterate through the components until the child is found
        for component in components:
            if component is child:
                break
        else:
            raise ManifestError('unable to find target component')

        # Continue iterating until a component of the ancestors class is found
        for component in components:
            if isinstance(component, cls):
                return component

        return None

    def find_enclosing_cluster(self, target):
        """Returns the cluster that contains the target component

        Arguments:
            target  - a component of the manifest

        Raises:
            A ManifestError is raised if the target cannot be found in the
            manifest.

        Returns:
            A cluster component is return or None if an enclosing cluster
            cannot be found.

        """
        return self.find_ancestor_by_class(RaCluster, target)

    def find_enclosing_colony(self, target):
        """Returns the colony that contains the target component

        Arguments:
            target  - a component of the manifest

        Raises:
            A ManifestError is raised if the target cannot be found in the
            manifest.

        Returns:
            A colony component is return or None if an enclosing colony cannot
            be found.

        """
        return self.find_ancestor_by_class(RaColony, target)


class ProtobufManifestConverter(object):
    """
    This class is used to create a protobuf representation of an RaManifest
    object. Importantly, the conversion does not modify the RaManifest object,
    the object is purely a source of information that is used by the converter
    to create the corresponding protobuf object.
    """

    def manifest_to_protobuf(self, manifest):
        """Returns a protobuf representation of the manifest

        Arguments:
            manifest - an RaManifest object

        Returns:
            a protobuf manifest object

        """
        pb_manifest = RwManifestYang.Manifest()

        self.create_bootstrap_section(pb_manifest, manifest)
        self.create_init_section(pb_manifest, manifest)
        self.create_inventory_section(pb_manifest, manifest)

        return pb_manifest

    def create_bootstrap_section(self, pb_manifest, manifest):
        """Creates the bootstrap section of the manifest

        Arguments:
            pb_manifest - a protobuf manifest object
            manifest    - an RaManifest object

        """
        bootstrap_phase = pb_manifest.bootstrap_phase

        # Define the bootstrap tasklet
        rwtasklet = bootstrap_phase.rwtasklet
        rwtasklet.plugin_name = manifest.bootstrap.plugin_name

        # Set rwtrace options
        rwtrace = bootstrap_phase.rwtrace
        rwtrace.enable =  manifest.rwtraceEnable
        rwtrace.level = manifest.traceLevel

        # Set filter as present
        rwtrace.filter

        #Set rwlog boostrap options
        rwlog = bootstrap_phase.log
        rwlog.enable = True
        rwlog.bootstrap_time = manifest.bootstrapLogPeriod
        rwlog.severity = manifest.bootstrapLogSeverity
        rwlog.console_severity = manifest.consoleLogSeverity

        # Add northbound schema listing to the bootstrap phase
        if manifest.northbound_listing is not None:
            bootstrap_phase.rwmgmt.northbound_listing.append(manifest.northbound_listing)

        # Add agent mode to the bootstrap phase
        if manifest.agent_mode in ["CONFD", "RWXML"]:
            bootstrap_phase.rwmgmt.agent_mode = manifest.agent_mode
        else:
            bootstrap_phase.rwmgmt.agent_mode = rift.vcs.mgmt.default_agent_mode()

        if manifest.agent_mode == "CONFD":
           bootstrap_phase.rwmgmt.northbound_listing.append("confd_nb_schema_list.txt")

        if not manifest.persist_dir_name.startswith("persist."):
           bootstrap_phase.rwmgmt.persist_dir_name = "persist." + manifest.persist_dir_name
        else:
           bootstrap_phase.rwmgmt.persist_dir_name = manifest.persist_dir_name

        if manifest.test_name is not None:
           bootstrap_phase.test_name = manifest.test_name
        else:
           bootstrap_phase.test_name = str(uuid.uuid4())

        if manifest.rift_var_root is not None:
           bootstrap_phase.rift_var_root = manifest.rift_var_root

        # Add zookeeper info to the bootstrap phase
        if manifest.zookeeper is not None:
            zookeeper = bootstrap_phase.zookeeper
            zookeeper.unique_ports = manifest.zookeeper.unique_ports
            zookeeper.master_ip = manifest.zookeeper.master_ip
            zookeeper.zake = manifest.zookeeper.zake

        # Add serf info into bootstrap phase
        bootstrap_phase.serf.start = False

        # Add initial ssl certs to bootstraph phase
        try:
            use_ssl, cert, key = certs.get_bootstrap_cert_and_key()
        except certs.BootstrapSslMissingException:
            logger.error('No bootstrap certificates found')
            bootstrap_phase.rwsecurity.use_ssl = False
        else:
            bootstrap_phase.rwsecurity.use_ssl = use_ssl
            bootstrap_phase.rwsecurity.cert = cert
            bootstrap_phase.rwsecurity.key = key

        for ip_addr in manifest.mgmt_ip_list:
            mgmt_ip = bootstrap_phase.ip_addrs_list.add()
            mgmt_ip.ip_addr = ip_addr

        for component in manifest.vm_bootstrap:
            c = bootstrap_phase.rwvm.instances.add()
            c.component_name = component.name
            c.config_ready = True
            c.recovery_action = "FAILCRITICAL"
            c.data_storetype = "NOSTORE"

        for component in manifest.proc_bootstrap:
            c = bootstrap_phase.rwproc.instances.add()
            c.component_name = component.name
            c.config_ready = True
            c.recovery_action = "FAILCRITICAL"
            c.data_storetype = "NOSTORE"

    def create_init_section(self, pb_manifest, manifest):
        """Creates the init section of the manifest

        Arguments:
            pb_manifest - a protobuf manifest object
            manifest    - an RaManifest object

        """
        init_phase = pb_manifest.init_phase

        # Initialize the environment section
        environment = init_phase.environment

        if isinstance(manifest.init, RaVm):
            component_type = "rwvm"
        elif isinstance(manifest.init, RaProc):
            component_type = "rwproc"
        else:
            msg = 'Init object must be either RaVm or RaProc, not {}'
            raise ManifestError(msg.format(type(manifest.init)))

        environment.python_variable.extend(manifest.init.python_vars)
        environment.python_variable.extend([
            "rw_component_name = '{}'".format(manifest.init.name),
            "instance_id = {}".format(manifest.init.instance_id),
            "component_type = '{}'".format(component_type),
            ])

        environment.component_name = '$python(rw_component_name)'
        environment.component_type = '$python(rw_component_type)'
        environment.instance_id = '$python(instance_id)'

        if isinstance(manifest.a_vm, RaVm):
            component_type = "rwvm"
        else:
            msg = 'Active VM object must be RaVm , not {}'
            raise ManifestError(msg.format(type(manifest.a_vm)))

        environment.active_component.python_variable.extend([
            "rw_component_name = '{}'".format(manifest.a_vm.name),
            "component_type = '{}'".format(component_type),
            "instance_id = {}".format(manifest.a_vm.instance_id),
            "parent_id = '{}'".format(manifest.a_vm.parent),
            ])

        environment.active_component.component_name = '$python(rw_component_name)'
        environment.active_component.component_type = '$python(component_type)'
        environment.active_component.instance_id = '$python(instance_id)'
        environment.active_component.parent_id = '$python(parent_id)'

        if manifest.s_vm:
            if isinstance(manifest.s_vm, RaVm):
                component_type = "rwvm"
            else:
                msg = 'Standby VM object must be RaVm , not {}'
                raise ManifestError(msg.format(type(manifest.a_vm)))

            environment.standby_component.python_variable.extend([
                "rw_component_name = '{}'".format(manifest.s_vm.name),
                "component_type = '{}'".format(component_type),
                "instance_id = 0",
                "parent_id = '{}'".format(manifest.s_vm.parent),
                ])

            environment.standby_component.component_name = '$python(rw_component_name)'
            environment.standby_component.component_type = '$python(component_type)'
            environment.standby_component.instance_id = '$python(instance_id)'
            environment.standby_component.parent_id = '$python(parent_id)'

        # Initialize the settings section
        settings = init_phase.settings

        # Initialize broker support
        rwmsg = settings.rwmsg
        brokers = manifest.list_by_class(RaBroker)
        if manifest.multi_broker:
            rwmsg.multi_broker.enable = True
            for broker in brokers:
                manifest.remove_component(broker)
        else:
            if len(brokers) == 0:
                rwmsg.single_broker.enable_broker = False
                rwmsg.single_broker.broker_port = 0
                rwmsg.single_broker.broker_host = "NULL"
            else:
                broker = brokers[0]
                rwmsg.single_broker.enable_broker = True
                rwmsg.single_broker.broker_port = broker.port
                if manifest.collapse_vm:
                    rwmsg.single_broker.broker_host = "127.0.0.1"
                else:
                    rwmsg.single_broker.broker_host = manifest.broker_vm.ipaddress

        # Initialize dtsrouter support
        rwdtsrouter = settings.rwdtsrouter
        dtsrouters = manifest.list_by_class(RaDtsRouter)
        if manifest.multi_dtsrouter:
            rwdtsrouter.multi_dtsrouter.enable = True
            for dtsrouter in dtsrouters:
                manifest.remove_component(dtsrouter)
        else:
            if len(dtsrouters) == 0:
                rwdtsrouter.single_dtsrouter.enable = False
            else:
                dtsrouter = dtsrouters[0]
                rwdtsrouter.single_dtsrouter.enable = True

        # Add rwtrace field to init section and set in parent
        rwtrace = settings.rwtrace

        # Set the collapsed fields
        rwvcs = settings.rwvcs
        rwvcs.collapse_each_rwvm = manifest.collapse_vm
        rwvcs.collapse_each_rwprocess = manifest.collapse_proc

        # Set VMs in pool
        if manifest.ipaddresses and manifest.use_pool:
            pool = init_phase.rwcal.rwvm_pool.add()
            pool.name = manifest.pool_name
            for ip in manifest.ipaddresses:
                vm = pool.static_vm_list.add()
                vm.vm_name = "vm_{}".format(ip.replace(".", "_"))
                vm.vm_ip_address = ip

        # Set the netconf trace
        if settings.mgmt_agent is not None:
            settings.mgmt_agent.netconf_trace = manifest.netconf_trace

    def create_inventory_section(self, pb_manifest, manifest):
        """Creates the inventory section of the manifest

        Arguments:
            pb_manifest - a protobuf manifest object
            manifest    - an RaManifest object

        """
        inventory = pb_manifest.inventory

        known = set()
        for component in manifest.components:
            component._add_to_inventory(inventory.component, manifest, known)
            logger.debug("added component to inventory {}".format(component.name))

        for component in manifest.vm_bootstrap:
            component._add_to_inventory(inventory.component, manifest, known)
            logger.debug("added component to inventory {}".format(component.name))

        for component in manifest.proc_bootstrap:
            component._add_to_inventory(inventory.component, manifest, known)
            logger.debug("added component to inventory {}".format(component.name))


# vim: sw=4 et
