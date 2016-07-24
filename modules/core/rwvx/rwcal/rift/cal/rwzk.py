
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import abc
import collections
import inspect
import os
import weakref

import kazoo.client
import kazoo.exceptions
import zake.fake_client

class UnlockedError(Exception):
    pass

def create_server_config(myid, client_port, unique_ports, server_details):
    """
    This function generates the configuration file needed
    for the zookeeper server.

    An example configutaion is
    shown below:
            tickTime=2000
            dataDir=/var/zookeeper
            clientPort=2181
            initLimit=5
            syncLimit=2
            server.1=zoo1:2888:3888
            server.2=zoo2:2888:3888
            server.3=zoo3:2888:3888

    To test multiple servers on a single machine, specify the servername
    as localhost with unique quorum & leader election ports
    (i.e. 2888:3888, 2889:3889, 2890:3890 in the example above) for each
    server.X in that server's config file.  Of course separate dataDirs
    and distinct clientPorts are also necessary (in the above replicated
    example, running on a single localhost, you would still have three
    config files)."


    @param myid         - the id of the zookeeper server in the ensemble
    @param unique_ports - Assign unique ports for server arbitration
    @param server_details - list of server port details
    """
    install_dir = os.environ.get('INSTALLDIR', '')

    config_fname = "%s/etc/zookeeper/server-%d.cfg" % (install_dir, myid)

    config = "tickTime=2000\n"
    config += "dataDir=%s/zk/server-%d/data\n" % (install_dir, myid)
    config += "clientPort=%d\n" % (client_port)
    config += "initLimit=5\n"
    config += "syncLimit=2\n"

    quorum_port = 2888
    election_port = 3888
    for server_detail in server_details:
        if server_detail.zk_in_config != True:
            continue
        if unique_ports:
            quorum_port += 1
            election_port += 1
        config += "server.%d=%s:%d:%d\n" % (server_detail.zk_server_id, server_detail.zk_server_addr, quorum_port, election_port)

    if not os.path.isdir(os.path.dirname(config_fname)):
        os.makedirs(os.path.dirname(config_fname))

    with open(config_fname, "w") as config_file:
        config_file.write(config)

    try:
        chk_dir = os.stat("%s/zk/server-%d/data/" % (install_dir, myid))
    except FileNotFoundError:
        try:
            chk_dir = os.stat("%s/zk/server-%d/" % (install_dir, myid))
            os.mkdir("%s/zk/server-%d/data/" % (install_dir, myid))
        except FileNotFoundError:
            try:
                chk_dir = os.stat("%s/zk/" % (install_dir))
                os.mkdir("%s/zk/server-%d/" % (install_dir, myid))
                os.mkdir("%s/zk/server-%d/data/" % (install_dir, myid))
            except FileNotFoundError:
                os.mkdir("%s/zk/" % (install_dir))
                os.mkdir("%s/zk/server-%d/" % (install_dir, myid))
                os.mkdir("%s/zk/server-%d/data/" % (install_dir, myid))

    id_fname = "%s/zk/server-%d/data/myid" % (install_dir, myid)
    with open(id_fname, "w") as id_file:
        id_file.write(str(myid))

def to_java_compatible_path(path):
    if os.name == 'nt':
        path = path.replace('\\', '/')
    return path

def server_start(myid):
    """
    Start the zookeeper server

    @param myid - the id of the zookeeper server in the ensemble
    """
    install_dir = os.environ.get('INSTALLDIR')

    classpath = ':'.join([
        "/etc/zookeeper",
        "/usr/share/java/slf4j/slf4j-log4j12.jar",
        "/usr/share/java/slf4j/slf4j-api.jar",
        "/usr/share/java/netty.jar",
        "/usr/share/java/log4j.jar",
        "/usr/share/java/jline.jar",
        "/usr/share/java/zookeeper/zookeeper.jar"])

    log4j_path = os.path.join("%s/etc/zookeeper/" % (install_dir), "log4j-%d.properties" % (myid,))
    import socket
    with open(log4j_path, "w") as log4j:
            log4j.write("""
# DEFAULT: console appender only
log4j.rootLogger=INFO, ZLOG
log4j.appender.ZLOG.layout=org.apache.log4j.PatternLayout
log4j.appender.ZLOG.layout.ConversionPattern=%d{ISO8601} [""" + socket.getfqdn()  +"""] - %-5p [%t:%C{1}@%L] - %m%n
log4j.appender.ZLOG=org.apache.log4j.RollingFileAppender
log4j.appender.ZLOG.Threshold=DEBUG
log4j.appender.ZLOG.File=""" + to_java_compatible_path(  # NOQA
                "%s/zk/server-%d/log" % (install_dir, myid) + os.sep + "zookeeper.log\n"))


    argv = [
        '/usr/bin/java',
        '-cp',
        classpath,
        #'-Dlog4j.debug',
        '-Dzookeeper.log.dir="%s"' % (install_dir,),
        '-Dlog4j.configuration=file:%s' % log4j_path,
        '-Dcom.sun.management.jmxremote',
        '-Dcom.sun.management.jmxremote.local.only=false',
        'org.apache.zookeeper.server.quorum.QuorumPeerMain',
        '%s/etc/zookeeper/server-%d.cfg' % (install_dir, myid),
    ]

    print('Running zookeeper: %s' %  (' '.join(argv),))

    pid = os.fork()
    if pid < 0:
        raise OSError("Failed to fork")
    elif pid == 0:
        # instruct the child process to TERM when the parent dies
        import ctypes
        libc = ctypes.CDLL('/lib64/libc.so.6')
        PR_SET_PDEATHSIG = 1; TERM = 15
        libc.prctl(PR_SET_PDEATHSIG, TERM)

        os.execv(argv[0], argv)

        # Should never reach here CRASH
        raise OSError("execv() failed")
    return pid

class NodeWatcher(object):
    def __init__(self, zkcli, path):
        """
        Create a watcher for the given zookeeper path.  This
        serves as a single place for interested parties to
        register callbacks when the given path changes.  This is
        used instead of the kazoo DataWatcher recipe as:
            - It stores references to callbacks indefinitely
            - It does not support methods
            - It fires twice for every event
        Obviously, none of that is an issue with -this- object!

        @param zkcli    - zookeeper client
        @param path     - path to monitor
        """
        self._zkcli = zkcli
        self._path = path

        # Weak references to the registered functions/methods.  This
        # is so the reference count does not increase leading to the
        # callbacks persisting only due to their use here.
        self._funcs = weakref.WeakSet()
        self._methods = weakref.WeakKeyDictionary()
        self._calls = []

    @property
    def registered(self):
        """
        The number of callbacks currently registered
        """
        return len(self._funcs) + len(self._methods) + len(self._calls)

    def _on_event(self, event):
        """
        Handle a zookeeper event on the monitored path.  This
        will execute every callback once.  If the callback accepts
        at least two argument (three for methods) then the event type
        and event path will be passed as the first two arguments.
        """
        def call(function, obj=None):
            # The gi module does a lot of magic.  Just importing gi will not
            # load the FunctionInfo type.  This seems to be the easiest way
            # to check without being able to use isinstance.
            if 'gi.FunctionInfo' in str(type(function)):
                if len(function.get_arguments()) >= 2:
                    function(event.type, event.path)
                else:
                    function()
                return

            args, varargs, _, _= inspect.getargspec(function)
            if obj is not None:
                if len(args) >= 3 or varargs is not None:
                    function(obj, event.type, event.path)
                else:
                    function(obj)
            else:
                if len(args) >= 2 or varargs is not None:
                    function(event.type, event.path)
                else:
                    function()


        _ = [call(f) for f in self._funcs]

        for obj, methods in self._methods.items():
            _ = [call(m, obj=obj) for m in methods]

        _ = [call(f) for f in self._calls]

        if self.registered > 0:
            _ = self._zkcli.exists(self._path, watch=self._on_event)

    def register(self, slot):
        """
        Register the specified method/function as a callback for when
        the monitored path is changed.

        @param slot - method/function to call when signal is emitted.

        On an event, the slot will be executed with two
        parameters: the type of event that was generated and the
        path to the node that changed.
        """
        if inspect.ismethod(slot):
            if not slot.__self__ in self._methods:
                self._methods[slot.__self__] = set()
            self._methods[slot.__self__].add(slot.__func__)
        elif inspect.isfunction(slot):
            self._funcs.add(slot)
        elif callable(slot):
            self._calls.append(slot)

        if self.registered > 0:
            _ = self._zkcli.exists(self._path, watch=self._on_event)

    def unregister(self, slot):
        """
        Unregister the specified method/function.

        @param slot - method/function to unregister.
        """
        if inspect.ismethod(slot):
            if slot.__self__ in self._methods:
                self._methods[slot.__self__].remove(slot.__func__)
        elif inspect.isfunction(slot):
            if slot in self._funcs:
                self._funcs.remove(slot)
        elif callable(slot):
            if slot in self._calls:
                self._calls.remove(slot)


class ZkClient(object):
    """Abstract class that all the different implementations of zookeeper
    clients must implement"""
    __metaclass__ = abc.ABCMeta

    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = super(ZkClient, cls).__new__(
                                cls, *args, **kwargs)
        return cls._instance

    def __init__(self):
        self._client_context = None
        self._locks = {}
        self._node_watchers = {}

    @abc.abstractmethod
    def client_init(self, server_details, timeout=120):
        """
        Initialize the client connection to the zookeeper

        @param server_names - List of zookeeper server names

        Note:
            It would be really nice to provide this information during __init__()
        However, the CAL is created, and used, by rwvx well before the manifest
        which contains the unique_ports and server port details parsed.
        """
        pass

    def lock_node(self, path, timeout=None):
        """
        Lock a node for writing.  The lock is reentrant.

        @param path     - path to lock
        @param timeout  - time, in seconds, to wait for the lock
        @raises         - kazoo.exceptions.NoNodeError if the path does not exist
                          UnlockedError if lock acquisition fails
                          kazoo.exceptions.LockTimeout if a timeout was specified and not met
        """
        if not self.exists(path):
            raise kazoo.exceptions.NoNodeError()

        if path in self._locks:
            lock, count = self._locks[path]
            count += 1
            self._locks[path] = lock, count
        else:
            lock = kazoo.recipe.lock.Lock(self._client_context, path)
            self._locks[path] = lock, 1

        if lock.is_acquired:
            return

        if not lock.acquire(timeout=timeout):
            raise UnlockedError()
        return

    def unlock_node(self, path):
        """
        Unlock a node for writing.  If the path is not locked by this process,
        this is a no-op.

        @param path - path to unlock
        @raises     - kazoo.exceptions.NoNodeError if the path does not exist
        """
        if not self.exists(path):
            raise kazoo.exceptions.NoNodeError()

        if not path in self._locks:
            return

        lock, count = self._locks[path]
        count -= 1
        self._locks[path] = lock, count 
        if count:
            return

        self._locks[path][0].release()

    def locked(self, path):
        """
        Test if a path is locked or not.

        @param path - path to test
        @raises     - kazoo.exceptions.NoNodeError if the path does not exist
        """
        if not self.exists(path):
            raise kazoo.exceptions.NoNodeError()
       
        # Always create a new lock object as we cannot tell if we currently
        # are holding a re-entrant lock
        lock = kazoo.recipe.lock.Lock(self._client_context, path)

        got_lock = lock.acquire(blocking=False)
        if got_lock:
            lock.release()

        return not got_lock
 
    def my_callback(self, async_obj=None):
        def call(function, obj=None):
            # The gi module does a lot of magic.  Just importing gi will not
            # load the FunctionInfo type.  This seems to be the easiest way
            # to check without being able to use isinstance.
            if 'gi.FunctionInfo' in str(type(function)):
                if len(function.get_arguments()) >= 2:
                    print (event.type, event.path)
                    function(event.type, event.path)
                else:
                    if obj:
                        function(obj)
                    else:
                        function()
                return

            args, varargs, _, _= inspect.getargspec(function)
            if obj is not None:
                if len(args) >= 3 or varargs is not None:
                    function(obj, event.type, event.path)
                else:
                    function(obj)
            else:
                if len(args) >= 2 or varargs is not None:
                    function(event.type, event.path)
                else:
                    function()

        if async_obj._store_node_data:
            get_obj,_ = async_obj.get(timeout=1000)
            async_obj._get_obj_list.append(get_obj.decode())
            _ = call(async_obj._store_node_data, async_obj._get_obj_list)
        if async_obj._store_children:
            _ = call(async_obj._store_children, async_obj.get(timeout=1000))
        _ = call(async_obj._callback)
        async_obj._callback=None

    def acreate_node(self, path, callback, create_ancestors=True):
        async_obj = self._client_context.create_async(path, makepath=create_ancestors)
        async_obj._store_data = None
        async_obj._store_node_data = None
        async_obj._store_children = False
        async_obj._callback = callback
        async_obj.rawlink(self.my_callback)


    def adelete_node(self, path, callback, delete_children=False):
        async_obj = self._client_context.delete_async(path, recursive=delete_children)
        async_obj._store_data = None
        async_obj._store_node_data = None
        async_obj._store_children = False
        async_obj._callback = callback
        async_obj.rawlink(self.my_callback)

    def aget_node_children(self, path, store_children, callback):
        async_obj = self._client_context.get_children_async(path)
        async_obj._store_data = None
        async_obj._store_node_data = None
        async_obj._store_children = store_children
        async_obj._callback = callback
        async_obj.rawlink(self.my_callback)

    def aset_node_data(self, path, data, callback):
        async_obj = self._client_context.set_async(path, data)
        async_obj._store_data = None
        async_obj._store_node_data = None
        async_obj._store_children = False
        async_obj._callback = callback
        async_obj.rawlink(self.my_callback)

    def aget_node_data(self, path, store_node_data, callback):
        async_obj = self._client_context.get_async(path)
        async_obj._store_node_data = store_node_data
        async_obj._store_children = False
        async_obj._callback = callback
        async_obj._get_obj_list = []
        async_obj.rawlink(self.my_callback)

    def exists(self, path):
        return self._client_context.exists(path) is not None

    def create_node(self, path, create_ancestors=True):
        realpath = self._client_context.create(path, makepath=create_ancestors)
        return realpath

    def get_client_state(self):
        return self._client_context.client_state

    def delete_node(self, path, delete_children = False):
        self._client_context.delete(path, recursive=delete_children)

    def get_node_data(self, path):
        node_data, _ = self._client_context.get(path)
        return node_data

    def set_node_data(self, path, data, _):
        self._client_context.set(path, data)

    def get_node_children(self, path):
        children = self._client_context.get_children(path)
        return children

    def register_watcher(self, path, watcher):
        """
        Register a function to be called whenever the data
        contained in the specified path is modified.  Note that
        this will not fire when the path metadata is changed.

        @param path     - path to node to monitor
        @param watcher  - function to call when data at the
                          specified path is modified.
        """
        if not path in self._node_watchers:
            self._node_watchers[path] = NodeWatcher(self._client_context, path)

        self._node_watchers[path].register(watcher)

    def unregister_watcher(self, path, watcher):
        """
        Unregister a function that was previously setup to
        monitor a path with register_watcher().

        @param path     - path to stop watching
        @param watcher  - function to no longer call when data at
                          the specified path changes
        """
        if not path in self._node_watchers:
            return

        self._node_watchers[path].unregister(watcher)

    def stop(self):
        self._client_context.stop()
        self._client_context.close()


class Zake(ZkClient):
    """This class implements the abstract methods in the ZkClient.
    ZAKE is pseudo implementation of zookeeper."""
    _zake_client_context = None

    def client_init(self, _):
        if Zake._zake_client_context is None:
            # In collapsed mode, this is going to be called multiple times but
            # we want to make sure we only have a single FakeClient, so the
            # client_context is a class attribute.
            Zake._zake_client_context = zake.fake_client.FakeClient()
            Zake._zake_client_context.start()
            print("starting ZAKE")
        self._client_context = Zake._zake_client_context


class Kazoo(ZkClient):
    """This class implements the abstract methods in the ZkClient.
    Kazoo is a python implementation of zookeeper."""

    @staticmethod
    def my_listener( state):
        if state == kazoo.client.KazooState.LOST:
            # Register somewhere that the session was lost
            print("Kazoo connection lost")
        elif state == kazoo.client.KazooState.SUSPENDED:
            # Handle being disconnected from Zookeeper
            print("Kazoo connection suspended")
        else:
            # Handle being connected/reconnected to Zookeeper
            print("Kazoo connection established")

    def client_init(self, server_details, timeout=120):
        connection_str = ""
        for server_detail in server_details:
            if server_detail.zk_client_connect != True:
                continue
            if connection_str is not "":
                connection_str += (",")

            connection_str += ("%s:%d" % (server_detail.zk_server_addr, server_detail.zk_client_port))

        print("KazooClient connecting to %s" % (connection_str))
        self._client_context = kazoo.client.KazooClient(
                                   connection_str,
                                   timeout=120,
                               )
                                   #connection_retry=kazoo.retry.KazooRetry(max_tries=-1),
                                   #command_retry=kazoo.retry.KazooRetry(max_tries=-1),
        self._client_context.add_listener(self.my_listener)
        self._client_context.start(timeout)

    def start(self, timeout=120):
        self._client_context.start(timeout)

    @property
    def client_state(self):
        self._client_context.client_state

# vim: sw=4 ts=4

