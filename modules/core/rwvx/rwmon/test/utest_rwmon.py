#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import os
import unittest
import xmlrunner

import rw_peas

import gi
gi.require_version("RwcalYang", "1.0")
gi.require_version("RwMon", "1.0")
gi.require_version("RwmonYang", "1.0")
gi.require_version("RwTypes", "1.0")


from gi.repository import (
        RwmonYang,
        RwcalYang,
        RwTypes,
        )


class TestNullDataSource(unittest.TestCase):
    def setUp(self):
        plugin = rw_peas.PeasPlugin("rwmon_mock", 'RwMon-1.0')
        self.plugin = plugin.get_interface("Monitoring")

        self.account = RwcalYang.CloudAccount()
        self.vim_id = "test-vim-id"

    def test_null_data_source(self):
        """
        By default, the NFVI metrics plugin mock installs a 'null'
        implementation that simply returns empty NFVI structures.

        """
        status, metrics = self.plugin.nfvi_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics, RwmonYang.NfviMetrics())

        status, metrics = self.plugin.nfvi_vcpu_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics, RwmonYang.NfviMetrics_Vcpu())

        status, metrics = self.plugin.nfvi_memory_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics, RwmonYang.NfviMetrics_Memory())

        status, metrics = self.plugin.nfvi_storage_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics, RwmonYang.NfviMetrics_Storage())

        status, result = self.plugin.nfvi_metrics_available(self.account)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertTrue(result)


class TestMockDataSource(unittest.TestCase):
    def setUp(self):
        plugin = rw_peas.PeasPlugin("rwmon_mock", 'RwMon-1.0')
        self.plugin = plugin.get_interface("Monitoring")
        self.plugin.set_impl(MockDataSource())

        self.account = RwcalYang.CloudAccount()
        self.vim_id = "test-vim-id"

    def test_mock_data_source(self):
        """
        This test installs a mock data source implementation in the plugin,
        which returns known values. This test simply checks the expected values
        are indeed returned.

        """
        expected_vcpu_metrics = RwmonYang.NfviMetrics_Vcpu()
        expected_vcpu_metrics.utilization = 50.0
        expected_vcpu_metrics.total = 100

        status, metrics = self.plugin.nfvi_vcpu_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics.total, expected_vcpu_metrics.total)
        self.assertEqual(metrics.utilization, expected_vcpu_metrics.utilization)

        expected_memory_metrics = RwmonYang.NfviMetrics_Memory()
        expected_memory_metrics.used = 90
        expected_memory_metrics.total = 100
        expected_memory_metrics.utilization = 90/100

        status, metrics = self.plugin.nfvi_memory_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics.used, expected_memory_metrics.used)
        self.assertEqual(metrics.total, expected_memory_metrics.total)
        self.assertEqual(metrics.utilization, expected_memory_metrics.utilization)

        expected_storage_metrics = RwmonYang.NfviMetrics_Storage()
        expected_storage_metrics.used = 300
        expected_storage_metrics.total = 500
        expected_storage_metrics.utilization = 300/500

        status, metrics = self.plugin.nfvi_storage_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics.used, expected_storage_metrics.used)
        self.assertEqual(metrics.total, expected_storage_metrics.total)
        self.assertEqual(metrics.utilization, expected_storage_metrics.utilization)

        status, metrics = self.plugin.nfvi_metrics(self.account, self.vim_id)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertEqual(metrics.vcpu.total, expected_vcpu_metrics.total)
        self.assertEqual(metrics.vcpu.utilization, expected_vcpu_metrics.utilization)
        self.assertEqual(metrics.storage.used, expected_storage_metrics.used)
        self.assertEqual(metrics.storage.total, expected_storage_metrics.total)
        self.assertEqual(metrics.storage.utilization, expected_storage_metrics.utilization)
        self.assertEqual(metrics.memory.used, expected_memory_metrics.used)
        self.assertEqual(metrics.memory.total, expected_memory_metrics.total)
        self.assertEqual(metrics.memory.utilization, expected_memory_metrics.utilization)

        status, result = self.plugin.nfvi_metrics_available(self.account)
        self.assertEqual(status, RwTypes.RwStatus.SUCCESS)
        self.assertTrue(result)


class TestMockAlarms(unittest.TestCase):
    def setUp(self):
        plugin = rw_peas.PeasPlugin("rwmon_mock", 'RwMon-1.0')

        self.mock = MockAlarmInterface()
        self.plugin = plugin.get_interface("Monitoring")
        self.plugin.set_impl(self.mock)

        self.account = RwcalYang.CloudAccount()
        self.alarm = RwmonYang.Alarm(name='test-alarm')
        self.vim_id = 'test-vim-id'

    def test(self):
        """
        This test uses a simple, mock implementation of the alarm interface to
        check that create, update, delete, and list work correctly.

        """
        # In the beginning, there were no alarms
        _, alarms = self.plugin.do_alarm_list(self.account)
        self.assertEqual(0, len(alarms))

        # Create two alarms
        self.plugin.do_alarm_create(self.account, self.vim_id, RwmonYang.Alarm())
        self.plugin.do_alarm_create(self.account, self.vim_id, RwmonYang.Alarm())

        _, alarms = self.plugin.do_alarm_list(self.account)
        self.assertEqual(2, len(alarms))

        # The alarms should have no names
        alarms.sort(key=lambda a: a.alarm_id)
        self.assertEqual('test-alarm-id-1', alarms[0].alarm_id)
        self.assertEqual('test-alarm-id-2', alarms[1].alarm_id)
        self.assertTrue(all(a.name is None for a in alarms))

        # Give names to the alarms
        alarms[0].name = 'test-alarm'
        alarms[1].name = 'test-alarm'
        self.plugin.do_alarm_update(self.account, alarms[0])
        self.plugin.do_alarm_update(self.account, alarms[1])
        self.assertTrue(all(a.name == 'test-alarm' for a in alarms))

        # Delete the alarms
        self.plugin.do_alarm_delete(self.account, alarms[0].alarm_id)
        self.plugin.do_alarm_delete(self.account, alarms[1].alarm_id)
        _, alarms = self.plugin.do_alarm_list(self.account)
        self.assertEqual(0, len(alarms))


class MockAlarmInterface(object):
    """
    This class is mock impementation for the alarm interface on the monitoring
    plugin.
    """

    def __init__(self):
        self.count = 0
        self.alarms = dict()

    def alarm_create(self, account, vim_id, alarm):
        self.count += 1
        alarm_id = 'test-alarm-id-{}'.format(self.count)
        alarm.alarm_id = alarm_id
        self.alarms[alarm_id] = alarm

    def alarm_update(self, account, alarm):
        assert alarm.alarm_id is not None
        self.alarms[alarm.alarm_id] = alarm

    def alarm_delete(self, account, alarm_id):
        del self.alarms[alarm_id]

    def alarm_list(self, account):
        return list(self.alarms.values())


class MockDataSource(object):
    """
    This class implements the data source interface used by the monitoring
    plugin and provides mock data for testing.
    """

    def nfvi_metrics(self, account, vm_id):
        metrics = RwmonYang.NfviMetrics()
        metrics.vcpu = self.nfvi_vcpu_metrics(account, vm_id)
        metrics.memory = self.nfvi_memory_metrics(account, vm_id)
        metrics.storage = self.nfvi_storage_metrics(account, vm_id)
        return metrics

    def nfvi_vcpu_metrics(self, account, vm_id):
        metrics = RwmonYang.NfviMetrics_Vcpu()
        metrics.total = 100
        metrics.utilization = 50.0
        return metrics

    def nfvi_memory_metrics(self, account, vm_id):
        metrics = RwmonYang.NfviMetrics_Memory()
        metrics.used = 90
        metrics.total = 100
        metrics.utilization = 90/100
        return metrics

    def nfvi_storage_metrics(self, account, vm_id):
        metrics = RwmonYang.NfviMetrics_Storage()
        metrics.used = 300
        metrics.total = 500
        metrics.utilization = 300/500
        return metrics

    def nfvi_metrics_available(self, account):
        return True


def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)


if __name__ == '__main__':
    main()

# vim: sw=4
