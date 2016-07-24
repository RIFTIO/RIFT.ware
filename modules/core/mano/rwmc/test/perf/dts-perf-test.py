#!/usr/bin/env python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import collections
import os
import socket
import sys
import subprocess
import time


class ProcessError(Exception):
    pass


def check_dependency(package):
    requirement = subprocess.Popen(
            'which {}'.format(package),
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            )

    _, stderr = requirement.communicate()
    requirement.wait()

    if stderr:
        print("'{}' is required to test the system".format(package))
        sys.exit(1)


class Autobench(collections.namedtuple(
    "Autobench", [
        "host",
        "port",
        "uri",
        "file",
        "num_connections",
        "timeout",
        "low_rate",
        "high_rate",
        "rate_step",
        ]
    )):
    def __repr__(self):
        args = [
            "autobench --single_host",
            "--host1 {}".format(self.host),
            "--port1 {}".format(self.port),
            "--uri1 {}".format(self.uri),
            "--file {}".format(self.file),
            "--num_conn {}".format(self.num_connections),
            "--timeout {}".format(self.timeout),
            "--low_rate {}".format(self.low_rate),
            "--high_rate {}".format(self.high_rate),
            "--rate_step {}".format(self.rate_step),
            ]

        return ' '.join(args)


def launch_remote_system(host, port, autobench):
    # Check dependencies
    check_dependency('autobench')
    check_dependency('httperf')

    # Retrieve the necessary rift paths
    rift_root = os.environ["RIFT_ROOT"]
    rift_install = os.environ["RIFT_INSTALL"]
    rift_artifacts = os.environ["RIFT_ARTIFACTS"]

    cmd="{RIFT_INSTALL}/demos/dts-perf-system.py -m ethsim --ip-list {host} --skip-prepare-vm".format(RIFT_INSTALL=rift_install, host=host)
    rift_shell_cmd="sudo {RIFT_ROOT}/rift-shell -e -- {cmd}".format(cmd=cmd, RIFT_ROOT=rift_root)
    remote_cmd="shopt -s huponexit; cd {RIFT_ROOT}; {rift_shell_cmd}".format(RIFT_ROOT=rift_root, rift_shell_cmd=rift_shell_cmd)
    ssh_opt="-o ConnectTimeout=5 -o StrictHostKeyChecking=no"


    cmd='ssh {ssh_opt} {host} -t -t "{remote_cmd}"'.format(
            ssh_opt=ssh_opt,
            remote_cmd=remote_cmd,
            host=host,
            )

    try:
        print('starting system')

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

        print('system ready')

        # Launch autobench on another process
        print('testing started')
        test = subprocess.Popen(
                str(autobench),
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                )

        (stdout, stderr) = test.communicate()
        test.wait()

        if test.stderr is not None:
            print(stderr)

        print('testing complete')

    except Exception as e:
        print(str(e))

    finally:
        process.terminate()
        process.wait()

        fout.close()
        ferr.close()


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

    autobench = Autobench(
            host=args.host,
            port=args.port,
            uri=args.uri,
            file=args.output,
            num_connections=args.num_conn,
            timeout=args.timeout,
            low_rate=args.low_rate,
            high_rate=args.high_rate,
            rate_step=args.rate_step,
            )

    launch_remote_system(args.host, args.port, autobench)


if __name__ == "__main__":
    main()
