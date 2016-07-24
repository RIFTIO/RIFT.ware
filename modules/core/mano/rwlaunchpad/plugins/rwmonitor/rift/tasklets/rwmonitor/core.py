
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import abc
import asyncio
import collections
import concurrent.futures
import importlib
import time

import gi
gi.require_version('RwVnfrYang', '1.0')
gi.require_version('RwMon', '1.0')
from gi.repository import (
        RwTypes,
        RwVnfrYang,
        )

import rw_peas


class VdurMissingVimIdError(Exception):
    def __init__(self, vdur_id):
        super().__init__("VDUR:{} is has no VIM ID".format(vdur_id))


class VdurAlreadyRegisteredError(Exception):
    def __init__(self, vdur_id):
        super().__init__("VDUR:{} is already registered".format(vdur_id))


class AccountInUseError(Exception):
    pass


class UnknownAccountError(Exception):
    pass


class AccountAlreadyRegisteredError(Exception):
    def __init__(self, account_name):
        msg = "'{}' already registered".format(account_name)
        super().__init__(account_name)


class PluginUnavailableError(Exception):
    pass


class PluginNotSupportedError(PluginUnavailableError):
    pass


class AlarmCreateError(Exception):
    def __init__(self):
        super().__init__("failed to create alarm")


class AlarmDestroyError(Exception):
    def __init__(self):
        super().__init__("failed to destroy alarm")


class PluginFactory(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def create(self, cloud_account, plugin_name):
        pass

    @property
    def name(self):
        return self.__class__.PLUGIN_NAME

    @property
    def fallbacks(self):
        try:
            return list(self.__class__.FALLBACKS)
        except Exception:
            return list()


class MonascaPluginFactory(PluginFactory):
    PLUGIN_NAME = "monasca"
    FALLBACKS = ["ceilometer",]

    def create(self, cloud_account):
        raise PluginUnavailableError()


class CeilometerPluginFactory(PluginFactory):
    PLUGIN_NAME = "ceilometer"
    FALLBACKS = ["unavailable",]

    def create(self, cloud_account):
        plugin = rw_peas.PeasPlugin("rwmon_ceilometer", 'RwMon-1.0')
        impl = plugin.get_interface("Monitoring")

        # Check that the plugin is available on the platform associated with
        # the cloud account
        _, available = impl.nfvi_metrics_available(cloud_account)
        if not available:
            raise PluginUnavailableError()

        return impl


class UnavailablePluginFactory(PluginFactory):
    PLUGIN_NAME = "unavailable"

    class UnavailablePlugin(object):
        def nfvi_metrics_available(self, cloud_account):
            return None, False

    def create(self, cloud_account):
        return UnavailablePluginFactory.UnavailablePlugin()


class MockPluginFactory(PluginFactory):
    PLUGIN_NAME = "mock"
    FALLBACKS = ["unavailable",]

    def create(self, cloud_account):
        plugin = rw_peas.PeasPlugin("rwmon_mock", 'RwMon-1.0')
        impl = plugin.get_interface("Monitoring")

        # Check that the plugin is available on the platform associated with
        # the cloud account
        _, available = impl.nfvi_metrics_available(cloud_account)
        if not available:
            raise PluginUnavailableError()

        return impl


class NfviMetricsPluginManager(object):
    def __init__(self, log):
        self._plugins = dict()
        self._log = log
        self._factories = dict()

        self.register_plugin_factory(MockPluginFactory())
        self.register_plugin_factory(CeilometerPluginFactory())
        self.register_plugin_factory(MonascaPluginFactory())
        self.register_plugin_factory(UnavailablePluginFactory())

    @property
    def log(self):
        return self._log

    def register_plugin_factory(self, factory):
        self._factories[factory.name] = factory

    def plugin(self, account_name):
        return self._plugins[account_name]

    def register(self, cloud_account, plugin_name):
        # Check to see if the cloud account has already been registered
        if cloud_account.name in self._plugins:
            raise AccountAlreadyRegisteredError(cloud_account.name)

        if plugin_name not in self._factories:
            raise PluginNotSupportedError(plugin_name)

        # Create a plugin from one of the factories
        fallbacks = [plugin_name,]

        while fallbacks:
            name = fallbacks.pop(0)
            try:
                factory = self._factories[name]
                plugin = factory.create(cloud_account)
                self._plugins[cloud_account.name] = plugin
                return

            except PluginUnavailableError as e:
                self.log.warning("plugin for {} unavailable".format(name))
                fallbacks.extend(factory.fallbacks)

        raise PluginUnavailableError()

    def unregister(self, account_name):
        if account_name in self._plugins:
            del self._plugins[account_name]


class NfviMetrics(object):
    """
    The NfviMetrics class contains the logic to retrieve NFVI metrics for a
    particular VDUR. Of particular importance is that this object caches the
    metrics until the data become stale so that it does not create excessive
    load upon the underlying data-source.
    """

    # The sample interval defines the maximum time (secs) that metrics will be
    # cached for. This duration should coincide with the sampling interval used
    # by the underlying data-source to capture metrics.
    SAMPLE_INTERVAL = 10

    # The maximum time (secs) an instance will wait for a request to the data
    # source to be completed
    TIMEOUT = 2

    def __init__(self, log, loop, account, plugin, vdur):
        """Creates an instance of NfviMetrics

        Arguments:
            manager - a NfviInterface instance
            account - a CloudAccount instance
            plugin  - an NFVI plugin
            vdur    - a VDUR instance

        """
        self._log = log
        self._loop = loop
        self._account = account
        self._plugin = plugin
        self._timestamp = 0
        self._metrics = RwVnfrYang.YangData_Vnfr_VnfrCatalog_Vnfr_Vdur_NfviMetrics()
        self._vdur = vdur
        self._vim_id = vdur.vim_id
        self._updating = None

    @property
    def log(self):
        """The logger used by NfviMetrics"""
        return self._log

    @property
    def loop(self):
        """The current asyncio loop"""
        return self._loop

    @property
    def vdur(self):
        """The VDUR that these metrics are associated with"""
        return self._vdur

    def retrieve(self):
        """Return the NFVI metrics for this VDUR

        This function will immediately return the current, known NFVI metrics
        for the associated VDUR. It will also, if the data are stale, schedule
        a call to the data-source to retrieve new data.

        """
        if self.should_update():
            self._updating = self.loop.create_task(self.update())

        return self._metrics

    def should_update(self):
        """Return a boolean indicating whether an update should be performed"""
        running = self._updating is not None and not self._updating.done()
        overdue = time.time() > self._timestamp + NfviMetrics.SAMPLE_INTERVAL

        return overdue and not running

    @asyncio.coroutine
    def update(self):
        """Update the NFVI metrics for the associated VDUR

        This coroutine will request new metrics from the data-source and update
        the current metrics.

        """
        try:
            try:
                # Make the request to the plugin in a separate thread and do
                # not exceed the timeout
                _, metrics = yield from asyncio.wait_for(
                        self.loop.run_in_executor(
                            None,
                            self._plugin.nfvi_metrics,
                            self._account,
                            self._vim_id,
                            ),
                        timeout=NfviMetrics.TIMEOUT,
                        loop=self.loop,
                        )

            except asyncio.TimeoutError:
                msg = "timeout on request for nfvi metrics (vim-id = {})"
                self.log.warning(msg.format(self._vim_id))
                return

            except Exception as e:
                self.log.exception(e)
                return

            try:
                # Create uninitialized metric structure
                vdu_metrics = RwVnfrYang.YangData_Vnfr_VnfrCatalog_Vnfr_Vdur_NfviMetrics()

                # VCPU
                vdu_metrics.vcpu.total = self.vdur.vm_flavor.vcpu_count
                vdu_metrics.vcpu.utilization = metrics.vcpu.utilization

                # Memory (in bytes)
                vdu_metrics.memory.used = metrics.memory.used
                vdu_metrics.memory.total = self.vdur.vm_flavor.memory_mb
                vdu_metrics.memory.utilization = 100 * vdu_metrics.memory.used / vdu_metrics.memory.total

                # Storage
                vdu_metrics.storage.used = metrics.storage.used
                vdu_metrics.storage.total = 1e9 * self.vdur.vm_flavor.storage_gb
                vdu_metrics.storage.utilization = 100 * vdu_metrics.storage.used / vdu_metrics.storage.total

                # Network (incoming)
                vdu_metrics.network.incoming.packets = metrics.network.incoming.packets
                vdu_metrics.network.incoming.packet_rate = metrics.network.incoming.packet_rate
                vdu_metrics.network.incoming.bytes = metrics.network.incoming.bytes
                vdu_metrics.network.incoming.byte_rate = metrics.network.incoming.byte_rate

                # Network (outgoing)
                vdu_metrics.network.outgoing.packets = metrics.network.outgoing.packets
                vdu_metrics.network.outgoing.packet_rate = metrics.network.outgoing.packet_rate
                vdu_metrics.network.outgoing.bytes = metrics.network.outgoing.bytes
                vdu_metrics.network.outgoing.byte_rate = metrics.network.outgoing.byte_rate

                # External ports
                vdu_metrics.external_ports.total = len(self.vdur.external_interface)

                # Internal ports
                vdu_metrics.internal_ports.total = len(self.vdur.internal_interface)

                self._metrics = vdu_metrics

            except Exception as e:
                self.log.exception(e)

        finally:
            # Regardless of the result of the query, we want to make sure that
            # we do not poll the data source until another sample duration has
            # passed.
            self._timestamp = time.time()


class NfviMetricsCache(object):
    def __init__(self, log, loop, plugin_manager):
        self._log = log
        self._loop = loop
        self._plugin_manager = plugin_manager
        self._nfvi_metrics = dict()

        self._vim_to_vdur = dict()
        self._vdur_to_vim = dict()

    def create_entry(self, account, vdur):
        plugin = self._plugin_manager.plugin(account.name)
        metrics = NfviMetrics(self._log, self._loop, account, plugin, vdur)
        self._nfvi_metrics[vdur.vim_id] = metrics

        self._vim_to_vdur[vdur.vim_id] = vdur.id
        self._vdur_to_vim[vdur.id] = vdur.vim_id

    def destroy_entry(self, vdur_id):
        vim_id = self._vdur_to_vim[vdur_id]

        del self._nfvi_metrics[vim_id]
        del self._vdur_to_vim[vdur_id]
        del self._vim_to_vdur[vim_id]

    def retrieve(self, vim_id):
        return self._nfvi_metrics[vim_id].retrieve()

    def to_vim_id(self, vdur_id):
        return self._vdur_to_vim[vdur_id]

    def to_vdur_id(self, vim_id):
        return self._vim_to_vdur[vim_id]

    def contains_vdur_id(self, vdur_id):
        return vdur_id in self._vdur_to_vim

    def contains_vim_id(self, vim_id):
        return vim_id in self._vim_to_vdur


class NfviInterface(object):
    """
    The NfviInterface serves as an interface for communicating with the
    underlying infrastructure, i.e. retrieving metrics for VDURs that have been
    registered with it and managing alarms.

    The NfviInterface should only need to be invoked using a cloud account and
    optionally a VIM ID; It should not need to handle mapping from VDUR ID to
    VIM ID.
    """

    def __init__(self, loop, log, plugin_manager, cache):
        """Creates an NfviInterface instance

        Arguments:
            loop           - an event loop
            log            - a logger
            plugin_manager - an instance of NfviMetricsPluginManager
            cache          - an instance of NfviMetricsCache

        """
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=16)
        self._plugin_manager = plugin_manager
        self._cache = cache
        self._loop = loop
        self._log = log

    @property
    def loop(self):
        """The event loop used by this NfviInterface"""
        return self._loop

    @property
    def log(self):
        """The event log used by this NfviInterface"""
        return self._log

    @property
    def metrics(self):
        """The list of metrics contained in this NfviInterface"""
        return list(self._cache._nfvi_metrics.values())

    def nfvi_metrics_available(self, account):
        plugin = self._plugin_manager.plugin(account.name)
        _, available = plugin.nfvi_metrics_available(account)
        return available

    def retrieve(self, vdur_id):
        """Returns the NFVI metrics for the specified VDUR

        Note, a VDUR must be registered with a NfviInterface before
        metrics can be retrieved for it.

        Arguments:
            vdur_id - the ID of the VDUR to whose metrics should be retrieve

        Returns:
            An NfviMetrics object for the specified VDUR

        """
        return self._cache.retrieve(self._cache.to_vim_id(vdur_id))

    @asyncio.coroutine
    def alarm_create(self, account, vim_id, alarm, timeout=5):
        """Create a new alarm

        Arguments:
            account - a CloudAccount instance
            vim_id  - the VM to associate with this alarm
            alarm   - an alarm structure
            timeout - the request timeout (sec)

        Raises:
            If the data source does not respond in a timely manner, an
            asyncio.TimeoutError will be raised.

        """
        plugin = self._plugin_manager.plugin(account.name)
        status = yield from asyncio.wait_for(
                self.loop.run_in_executor(
                    None,
                    plugin.do_alarm_create,
                    account,
                    vim_id,
                    alarm,
                    ),
                timeout=timeout,
                loop=self.loop,
                )

        if status == RwTypes.RwStatus.FAILURE:
            raise AlarmCreateError()

    @asyncio.coroutine
    def alarm_destroy(self, account, alarm_id, timeout=5):
        """Destroy an existing alarm

        Arguments:
            account  - a CloudAccount instance
            alarm_id - the identifier of the alarm to destroy
            timeout  - the request timeout (sec)

        Raises:
            If the data source does not respond in a timely manner, an
            asyncio.TimeoutError will be raised.

        """
        plugin = self._plugin_manager.plugin(account.name)
        status = yield from asyncio.wait_for(
                self.loop.run_in_executor(
                    None,
                    plugin.do_alarm_delete,
                    account,
                    alarm_id,
                    ),
                timeout=timeout,
                loop=self.loop,
                )

        if status == RwTypes.RwStatus.FAILURE:
            raise AlarmDestroyError()


class InstanceConfiguration(object):
    """
    The InstanceConfiguration class represents configuration information that
    affects the behavior of the monitor. Essentially this class should contain
    not functional behavior but serve as a convenient way to share data amongst
    the components of the monitoring system.
    """

    def __init__(self):
        self.polling_period = None
        self.max_polling_frequency = None
        self.min_cache_lifetime = None
        self.public_ip = None


class Monitor(object):
    """
    The Monitor class is intended to act as a unifying interface for the
    different sub-systems that are used to monitor the NFVI.
    """

    def __init__(self, loop, log, config):
        """Create a Monitor object

        Arguments:
            loop   - an event loop
            log    - the logger used by this object
            config - an instance of InstanceConfiguration

        """
        self._loop = loop
        self._log = log

        self._cloud_accounts = dict()
        self._nfvi_plugins = NfviMetricsPluginManager(log)
        self._cache = NfviMetricsCache(log, loop, self._nfvi_plugins)
        self._nfvi_interface = NfviInterface(loop, log, self._nfvi_plugins, self._cache)
        self._config = config
        self._vnfrs = dict()
        self._vnfr_to_vdurs = collections.defaultdict(set)
        self._alarms = collections.defaultdict(list)

    @property
    def loop(self):
        """The event loop used by this object"""
        return self._loop

    @property
    def log(self):
        """The event log used by this object"""
        return self._log

    @property
    def cache(self):
        """The NFVI metrics cache"""
        return self._cache

    @property
    def metrics(self):
        """The list of metrics contained in this Monitor"""
        return self._nfvi_interface.metrics

    def nfvi_metrics_available(self, account):
        """Returns a boolean indicating whether NFVI metrics are available

        Arguments:
            account - the name of the cloud account to check

        Returns:
            a boolean indicating availability of NFVI metrics

        """
        if account not in self._cloud_accounts:
            return False

        cloud_account = self._cloud_accounts[account]
        return self._nfvi_interface.nfvi_metrics_available(cloud_account)

    def add_cloud_account(self, account):
        """Add a cloud account to the monitor

        Arguments:
            account - a cloud account object

        Raises:
            If the cloud account has already been added to the monitor, an
            AccountAlreadyRegisteredError is raised.

        """
        if account.name in self._cloud_accounts:
            raise AccountAlreadyRegisteredError(account.name)

        self._cloud_accounts[account.name] = account

        if account.account_type == "openstack":
            self.register_cloud_account(account, "monasca")
        else:
            self.register_cloud_account(account, "mock")

    def remove_cloud_account(self, account_name):
        """Remove a cloud account from the monitor

        Arguments:
            account_name - removes the cloud account that has this name

        Raises:
            If the specified cloud account cannot be found, an
            UnknownAccountError is raised.

        """
        if account_name not in self._cloud_accounts:
            raise UnknownAccountError()

        # Make sure that there are no VNFRs associated with this account
        for vnfr in self._vnfrs.values():
            if vnfr.cloud_account == account_name:
                raise AccountInUseError()

        del self._cloud_accounts[account_name]
        self._nfvi_plugins.unregister(account_name)

    def get_cloud_account(self, account_name):
        """Returns a cloud account by name

        Arguments:
            account_name - the name of the account to return

        Raises:
            An UnknownAccountError is raised if there is not account object
            associated with the provided name

        Returns:
            A cloud account object

        """
        if account_name not in self._cloud_accounts:
            raise UnknownAccountError()

        return self._cloud_accounts[account_name]

    def register_cloud_account(self, account, plugin_name):
        """Register a cloud account with an NFVI plugin

        Note that a cloud account can only be registered for one plugin at a
        time.

        Arguments:
            account     - the cloud account to associate with the plugin
            plugin_name - the name of the plugin to use

        """
        self._nfvi_plugins.register(account, plugin_name)

    def add_vnfr(self, vnfr):
        """Add a VNFR to the monitor

        Arguments:
            vnfr - a VNFR object

        Raises:
            An UnknownAccountError is raised if the account name contained in
            the VNFR does not reference a cloud account that has been added to
            the monitor.

        """
        if vnfr.cloud_account not in self._cloud_accounts:
            raise UnknownAccountError()

        account = self._cloud_accounts[vnfr.cloud_account]

        for vdur in vnfr.vdur:
            try:
                self.add_vdur(account, vdur)
                self._vnfr_to_vdurs[vnfr.id].add(vdur.id)
            except (VdurMissingVimIdError, VdurAlreadyRegisteredError):
                pass

        self._vnfrs[vnfr.id] = vnfr

    def update_vnfr(self, vnfr):
        """Updates the VNFR information in the monitor

        Arguments:
            vnfr - a VNFR object

        Raises:
            An UnknownAccountError is raised if the account name contained in
            the VNFR does not reference a cloud account that has been added to
            the monitor.

        """
        if vnfr.cloud_account not in self._cloud_accounts:
            raise UnknownAccountError()

        account = self._cloud_accounts[vnfr.cloud_account]

        for vdur in vnfr.vdur:
            try:
                self.add_vdur(account, vdur)
                self._vnfr_to_vdurs[vnfr.id].add(vdur.id)
            except (VdurMissingVimIdError, VdurAlreadyRegisteredError):
                pass

    def remove_vnfr(self, vnfr_id):
        """Remove a VNFR from the monitor

        Arguments:
            vnfr_id - the ID of the VNFR to remove

        """
        vdur_ids = self._vnfr_to_vdurs[vnfr_id]

        for vdur_id in vdur_ids:
            self.remove_vdur(vdur_id)

        del self._vnfrs[vnfr_id]
        del self._vnfr_to_vdurs[vnfr_id]

    def add_vdur(self, account, vdur):
        """Adds a VDUR to the monitor

        Adding a VDUR to the monitor will automatically create a NFVI metrics
        object that is associated with the VDUR so that the monitor cane
        provide the NFVI metrics associated with the VDUR.

        Arguments:
            account - the cloud account associated with the VNFR that contains
                      the provided VDUR
            vdur    - a VDUR object

        Raises:
            A VdurMissingVimIdError is raised if the provided VDUR does not
            contain a VIM ID. A VdurAlreadyRegisteredError is raised if the ID
            associated with the VDUR has already been registered.

        """
        if vdur.vim_id is None:
            raise VdurMissingVimIdError(vdur.id)

        if self.is_registered_vdur(vdur.id):
            raise VdurAlreadyRegisteredError(vdur.id)

        self.cache.create_entry(account, vdur)

    def remove_vdur(self, vdur_id):
        """Removes a VDUR from the monitor

        Arguments:
            vdur_id - the ID of the VDUR to remove

        """
        self.cache.destroy_entry(vdur_id)

        # Schedule any alarms associated with the VDUR for destruction
        for account_name, alarm_id in self._alarms[vdur_id]:
            self.loop.create_task(self.destroy_alarm(account_name, alarm_id))

        del self._alarms[vdur_id]

    def list_vdur(self, vnfr_id):
        """Returns a list of VDURs

        Arguments:
            vnfr_id - the identifier of the VNFR contains the VDURs

        Returns:
            A list of VDURs

        """
        return self._vnfrs[vnfr_id].vdur

    def is_registered_vnfr(self, vnfr_id):
        """Returns True if the VNFR is registered with the monitor

        Arguments:
            vnfr_id - the ID of the VNFR to check

        Returns:
            True if the VNFR is registered and False otherwise.

        """
        return vnfr_id in self._vnfrs

    def is_registered_vdur(self, vdur_id):
        """Returns True if the VDUR is registered with the monitor

        Arguments:
            vnfr_id - the ID of the VDUR to check

        Returns:
            True if the VDUR is registered and False otherwise.

        """
        return self.cache.contains_vdur_id(vdur_id)

    def retrieve_nfvi_metrics(self, vdur_id):
        """Retrieves the NFVI metrics associated with a VDUR

        Arguments:
            vdur_id - the ID of the VDUR whose metrics are to be retrieved

        Returns:
            NFVI metrics for a VDUR

        """
        return self._nfvi_interface.retrieve(vdur_id)

    @asyncio.coroutine
    def create_alarm(self, account_name, vdur_id, alarm):
        """Create a new alarm

        This function create an alarm and augments the provided endpoints with
        endpoints to the launchpad if the launchpad has a public IP. The added
        endpoints are of the form,

            http://{host}:4568/{platform}/{vdur_id}/{action}

        where the 'action' is one of 'ok', 'alarm', or 'insufficient_data'. The
        messages that are pushed to the launchpad are not defined by RIFT so
        we need to know which platform an alarm is sent from in order to
        properly parse it.


        Arguments:
            account_name - the name of the account to use to create the alarm
            vdur_id      - the identifier of the VDUR to associated with the
                           alarm. If the identifier is None, the alarm is not
                           associated with a specific VDUR.
            alarm        - the alarm data

        """
        account = self.get_cloud_account(account_name)
        vim_id = self.cache.to_vim_id(vdur_id)

        # If the launchpad has a public IP, augment the action webhooks to
        # include the launchpad so that the alarms can be broadcast as event
        # notifications.
        if self._config.public_ip is not None:
            url = "http://{host}:4568/{platform}/{vdur_id}".format(
                    host=self._config.public_ip,
                    platform=account.account_type,
                    vdur_id=vudr_id,
                    )
            alarm.actions.ok.add().url = url + "/ok"
            alarm.actions.alarm.add().url = url + "/alarm"
            alarm.actions.alarm.add().url = url + "/insufficient_data"

        yield from self._nfvi_interface.alarm_create(account, vim_id, alarm)

        # Associate the VDUR ID with the alarm ID
        self._alarms[vdur_id].append((account_name, alarm.alarm_id))

    @asyncio.coroutine
    def destroy_alarm(self, account_name, alarm_id):
        """Destroy an existing alarm

        Arugments:
            account_name - the name of the account that owns the alert
            alarm_id     - the identifier of the alarm to destroy

        """
        account = self.get_cloud_account(account_name)
        yield from self._nfvi_interface.alarm_destroy(account, alarm_id)
