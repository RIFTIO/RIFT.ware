#!/usr/bin/env python3
import argparse
import logging
import os
import stat
import subprocess
import sys
import time
import yaml
import requests
from requests.auth import HTTPBasicAuth

'''
Input YAML cfg: {
           'rpc_ip': {'user_defined_script': 'tgvrouterts_config.py', 'nsr_id_ref': '89f82e1b-b58d-4f18-83c0-624ed3de5b8e', 
           'parameter': [{'name': 'Trigger', 'value': 'start'}, 'name': 'tgtraffic_startstop'}, 
           'vnfr_index_map': {1: '85846d20-276a-41d3-8614-81ae9a4c42ce', 2: 'e1262d43-b706-4d8d-9233-8f91f377e5e1', 3: '8c45f6bf-a409-4474-85fc-db0367307c56'}, 
           'config_agent': {'name': 'RiftCA'}, 
           'vnfr_data_map': {1: {'connection_point': [{'ip_address': '11.0.0.3', 'name': 'trafgen_vnfd/cp0'}], 'mgmt_interface': {'ip_address': '10.66.202.178', 'port': 2022}},
                     2: {'connection_point': [{'ip_address': '11.0.0.2', 'name': 'vrouter_vnfd/cp0'}, {'ip_address': '12.0.0.2', 'name': 'vrouter_vnfd/cp1'}], 'mgmt_interface': {'ip_address': '10.66.202.155', 'port': 80}}, 
                   3: {'connection_point': [{'ip_address': '12.0.0.3', 'name': 'trafsink_vnfd/cp0'}], 'mgmt_interface': {'ip_address': '10.66.202.181', 'port': 2022}}}, 
           'unit_names': {'e1262d43-b706-4d8d-9233-8f91f377e5e1': 'vrouter_vnfd', '85846d20-276a-41d3-8614-81ae9a4c42ce': 'trafgen_vnfd', '8c45f6bf-a409-4474-85fc-db0367307c56': 'trafsink_vnfd'}, 
           'init_config': {'e1262d43-b706-4d8d-9233-8f91f377e5e1': {}, '85846d20-276a-41d3-8614-81ae9a4c42ce': {}, '8c45f6bf-a409-4474-85fc-db0367307c56': {}}
        }

'''
class ConfigurationError(Exception):
    pass

def get_trafgen_port_data(logger, tg_mgmt_ip):
    headers={'Content-type':'application/vnd.yang.data+json',
                'Accept':'json'}

    try:
       url = 'https://{tg_mgmt_ip}:8008/api/operational/vnf-opdata/vnf/trafgen,0/port-state'.format(tg_mgmt_ip=tg_mgmt_ip)
       resp = requests.get(
                       url, timeout=10, auth=HTTPBasicAuth('admin', 'admin'),
                    headers=headers, verify=False
                    )
       resp.raise_for_status()
    except requests.exceptions.RequestException as e:
       msg = "Got HTTP error when request {}".format( str(e))
       logger.debug(msg)

    data = resp.json()
    return data

def parse_trafgen_port_data(logger, port_data):
    port_list = []
    for port_info in port_data["rw-vnf-base-opdata:port-state"]:
       if port_info["info"]["virtual-fabric"] == "No":
          port_list.append(port_info["portname"])

    return port_list
     

'''
start/stop vnf trafgen-traffic vnf-name trafgen vnf-instance 0
'''
def startstop_trafgen(logger, run_dir, mgmt_ip, port_list, trigger, pkt_size, tx_rate):
    sh_file = "{}/trafgen_startstop-traffic-{}.sh".format(run_dir, time.strftime("%Y%m%d%H%M%S"))
    logger.debug("Creating script file %s", sh_file)
    with open(sh_file, "w") as f:
        f.write(r'''#!/usr/bin/expect -f
set login "root"
set pw "toor"
set success 0
spawn ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $login@{trafgen_mgmt_ip}
set spid $spawn_id
set timeout 60

if {{[llength $argv] <= 2}} {{
    send_user "Usage: trafgen_startstop.exp <trigger> <port-list> \n"
    send_user "      Trigger values: start/stop \n"
    exit 1
}}
  
foreach {{trigger}} $argv break
set portnames [lrange $argv 1 end]


expect -i $spid \
                  "*?assword:"      {{
                                exp_send -i $spid "$pw\r"
                                if {{ $success == 0 }} {{
                                        incr success -1
                                        exp_continue
                                }}
                }} "]# "  {{
                        set success 1
                }} "yes/no"      {{
                        exp_send -i $spid "yes\r"
                        exp_continue
                }} timeout       {{
                        set success -1
                }}

send "riftcli\r"
expect "rift# "
sleep 2

if {{ $trigger eq "start" }} {{

   foreach portname $portnames {{
      # your existing code goes here
      send "conf\r"
      expect "# "
      send "vnf-config vnf trafgen 0\r"
      expect "# "
      send "port $portname\r"
      expect "# "
      send "trafgen\r"
      expect "# "
      send "range-template\r"
      expect "# "
      send "packet-size start {pkt_size} minimum {pkt_size} maximum {pkt_size} increment 1\r"
      expect "# "
      send "exit\r"
      expect "# "
      send "transmit-params\r"
      expect "# "
      send "tx-rate {tx_rate}\r"
      expect "# "
      send "end\r"
      expect "# "
   }}
}}

if {{ $trigger eq "start" }} {{
  send "start vnf trafgen-traffic vnf-name trafgen vnf-instance 0\r"
}} else {{
  send "stop vnf trafgen-traffic vnf-name trafgen vnf-instance 0\r"
}}

expect "rift# "
exp_close -i $spid
'''.format(trafgen_mgmt_ip=mgmt_ip, pkt_size=pkt_size, tx_rate=tx_rate))

    os.chmod(sh_file, stat.S_IRWXU)
    cmd = "{sh_file} {arg1} ".format(sh_file=sh_file, arg1=trigger)
    for port in port_list:
      cmd += " " + port
    logger.debug("Executing shell cmd : %s", cmd)
    rc = subprocess.call(cmd, shell=True)
    if rc != 0:
        raise ConfigurationError("Trafgen NS start/stop failed: {}".format(rc))

def main(argv=sys.argv[1:]):
    try:
        parser = argparse.ArgumentParser()
        parser.add_argument("yaml_cfg_file", type=argparse.FileType('r'))
        parser.add_argument("--quiet", "-q", dest="verbose", action="store_false")
        args = parser.parse_args()

        run_dir = os.path.join(os.environ['RIFT_INSTALL'], "var/run/rift")
        if not os.path.exists(run_dir):
            os.makedirs(run_dir)
        log_file = "{}/tgtraffic_startstop-{}.log".format(run_dir, time.strftime("%Y%m%d%H%M%S"))
        logging.basicConfig(filename=log_file, level=logging.DEBUG)
        logger = logging.getLogger()

        ch = logging.StreamHandler()
        if args.verbose:
            ch.setLevel(logging.DEBUG)
        else:
            ch.setLevel(logging.INFO)

        # create formatter and add it to the handlers
        formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        ch.setFormatter(formatter)
        logger.addHandler(ch)

    except Exception as e:
        print("Got exception:{}".format(e))
        raise

    try:
        yaml_str = args.yaml_cfg_file.read()
        logger.debug("Input YAML file: %s", yaml_str)
        yaml_cfg = yaml.load(yaml_str)
        logger.debug("Input YAML cfg: %s", yaml_cfg)

        # Check if this is post scale out trigger
        def find_vnfr(vnfr_dict, mbr_vnf_index):
            try:
               return vnfr_dict[mbr_vnf_index]
            except KeyError:
               logger.warn("Could not find vnfr for mbr vnf index : %d", mbr_vnf_index)
                  
                
        def find_cp_ip(vnfr, cp_name):
            for cp in vnfr['connection_point']:
               logger.debug("Connection point: %s", format(cp))
               if cp_name in cp['name']:
                  return cp['ip_address']

            raise ValueError("Could not find vnfd %s connection point %s", cp_name)

        def find_mgmt_ip(vnfr):
            return vnfr['mgmt_interface']['ip_address']

        def find_param_value(param_list, input_param):
            for item in param_list:
               logger.debug("Parameter: %s", format(item))
               if item['name'] == input_param:
                  return item['value']

        tg_vnfr = find_vnfr(yaml_cfg['vnfr_data_map'], 1)
        tg_mgmt_ip = find_mgmt_ip(tg_vnfr)
        # parameter': [{'name': 'Trigger', 'value': 'start'}]
        trigger = find_param_value(yaml_cfg['rpc_ip']['parameter'], 'Trigger')
        pkt_size = find_param_value(yaml_cfg['rpc_ip']['parameter'], 'Packet size')
        tx_rate = find_param_value(yaml_cfg['rpc_ip']['parameter'], 'Tx rate')

        if pkt_size is None:
           pkt_size = 512
        if tx_rate is None:
           tx_rate = 5

        port_data = get_trafgen_port_data(logger,tg_mgmt_ip)
        logger.debug("Received port data is %s", port_data)
        port_list = parse_trafgen_port_data(logger, port_data)
        logger.debug("Generated port list is %s", port_list)
        startstop_trafgen(logger, run_dir, tg_mgmt_ip, port_list, trigger, pkt_size, tx_rate)

    except Exception as e:
        logger.exception(e)
        raise

if __name__ == "__main__":
    main()
