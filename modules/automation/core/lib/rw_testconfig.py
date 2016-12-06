"""
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

@file rw_testconfig
@author Karun
@author Jeremy Mordkoff (Jeremy.Mordkoff@riftio.com)

11/20/14 -- added accessor functions

"""




import json
from jsonschema import validate
from jsonschema import ValidationError
import os
import re
import string
import sys
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
if '--debug' in sys.argv:
    logger.setLevel(logging.DEBUG)


"""
This class is to read and act on the test configuration file
Addtional python pacakges required: jsonschema
"""

class TestConfiguration(object):
    
    
    def __init__(self, configfile, sut_file=None):
        """
        @param configfile: input configuration file to be processed (a JSON file)
        """
        self.configfile = configfile
        self._errmsg = ""
        self.sut_file = sut_file
        self.suts = None
        ''' DANGER WILL ROBINSON
        this really should be the rift root of the test, as if the test config schema ever changes, this will simply fail
        as it stands, it is the rift root of the parent process, which may be the harness which may be running in a different
        rift shell
        '''
        json_schema = os.path.join(
                os.environ["RIFT_INSTALL"],
                "usr/rift/systemtest/etc",
                "racfg_schema.json"
                )
        try:
            self._json_schema = self._parse_json(json_schema)
        except Exception as e:
            self.set_error("error parsing %s" % json_schema)
            raise
        try:
            self._config = self._parse_json(configfile)
        except Exception as e:
            self.set_error("error parsing %s" % configfile)
            raise
        
        if not self.is_valid():
            print("WARNING - config is not valid: %s" % self._errmsg)
        if self.sut_file is not None:
            self.load_sut_file()

        self.post_reset_vms = bool(self._config['vms'])
        self.required_tenants = int(self._config.get('required_tenants', 1))
    
    
    def validate_suts(self):
        for vm in self.vms:
            test_vm_name = vm['name']
            if not test_vm_name in self.suts['vms']:
                logger.critical("no mapping for logical vm %s" % test_vm_name)
                raise ValueError("no mapping for logical vm %s" % test_vm_name)
            actual_vm = self.suts['vms'][test_vm_name]
            logger.info("logical vm %s will be played by actual vm %s" % ( test_vm_name, actual_vm ))
            
        for test_net in self.networks:
            if not test_net in self.suts['nets']:
                logger.critical("no mapping for test net %s" % test_net )
                raise ValueError("no mapping for test net %s" % test_net )
            actual_switch_name = self.suts['nets'][test_net]
            logger.info("using switch %s for test net %s" % ( actual_switch_name, test_net ))
        if self.target_vm:
            if not self.target_vm in self.suts['vms']:
                raise ValueError("target VM is not in the list of vms")
        if not self.target_vm:
            logger.info("No target VM specfied, checking for defaults")
            for vm_name in ['VM', 'rift_auto_launchpad']:
                if vm_name in self.suts['vms']:
                    self._config['target_vm'] = vm_name
                    break
        if not self.target_vm:
            logger.info("No default target available, choosing first available")
            self._config['target_vm'] = list(self.suts['vms'].keys())[0]
        logger.info("Target VM is %s", self.target_vm)

    
    def set_sut_file(self, sut_file):
        self.sut_file = sut_file
        self.load_sut_file()
        
    def load_sut_file(self):
        with open(self.sut_file, "r") as f:
            self.suts = json.loads(f.read() )
        self.validate_suts()
            
    def get_actual_vm(self, logical_vm):
        ''' map a logical VM to an actual VM name
        '''
        if self.suts is None:
            raise ValueError("suts not loaded")
        if not logical_vm in self.suts['vms']:
            raise ValueError("VM %s not found in SUTs" % logical_vm)
        return self.suts['vms'][logical_vm]

    def get_assigned_vm_addr(self, logical_vm):
        if self.suts is None:
            raise ValueError("suts not loaded")
        qualified_vm_addr = "%s.%s" % (self._config['test_name'], logical_vm)
        if 'vm_addrs' not in self.suts:
            return None
        if not qualified_vm_addr in self.suts['vm_addrs']:
            return None
        return self.suts['vm_addrs'][qualified_vm_addr]
    
    def get_actual_network(self, logical_net):
        ''' map a logical network name to an actual net
        '''
        if self.suts is None:
            raise ValueError("suts not loaded")
        if not logical_net in self.suts['nets']:
            raise ValueError("network %s not found in SUTs" % logical_net)
        return self.suts['nets'][logical_net]
    
    def _parse_json(self, jsfile_in):
        
        jsfile = self._locate_file(jsfile_in)
        if jsfile is None:
            raise ValueError("cannot find file %s" % jsfile_in)
        try:
            with open(jsfile) as js:
                json_data_str = js.read()
                js.close()
                return json.loads(json_data_str)
        except IOError as e:
            self.set_error("Cannot open file: %s: %s" % ( jsfile, e ))
        except ValueError as e:
            self.set_error("Invalid JSON data in file: %s: %s" % ( jsfile, e))
            raise ValueError(self._errmsg)
        return None
    
    def set_error(self, err):
        logger.error(err)
        self._errmsg = err
        
    @property
    def test_status(self):
        return self._config.get('status','working')
    
    @property
    def keywords(self):
        return self._config.get('keywords', [])
    
    @property
    def submitted_by(self):
        print(self._config)
        
        return self._config.get('submitted_by', None)
        
    @property
    def get_error(self):
        """
        To get the last error message during the action on configuration file
        This shall be used if a call returns False, empty value or None, 
        to check the cause of failure
        """
        return self._errmsg
    
    def is_valid(self):
        if self._json_schema is None:
            self._errmsg="JSON schema is not loaded"
            print(self._errmsg)
            return False
        if self._config is None:
            self._errmsg="config is not loaded"
            print(self._errmsg)
            return False
        return self.validate_configuration_file()
    
    @property
    def configuration(self):
        """
        To get the configurtion as JSON object
        """
        return self._config

    @property 
    def target_vm(self):
        return self._config.get('target_vm', None)
    
    def get_target_vm_ip_address(self, testbed):
        vm_name = self.target_vm
        if vm_name is None:
            return None
        else:
            return self.get_ip_address(vm_name, testbed)
    
    def get_ip_address(self, logical_vm_name, testbed):
        print("LOGICAL VM NAME IS %s" % logical_vm_name)
        vm_addr = self.get_assigned_vm_addr(logical_vm_name)
        if vm_addr:
            return vm_addr
        actual_vm_name = self.get_actual_vm(logical_vm_name)
        host = testbed.find_host(actual_vm_name)
        if host.ipaddress is None:
            raise ValueError("host %s has no IP address" % actual_vm_name)

        return host.ipaddress

    def get_all_vm_ip_addresses(self, testbed):
        if not self.vms:
            return ['127.0.0.1']
        return [ self.get_ip_address(vm, testbed) for vm in self.suts['vms'] ]

    @property 
    def run_as_root(self):
        return self._config.get('run_as_root', False)
    
    @property 
    def networks(self):
        return self._config.get('networks', [])

    @property
    def vms(self):
        return self._config['vms']
    
    def _check_script_existence(self):
        cmd = self._config['commandline'].split()[0]
        if os.path.dirname(cmd) == "":
            # no validation if no path
            return True
        cmd = self._get_script_path()
        if cmd is None:
            self._errmsg = "script file not found" 
            print(self._errmsg)
            return False
        return True
    
    def _validate_json(self):
        try:
            validate(self._config, self._json_schema)
            return True
        except ValidationError as e:
            self._errmsg = e.message
            print(self._errmsg)
            return False
    
    def _get_pretty_config(self):
        if not self.is_valid:
            return ""
        return json.dumps(self._config, sort_keys=False, indent=4, separators=(',', ': '))
    
    def validate_configuration_file(self):
        """
        To validate the user's configuraton file
        1. Validate the configuraton file (json) against the schema
        2. Check for existence of the the test script provided in the command line
        
        Return the result (boolean) of the validation. Check the property "error" 
        for any error message (when False returned)
        """
        if not self._validate_json(): return False
        if not self._check_script_existence(): return False
        net_counts=dict()
        for netname in self.networks:
            net_counts[netname] = 0
        for vm in self.vms:
            for keyname in vm.keys():
                if keyname in self.networks: 
                    if not keyname in net_counts:
                        self._errmsg = "network %s referenced in VMs is not defined under networks" % keyname
                        print(self._errmsg)
                        return False
                    net_counts[keyname] = net_counts[keyname] + 1
   #     for netname in self.networks:
   #         if net_counts[netname] < 2 and netname != "internet":
   #             self._errmsg = "network %s is not referenced by at least 2 VMs" % netname
   #             print(self._errmsg)
   #             return False
        return True
    
    
    
    def _get_script_path(self):
        # locate the script
        cmd = self._config['commandline'].split()[0]
        pth = self._locate_file(cmd)
        if pth is None:
            return cmd
        else:
            return pth
        
    def _locate_file(self,fn):
        if os.path.exists(fn):
            return os.path.abspath(fn)
        RIFT_ROOT = os.environ.get('RIFT_ROOT', '/usr/rift')
        HERE = os.path.dirname(self.configfile)
        paths = [    
                    ".",
                    RIFT_ROOT,
                    HERE,
                    os.path.join(RIFT_ROOT, HERE ),
                ]
        script = None
        for pth in paths:
            if os.path.exists(os.path.join(pth, fn)):
                script = os.path.abspath(os.path.join(pth, fn))
                break
        return script
        
    def get_commandline(self, cmdln_options=[], testbed=None):
        """
        To get the updated original command_line with any addional/updated arguments
        (original command line in the config file will be intact)
        
        @param cmdln_options: dictionary with option and value (note: a switch followed no value 
            is denoted by an empty string, example, "-x": "")
            e.g., cmdln_options = {'--logfile': '/tmp/scriptlog.txt', '--debug': 'trace', "-x":""}
            
        Check the property "error" for any error message (when empty string returned)
        """
        if not self.is_valid():
            raise ValueError
            return ""

        # make sure the script path is fully qualified        
        splitted_cmdln = self._config['commandline'].split()
        splitted_cmdln[0] = self._get_script_path()
        args_dict = dict(cmdln_options)
        args_dict['config_file']=self._locate_file(self.configfile)
        args_dict['sut_file'] = self.sut_file
        args_dict['pwd'] = os.path.dirname(args_dict['config_file'])

        if 'rpm' in args_dict:
            args_dict['rift_root'] = '/usr/rift'
        else:
            args_dict['rift_root'] = os.environ.get('RIFT_ROOT', '/usr/rift')

        if testbed:
            # make sure target_vm is first
            ips = self.get_all_vm_ip_addresses(testbed)
            if self.target_vm:
                target_ip = self.get_target_vm_ip_address(testbed)
                ips.remove(target_ip)
                ips.insert(0,target_ip)
                args_dict['target_vm'] = target_ip
            args_dict['iplist'] = ",".join(ips)

        if 'tenants' in args_dict:
            args_dict['tenants'] = ' '.join(["--tenant %s" % (tenant) for tenant in args_dict['tenants']])

        for k,v in args_dict.items():
            logger.debug("keyword \"%s\" => \"%s\"" % ( k, v))
        cmdline = " ".join(splitted_cmdln)
        try:
            cmdline = cmdline.format(**args_dict)
        except KeyError as e:
            logger.critical("error formatting cmdline: %s" % e)
            logger.critical("base cmd line: \"%s\"" % cmdline )
            logger.critical("args: %s" % args_dict )
            raise
        logger.debug("cmdline: \"%s\"" % cmdline )
        return cmdline
        
    def print_config(self):
        """
        To pretty printing the resulted json
        Check the property "error" for any error message (if nothing gets printed)
        """
        print(self._get_pretty_config())

    def write_config(self, output_file):
        """
        To write the processed configuration file to a given file. Content of the file will 
        be the same except that its format will be in pretty-printed format
        
        @param output_file: output file
        
        Return the result (boolean) of the write operation. Check the property "error" for any 
        error message (when False returned, or nothing written to file)
        """
        try:
            with open(output_file, "w") as output:
                output.write(self._get_pretty_config())
            return True
        except IOError:
            self._errmsg = "Cannot write to file: %s" % jsfile
        return False
    
    @property
    def timeout(self):
        if 'timelimit' in self._config:
            return self._config['timelimit']
        else:
            return 120

    @property
    def test_name(self):
        return self._config['test_name']

    @property
    def description(self):
        return self._config['test_description']

    @property
    def junit_pathname(self):
        fn=os.path.basename(self.configfile).replace('-','_').replace('racfg', 'xml')
        pth=os.path.join(os.environ.get('RIFT_ARTIFACTS'), 'systemtest')
        if not os.path.exists(pth):
            os.mkdir(pth)
        return os.path.join(pth, fn)



#end of TestConfiguration

if __name__ == "__main__":
    tc = TestConfiguration('raconfg.json')
    print(tc.get_error)
    print(tc.get_commandline(
                            {"--debug": "debug", "-k":""}
                            ))
    print("=" * 50)
    print(tc.configuration['vms'][0]['fabric1'])
    tc.write_config("/tmp/myfile")
    print("=" * 50)
    tc.print_config()
