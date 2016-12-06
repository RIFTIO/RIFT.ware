
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

import _pytest.mark
import _pytest.runner

import argparse
import itertools
import os
import pytest
import re
import subprocess
import uuid

from collections import OrderedDict

from rift.rwlib.util import certs
import rift.auto.log
import rift.auto.session
import rift.auto.mano
import rift.vcs.vcs
import logging
import logging.handlers

def pytest_addoption(parser):
    """pytest hook
    Add arguments options to the py.test command line

    Arguments:
        parser - An instance of _pytest.config.Parser
    """

    # --confd-host <addr>
    #   Host address of the confd instance
    parser.addoption("--confd-host", action="store", default="127.0.0.1")

    # --cloud-host <addr>
    #   Host address of cloud provider
    parser.addoption("--cloud-host", action="store", default="127.0.0.1")

    # --repeat <times>
    #   Number of times to repeat the entire test
    parser.addoption("--repeat", action="store", default=1)

    # --repeat-keyword <keywordexpr>
    #   Only repeat tests selected by pytest keyword expression
    parser.addoption("--repeat-keyword", action="store", default="")

    # --repeat-mark <markexpr>
    #   Only repeat tests selected by a pytest mark expression
    parser.addoption("--repeat-mark", action="store", default="")

    # The following options specify which session type to use
    # if multiple of these options are specified tests will run for each type specified

    # --netconf
    #   Provide a netconf session to test cases
    parser.addoption("--netconf", dest='session_type', action="append_const", const='netconf')

    # --restconf
    #   Provide a restconf session to test cases
    parser.addoption("--restconf", dest='session_type', action="append_const", const='restconf')

    # --lxc
    #   Use lxc cloud account
    parser.addoption("--lxc", dest="cloud_type", action="append_const", const="lxc")


    # --mock
    #   Use mock cloud account
    parser.addoption("--mock", dest="cloud_type", action="append_const", const="mock")

    # --openstack
    #   Use openstack cloud account
    parser.addoption("--openstack", dest="cloud_type", action="append_const", const="openstack")

    # --slow --include-slow
    #   Include tests marked slow in test run
    parser.addoption("--slow", dest="include_slow", action="store_true")
    parser.addoption("--include-slow", dest="include_slow", action="store_true")

    # --launchpad-vm-id
    #   Assign the vm matching this id to vm pool (used to pipe vm_id from resource creation to tests)
    parser.addoption("--launchpad-vm-id", dest="vm_id", action="store")

    # --fail-on-error
    #   Raise exception on errors observed in the system log
    parser.addoption("--fail-on-error", action="store_true")

    rift_artifacts = os.environ['RIFT_ARTIFACTS']
    # --log-stdout
    #   Indicates the location the host rift instance is logging stdout to
    parser.addoption("--log-stdout", action="store", default="%s/systemtest_stdout.log" % (rift_artifacts))

    # --log-stderr
    #   Indicates the location the host rift instance is logging stderr to
    parser.addoption("--log-stderr", action="store", default="%s/systemtest_stderr.log" % (rift_artifacts))

    # --network-service
    #   The network service to be on-boarded
    choices = ["pingpong", "haproxy", "pingpong_noimg"]
    parser.addoption("--network-service", type="choice", action="store", default="pingpong", choices=choices)

    # --no-update <addr>
    #   If specified don't run the descriptors with update APIs
    parser.addoption("--no-update", dest="disabled_features", action="append_const", const='update-api', default=[])

    # --recovery
    #   If specified run tests specific to recovery
    parser.addoption("--recovery", dest="enabled_features", action="append_const", const='recovery', default=[])

    # --no-upload-image
    #   If specified don't upload images needed for the network service
    parser.addoption("--no-upload-image", dest="disabled_features", action="append_const", const='upload-image', default=[])

    # --tenants
    #   List of tenants to run the test on in the cloud-host.
    parser.addoption("--tenants", type=rift.auto.mano.Openstack.parse_tenants)

    # --tenant
    #   List of tenants attached to test run
    parser.addoption("--tenant", action="append", default=[])

    # --user
    #   User to use when creating cloud accounts
    parser.addoption("--user", action="store", default=os.getenv('USER', 'pluto'))

    # --use-xml-mode
    #   System is running using xml agent
    parser.addoption("--use-xml-mode", action="store_true")

@pytest.fixture(autouse=True)
def skip_disabled_features(request, logger):
    """Fixture to skip any tests that rely on a disabled feature
    """
    item_features = request.node.get_marker('feature')
    DEFAULT_DISABLED_FEATURES = ["recovery"]


    disabled_features = set(request.config.option.disabled_features).union(
                        set(DEFAULT_DISABLED_FEATURES))

    disabled_features = disabled_features.difference(
                        set(request.config.option.enabled_features))

    if item_features:
        feature_disabled = set(item_features.args).intersection(
                           disabled_features)

        if feature_disabled:
            msg = 'Test relies on disabled feature ({})'.format(feature_disabled)
            logger.info("SKIPPED - %s", msg)
            pytest.skip(msg)

@pytest.fixture(scope='session', autouse=True)
def xml_mode(request):
    """Fixture that returns --use-xml-mode option value"""
    return request.config.getoption("--use-xml-mode")

@pytest.fixture(scope='session', autouse=True)
def cloud_host(request):
    """Fixture that returns --cloud-host option value"""
    return request.config.getoption("--cloud-host")

@pytest.fixture(scope='session', autouse=True)
def cloud_tenants(request):
    """Fixture that returns the list of tenants from the --tenant option value"""
    return request.config.getoption("--tenant") or [os.getenv('PROJECT_NAME')] or [os.geteven('AUTO_USER')] or [os.getenv('USER', 'demo')]

@pytest.fixture(scope='session', autouse=True)
def cloud_tenant(cloud_tenants):
    """Fixture that returns a tenant on which to create a cloud account"""
    return cloud_tenants[0]

@pytest.fixture(scope='session', autouse=True)
def cloud_user(request):
    """Fixture that returns the user from the --user option value"""
    return request.config.getoption("--user") or os.getenv('AUTO_USER') or os.getenv('USER', 'demo')

@pytest.fixture(scope='session', autouse=True)
def confd_host(request):
    """Fixture that returns --confd-host option value"""
    return request.config.getoption("--confd-host")

@pytest.fixture(scope='session', autouse=True)
def launchpad_vm_id(request):
    """Fixture that returns --launchpad-vm-id option value"""
    return request.config.getoption("--launchpad-vm-id")

@pytest.fixture(scope='session')
def rsyslog_dir():
    return os.getenv('RW_AUTO_RSYSLOG_DIR', None)

@pytest.fixture(scope='session')
def rsyslog_host():
    return os.getenv('RW_AUTO_RSYSLOG_HOST', None)

@pytest.fixture(scope='session')
def rsyslog_port():
    port = os.getenv('RW_AUTO_RSYSLOG_PORT', None)
    if port:
        port = int(port)
    return port

@pytest.fixture(scope='session', autouse=True)
def logger(request, rsyslog_host, rsyslog_port):
    """Fixture that returns a logger"""
    logging.basicConfig(
        format='%(asctime)-15s|%(module)s|%(levelname)s|%(message)s',
        level=logging.INFO,
        handlers=[
            logging.StreamHandler(),
        ]
    )

    logger = logging.getLogger()
    if rsyslog_host and rsyslog_port:
        logger.addHandler(logging.handlers.SysLogHandler(address=(rsyslog_host, rsyslog_port)))
    logger.setLevel(logging.DEBUG if request.config.option.verbose else logging.INFO)
    return logger


@pytest.fixture(scope='session')
def scheme(request, use_https):
    """Fixture to return the protocol to be used in url queries.
    """
    if use_https:
        return "https"

    return "http"


@pytest.fixture(scope='session')
def use_https(request):
    return certs.USE_SSL


@pytest.fixture(scope='session')
def cert(request):
    """Fixture to get the cert & key to be used for https requests.
    """
    _, cert, key = certs.get_bootstrap_cert_and_key()

    return (cert, key)


@pytest.fixture(scope='session', autouse=True)
def mgmt_session(request, confd_host, session_type, cloud_type):
    """Fixture that returns mgmt_session to be used

    # Note: cloud_type's exists in this fixture to ensure it appears in test item's _genid

    Arguments:
        confd_host    - host on which confd is running
        session_type  - communication protocol to use [restconf, netconf]
        cloud_type    - cloud account type to use [lxc, openstack]
    """
    if session_type == 'netconf':
        mgmt_session = rift.auto.session.NetconfSession(host=confd_host)
    elif session_type == 'restconf':
        mgmt_session = rift.auto.session.RestconfSession(host=confd_host)

    mgmt_session.connect()
    rift.vcs.vcs.wait_until_system_started(mgmt_session)
    return mgmt_session

@pytest.fixture(scope='session')
def package_gen_script():
    install_dir = os.path.join(
        os.environ["RIFT_INSTALL"],
        "usr/rift/mano/common/rw_gen_package.py"
        )
    return install_dir

@pytest.fixture(scope='session', autouse=True)
def log_manager():
    return rift.auto.log.LogManager()

@pytest.fixture(scope='session', autouse=True)
def log_sink(log_manager):
    '''Fixture which returns an instance of rift.auto.log.Sink to serve as a collector
    for all logs
    '''
    log_id = uuid.uuid4()
    rift_artifacts = os.environ['RIFT_ARTIFACTS']
    file_sink = rift.auto.log.FileSink('%s/%s.unified.log' % (rift_artifacts, log_id))
    log_manager.sink(file_sink)
    return file_sink


@pytest.fixture(scope='function', autouse=True)
def log_test_name(request, logger):
    '''Log the name of test being executed at the start of each test'''
    logger.info('BEGIN [%s]', request._pyfuncitem.name)

@pytest.fixture(scope='function', autouse=True)
def finally_log_test_result(request, logger):
    '''Log the result of test execution at the end of each test
    '''
    def log_test_result(result):
        status = 'FAILED'
        if result:
            status = 'SUCCESS'
        logger.info('FINISH [%s]: %s', request._pyfuncitem.name, status)

    return log_test_result

@pytest.fixture(scope='function', autouse=True)
def _log_set_test_name(request, log_manager):
    ''' Fixture which sets the current test name in the log manager to
    the value of the currently executed test item

    Arguments:
        request - fixture request object
        log_manager - manager of logging sources and sinks
    '''
    log_manager.set_test_name(request._pyfuncitem.name)

@pytest.fixture(scope='session', autouse=True)
def log_recent(log_manager):
    '''Fixture which returns an instance of rift.auto.log.Sink which retains recently
    collected logs
    '''
    buffer_sink = rift.auto.log.MemorySink()
    log_manager.sink(buffer_sink)
    return buffer_sink

@pytest.fixture(scope='function', autouse=True)
def _truncate_log_recent(log_recent):
    '''Fixture which truncates the contents memory sink log_recent

    Arguments
        log_recent - sink containing recently collected logs
    '''
    log_recent.truncate(generations=0)

@pytest.fixture(scope='session', autouse=True)
def _syslog_scraper_session(request, log_manager, rsyslog_dir):
    '''Fixture which returns an instance of rift.auto.log.FileSource which scrapes
    the output of syslog (rsyslog)

    Arguments:
        request - pytest request item
        log_manager - manager of logging sources and sinks
        rsyslog_dir - directory rsyslog is logging to
    '''
    if not rsyslog_dir:
        return None
    scraper = rift.auto.log.FileSource(host='localhost', path="{rsyslog_dir}/log/syslog".format(rsyslog_dir=rsyslog_dir))
    log_manager.source(source=scraper)
    return scraper

@pytest.fixture(scope='session', autouse=True)
def _logger_scraper_session(logger, log_manager, rsyslog_dir):
    '''Fixture which returns an instance of rift.auto.log.LoggerSource to scrape logger

    Arguments:
        log_manager - manager of logging sources and sinks
    '''
    if rsyslog_dir:
        return None
    scraper = rift.auto.log.LoggerSource(logger)
    log_manager.source(source=scraper)
    return scraper

@pytest.fixture(scope='session', autouse=True)
def _stdout_scraper_session(request, log_manager, confd_host, rsyslog_dir):
    '''Fixture which returns an instance of rift.auto.log.FileSource to scrape
    stdout of the host rift process

    Arguments:
        request - pytest request item
        log_manager - manager of logging sources and sinks
        confd_host -  host on which confd is running
    '''
    if rsyslog_dir:
        return None
    scraper = rift.auto.log.FileSource(host=confd_host, path=request.config.getoption("--log-stdout"))
    log_manager.source(source=scraper)
    return scraper

def _stderr_scraper_session(request, log_manager, confd_host, rsyslog_dir):
    '''Fixture which returns an instance of rift.auto.log.FileSource to scrape
    stderr of the host rift process

    Arguments:
        request - pytest request item
        log_manager - manager of logging sources and sinks
        confd_host -  host on which confd is running
    '''
    if rsyslog_dir:
        return None
    scraper = rift.auto.log.FileSource(host=confd_host, path=request.config.getoption("--log-stderr"))
    log_manager.source(source=scraper)
    return scraper

@pytest.fixture(scope='function', autouse=True)
def finally_handle_logs(log_manager):
    '''Fixture that causes the log_manager to capture from log sources and
       distribute to log sinks

        # finally_ : the function returned by this fixture will be run unconditionally
        #            at the end of each test that uses it

    Arguments:
        log_manager - handler of logging events
    '''
    def dispatch_logs(result):
        log_manager.dispatch()

    return dispatch_logs

@pytest.fixture(scope='function', autouse=True)
def splat_log(log_recent):
    '''Fixture which emits captured logs on splat

    # splat_ : the function returned by this fixture will be run when a
    #          test that uses this fixture fails

    Arguments:
        log_recent - sink containing recently collected logs
    '''
    def on_test_failure():
        '''When a test fails emit collected logs'''
        print(log_recent.read())
    return on_test_failure

@pytest.fixture(scope='session', autouse=True)
def test_iteration(request, iteration):
    """Fixture that returns the current iteration in a repeated test"""
    return iteration

def pytest_generate_tests(metafunc):
    """pytest hook
    Allows additional tests to be generated from the results of test discovery

    Arguments:
        metafunc  - object which describes a test function discovered by pytest
    """
    if 'session_type' in metafunc.fixturenames:
        if metafunc.config.option.session_type:
            metafunc.parametrize(
                    "session_type",
                    metafunc.config.option.session_type,
                    scope="session")
        else:
            metafunc.parametrize(
                    "session_type",
                    ['restconf'],
                    scope="session")

    if 'cloud_type' in metafunc.fixturenames:
        if metafunc.config.option.cloud_type:
            metafunc.parametrize(
                    "cloud_type",
                    metafunc.config.option.cloud_type,
                    scope="session")
        else:
            # Default to use lxc
            metafunc.parametrize(
                    "cloud_type",
                    ['lxc'],
                    scope='session')

    if 'iteration' in metafunc.fixturenames:
        iterations = range(int(metafunc.config.option.repeat))
        metafunc.parametrize("iteration", iterations, scope="session")

    if 'tenant' in metafunc.fixturenames:
        metafunc.parametrize(
            "tenant",
            metafunc.config.option.tenants,
            ids=lambda val: val[0],
            scope="session")

    fixture_name = 'recover_tasklet'
    if fixture_name in metafunc.fixturenames:
        if "recovery" in metafunc.config.option.enabled_features:
            TASKLETS = ["nfvi-metrics-monitor",
                        "Resource-Manager",
                        "virtual-network-function-manager",
                        "virtual-network-service",
                        "network-services-manager"]
            metafunc.parametrize(fixture_name, TASKLETS)
        else:
            metafunc.parametrize(fixture_name, [""])

@pytest.mark.hookwrapper
def pytest_pyfunc_call(pyfuncitem):
    """pytest hook
    Allows additional steps to be added surrounding the calling of a test

    Arguments:
        pyfuncitem - test item that is being called
    """
    test_outcome = yield

    splat = False
    result = None
    try:
        result = test_outcome.get_result()
    except:
        splat = True

    for funcarg in pyfuncitem.funcargs:
        # run all 'finally_' methods attached to the test
        if funcarg.startswith('finally_'):
            pyfuncitem.funcargs[funcarg](result)

    if splat:
        # Test threw an exception (i.e. failed)
        # run all 'splat_' methods attached to the test
        # logging that occurs during a splat will be added to the captured log (unlike a finalizer)
        for funcarg in pyfuncitem.funcargs:
            if funcarg.startswith('splat_'):
                pyfuncitem.funcargs[funcarg]()

def pytest_runtest_makereport(item, call):
    """pytest hook
    Allows additions to be made to the reporting at the end of each test item

    Arguments:
        item - test item that was last run
        call - Result or Exception info of a test function invocation.
    """
    if "incremental" in item.keywords:
        if call.excinfo is not None:
            if call.excinfo.type == _pytest.runner.Skipped:
                return
            marked_xfail = item.get_marker('xfail')
            marked_teardown = item.get_marker('teardown')
            if marked_xfail or marked_teardown:
                return
            parent = item.parent
            parent._previousfailed = item

def pytest_runtest_setup(item):
    """pytest hook
    Allows for additions to be made to each test item during its setup

    Arugments:
        item - test item that is about to run
    """
    if "incremental" in item.keywords:
        previousfailed = getattr(item.parent, "_previousfailed", None)
        if previousfailed is not None:
            pytest.xfail("previous test failed (%s)" % previousfailed.name)

class TestDependencyError(Exception):
    """ Exception raised when test case dependencies cannot be
    resolved"""
    pass

def pytest_configure(config):
    """ Define a marker used for tests are designated to run
    as root only"""
    config.addinivalue_line("markers",
           "rootonly: Run only if system is running as root")
    pytest.mark.rootonly = pytest.mark.skipif(
         os.getuid() != 0,
         reason="Test wouldn't run unless system is running as root"
    )

def pytest_collection_modifyitems(session, config, items):
    """ pytest hook
    Called after collection has been performed, may filter or sort
    test items.

    Arguments:
        session - pytest session. # Note - does not appear to correspond to scope=session
        config  - contents of the pytest configuration
        items   - discovered set of test items
    """

    def sort_items(items, satisfied_deps=None):
        '''Sort items topologically based on their dependencies

        Arguments:
            items - list of items to be considered
            satisfied_deps - set of dependencies that should be considered already satisfied

        Raises:
            TestDependencyError if a cycle is found while resolving dependencies
            TestDependencyError if a required dependency is left unsatisfied

        Returns a sorted list of items
        '''
        dependencies = set([])
        if satisfied_deps:
            setup = set(satisfied_deps)
        else:
            setup = set([])
        edges = OrderedDict()
        groups = {}
        parameter_ordering_groups = {}
        for item in items:
            item_name = item.parent.parent.name + "::" + item.name

            if item.get_marker('incremental'):
                parent_name = item.parent.parent.name
                group_name = parent_name
            else:
                group_name = item_name

            if item.get_marker('preserve_fixture_order'):
                test_name = item_name.split('[')[0]
                if test_name in parameter_ordering_groups:
                    edges[(parameter_ordering_groups[test_name], item_name)] = True
                parameter_ordering_groups[test_name] = item_name

            if not group_name in groups:
                groups[group_name] = []

            groups[group_name].append(item)

            m_setup = item.get_marker('setup')
            if m_setup:
                for arg in m_setup.args:
                    setup.add(arg)
                    edges[(group_name, arg)] = True

            m_depends = item.get_marker('depends')
            if m_depends:
                for arg in m_depends.args:
                    dependencies.add(arg)
                    edges[(arg, group_name)] = True

            m_teardown = item.get_marker('teardown')
            if m_teardown:
                for arg in m_teardown.args:
                    dependencies.add(arg)
                    edges[(arg, group_name)] = True
                    edges[('teardown', group_name)] = True
            else:
                # all groups that aren't part of teardown are dependencies of teardown
                edges[(group_name, 'teardown')] = True

        if not edges:
            return []

        missing_deps = dependencies - setup
        if missing_deps != set([]):
            raise TestDependencyError("Failed to satisfy all test dependencies.\nMissing dependencies: {}".format(', '.join(missing_deps)))

        edge_list = ' '.join(itertools.chain(*edges))
        sorted_items = subprocess.check_output('echo "{}" | tsort -'.format(edge_list), shell=True).decode()
        if sorted_items.find('input contains a loop') != -1:
            raise TestDependencyError("Cyclical dependency detected in test sequence")

        sorted_groups = sorted_items.split('\n')

        sorted_items = []
        for group_name in sorted_groups:
            if group_name in groups:
                sorted_items.extend(groups[group_name])

        return sorted_items

    def filter_items(items, filter_value):
        filtered_items = []
        for item in items:

            if item.get_marker("slow") and not item.config.option.include_slow:
                continue

            if item._genid.find(filter_value) != -1:
                filtered_items.append(item)
        return filtered_items

    def repeat_filter(items, config):
        '''Filter items to be repeated using the repeat_keyword and repeat_mark flags

        Arguments:
            items - list of test items to be considered
            config - pytest configuration

        Returns a list of test items to be repeated.
        '''
        keywordexpr = config.option.repeat_keyword
        matchexpr = config.option.repeat_mark

        if not keywordexpr and not matchexpr:
            return []

        if keywordexpr.startswith("-"):
            keywordexpr = "not " + keywordexpr[1:]

        selectuntil = False
        if keywordexpr[-1:] == ":":
            selectuntil = True
            keywordexpr = keywordexpr[:-1]

        remaining = []
        for colitem in items:

            try:
                if keywordexpr and not _pytest.mark.matchkeyword(colitem, keywordexpr):
                    continue
            except:
                if keywordexpr and not _pytest.mark.skipbykeyword(colitem, keywordexpr):
                    continue

            if selectuntil:
                keywordexpr = None

            if matchexpr:
                if not _pytest.mark.matchmark(colitem, matchexpr):
                    continue

            remaining.append(colitem)

        return remaining

    def generate_global_ordering(items, config):
        '''Create a single global ordering for all tests
        test items are divided into one_shot_items which should be invoked only once
        and repeat_items, which should be invoked on each test iteration

        Global test item ordering:
          one_shot_items - setup
          one_shot_items

          per protocol
            per repeat iteration
              repeat_items - setup
              repeat_items
              repeat_items - teardown

          one_shot_items - teardown

        Arguments:
            items - list of test items to be considered
            config - pytest configuration

        Returns:
            globally ordered list of test items
        '''
        ordered_items = []
        iterations = range(int(config.option.repeat))

        repeat_items = repeat_filter(items, config)
        if not repeat_items:
            # If there are no items specified for repeat the set of items to repeat is all items
            one_shot_itemlist = []
            repeat_items = items
        else:
            repeat_items_set = set(repeat_items)
            one_shot_itemlist = [item for item in items if item not in repeat_items_set]

        need_deps = False
        for item in items:
            for marker_type in ['setup','depends','teardown']:
                if item.get_marker(marker_type):
                    need_deps = True
                    break

            if need_deps:
                break


        satisfied_deps = set()
        if one_shot_itemlist:
            # make sure to use only using one implementation of the one_shot_items as they aren't being repeated
            if need_deps:
                for item_type in ['netconf-mock','restconf-mock', 'netconf-lxc','restconf-lxc','netconf-openstack','restconf-openstack']:
                    one_shot_items = sort_items(filter_items(one_shot_itemlist, '{}-0'.format(item_type)))
                    if one_shot_items:
                        break
            else:
                for item_type in ['netconf-mock','restconf-mock','netconf-lxc','restconf-lxc','netconf-openstack','restconf-openstack']:
                    one_shot_items = filter_items(one_shot_itemlist, '{}-0'.format(item_type))
                    if one_shot_items:
                        break

            item_index = 0
            while item_index < len(one_shot_items):
                item = one_shot_items[item_index]
                setup = item.get_marker('setup')
                if setup:
                    [satisfied_deps.add(arg) for arg in setup.args]
                if item.get_marker('teardown'):
                    break
                item_index += 1

            ordered_items.extend(one_shot_items[:item_index])

        if need_deps:
            for iteration in iterations:
                ordered_items.extend(sort_items(filter_items(repeat_items, 'netconf-mock-{}'.format(iteration)), satisfied_deps))
                ordered_items.extend(sort_items(filter_items(repeat_items, 'restconf-mock-{}'.format(iteration)), satisfied_deps))
                ordered_items.extend(sort_items(filter_items(repeat_items, 'netconf-lxc-{}'.format(iteration)), satisfied_deps))
                ordered_items.extend(sort_items(filter_items(repeat_items, 'restconf-lxc-{}'.format(iteration)), satisfied_deps))
                ordered_items.extend(sort_items(filter_items(repeat_items, 'netconf-openstack-{}'.format(iteration)), satisfied_deps))
                ordered_items.extend(sort_items(filter_items(repeat_items, 'restconf-openstack-{}'.format(iteration)), satisfied_deps))
        else:
            for iteration in iterations:
                ordered_items.extend(filter_items(repeat_items, 'netconf-lxc-{}'.format(iteration)))
                ordered_items.extend(filter_items(repeat_items, 'restconf-lxc-{}'.format(iteration)))
                ordered_items.extend(filter_items(repeat_items, 'netconf-mock-{}'.format(iteration)))
                ordered_items.extend(filter_items(repeat_items, 'restconf-mock-{}'.format(iteration)))
                ordered_items.extend(filter_items(repeat_items, 'netconf-openstack-{}'.format(iteration)))
                ordered_items.extend(filter_items(repeat_items, 'restconf-openstack-{}'.format(iteration)))

        if one_shot_itemlist and one_shot_items:
            ordered_items.extend(one_shot_items[item_index:])

        return ordered_items

    final_items = generate_global_ordering(items, config)
    deselected = list(set(items) - set(final_items))
    config.hook.pytest_deselected(items=deselected)
    items[:] = final_items

