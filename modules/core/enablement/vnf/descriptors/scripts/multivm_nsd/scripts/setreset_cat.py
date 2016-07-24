#!/usr/bin/env python3
import argparse
import logging
import os
import stat
import subprocess
import sys
import time
import yaml

'''
Input YAML cfg: {
           'rpc_ip': {'user_defined_script': 'setreset_cat.py', 'nsr_id_ref': '89f82e1b-b58d-4f18-83c0-624ed3de5b8e', 
           'parameter': [{'name': 'Host', 'value': 'grunt113.lab'}, 'name': 'ToggleCat'}, 
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

'''
sudo /usr/src/cat/pqos/pqos -R
'''

def toggle_cat(logger, run_dir, host, trigger):
    sh_file = "{}/toggle_cat-{}.sh".format(run_dir, time.strftime("%Y%m%d%H%M%S"))
    logger.debug("Creating script file %s", sh_file)
    with open(sh_file, "w") as f:
        f.write(r'''#!/usr/bin/expect -f

if {{[llength $argv] != 1}} {{
  send_user "Usage: ./pqos -f <config-file>/-R\n"
  exit 1
}}

set trigger [lindex $argv 0]
set login "root"
set pw "riftIO"
set success 0
spawn ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $login@{host}
set spid $spawn_id
set timeout 60

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
if {{ $trigger eq "start" }} {{
    send "sudo timeout -s SIGINT 2.0 /usr/src/cat/pqos/pqos -f /usr/src/cat/pqos/intel-demo.cfg \r"
}}  else {{
    send "sudo /usr/src/cat/pqos/pqos -R \r"
}}
expect "]# "
exp_close -i $spid
'''.format(host=host))

    os.chmod(sh_file, stat.S_IRWXU)
    cmd = "{sh_file} {args}".format(sh_file=sh_file,args=trigger)
    rc = subprocess.call(cmd, shell=True)
    if rc != 0:
        raise ConfigurationError("Toggle CAT failed: {}".format(rc))

def main(argv=sys.argv[1:]):
    try:
        parser = argparse.ArgumentParser()
        parser.add_argument("yaml_cfg_file", type=argparse.FileType('r'))
        parser.add_argument("--quiet", "-q", dest="verbose", action="store_false")
        args = parser.parse_args()

        run_dir = os.path.join(os.environ['RIFT_INSTALL'], "var/run/rift")
        if not os.path.exists(run_dir):
            os.makedirs(run_dir)
        log_file = "{}/setreset_cat-{}.log".format(run_dir, time.strftime("%Y%m%d%H%M%S"))
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

        def find_param_value(param_list, input_param):
            for item in param_list:
               logger.debug("Parameter: %s", format(item))
               if item['name'] == input_param:
                  return item['value']

            raise ValueError("Could not find input param  %s", input_param)

        # parameter': [{'name': 'Host', 'value': 'grunt113.lab'}]
        host = find_param_value(yaml_cfg['rpc_ip']['parameter'], 'Host')
        trigger = find_param_value(yaml_cfg['rpc_ip']['parameter'], 'Trigger')

        toggle_cat(logger, run_dir, host, trigger)

    except Exception as e:
        logger.exception(e)
        raise

if __name__ == "__main__":
    main()
