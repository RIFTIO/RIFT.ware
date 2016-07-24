
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import dateutil.parser
import json
import logging
import urllib.parse

import requests

import gi
gi.require_version('RwTypes', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwmonYang', '1.0')

from gi.repository import (
    GObject,
    RwMon,
    RwTypes,
    RwmonYang,
    )

import rift.rwcal.openstack as openstack_drv
import rw_status
import rwlogger

logger = logging.getLogger('rwmon.ceilometer')

rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    })


class UnknownService(Exception):
    pass


class CeilometerMonitoringPlugin(GObject.Object, RwMon.Monitoring):
    def __init__(self):
        GObject.Object.__init__(self)
        self._driver_class = openstack_drv.OpenstackDriver

    def _get_driver(self, account):
        return self._driver_class(username = account.openstack.key,
                                  password = account.openstack.secret,
                                  auth_url = account.openstack.auth_url,
                                  tenant_name = account.openstack.tenant,
                                  mgmt_network = account.openstack.mgmt_network)

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    category="rw-monitor-log",
                    subcategory="ceilometer",
                    log_hdl=rwlog_ctx,
                )
            )

    @rwstatus(ret_on_failure=[None])
    def do_nfvi_metrics(self, account, vm_id):
        try:
            samples = self._get_driver(account).ceilo_nfvi_metrics(vm_id)

            metrics = RwmonYang.NfviMetrics()

            vcpu = samples.get("cpu_util", {})
            memory = samples.get("memory_usage", {})
            storage = samples.get("disk_usage", {})

            metrics.vcpu.utilization = vcpu.get("volume", 0)
            metrics.memory.used = memory.get("volume", 0)
            metrics.storage.used = storage.get("volume", 0)

            def convert_timestamp(t):
                return dateutil.parser.parse(t).timestamp()

            timestamps = []
            if 'timestamp' in vcpu:
                timestamps.append(convert_timestamp(vcpu['timestamp']))
            if 'timestamp' in memory:
                timestamps.append(convert_timestamp(memory['timestamp']))
            if 'timestamp' in storage:
                timestamps.append(convert_timestamp(storage['timestamp']))

            metrics.timestamp = max(timestamps) if timestamps else 0.0

            return metrics

        except Exception as e:
            logger.exception(e)

    @rwstatus(ret_on_failure=[None])
    def do_nfvi_vcpu_metrics(self, account, vm_id):
        try:
            samples = self._get_driver(account).ceilo_nfvi_metrics(vm_id)

            metrics = RwmonYang.NfviMetrics_Vcpu()
            metrics.utilization = samples.get("cpu_util", 0)

            return metrics

        except Exception as e:
            logger.exception(e)

    @rwstatus(ret_on_failure=[None])
    def do_nfvi_memory_metrics(self, account, vm_id):
        try:
            samples = self._get_driver(account).ceilo_nfvi_metrics(vm_id)

            metrics = RwmonYang.NfviMetrics_Memory()
            metrics.used = samples.get("memory_usage", 0)

            return metrics

        except Exception as e:
            logger.exception(e)

    @rwstatus(ret_on_failure=[None])
    def do_nfvi_storage_metrics(self, account, vm_id):
        try:
            samples = self._get_driver(account).ceilo_nfvi_metrics(vm_id)

            metrics = RwmonYang.NfviMetrics_Storage()
            metrics.used = samples.get("disk_usage", 0)

            return metrics

        except Exception as e:
            logger.exception(e)

    @rwstatus(ret_on_failure=[False])
    def do_nfvi_metrics_available(self, account):
        try:
            endpoint = self._get_driver(account).ceilo_meter_endpoint()
        except Exception:
            return False

        return endpoint is not None

    @rwstatus(ret_on_failure=[None])
    def do_alarm_create(self, account, vim_id, alarm):
        # Retrieve a token using account information
        token = openstack_auth_token(account)
        service = token.service("ceilometer")
        headers = {"content-type": "application/json", "x-auth-token": token.id}

        # Convert the alarm from its YANG representation into something that
        # can be passed to the openstack interface
        ceilometer_alarm = CeilometerAlarm.from_gi_obj(alarm, vim_id).to_dict()

        # POST the data to ceilometer
        response = requests.post(
                service.url.public + "/v2/alarms",
                headers=headers,
                data=json.dumps(ceilometer_alarm),
                timeout=5,
                )

        # Returns the response object and update the alarm ID
        obj = response.json()
        alarm.alarm_id = obj['alarm_id']
        return obj

    @rwstatus(ret_on_failure=[None])
    def do_alarm_update(self, account, alarm):
        # Retrieve a token using account information
        token = openstack_auth_token(account)
        service = token.service("ceilometer")
        headers = {"content-type": "application/json", "x-auth-token": token.id}

        # Convert the alarm from its YANG representation into something that
        # can be passed to the openstack interface
        ceilometer_alarm = CeilometerAlarm.from_gi_obj(alarm).to_dict()

        # PUT the data to ceilometer
        response = requests.put(
                service.url.public + "/v2/alarms/{}".format(alarm.alarm_id),
                headers=headers,
                data=json.dumps(ceilometer_alarm),
                timeout=5,
                )

        return response.json()

    @rwstatus(ret_on_failure=[None])
    def do_alarm_delete(self, account, alarm_id):
        # Retrieve a token using account information
        token = openstack_auth_token(account)
        service = token.service("ceilometer")
        headers = {"content-type": "application/json", "x-auth-token": token.id}

        # DELETE the alarm
        _ = requests.delete(
                service.url.public + "/v2/alarms/{}".format(alarm_id),
                headers=headers,
                timeout=5,
                )

    @rwstatus(ret_on_failure=[None])
    def do_alarm_list(self, account):
        # Retrieve a token using account information
        token = openstack_auth_token(account)
        service = token.service("ceilometer")
        headers = {"x-auth-token": token.id}

        # GET a list of alarms
        response = requests.get(
                service.url.public + "/v2/alarms",
                headers=headers,
                timeout=5,
                )

        return response.json()


class OpenstackAuthTokenV2(object):
    def __init__(self, data):
        self._data = data

    @classmethod
    def request(cls, account):
        """Create an OpenstackAuthTokenV2 using account information

        Arguments:
            account - an RwcalYang.CloudAccount object

        Returns:
            an openstack token

        """
        headers = {"content-type": "application/json"}
        data = json.dumps({
            "auth": {
                "tenantName": account.openstack.tenant,
                "passwordCredentials": {
                    "username": account.openstack.key,
                    "password": account.openstack.secret,
                    }
                }
            })

        url = "{}/tokens".format(account.openstack.auth_url)
        response = requests.post(url, headers=headers, data=data)
        response.raise_for_status()

        return cls(response.json())

    @property
    def id(self):
        """The token identifier"""
        return self._data["access"]["token"]["id"]

    def service(self, name):
        """Returns information about the specified service

        Arguments:
            name - the name of the service to return

        Raises:
            If the requested service cannot be found, an UnknownService
            exception is raised.

        Returns:
            an OpenstackService object

        """
        for s in self._data["access"]["serviceCatalog"]:
            if s["name"] == name:
                return OpenstackService(
                        name=name,
                        url=OpenstackServiceURLs(
                            public=s["endpoints"][0]["publicURL"],
                            internal=s["endpoints"][0]["internalURL"],
                            admin=s["endpoints"][0]["adminURL"],
                            )
                        )

        raise UnknownService(name)


class OpenstackAuthTokenV3(object):
    def __init__(self, token, data):
        self._data = data
        self._token = token

    @classmethod
    def request(cls, account):
        """Create an OpenstackAuthTokenV3 using account information

        Arguments:
            account - an RwcalYang.CloudAccount object

        Returns:
            an openstack token

        """
        headers = {"content-type": "application/json"}
        data = json.dumps({
            "auth": {
                "identity": {
                    "methods": ["password"],
                    "password": {
                        "user": {
                            "name": account.openstack.key,
                            "password": account.openstack.secret,
                            "domain": {"id": "default"},
                            }
                        }
                    },
                "scope": {
                    "project": {
                        "name": account.openstack.tenant,
                        "domain": {"id": "default"},
                        }
                    }
                }
            })

        url = account.openstack.auth_url + "/auth/tokens"

        response = requests.post(url, headers=headers, data=data)
        response.raise_for_status()

        return cls(response.headers['x-subject-token'], response.json())

    @property
    def id(self):
        """The token identifier"""
        return self._token

    def service(self, name):
        """Returns information about the specified service

        Arguments:
            name - the name of the service to return

        Raises:
            If the requested service cannot be found, an UnknownService
            exception is raised.

        Returns:
            an OpenstackService object

        """
        for s in self._data["token"]["catalog"]:
            if s["name"] == name:
                endpoints = {e["interface"]:e["url"] for e in s["endpoints"]}
                return OpenstackService(
                        name=name,
                        url=OpenstackServiceURLs(
                            public=endpoints["public"],
                            internal=endpoints["internal"],
                            admin=endpoints["admin"],
                            )
                        )

        raise UnknownService(name)


def openstack_auth_token(account):
    url = urllib.parse.urlparse(account.openstack.auth_url)

    if url.path in ('/v3',):
        return OpenstackAuthTokenV3.request(account)

    if url.path in ('/v2.0', 'v2.1'):
        return OpenstackAuthTokenV2.request(account)

    raise ValueError("Unrecognized keystone version")


class OpenstackService(collections.namedtuple(
    "OpenstackServer",
    "name url")):
    pass


class OpenstackServiceURLs(collections.namedtuple(
    "OpenstackServiceURLs",
    "public internal admin")):
    pass


class CeilometerAlarm(collections.namedtuple(
    "CeilometerAlarm",
    "name type description severity repeat_actions enabled alarm_actions ok_actions insufficient_data_actions threshold_rule")):
    @classmethod
    def from_gi_obj(cls, alarm, vim_id=None):
        severity = CeilometerAlarmSeverity.from_gi_obj(alarm.severity).severity
        actions = CeilometerAlarmActions.from_gi_obj(alarm.actions)

        alarm_id = alarm.alarm_id if vim_id is None else vim_id
        threshold_rule = CeilometerThresholdRule.from_gi_obj(alarm_id, alarm)

        return cls(
                type="threshold",
                name=alarm.name,
                description=alarm.description,
                severity=severity,
                repeat_actions=alarm.repeat,
                enabled=alarm.enabled,
                threshold_rule=threshold_rule,
                ok_actions=actions.ok,
                alarm_actions=actions.alarm,
                insufficient_data_actions=actions.insufficient_data,
                )

    def to_dict(self):
        """Returns a dictionary containing the tuple data"""
        def recursive_to_dict(obj):
            if not hasattr(obj, '_fields'):
                return obj

            return {k: recursive_to_dict(getattr(obj, k)) for k in obj._fields}

        return recursive_to_dict(self)


class CeilometerThresholdRule(collections.namedtuple(
    "CeilometerThresholdRule",
    "evaluation_periods threshold statistic meter_name comparison_operator period query")):
    @classmethod
    def from_gi_obj(cls, vim_id, alarm):
        meter = CeilometerAlarmMeter.from_gi_obj(alarm.metric).meter
        statistic = CeilometerAlarmStatistic.from_gi_obj(alarm.statistic).statistic
        operation = CeilometerAlarmOperation.from_gi_obj(alarm.operation).operation

        return cls(
                evaluation_periods=alarm.evaluations,
                threshold=alarm.value,
                statistic=statistic,
                meter_name=meter,
                comparison_operator=operation,
                period=alarm.period,
                query=[{
                    "op": "eq",
                    "field": "resource_id",
                    "value": vim_id,
                    }]
                )

    def to_dict(self):
        """Returns a dictionary containing the tuple data"""
        def recursive_to_dict(obj):
            if not hasattr(obj, '_fields'):
                return obj

            return {k: recursive_to_dict(getattr(obj, k)) for k in obj._fields}

        return recursive_to_dict(self)


class CeilometerAlarmMeter(collections.namedtuple(
    "CeiloemterAlarmMeter",
    "meter")):
    __mapping__ = {
            "CPU_UTILIZATION" : "cpu_util",
            "MEMORY_UTILIZATION" : "memory_usage",
            "STORAGE_UTILIZATION" : "disk_usage",
            }
    @classmethod
    def from_gi_obj(cls, meter):
        return cls(meter=cls.__mapping__[meter])


class CeilometerAlarmStatistic(collections.namedtuple(
    "CeilometerAlarmStatistic",
    "statistic")):
    __mapping__ = {
            "AVERAGE": "avg",
            "MINIMUM": "min",
            "MAXIMUM": "max",
            "COUNT": "count",
            "SUM": "sum",
            }
    @classmethod
    def from_gi_obj(cls, statistic):
        return cls(statistic=cls.__mapping__[statistic])


class CeilometerAlarmOperation(collections.namedtuple(
    "CeilometerAlarmOperation",
    "operation")):
    __mapping__ = {
            "LT": "lt",
            "LE": "le",
            "EQ": "eq",
            "GE": "ge",
            "GT": "gt",
            }
    @classmethod
    def from_gi_obj(cls, operation):
        return cls(operation=cls.__mapping__[operation])


class CeilometerAlarmSeverity(collections.namedtuple(
    "CeilometerAlarmSeverity",
    "severity")):
    __mapping__ = {
            "LOW": "low",
            "MODERATE": "moderate",
            "CRITICAL": "critical",
            }
    @classmethod
    def from_gi_obj(cls, severity):
        return cls(severity=cls.__mapping__[severity])


class CeilometerAlarmActions(collections.namedtuple(
    "CeilometerAlarmActions",
    "ok alarm insufficient_data")):
    @classmethod
    def from_gi_obj(cls, actions):
        return cls(
            ok=[obj.url for obj in actions.ok],
            alarm=[obj.url for obj in actions.alarm],
            insufficient_data=[obj.url for obj in actions.insufficient_data],
            )
