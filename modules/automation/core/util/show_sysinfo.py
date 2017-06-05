#!/usr/bin/env python3
"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file show_sysinfo.py

"""

import argparse
import logging
import os
import sys
import tempfile
import subprocess

import rift.auto.ip_utils



logger = logging.getLogger(__name__)


def _show_system_information(confd_ip, rift_var_root):
    """Show system information."""
    rift_root= os.environ.get('RIFT_ROOT')

    rift_artifacts = os.environ.get('RIFT_ARTIFACTS', tempfile.mkdtemp())
    tmp_file = tempfile.NamedTemporaryFile(delete=False, dir=rift_artifacts)
    tmp_file.close()

    # Write to a file and then print it out to the console.
    # Why? Getting the data as string causes unnecessary conversions
    # between uAgent and confd, causing the Timeout to expire, whereas writing
    # to a file is much faster.
    cmd = (
        "/usr/rift/bin/ssh_root {confd_ip} -- "
        "{rift_root}/rift-shell -e -- "
        "\"rwcli --rwmsg --username admin --passwd admin "
        "--rift_var_root {rift_var_root} "
        "-c 'show-system-info file {tmp_file}'\""
    ).format(
        confd_ip=confd_ip,
        rift_root=rift_root,
        tmp_file=tmp_file.name,
        rift_var_root=rift_var_root
    )

    try:
        subprocess.call(cmd, shell=True)
    except subprocess.CalledProcessError:
        logger.error("Unable to fetch data from SSID command")
        return

    with open(tmp_file.name, "r") as fh:
        logger.info(fh.read())

def main(args):
    """ Script entry point to show system information

    Arguments:
        confd_host - Confd Netconf IP Address
        args       - command-line arguments
    """
    rift_var_root = os.environ.get("RIFT_VAR_ROOT", "")
    if rift_var_root == "":
        logger.error("Failed to show-system-info - RIFT_VAR_ROOT not set")

    for confd_host in args.confd_host:
        logger.info("\n\n===== Sysinfo for %s =====\n" % (confd_host))

        _show_system_information(confd_host, rift_var_root)


def parse_args(argv=sys.argv[1:]):
    """ Parse commandarguments.

    Arguments:
        argv - List of argument strings
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--confd-host',
                        default=["127.0.0.1"],
                        type=rift.auto.ip_utils.ip_list_from_string,
                        help="Hostname or IP where the confd netconf server is running.")
    parser.add_argument('-v', '--verbose',
                        action='store_true',
                        help="Logging is normally set to an INFO level. When this flag "
                             "is used logging is set to DEBUG. ")
    args = parser.parse_args(argv)
    return args


if __name__ == "__main__":
    try:
        logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')
        args = parse_args()
        logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.INFO)

        main(args)
    finally:
        os.system("stty sane")
