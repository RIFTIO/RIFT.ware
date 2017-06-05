#!/usr/bin/env python

# STANDARD_RIFT_IO_COPYRIGHT


import argparse
import sys
import os

import dns

def get_host_ip_map(controller_host):
    """ Returns a host->ip mapping on the openstack controller node

    Arguments:
        controller_host - Openstack controller host
    """
    host_ip_map = dns.vm_list(controller_host)

    return host_ip_map


def parse_args(argv=sys.argv[1:]):
    """ Returns parsed command line arguments

    Arguments:
        argv - The argument list to parse

    Returns:
        argparse namespace object
    """
    parser = argparse.ArgumentParser(
            description="Converts a list of hostnames (seperated by commas) into "
            "the corresponding ip addreses",
            )

    parser.add_argument(
            'controller_host',
            help='Openstack controller host name or ip address',
            )

    parser.add_argument(
            'hostnames',
            metavar='host',
            nargs='+',
            help='List of hostnames',
            )

    parser.add_argument(
            '-p', '--prefix',
            help="A prefix to prepend to hostnames before querying"
            )

    parser.add_argument(
            '-c', '--collapsed',
            action="store_true",
            help="Collapsed the printed ip list representation (e.g 1.1.1.[1,2,3])"
            )

    args = parser.parse_args(args=argv)

    return args


def collapse_ip_list(ips):
    """ If possible, collapse a list of IP's into a collapsed representation

    This will only collapsed a list of IP's if the first three bytes are the same.

    Arguments:
        ips - List of IP addresses to

    Returns:
        A (potentially collapsed) comma seperated ip list string

    Example:
        collapse_ip_list(["1.1.1.1","1.1.1.2"]) -> "1.1.1.[1,2]"
        collapse_ip_list(["1.1.1.1","1.1.2.1"]) -> "1.1.1.1,1.1.2.1"
    """
    prefix = os.path.commonprefix(ips)
    num_dots = prefix.count(".")
    if num_dots != 3:
        return ",".join(ips)

    last_dot = prefix.rfind(".")
    prefix = prefix[:last_dot + 1]
    collapsed_list = "{}[{}]".format(
            prefix,
            ",".join([i.replace(prefix, "") for i in ips])
            )

    return collapsed_list


def main():
    args = parse_args()
    hosts = args.hostnames
    if args.prefix is not None:
        hosts = [args.prefix + host for host in hosts]

    try:
        host_ip_map = get_host_ip_map(args.controller_host)
    except Exception as e:
        sys.stderr.write("Failure retreiving host ip map: {}".format(str(e)))
        sys.exit(1)

    try:
        ips = [host_ip_map[host] for host in hosts]
    except KeyError as e:
        sys.stderr.write("Could not find {} host in host ip map".format(str(e)))
        sys.exit(1)

    if args.collapsed:
        print collapse_ip_list(ips)
    else:
        print ",".join(ips)


if __name__ == "__main__":
    main()

