
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

import os
import random

from . import core
from .ext import ClassProperty
from .fastpath import Fastpath


class Controller(core.Tasklet):
    """
    This class represents a fastpath controller tasklet
    """

    def __init__(self, name="RW.FpCtrl", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a Controller object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action  - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(Controller, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                         recovery_action=recovery_action,
                                         start=start, 
                                         data_storetype=data_storetype,
                                         mode_active=mode_active,
                                         )

    plugin_name = ClassProperty("rwfpctrl-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwfpctrl-c")


class CalProxy(core.Tasklet):
    def __init__(self, name="RW.CalProxy", config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(CalProxy, self).__init__(name=name, config_ready=config_ready,
                                       recovery_action=recovery_action,
                                       start=start, 
                                       data_storetype=data_storetype,
                                       mode_active=mode_active,
                                       )

    plugin_name = "rwcalproxytasklet"
    plugin_directory = "./usr/lib/rift/plugins/rwcalproxytasklet"


class NNLatencyTasklet(core.Tasklet):
    """
    This class represents a fastpath Noisy Neighbor (NN) Latency tasklet
    """

    def __init__(self, name="RW.NNLatencyTasklet", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True, 
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a NN Latency object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(NNLatencyTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )

    plugin_name = ClassProperty("rwnnlatencytasklet")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwnnlatencytasklet")


class SfMgr(core.Tasklet):
    """
    This class represents a Service Function manager tasklet
    """

    def __init__(self, name="RW.SfMgr", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a SF Mgr object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(SfMgr, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                    recovery_action=recovery_action,
                                    start=start,
                                    data_storetype=data_storetype,
                                    mode_active=mode_active,
                                    )

    plugin_name = ClassProperty("rwsfmgr")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/sfmgr")

class SffMgr(core.Tasklet):
    """
    This class represents a Service Function Forwardermanager tasklet
    """

    def __init__(self, name="RW.SffMgr", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a SFF Mgr object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(SffMgr, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                     recovery_action=recovery_action,
                                     start=start,
                                     data_storetype=data_storetype,
                                     mode_active=mode_active,
                                     )

    plugin_name = ClassProperty("rwsffmgr")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/sffmgr")

class NetworkContextManager(core.Tasklet):
    """
    This class represents a network context manager tasklet.
    """

    def __init__(self, name="RW.NcMgr", uid=None, fp_uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a NetworkContextManager object.

        Arguments:
            name   - the name of the tasklet
            uid    - a unique identifier
            fp_uid - the instance ID of the fastpath tasklet
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(NetworkContextManager, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                    recovery_action=recovery_action,
                                                    start=start,
                                                    data_storetype=data_storetype,
                                                    mode_active=mode_active,
                                                    )
        self.fp_uid = fp_uid

    plugin_name = ClassProperty("rwncmgr-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwncmgr-c")


class InterfaceManager(core.Tasklet):
    """
    This class represents an interface manager tasklet.
    """

    def __init__(self, name="RW.IfMgr", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.RESTART.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates an InterfaceManager object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(InterfaceManager, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )

    plugin_name = ClassProperty("rwifmgr-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwifmgr-c")


class ApplicationManager(core.Tasklet):
    """
    This class represents an application manager tasklet.
    """

    def __init__(self, name="RW.AppMgr", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates an ApplicationManager object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(ApplicationManager, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                 recovery_action=recovery_action,
                                                 start=start,
                                                 data_storetype=data_storetype,
                                                 mode_active=mode_active,
                                                 )

    plugin_name = ClassProperty("rwappmgr-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwappmgr-c")


class CliTasklet(core.Tasklet):
    """
    This class represents a CLI tasklet.
    """

    def __init__(self, name="RW.Cli", uid=None, manifest_file="cli_rwfpath.xml", config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a CliTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(CliTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                         recovery_action=recovery_action,
                                         start=start,
                                         data_storetype=data_storetype,
                                         mode_active=mode_active,
                                         )
        self.manifest_file = manifest_file

    plugin_name = ClassProperty('rwcli-c')
    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwcli-c')

class MockCliTasklet(CliTasklet):
    """
    This class represents a Mock CLI tasklet.
    """

    def __init__(self, name="RW.MockCli", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a MockCliTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(MockCliTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                             recovery_action=recovery_action,
                                             start=start,
                                             data_storetype=data_storetype,
                                             mode_active=mode_active,
                                             )

    plugin_name = ClassProperty("pytoytasklet")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/pytoytasklet")


class uAgentTasklet(core.Tasklet):
    """
    This class represents a uAgent tasklet.
    """

    def __init__(self,
            name="RW.uAgent",
            uid=None,
            port=None,
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a uAgentTasklet object.

        Arguments:
            name   - the name of the tasklet
            uid    - a unique identifier
            port   - the port that the tasklet uses for communication
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(uAgentTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                            recovery_action=recovery_action,
                                            start=start,
                                            data_storetype=data_storetype,
                                            mode_active=mode_active,
                                            )
        self.port = str(os.getuid()) if port is None else port
        self.port = os.environ.get("_UAGENT_PORT", self.port)

    plugin_name = ClassProperty("rwuagent-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwuagent-c")

class MgmtMockTasklet(core.Tasklet):
    """
    This class represents a mgmtmock tasklet.
    """

    def __init__(self, name="RW.MgmtMock", uid=None, port=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a MgmtMock object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            port - the port that the tasklet uses for communication
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting 

        """
        super(MgmtMockTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                              recovery_action=recovery_action,
                                              start=start,
                                              data_storetype=data_storetype,
                                              mode_active=mode_active,
                                              )
        self.port = str(os.getuid()) if port is None else port
        self.port = os.environ.get("_UAGENT_PORT", self.port)

    plugin_name = ClassProperty("rwmgmtmock-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwmgmtmock-c")

class RestconfTasklet(core.Tasklet):
    """
    This class represents a Restconf tasklet.
    """

    def __init__(self,
            uid=None,
            name="RW.Restconf",
            confd_host="127.0.0.1",
            confd_port="2022",
            rest_port="8888",
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a Rift Restconf Server object.

        Arguments:
            uid        - a unique identifier
            name       - the name of the process
            confd_host - the host that confd is on
            confd_port - the port that confd is communicating on
            rest_port  - the port that the restconf server listens on
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(RestconfTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                              recovery_action=recovery_action,
                                              start=start,
                                              data_storetype=data_storetype,
                                              mode_active=mode_active,
                                              )
        self.confd_host = confd_host
        self.confd_port = confd_port
        self.rest_port = rest_port

    plugin_name = ClassProperty("restconf")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/restconf")


class RestStreamTasklet(core.Tasklet):
    """
    This class represents a RestStream tasklet.
    """

    def __init__(self,
            uid=None,
            name="RW.RestStream",
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a Rift RestStream Server object.

        Arguments:
            uid    - a unique identifier
            name   - the name of the process
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(RestStreamTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                recovery_action=recovery_action,
                                                start=start,
                                                data_storetype=data_storetype,
                                                mode_active=mode_active,
                                                )

    plugin_name = ClassProperty("reststream")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/reststream")

class RestPortForwardTasklet(core.Tasklet):
    """
    This class represents a RestStream tasklet.
    """

    def __init__(self,
            uid=None,
            name="RW.RestPortForward",
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a Rift RestStream Server object.

        Arguments:
            uid    - a unique identifier
            name   - the name of the process
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(RestPortForwardTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                     recovery_action=recovery_action,
                                                     start=start,
                                                     data_storetype=data_storetype,
                                                     mode_active=mode_active,
                                                     )

    plugin_name = ClassProperty("restportforward")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/restportforward")

class ToyTasklet(core.Tasklet):
    """
    This class represents a toy tasklet.
    """

    def __init__(self, name="RW.toytasklet", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a ToyTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action  - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(ToyTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                         recovery_action=recovery_action,
                                         start=start,
                                         data_storetype=data_storetype,
                                         mode_active=mode_active,
                                         )

    plugin_name = ClassProperty("rwtoytasklet-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwtoytasklet-c")


class ToyTaskletPython(core.Tasklet):
    """
    This class represents a python toy tasklet.
    """

    def __init__(self, name="RW.toytasklet.python", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a Python ToyTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action  - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(ToyTaskletPython, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )

    plugin_name = ClassProperty("pytoytasklet")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/pytoytasklet")


class DtsTaskletPython(core.Tasklet):
    """
    This class represents a python DTS toy tasklet.
    """

    def __init__(self, name="RW.dtstasklet.python", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a Python DTS ToyTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action  - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(DtsTaskletPython, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )

    plugin_name = ClassProperty("rwdtstasklet")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwdtstasklet")


class MsgBrokerTasklet(core.Tasklet):
    """
    This class represents a message broker tasklet.
    """

    def __init__(self, name="RW.Msgbroker", port=None, uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a MsgBrokerTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(MsgBrokerTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )
        self.port = random.randint(20000, 30000) if port is None else port

    plugin_name = ClassProperty("rwmsgbroker-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwmsgbroker-c")


class DtsRouterTasklet(core.Tasklet):
    """
    This class represents a DTS router tasklet.
    """

    def __init__(self, name="RW.DTS.Router", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a DtsRouterTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(DtsRouterTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )

    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwdtsrouter-c")
    plugin_name = ClassProperty("rwdtsrouter-c" )


class LogdTasklet(core.Tasklet):
    """
    This class represents a Logd tasklet.
    """

    def __init__(self, name='rwlog', uid=None, schema="rw-mgmt", config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a LogdTasklet object.

        Arguments:
            name   - the name of the tasklet
            uid    - a unique identifier
            schema - the name of the schema that the tasklet should use
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(LogdTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                          recovery_action=recovery_action,
                                          start=start,
                                          data_storetype=data_storetype,
                                          mode_active=mode_active,
                                          )
        self.schema = schema

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwlogd-c')
    plugin_name = ClassProperty('rwlogd-c')


class DtsPerfTasklet(core.Tasklet):
    """
    This class represents a DTS router peformance test tasklet.
    """

    def __init__(self, name="RW.DTSPerf", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a DtsPerfTasklet object.

        Arguments:
            name - the name of the tasklet
            uid  - a unique identifier
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting

        """
        super(DtsPerfTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                             recovery_action=recovery_action,
                                             start=start,
                                             data_storetype=data_storetype,
                                             mode_active=mode_active,
                                             )

    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwdtsperf-c")
    plugin_name = ClassProperty("rwdtsperf-c" )


class DtsPerfMgrTasklet(core.Tasklet):
    """This class represents a DTS router performance manger tasklet."""

    def __init__(self, name="RW.DTSPerfMgrTasklet", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a DtsPerfMgrTasklet object.

        Arguments:
            name    - the name of the tasklet
            uid     - a unique identifier
            config_ready -  config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting 
        """
        super(DtsPerfMgrTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                recovery_action=recovery_action,
                                                start=start,
                                                data_storetype=data_storetype,
                                                mode_active=mode_active,
                                                )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwdtsperfmgrtasklet')
    plugin_name = ClassProperty('rwdtsperfmgrtasklet')


class DtsMockServerTasklet(core.Tasklet):
    """This class represents a DTS mock server tasklet."""

    def __init__(self, name="RW.DTSMockServerTasklet", uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a DtsMockServerTasklet object.

        Arguments:
            name    - the name of the tasklet
            uid     - a unique identifier
            config_ready -   config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(DtsMockServerTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                   recovery_action=recovery_action,
                                                   start=start,
                                                   data_storetype=data_storetype,
                                                   mode_active=mode_active,
                                                   )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwdtsmockservertasklet')
    plugin_name = ClassProperty('rwdtsmockservertasklet')


class VnfMgrTasklet(core.Tasklet):
    """
    This class represents a VnfMgr tasklet.
    """

    def __init__(self, name='vnfmgr', uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """
        Creates a VnfMgrTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(VnfMgrTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                            recovery_action=recovery_action,
                                            start=start,
                                            data_storetype=data_storetype,
                                            mode_active=mode_active,
                                            )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/vnfmgr')
    plugin_name = ClassProperty('vnfmgr')

class Launchpad(core.Tasklet):
    """
    This class represents a launchpad tasklet.
    """

    def __init__(self, name='launchpad', uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """
        Creates a Launchpad object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(Launchpad, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                        recovery_action=recovery_action,
                                        start=start,
                                        data_storetype=data_storetype,
                                        mode_active=mode_active,
                                        )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwlaunchpad')
    plugin_name = ClassProperty('rwlaunchpad')


class MissionControl(core.Tasklet):
    """
    This class represents a mission control tasklet.
    """

    def __init__(self, name='mission-control', uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """
        Creates a MissionControl object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(MissionControl, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                             recovery_action=recovery_action,
                                             start=start,
                                             data_storetype=data_storetype,
                                             mode_active=mode_active,
                                             )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwmctasklet')
    plugin_name = ClassProperty('rwmctasklet')


class MissionControlCTasklet(core.Tasklet):
    """
    This class represents a mission control tasklet.
    """

    def __init__(self, name='mission-control-ctasklet', uid=None, config_ready=True,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """
        Creates a MissionControlCTasklet object.

        Arguments:
            name  - the name of the tasklet
            uid   - a unique identifier
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        super(MissionControlCTasklet, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                                     recovery_action=recovery_action,
                                                     start=start,
                                                     data_storetype=data_storetype,
                                                     mode_active=mode_active,
                                                     )

    plugin_directory = ClassProperty('./usr/lib/rift/plugins/rwmctasklet-c')
    plugin_name = ClassProperty('rwmctasklet-c')


class ContainerManager(core.Tasklet):
    def __init__(self, name="RW.CntMgr", config_ready=False,
                 recovery_action=core.RecoveryType.FAILCRITICAL.value,
                 start=True,
                 data_storetype=core.DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        super(ContainerManager, self).__init__(name=name, config_ready=config_ready,
                                               recovery_action=recovery_action,
                                               start=start,
                                               data_storetype=data_storetype,
                                               mode_active=mode_active,
                                               )

    plugin_name = "rwcntmgrtasklet"
    plugin_directory = "./usr/lib/rift/plugins/rwcntmgrtasklet"

# vim: sw=4
