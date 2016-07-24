
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import pytest
import os
import subprocess
import sys

import rift.auto.log
import rift.auto.session
import rift.vcs.vcs
import logging

import gi
gi.require_version('RwCloudYang', '1.0')
gi.require_version('RwMcYang', '1.0')

from gi.repository import RwMcYang, RwCloudYang

@pytest.fixture(scope='session')
def cloud_name_prefix():
    '''fixture which returns the prefix used in cloud account names'''
    return 'cloud'

@pytest.fixture(scope='session')
def cloud_account_name(cloud_name_prefix):
    '''fixture which returns the name used to identify the cloud account'''
    return '{prefix}-0'.format(prefix=cloud_name_prefix)

@pytest.fixture(scope='session')
def mgmt_domain_name():
    '''fixture which returns the name used to identify the mgmt_domain'''
    return 'mgmt-0'

@pytest.fixture(scope='session')
def vm_pool_name():
    '''fixture which returns the name used to identify the vm resource pool'''
    return 'vm-0'

@pytest.fixture(scope='session')
def network_pool_name():
    '''fixture which returns the name used to identify the network resource pool'''
    return 'net-0'

@pytest.fixture(scope='session')
def port_pool_name():
    '''fixture which returns the name used to identify the port resource pool'''
    return 'port-0'

@pytest.fixture(scope='session')
def sdn_account_name():
    '''fixture which returns the name used to identify the sdn account'''
    return 'sdn-0'

@pytest.fixture(scope='session')
def sdn_account_type():
    '''fixture which returns the account type used by the sdn account'''
    return 'odl'

@pytest.fixture(scope='session', autouse=True)
def _riftlog_scraper_session(log_manager, confd_host, rsyslog_dir):
    '''Fixture which returns an instance of rift.auto.log.FileSource to scrape riftlog

    Arguments:
        log_manager - manager of logging sources and sinks
        confd_host - host on which confd is running (mgmt_ip)
    '''
    if rsyslog_dir:
        return None
    scraper = rift.auto.log.FileSource(host=confd_host, path='/var/log/rift/rift.log')
    scraper.skip_to('Configuration management startup complete.')
    log_manager.source(source=scraper)
    return scraper

@pytest.fixture(scope='session')
def cloud_module(standalone_launchpad):
    '''Fixture containing the module which defines cloud account

    Depending on whether or not the system is being run with a standalone
    launchpad, a different module will be used to configure the cloud
    account

    Arguments:
        standalone_launchpad - fixture indicating if the system is being run with a standalone launchpad

    Returns:
        module to be used when configuring a cloud account
    '''
    cloud_module = RwMcYang
    if standalone_launchpad:
        cloud_module = RwCloudYang
    return cloud_module

@pytest.fixture(scope='session')
def cloud_xpath(standalone_launchpad):
    '''Fixture containing the xpath that should be used to configure a cloud account

    Depending on whether or not the system is being run with a standalone
    launchpad, a different xpath will be used to configure the cloud
    account

    Arguments:
        standalone_launchpad - fixture indicating if the system is being run with a standalone launchpad

    Returns:
        xpath to be used when configure a cloud account
    '''
    xpath = '/cloud-account/account'
    if standalone_launchpad:
        xpath = '/cloud/account'
    return xpath

@pytest.fixture(scope='session')
def cloud_accounts(cloud_module, cloud_name_prefix, cloud_host, cloud_user, cloud_tenants, cloud_type):
    '''fixture which returns a list of CloudAccounts. One per tenant provided

    Arguments:
        cloud_module        - fixture: module defining cloud account
        cloud_name_prefix   - fixture: name prefix used for cloud account
        cloud_host          - fixture: cloud host address
        cloud_user          - fixture: cloud account user key
        cloud_tenants       - fixture: list of tenants to create cloud accounts on
        cloud_type          - fixture: cloud account type

    Returns:
        A list of CloudAccounts
    '''
    accounts = []
    for idx, cloud_tenant in enumerate(cloud_tenants):
        cloud_account_name = "{prefix}-{idx}".format(prefix=cloud_name_prefix, idx=idx)

        if cloud_type == 'lxc':
            accounts.append(
                    cloud_module.CloudAccount.from_dict({
                        "name": cloud_account_name,
                        "account_type": "cloudsim"})
            )
        elif cloud_type == 'openstack':
            password = 'mypasswd'
            auth_url = 'http://{cloud_host}:5000/v3/'.format(cloud_host=cloud_host)
            mgmt_network = os.getenv('MGMT_NETWORK', 'private')
            accounts.append(
                    cloud_module.CloudAccount.from_dict({
                        'name':  cloud_account_name,
                        'account_type': 'openstack',
                        'openstack': {
                            'admin': True,
                            'key': cloud_user,
                            'secret': password,
                            'auth_url': auth_url,
                            'tenant': cloud_tenant,
                            'mgmt_network': mgmt_network}})
            )
    return accounts


@pytest.fixture(scope='session', autouse=True)
def cloud_account(cloud_accounts):
    '''fixture which returns an instance of RwMcYang.CloudAccount

    Arguments:
        cloud_accounts - fixture: list of generated cloud accounts

    Returns:
        An instance of CloudAccount
    '''
    return cloud_accounts[0]

@pytest.fixture(scope='session')
def _launchpad_scraper_session(request, log_manager, mgmt_domain_name):
    '''fixture which returns an instance of rift.auto.log_scraper.FileSource to scrape the launchpad

    Arguments:
        log_manager - manager of log sources and sinks
        mgmt_domain_name - the management domain created for the launchpad
    '''
    if request.config.getoption("--lp-standalone"):
        return

    scraper = rift.auto.log.FileSource(host=None, path='/var/log/launchpad_console.log')
    log_manager.source(source=scraper)
    return scraper

@pytest.fixture(scope='function', autouse=False)
def _connect_launchpad_scraper(request, _launchpad_scraper_session, mgmt_session, mgmt_domain_name, standalone_launchpad):
    '''Determines the address of the launchpad and connects the launchpad scraper to it
       Needed because the launchpad address isn't known at the start of the test session.

    Arguments:
        mgmt_session - management interface session
        _launchpad_scraper_session - scraper responsible for collecting launchpad_console log
        mgmt_domain_name - mgmt-domain in which the launchpad is located
    '''
    if standalone_launchpad:
        return

    if not _launchpad_scraper_session.connected():
        proxy = mgmt_session.proxy(RwMcYang)
        launchpad_address = proxy.get("/mgmt-domain/domain[name='%s']/launchpad/ip_address" % mgmt_domain_name)
        if launchpad_address:
            _launchpad_scraper_session.connect(launchpad_address)

@pytest.fixture(scope='session')
def launchpad_scraper(_launchpad_scraper_session):
    '''Fixture exposing the scraper used to scrape the launchpad console log

    Arguments:
        _launchpad_scraper_session - instance of rift.auto.log_scraper.FileSource targeting the launchpad console log
    '''
    return _launchpad_scraper_session

