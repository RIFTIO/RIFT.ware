#!/usr/bin/env python

# RIFT_IO_STANDARD_COPYRIGHT_HEADER(BEGIN)
# Author(s): Austin Cormier
# Creation Date: 09/10/2014
# RIFT_IO_STANDARD_COPYRIGHT_HEADER(END)
#
# Automation IP Address utilities

import re
import subprocess
import logging
import ipaddress

logger = logging.getLogger(__name__)


def is_valid_ipv4_address(ipv4_address):
    """ Checks if argument is a valid IP address with 4 numeric fields separated by periods."""

    # Although it is technically possible to have a valid IP Address without 3 seperators,
    # we want IPv4 addresses to be fully specified for all intents and purposes.
    if ipv4_address.count('.') != 3:
      logging.error("IPv4 address does not have 3 periods: %s", ipv4_address)
      return False

    try:
        ipaddress.IPv4Address(ipv4_address)
    except ValueError:
        return False

    return True


def is_valid_ipv4_interface_address(ip_intf_address):
    """ Checks if argument is a valid IP interface address with 4 numeric fields separated by periods."""

    address_parts = ip_intf_address.split("/")
    if len(address_parts) != 2:
        logger.error("IPv4 interace address should have a / to indicate the subnet.")
        return False

    if not is_valid_ipv4_address(address_parts[0]):
        return False

    try:
        ipaddress.IPv4Interface(ip_intf_address)
    except ValueError:
        return False

    return True


def ip_list_from_string(ip_list_arg, as_string=False):
    """
    Generates a list of ip addresses from a string containing a combination
    of IP singlets (e.g. 1.1.1.1) and IP generators (1.1.1.[1,2-3,4,5])
    """

    def gen_field_to_ip_list(prefix, gen_field):
        """ Takes an IP Prefix (e.g. 1.1.1) and a generator field (e.g [1,2-3,5])
          and generates a list of IPs.
        """
        ip_list = []
        # Split the field
        fields = gen_field.split(",")
        for field in fields:
            # If this is a range, construct the IP's in that range.
            if "-" in field:
                # Get start and stop values for generator field
                start, stop = (int(num) for num in field.split("-"))
                if start > stop:
                    raise ValueError("Start ip is greater than end ip in generator: %s", gen_field)

                # Construct the list of ip addresses based on the generator
                gen_list = [".".join([prefix, str(i)]) for i in range(start, stop + 1)]
                ip_list.extend(gen_list)

            # This is a singlet generator
            else:
                singlet = ".".join([prefix, field])
                ip_list.append(singlet)

        return ip_list

    # Matches IP singlets or IP Generators
    # e.g. "1.1.1.1 1.1.1.[2-3,5,27-30]
    # Splits matches into the IP prefix (1.1.1) and the gen field ([1-3,4])
    ip_pattern = r'(\d{1,3}\.\d{1,3}\.\d{1,3})\.(\d{1,3}|\[(?:(?:\d{1,3}-\d{1,3}|\d{1,3}),?)+])'
    ip_matches = re.findall(ip_pattern, ip_list_arg)

    ip_list = []
    for ip_prefix, gen_field in ip_matches:
        # If this is just a singlet, reconstruct ip
        if "[" not in gen_field:
            ip = ".".join([ip_prefix,  gen_field])
            ip_list.append(ip)
            continue

        # This is a IP generator field, strip the brackets
        gen_field = gen_field[1:-1]
        gen_ip_list = gen_field_to_ip_list(ip_prefix, gen_field)
        ip_list.extend(gen_ip_list)

    for ip in ip_list:
        if not is_valid_ipv4_address(ip):
            raise ValueError ("Invalid IP address: %s", ip)

    if as_string:
         return ','.join(ip_list)

    return ip_list

def clusters_ip_list_from_string(ip_list_arg):
    """
    Generates a list that contains list of ip addresses (for set of VMs in a cluster) from
    a string containing a combination of IP singlets (e.g. 1.1.1.1) and
    IP generators (1.1.1.[1,2-3,4,5]:2.2.2.[4])
    """
    ip_lists = []
    # for set of VMs in each cluster
    for cluster_ip_list in ip_list_arg.split(':'):
        ip_lists.append(ip_list_from_string(cluster_ip_list))
    return ip_lists

def test_ip_generator():
    input_outputs = [("1.1.1.1 1.1.1.2", ["1.1.1.1", "1.1.1.2"]),
                     ("1.1.1.1,1.1.1.[2,4-5,6]", ["1.1.1.1", "1.1.1.2", "1.1.1.4", "1.1.1.5", "1.1.1.6"])]
    for gen, expected in input_outputs:
        output = ip_list_from_string(gen)
        if  output != expected:
            print(("Case failed: output({}), expected output({})".format(output, expected)))
            assert False

    print("Ip Generator test cases passed")


class IpCmd:
    def neighbours(self, netns=None, ipv6=False):
        """Runs the Ip neighbours command and returns the output as a dict.
        Format:
            {neigh_ip: <IP_ADDR>, dev: <Device>,
             lladdr: <LINK ADDR>, status: <STATUS>}

        Args:
            netns (None, optional): If specified the command is executed within
                the network namespace.
            ipv6 (bool, optional): If set run with the -6 flag.

        Returns:
            list of dicts
        """
        def _parse_data(data):
            regex = re.compile(r'(.*)\sdev\s(.*)\slladdr\s(.*)\s.*\s(.*)')
            datas = []
            for line in data.split("\n"):
                line = line.strip()
                if not line:
                    continue

                group = regex.match(line)
                data = {'neigh_ip': group.group(1),
                        'dev': group.group(2),
                        'lladdr': group.group(3),
                        'status': group.group(4)}

                datas.append(data)

            return datas

        version = "6" if ipv6 else "4"

        cmd = "ip -{} neigh".format(version)
        if netns:
            cmd = "ip netns exec {} ip -{} neigh".format(netns, version)

        data = subprocess.check_output(
                    cmd,
                    shell=True).decode('ascii')

        return _parse_data(data)

    def routes(self, netns=None, ipv6=False):
        """Runs the Ip route command and returns the output as dict
        Format:
            {neigh_ip: <IP_ADDR>, dev: <Device>}

        Args:
            netns (None, optional): If specified the command is executed within
                the network namespace
            ipv6 (bool, optional): If set run with the -6 flag.

        Returns:
            list of dict.
        """
        def _parse_data(data):
            regex = re.compile(r'(.*)\sdev\s(.*?)\s.*')
            datas = []
            for line in data.split("\n"):
                line = line.strip()
                if not line:
                    continue

                group = regex.match(line)

                neigh_ip = group.group(1)
                if "via" in neigh_ip:
                    neigh_ip, _ = neigh_ip.split("via")
                    neigh_ip = neigh_ip.strip()

                data = {'neigh_ip': neigh_ip,
                        'dev': group.group(2)}

                datas.append(data)

            return datas

        version = "6" if ipv6 else "4"

        cmd = "ip -{} route".format(version)
        if netns:
            cmd = "ip netns exec {} ip -{} route".format(netns, version)

        data = subprocess.check_output(
                    cmd,
                    shell=True).decode('ascii')

        return _parse_data(data)

    def get_netns_interface_ips(self, netns, interface_name, ipv6=False):
        """Get all the interface ips in an interface in network namespace

        Args:
            netns (str): Network namespace.
            interface_name (str): Interface name.
        """
        if_config_cmd = "ip netns exec {} ifconfig {}".format(netns, interface_name)
        if_config = subprocess.check_output(
                    if_config_cmd,
                    shell=True).decode('ascii')

        regex = re.compile("inet.*?(.*?)\s*netmask .*")
        if ipv6:
            regex = re.compile("inet6.*?(.*?)\s*prefixlen .*")

        ips = []
        for line in if_config.split("\n"):
            match = regex.search(line)
            if not match:
                continue

            ip = match.group(1).strip()
            exploded_ip = ipaddress.IPv6Address(ip).exploded
            ips.append(exploded_ip)

        return ips

if __name__ == "__main__":
    test_ip_generator()
