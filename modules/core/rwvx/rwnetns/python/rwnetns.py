
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
import logging

import rwlogger

import gi
gi.require_version('RwNetns', '1.0')
gi.require_version('RwNetnsLogYang', '1.0')

from gi.repository import RwNetns
import six

# Usually, just importing RwNetnsLogYang would be sufficient, but since
# some of these classes are only used in error paths, I want to
# make sure all names are resolved up front.
from gi.repository.RwNetnsLogYang import (
        EnteringNetns,
        ExitingNetns,
        MissingResolvConf,
        MountingResolvConf,
        MountingResolvConfFailed,
        UnmountingResolvConf,
        UnmountingResolvConfFailed,
        MultipleNetnsError,
        ChangeNetnsFailed,
        ChangeNetnsFdFailed,
        GetNetnsFdFailed
        )

rwlogger = rwlogger.RwLogger(subcategory="rw-netns")   # need better category?
logger = logging.getLogger(__name__)
logger.addHandler(rwlogger)

class NetworkNamespaceException(Exception):
    pass


class NetnsEnvironment(object):
    current_netns = None

    def __init__(self, netns_name, bind_resolv_conf=True):
        if not isinstance(netns_name, six.string_types):
            raise TypeError("netns is expected to be a string: %s", netns_name)

        self._netns_name = netns_name
        self._current_fd = RwNetns.get_current_netfd()
        self._bind_resolv_conf = bind_resolv_conf

        if self._current_fd <= 0:
            self._log_event(GetNetnsFdFailed(), rc=self._current_fd)
            raise NetworkNamespaceException("Get current namespace fd failed with rc == %s."
                                            % self._current_fd)
    def _log_event(self, event, **kwargs):
        try:
            event.netns_name = self.name
            event.resolv_conf_path = self.resolv_conf_path
            for key, value in kwargs.items():
                setattr(event, key, value)

        except (KeyboardInterrupt, SystemExit):
            raise

        except:
            logger.exception("Caught exception when attempting to log event")

        rwlogger.log_event(event, frames_above=1)

    @property
    def name(self):
        return self._netns_name

    @property
    def resolv_conf_path(self):
        return "/etc/netns/%s/resolv.conf" % self._netns_name

    def _mount_resolv_conf(self):
        if not os.path.isfile(self.resolv_conf_path):
            self._log_event(MissingResolvConf())
            raise NetworkNamespaceException("Network namespace resolv.conf file does not exist: %s"
                                            % self.resolv_conf_path)

        with open(self.resolv_conf_path) as resolv_conf_hdl:
            self._log_event(MountingResolvConf(), content=resolv_conf_hdl.read())

        bind_rc = RwNetns.mount_bind_resolv_conf(self.resolv_conf_path)
        if bind_rc != 0:
            self._log_event(MountingResolvConfFailed, rc=bind_rc)
            raise NetworkNamespaceException("Could not mount netns resolv conf (%s). rc=%s"
                                            % (self.resolv_conf_path, bind_rc))

    def _unmount_resolv_conf(self):
        self._log_event(UnmountingResolvConf())
        unbind_rc = RwNetns.unmount_resolv_conf()
        if unbind_rc != 0:
            self._log_event(UnmountingResolvConfFailed(), rc=unbind_rc)
            raise NetworkNamespaceException("Could not unbind netns resolv conf (%s). rc=%s"
                                            % (self.resolv_conf_path, unbind_rc))

    def enter(self):
        self._log_event(EnteringNetns())

        if NetnsEnvironment.current_netns is not None:
            self._log_event(MultipleNetnsError(),
                            current_netns_name=NetnsEnvironment.current_netns.name)
            raise NetworkNamespaceException("Cannot enter more than one network namespace (already in %s)",
                                            NetnsEnvironment.current_netns.name)

        change_rc = RwNetns.change(self._netns_name)
        if change_rc != 0:
            self._log_event(ChangeNetnsFailed(), rc=change_rc)
            raise NetworkNamespaceException("Change to namespace (%s) failed with rc == %s."
                                            % (self._netns_name, change_rc))

        if self._bind_resolv_conf:
            self._mount_resolv_conf()

    def exit(self):
        self._log_event(ExitingNetns())

        if self._bind_resolv_conf:
            self._unmount_resolv_conf()

        change_rc = RwNetns.change_fd(self._current_fd)
        if change_rc != 0:
            self._log_event(ChangeNetnsFdFailed(), rc=change_rc)
            raise NetworkNamespaceException("Change back to default namespace fd (%s) failed with rc == %s."
                                            % (self._current_fd, change_rc))

