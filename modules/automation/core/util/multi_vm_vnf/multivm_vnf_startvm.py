#!/usr/bin/env python3
import time
import sys
import os
import subprocess
import shlex

import kazoo.exceptions
import rift.auto.vm_utils
import rift.cal
import rift.cal.rwzk

arg_vnf_id = sys.argv[1]
arg_vdu_id = sys.argv[2]
arg_master_ip_addr = sys.argv[3]
arg_vm_ip_addr = sys.argv[4]

rift_root = os.environ["RIFT_ROOT"]
kmod_cmd = "sudo %s/scripts/cloud/install_kernel_mods --rift-root %s" % (rift_root, rift_root)
subprocess.check_call(shlex.split(kmod_cmd))

lock_path = '/sys/rwmain/iamup-LOCK'
uniq_id = 100

_cli = rift.cal.rwzk.Kazoo()
_cli.client_init(unique_ports = False, server_names = [ arg_master_ip_addr ], timeout=600)
while True:
  try:
    _cli.lock_node(lock_path, 100000.0)
    try:
      uniq_id = _cli.get_node_data(lock_path)
      print ('uniq_id=', uniq_id)
      print ('typeof-uniq_id=', type(uniq_id))
      if uniq_id is None:
        _cli.unlock_node(lock_path)
        time.sleep(5)
        continue
      break
    except:
      print('get_node_data - kazoo.exceptions.NoNodeError')
      _cli.unlock_node(lock_path)
      continue
  except kazoo.exceptions.LockTimeout:
    print('kazoo.exceptions.LockTimeout')
    exit
  except kazoo.exceptions.NoNodeError:
    print('kazoo.exceptions.NoNodeError')
    time.sleep(5)

new_uniq_id_val = int(uniq_id) + 1
new_uniq_id = '%d' %(new_uniq_id_val)
_cli.set_node_data(lock_path, new_uniq_id.encode(), None)

#node_path = '/sys/rwmain/iamup/%s:%s:%u' %(arg_vdu_id, arg_vm_ip_addr, new_uniq_id_val)
node_path = '/sys/rwmain/iamup/%s' %(arg_vm_ip_addr)
try:
  _cli.create_node(node_path)
except kazoo.exceptions.NodeExistsError:
  pass

vm_data = '%s:%s:%s:%u' %(arg_vnf_id, arg_vdu_id, arg_vm_ip_addr, new_uniq_id_val)
_cli.set_node_data(node_path, vm_data.encode(), None)
vm_data = _cli.get_node_data(node_path)
print(vm_data.decode())

data = arg_vm_ip_addr
_cli.set_node_data('/sys/rwmain/iamup', data.encode(), None)

#_cli.delete_node(node_path)
_cli.unlock_node(lock_path)

