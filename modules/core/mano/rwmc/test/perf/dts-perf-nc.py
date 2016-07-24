#!/usr/bin/env python2

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import subprocess
import contextlib
import rift.auto.proxy
import sys
import os
import time
import rw_peas
import requests
import argparse
import socket

import gi
gi.require_version('RwMcYang', '1.0')
gi.require_version('YangModelPlugin', '1.0')



from gi.repository import RwMcYang

# stress the system using netconf

yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
yang_model_api = yang.get_interface('Model')
yang_model = yang_model_api.alloc()
mc_module = yang_model_api.load_module(yang_model, 'rw-mc')

@contextlib.contextmanager
def start_system(host, port):
    print("Starting system")


    # Retrieve the necessary rift paths
    rift_root = os.environ["RIFT_ROOT"]
    rift_install = os.environ["RIFT_INSTALL"]
    rift_artifacts = os.environ["RIFT_ARTIFACTS"]

    cmd="{RIFT_INSTALL}/demos/dts-perf-system.py -m ethsim -c --ip-list {host} --skip-prepare-vm".format(RIFT_INSTALL=rift_install, host=host)
    rift_shell_cmd="sudo {RIFT_ROOT}/rift-shell -e -- {cmd}".format(cmd=cmd, RIFT_ROOT=rift_root)
    remote_cmd="shopt -s huponexit; cd {RIFT_ROOT}; {rift_shell_cmd}".format(RIFT_ROOT=rift_root, rift_shell_cmd=rift_shell_cmd)
    ssh_opt="-o ConnectTimeout=5 -o StrictHostKeyChecking=no"


    cmd='ssh {ssh_opt} {host} -t -t "{remote_cmd}"'.format(
            ssh_opt=ssh_opt,
            remote_cmd=remote_cmd,
            host=host,
            )

    fout = open(os.path.join(rift_artifacts, "dts-perf.stdout"), "w")
    ferr = open(os.path.join(rift_artifacts, "dts-perf.stderr"), "w")

    process = subprocess.Popen(
            cmd,
            shell=True,
            stdout=fout,
            stderr=ferr,
            stdin=subprocess.PIPE,
            )

    # Wait for confd to become available
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, 8008))
            sock.close()
            break

        except socket.error:
            time.sleep(1)

    print("System ready")

    try:
        yield
    finally:
        print("Killing confd")
        process.terminate()
        process.wait()

def run_rpc_perf_test(proxy, num_rpcs=1):
    start_time = time.time()

    for i in range(1, num_rpcs + 1):
        start = RwMcYang.StartLaunchpadInput()
        start.federation_name = "lp_%s" % i
        print(proxy.rpc(start.to_xml(yang_model)))

    stop_time = time.time()

    print("Retrieved %s rpc in %s seconds" % (num_rpcs, stop_time - start_time))
    return (stop_time - start_time)


def run_federation_config_http_perf_test(num_federations=1):
    session = requests.Session()

    start_time = time.time()
    for i in range(1, num_federations + 1):
        req = session.post(
                url="http://localhost:8008/api/config",
                json={"federation": {"name": "foo_%s" % i}},
                headers={'Content-Type': 'application/vnd.yang.data+json'},
                auth=('admin', 'admin')
                )
        req.raise_for_status()
    stop_time = time.time()

    print("Configured %s federations using restconf in %s seconds" % (num_federations, stop_time - start_time))
    return (stop_time - start_time)

def run_opdata_get_opdata_perf_test(proxy, num_gets=1):
    start_time = time.time()

    for i in range(1, num_gets + 1):
        print(proxy.get_from_xpath(filter_xpath="/opdata"))
        pass

    stop_time = time.time()
    print("Retrieved %s opdata in %s seconds" % (num_gets, stop_time - start_time))
    return (stop_time - start_time)

def run_federation_config_perf_test(proxy, num_federations=1):
    start_time = time.time()

    for i in range(1, num_federations + 1):
        fed = RwMcYang.FederationConfig()
        fed.name = "foobar_%s" % i
        print(proxy.merge_config(fed.to_xml(yang_model)))

    stop_time = time.time()

    print("Configured %s federations using netconf in %s seconds" % (num_federations, stop_time - start_time))
    return (stop_time - start_time)

def run_federation_get_config_perf_test(proxy, num_gets=1):
    start_time = time.time()

    for i in range(1, num_gets + 1):
        print(proxy.get_config(filter_xpath="/federation"))

    stop_time = time.time()

    print("Retrieved %s federations in %s seconds" % (num_gets, stop_time - start_time))
    return (stop_time - start_time)

def main(argv=sys.argv[1:]):

    parser = argparse.ArgumentParser()
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, default=8888)
    parser.add_argument('--output', default='dts-perf-results.tsv')
    parser.add_argument('--uri', default="/federation")
    parser.add_argument('--num-conn', type=int, default=5000)
    parser.add_argument('--timeout', type=int, default=5)
    parser.add_argument('--low-rate', type=int, default=20)
    parser.add_argument('--high-rate', type=int, default=200)
    parser.add_argument('--rate-step', type=int, default=20)

    args = parser.parse_args(argv)

    with start_system(args.host, args.port):
        nc_proxy = rift.auto.proxy.NetconfProxy()
        nc_proxy.connect()
        n_fed = 10;
        n_fed_get = 100
        n_opdata_get = 100
        n_rpc = 100
        config_time = run_federation_config_perf_test(nc_proxy, num_federations=n_fed)
        config_get_time = run_federation_get_config_perf_test(nc_proxy, num_gets=n_fed_get)
        opdata_get_time = run_opdata_get_opdata_perf_test(nc_proxy, num_gets=n_opdata_get)
        rpc_time = run_rpc_perf_test(nc_proxy, num_rpcs=n_rpc)

        print("")
        print("..............................................")
        print("CONFD Performance Results Using Netconf Client")
        print("..............................................")
        print("Rate of config writes: %d" % (n_fed/config_time))
        print("Rate of config reads : %d" % (n_fed_get/config_get_time))
        print("Rate of opdata reads : %d" % (n_opdata_get/opdata_get_time))
        print("Rate of rpc calls    : %d" % (n_rpc/rpc_time))
        print("* Config read is reading a list with %d entries" % n_fed)
        print("* Opdata read is reading a list with 5 entries")
        print("..............................................")

if __name__ == "__main__":
    if "RIFT_ROOT" not in os.environ:
        print("Must be in rift shell to run.")
        sys.exit(1)

    os.chdir(os.environ["RIFT_INSTALL"])
    main()

