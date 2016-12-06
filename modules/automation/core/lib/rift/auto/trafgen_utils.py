import itertools

from gi.repository import (
    RwTrafgenDataYang,
    RwTrafgenYang,
    RwVnfBaseConfigYang,
    RwVnfBaseOpdataYang)

class MgmtSession():
    def __init__(self, mgmt_session):
        self.mgmt_session = mgmt_session

    @property
    def proxy(self):
        return self.mgmt_session.proxy


class ConfigComposer(MgmtSession):
    """A convenience class that generates VLAN config for trafgen system

    Details:
    1. Create a single port for trafgen and trafsink. Configures the port
        with the desired number of VLANs
    2. The corresponding interfaces and ip routes are added to the LB.

    """
    def generate_interface_ips(self, first_octet, number, last_octet="1", ipv6_prefix=None, mask=None):
        """Generates specified number of ips using the first and last octet data.
        Can generate upto 4k IPs.

        Args:
            first_octet (str): First octet of the IP generated.
            number (int): Number of Ips to be generated.
            last_octet (str, optional): Last octet of the IPs generated

        Yields:
            A valid IP
        """
        ip = "{}.1-16.0-255.{}".format(first_octet, last_octet)

        if ipv6_prefix:
            ip = "{}::{}".format(ipv6_prefix, ip)

        for i, value in enumerate(self.ip_range(ip, mask)):
            if i >= number:
                break

            yield value

    def ip_range(self, input_string, mask=None):
        """For a given input string generate a list of IP addresses. Work for v6
        as well.

        Args:
            input_string (str): A template, to generate the IP address.
                E.g.: 1.1.1.1-3 will generate
                    1\ 1.1.1.1
                    2\ 1.1.1.2
                    3\ 1.1.1.3
                Similarly the range can be specified on any octet.

        Yields:
            A IP address
        """
        ipv6_mode = False
        ipv6_prefix = ""

        if "::" in input_string:
            ipv6_mode = True
            ipv6_prefix, input_string = input_string.split("::")

        octets = input_string.split('.')

        chunks = [list(map(int, octet.split('-'))) for octet in octets]
        ranges = [list(range(chunk[0], chunk[1] + 1)) if len(chunk) == 2 else chunk
                        for chunk in chunks]

        for address in itertools.product(*ranges):
            ipv4_address = '.'.join(map(str, address))
            ipv4_address = "{}/{}".format(ipv4_address, mask) if mask is not None else ""

            if ipv6_mode:
                yield "{}::{}".format(ipv6_prefix, ipv4_address)
            else:
                yield ipv4_address

    def generate_interface(self, interface_name, port_name=None, ips=None, vlan_id=None, interface_type=None):
        ips = ips or []

        config_data = {
                "name": interface_name,
                "ip": [{"address": ip} for ip in ips],
            }

        bind_data = {}
        if port_name:
            bind_data["port"] = port_name

        if vlan_id:
            bind_data["vlan"] = vlan_id

        if bind_data:
            config_data['bind'] = bind_data

        if interface_type:
            config_data['interface_type'] = interface_type

        interface = RwVnfBaseConfigYang.ConfigIpInterface.from_dict(config_data)

        return interface

    def generate_port(
            self,
            port_name,
            vm_id,
            application,
            vlan_count=None,
            include_range_template=False,
            include_echo=False,
            ipv6=False):

        def bridge_ip(prefix, ip):
            if ipv6:
                return "{}::{}".format(prefix, ip)
            return ip

        vlan_mode = True if vlan_count else False

        port_config = {
                "name": port_name,
                "receive_q_length": 2,
                "open": True,
                "application": {"rx": application, "tx": application},
                'port_identity': {
                        'port_mode': 'direct',
                        'private_name': 'eth_sim:iface={}-tmp-1'.format(port_name),
                        'vm_identity': vm_id}}

        if vlan_mode:
            port_config['vlan'] = [{"id": vlan_id, "open": True}
                    for vlan_id in range(1, vlan_count + 1 )]

        if include_range_template:
            port_config['trafgen'] = {
                    'range_template': {
                        'source_ip': {'increment': 1,
                                      'maximum': bridge_ip("fd4d:1", "1.1.1.16"),
                                      'minimum': bridge_ip('fd4d:1', '1.1.1.1'),
                                      'start': bridge_ip('fd4d:1', '1.1.1.1')},

                        'destination_ip': {'increment': 1,
                                           'maximum': bridge_ip("fd4d:3", "3.3.3.16"),
                                           'minimum': bridge_ip("fd4d:3", "3.3.3.1"),
                                           'start': bridge_ip("fd4d:3", "3.3.3.1")},
                        'destination_mac': {'dynamic': {'gateway': bridge_ip('fd4d:11', '11.1.1.2')}},
                        'destination_port': {'increment': 1, 'maximum': 5678, 'minimum': 5678, 'start': 5678},
                        'packet_size': {'increment': 1, 'maximum': 512, 'minimum': 512, 'start': 512},
                        'source_port': {'increment': 1, 'maximum': 10128, 'minimum': 10000, 'start': 10000}},
                   'transmit_params': {'transmit_mode': {'range': True}, 'tx_rate': 1}
                    }

            if vlan_mode:
                port_config['trafgen']['range_template']['vlan'] = \
                    {'increment': 1, 'maximum': vlan_count, 'minimum': 1, 'start': 1}

        elif include_echo:
            port_config['trafgen'] = {"receive_param": {"receive_echo": {"on": True}}}

        port_obj = RwTrafgenYang.PortConfig.from_dict(port_config)

        return port_obj

    def generate_config(self,
            nw_ctx_name,
            port_name,
            vm_id,
            interface_ips,
            loopback_ips=None,
            include_range_template=True,
            include_echo=False,
            application="rw_trafgen",
            nw_ctx=None,
            lb_portname=None,
            enable_ipv6_fwd=False,
            vlan_mode=False):
        """Generates the config data for the network context.

        Args:
            nw_ctx_name (str): Network context name to be used in the config.
            port_name (str): Port name
            vm_id (int): VM id
            interface_ips (list): IPs for the interfaces.
            loopback_ips (str, optional): Loopback IPs ranges for the loopback interface.
                Format is similar to the input_string in ip_range.
            include_range_template (bool, optional): If set, range template data
                is added to the port
            include_echo (bool, optional): If set, echo is enabled on the port.
            application (str, optional): Application name
            nw_ctx (Gi Obect, optional): If specified, the existing network
                context object is used instead of creating a new one.
            lb_portname (str, optional): The name of the corresponding port on
                LB. Used to compute the IP routes.
            vlan_mode: (bool, optional): If set, the VLAN interfaces are created.

        Returns:
            tuple of (NetworkContext, PortConfig, list of Ip routes.)
        """

        interface_ips = list(interface_ips)

        ipv6 = False
        if interface_ips and ":" in interface_ips[0]:
            ipv6 = True

        nw_ctx_options = {"name": nw_ctx_name}

        if enable_ipv6_fwd:
            nw_ctx_options['ipv6'] = {'forwarding': True}

        nw_ctx = nw_ctx or RwVnfBaseConfigYang.ConfigNetworkContext.from_dict(nw_ctx_options)

        # Create a dummy non-vlan interface. This is needed to move the
        # interface state from configured to UP.
        if vlan_mode:
            interface = self.generate_interface(
                    "{}-tmp-1".format(port_name),
                    port_name)

            nw_ctx.interface.append(interface)

        if loopback_ips is not None:
            interface = self.generate_interface(
                    "loopback-interface",
                    ips=loopback_ips,
                    interface_type="loopback")
            nw_ctx.interface.append(interface)

        ip_routes = []
        for interface_id, ip in enumerate(interface_ips, start=1):

            interface_type = "Vlan" if vlan_mode else "Interface"
            interface_name = "{}-{}-{}".format(port_name, interface_type, interface_id)

            vlan_id = interface_id if vlan_mode else None
            interface = self.generate_interface(
                    interface_name,
                    port_name=port_name,
                    ips=[ip],
                    vlan_id=vlan_id)

            nw_ctx.interface.append(interface)

            if lb_portname is not None:
                # Pre compute the interface name for LB
                lb_interface_name = "{}-{}-{}".format(
                        lb_portname,
                        interface_type,
                        interface_id)

                ip, _ = ip.split("/")
                ip_routes.append({
                        'gateway': ip,
                        'id': interface_id,  # Random, So use the interface id counter
                        'outgoing_interface': lb_interface_name})

        port_obj = self.generate_port(
                port_name,
                vm_id,
                application,
                vlan_count=len(list(interface_ips)) if vlan_mode else None,
                include_range_template=include_range_template,
                include_echo=include_echo,
                ipv6=ipv6)

        return nw_ctx, port_obj, ip_routes

    def create_config(self, vnf_name, nw_ctx_name, nw_ctx_obj, ports):
        """Pushes the config to the host.

        Args:
            vnf_name (str): VNF name
            nw_ctx_name (str): Network context name
            nw_ctx_obj (Gi Object): Gi object for NetworkContext
            ports (list): A list of PortConfig.

        """
        xpath = ("/vnf-config/vnf[name='{vnf_name}'][instance='0']/network-context"
                 "[name='{nw_ctx_name}']").format(vnf_name=vnf_name, nw_ctx_name=nw_ctx_name)
        self.proxy(RwVnfBaseConfigYang).create_config(xpath, nw_ctx_obj)   

        for port in ports:
            xpath = "/vnf-config/vnf[name='{vnf_name}'][instance='0']/port".format(vnf_name=vnf_name)
            self.proxy(RwTrafgenYang).create_config(xpath, port)


class VnfConfig(MgmtSession):
    """Convenience class that abstracts the xpath for VNF config data and fetches
    the data using the proxy.
    """
    def vrf_interface_xpath(self, vnf_name, nw_ctx_name, name=None):
        """
        Args:
            vnf_name (string): VNF name
            nw_ctx_name (string): Network context name.
            name (None, optional): If specified the xpath is keyed based on
                the interface name.

        Returns:
            xpath (str)
        """
        xpath = ("/vnf-config/vnf[name='{vnf_name}'][instance='0']/network-context"
                 "[name='{nw_ctx_name}']/interface").format(
                 vnf_name=vnf_name, nw_ctx_name=nw_ctx_name)

        if name:
            xpath += "[name='{}']".format(name)

        return xpath

    def vrf_interfaces(self, vnf, network_context_name, name=None):
        """Returns a list of interface configs.

        Args:
            vnf_name (string): VNF name
            nw_ctx_name (string): Network context name.
            name (None, optional): If specified the xpath is keyed based on
                the interface name.

        Returns:
            xpath (str)
        """

        xpath = self.vrf_interface_xpath(vnf, network_context_name, name)
        vrf_interfaces = self.proxy(RwVnfBaseConfigYang).get(xpath, list_obj=True)

        return vrf_interfaces


class VnfOpdata(MgmtSession):
    """Convenience class that abstracts the xpath for VNF opdata data and fetches
    the data using the proxy.
    """
    def network_context_xpath(self, vnf):
        """Returns the network context xpath.

        Args:
            vnf (str): Vnf name

        Returns:
            str: xpath
        """
        xpath = "/vnf-config/vnf[name='{}'][instance='0']/network-context".format(vnf)
        return xpath

    def vrf_info_xpath(self, vnf, network_context):
        """
        Args:
            vnf (str): VNF name
            network_context (str): Network context name

        Returns:
            str: xpath
        """
        xpath = ("/vnf-opdata/vnf[name='{vnf}'][instance='0']"
                 "/network-context-state[name='{context}']"
                 "/vrf[name='default']/info").format(
                vnf=vnf,
                context=network_context)

        return xpath

    def vrf_interface_xpath(self, vnf, network_context_name, name=None):
        """
        Args:
            vnf (str): Name of the Vnf
            network_context_name (str): NC name
            name (None, optional): if specified, create a network context keyed
                xpath

        Returns:
            str: VRF interface name.
        """
        xpath = ("/vnf-opdata/vnf[name='{vnf}'][instance='0']" 
                "/network-context-state[name='{context}']"
                "/vrf[name='default']/interface").format(
                    vnf=vnf,
                    context=network_context_name)
        if name:
            xpath += "[name='{}']".format(name)

        return xpath

    def counter_xpath(self, vnf, portname, stat_name=None):
        """
        Args:
            vnf (str): Name of the Vnf
            portname (str): portname name
            stat_name (None, optional): if specified, create a stat name keyed
                xpath

        Returns:
            str: xpath for the counter.
        """
        xpath = "/vnf-opdata/vnf[name='{}'][instance='0']/port-state[portname='{}']/counters"
        xpath = xpath.format(vnf, portname)

        if stat_name:
            xpath += "/{}".format(stat_name)

        return xpath

    def network_contexts(self, vnf):
        """Returns a list of Network context object in the VNF.

        Args:
            vnf (str): VNF name.

        Returns:
            list of network context Gi objects
        """
        xpath = self.network_context_xpath(vnf)
        ctx = self.proxy(RwTrafgenYang).get(xpath, list_obj=True)

        return ctx.network_context

    def vrf_info(self, vnf, network_context):
        """Fetch the VRF info Gi objects

        Args:
            vnf (str): VNF name
            network_context (str): Network context name.

        Returns:
            VrfInfo Gi object
        """
        xpath = self.vrf_info_xpath(vnf, network_context)
        vrf_info = self.proxy(RwVnfBaseOpdataYang).get(xpath)

        return vrf_info


    def vrf_interfaces(self, vnf, network_context_name, name=None):
        """Fetch the Vrf interface for a given network context.

        Args:
            vnf (str): Interface Name
            network_context_name (str): Network context name
            name (None, optional): if specified, only the specified interface is
                fetched.

        Returns:
            List of VRF interface.
        """
        xpath = self.vrf_interface_xpath(vnf, network_context_name, name)
        vrf_interfaces = self.proxy(RwVnfBaseOpdataYang).get(xpath, list_obj=True)

        return vrf_interfaces


class TrafgenTraffic(MgmtSession):
    """Convenience class to start/stop and verify traffic.
    """
    def __init__(self, mgmt_session):
        super().__init__(mgmt_session)
        self.vnf_opdata = VnfOpdata(mgmt_session)

    def start_traffic(self, port_name):
        '''Start traffic on the port with the specified name.

        Aruguments:
            port_name - name of port on which to start traffic
        '''
        rpc_input = RwTrafgenDataYang.RwStartTrafgenTraffic.from_dict({
            'vnf_name':'trafgen',
            'vnf_instance':0,
            'port_name':port_name
        })
        rpc_output = RwVnfBaseOpdataYang.YangOutput_RwVnfBaseOpdata_Start_VnfOutput()
        self.proxy(RwTrafgenDataYang).rpc(
                rpc_input,
                rpc_name='start',
                output_obj=rpc_output)


    def stop_traffic(self, port_name):
        '''Stop traffic on the port with the specified name.

        Arguments:
            port_name - name of port on which to stop traffic
        '''
        rpc_input = RwTrafgenDataYang.RwStopTrafgenTraffic.from_dict({
            'vnf_name':'trafgen',
            'vnf_instance':0,
            'port_name':port_name
        })
        rpc_output = RwVnfBaseOpdataYang.YangOutput_RwVnfBaseOpdata_Stop_VnfOutput()
        self.proxy(RwTrafgenDataYang).rpc(
                rpc_input,
                rpc_name='stop',
                output_obj=rpc_output)


    def wait_for_traffic_started(self, vnf_name, port_name, timeout=120, interval=2, threshold=60):
        '''Wait for traffic to be started on the specified port

        Traffic is determined to be started if the input/output packets on the port
        increment during the specified interval

        Arguments:
            port_name - name of the port being monitored 
            timeout - time allowed for traffic to start
            interval - interval at which the counters should be checked
            threhsold - values under the threshold treated as 0
        '''
        def value_incremented(previous_sample, current_sample):
            '''Comparison that returns True if the the sampled counter increased
            beyond the specified threshold during the sampling interval
            otherwise returns false
            '''
            if previous_sample is None or current_sample is None:
                return False
            return int(current_sample) > threshold

        xpath = self.vnf_opdata.counter_xpath(vnf_name, port_name, 'input-packets')
        self.proxy(RwVnfBaseOpdataYang).wait_for_interval(
            xpath, value_incremented, timeout=timeout, interval=interval)


    def wait_for_traffic_stopped(self, vnf_name, port_name, timeout=60, interval=2, threshold=60):
        '''Wait for traffic to be stopped on the specified port

        Traffic is determined to be stopped if the input/output packets on the port
        remain unchanged during the specified interval

        Arguments:
            port_name - name of the port being monitored 
            timeout - time allowed for traffic to start
            interval - interval at which the counters should be checked
            threshold - values under the threshold treated as 0
        '''
        def value_unchanged(previous_sample, current_sample):
            '''Comparison that returns True if the the sampled counter increased
            less than the specified threshold during the sampling interval
            otherwise returns False
            '''
            return (int(current_sample) - int(previous_sample)) < threshold

        xpath = self.vnf_opdata.counter_xpath(vnf_name, port_name, 'input-packets')
        self.proxy(RwVnfBaseOpdataYang).wait_for_interval(
            xpath, value_unchanged, timeout=timeout, interval=interval)
