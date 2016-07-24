
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import logging

import kazoo.exceptions

import gi
gi.require_version('RwCal', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwCal', '1.0')


from gi.repository import (
    GObject,
    RwCal,
    RwTypes)

import rw_status
import rwlogger

import rift.cal
import rift.cal.rwzk

logger = logging.getLogger('rwcal')

rwstatus = rw_status.rwstatus_from_exc_map({
                IndexError: RwTypes.RwStatus.NOTFOUND,
                KeyError: RwTypes.RwStatus.NOTFOUND,

                kazoo.exceptions.NodeExistsError: RwTypes.RwStatus.EXISTS,
                kazoo.exceptions.NoNodeError: RwTypes.RwStatus.NOTFOUND,
                kazoo.exceptions.NotEmptyError: RwTypes.RwStatus.NOTEMPTY,
                kazoo.exceptions.LockTimeout: RwTypes.RwStatus.TIMEOUT,

                rift.cal.rwzk.UnlockedError: RwTypes.RwStatus.NOTCONNECTED,
           })

class ZookeeperPlugin(GObject.Object, RwCal.Zookeeper):
    def __init__(self):
      GObject.Object.__init__(self)
      self._cli = None

    @rwstatus
    def do_create_server_config(self, zkid, client_port, unique_ports, server_details):
        rift.cal.rwzk.create_server_config(zkid, client_port, unique_ports, server_details)

    def do_server_start(self, zkid):
        return rift.cal.rwzk.server_start(zkid)

    @rwstatus
    def do_kazoo_init(self, server_details):
        if self._cli is not None:
            if isinstance(self._cli, rift.cal.rwzk.Kazoo):
                return
            else:
                raise AttributeError('Zookeeper client was already initialized')

        self._cli = rift.cal.rwzk.Kazoo()
        self._cli.client_init(server_details)

    @rwstatus
    def do_kazoo_start(self):
        if self._cli is not None:
            if not isinstance(self._cli, rift.cal.rwzk.Kazoo):
                raise AttributeError('No OR Bad client')
        self._cli.start(timeout=120)

    @rwstatus
    def do_kazoo_stop(self):
        if self._cli is not None:
            if not isinstance(self._cli, rift.cal.rwzk.Kazoo):
                raise AttributeError('No OR Bad client')
        self._cli.stop()

    @rwstatus
    def do_zake_init(self):
        if self._cli is not None:
            if isinstance(rift.cal.rwzk.Zake, self._cli):
                return
            else:
                raise AttributeError('Zookeeper client was already initialized')

        self._cli = rift.cal.rwzk.Zake()
        self._cli.client_init('')

    @rwstatus
    def do_lock(self, path, timeout):
        if timeout == 0.0:
            timeout = None

        self._cli.lock_node(path, timeout)

    @rwstatus
    def do_unlock(self, path):
        self._cli.unlock_node(path)

    def do_locked(self, path):
        try:
            return self._cli.locked(path)
        except kazoo.exceptions.NoNodeError:
            # A node that doesn't exist can't really be locked.
            return False

    @rwstatus
    def do_create(self, path, closure=None):
        if not closure:
            self._cli.create_node(path)
        else:
            self._cli.acreate_node(path, closure.callback)

    def do_exists(self, path):
        return self._cli.exists(path)

    @rwstatus(ret_on_failure=[""])
    def do_get(self, path, closure=None):
        if not closure:
            data = self._cli.get_node_data(path)
            return data.decode()
        self._cli.aget_node_data(path, closure.store_data, closure.callback)
        return 0


    @rwstatus
    def do_set(self, path, data, closure=None):
        if not closure:
            self._cli.set_node_data(path, data.encode(), None)
        else:
            self._cli.aset_node_data(path, data.encode(), closure.callback)


    @rwstatus(ret_on_failure=[[]])
    def do_children(self, path, closure=None):
        if not closure:
            return self._cli.get_node_children(path)
        self._cli.aget_node_children(path, closure.store_data, closure.callback)
        return 0

    @rwstatus
    def do_rm(self, path, closure=None):
        if not closure:
            self._cli.delete_node(path)
        else:
            self._cli.adelete_node(path, closure.callback)

    @rwstatus
    def do_register_watcher(self, path, closure):
        self._cli.register_watcher(path, closure.callback)

    @rwstatus
    def do_unregister_watcher(self, path, closure):
        self._cli.unregister_watcher(path, closure.callback)

    def do_get_client_state(self):
        mapping = {
            kazoo.client.KazooState.LOST:RwCal.KazooClientStateEnum.LOST,
            kazoo.client.KazooState.SUSPENDED:RwCal.KazooClientStateEnum.SUSPENDED,
            kazoo.client.KazooState.CONNECTED:RwCal.KazooClientStateEnum.CONNECTED,
        }
        return mapping[self._cli.client_state]



def main():
    @rwstatus
    def blah():
        raise IndexError()

    a = blah()
    assert(a == RwTypes.RwStatus.NOTFOUND)

    @rwstatus({IndexError: RwTypes.RwStatus.NOTCONNECTED})
    def blah2():
        """Some function"""
        raise IndexError()

    a = blah2()
    assert(a == RwTypes.RwStatus.NOTCONNECTED)
    assert(blah2.__doc__ == "Some function")

if __name__ == '__main__':
    main()

