#!/usr/bin/python

import os
import sys
import hashlib

def get_sha1(file_name):
  hobj = hashlib.sha1()
  try:
    with open(file_name, "rb") as fd:
      hobj.update(fd.read())
      return "{0}:{1}".format(os.path.basename(file_name), hobj.hexdigest())
  except Exception as e:
    return "{}:0".format(os.path.basename(file_name))

if __name__ == "__main__":
  yang_name    = sys.argv[1]
  fxs_name     = sys.argv[1]
  pc_name      = sys.argv[2]
  typelib_name = sys.argv[3]
  vapi_name    = sys.argv[4]
  lib_name     = sys.argv[5]
  src_dir      = sys.argv[6]
  bin_dir      = os.getcwd()

  op_file = "{0}/{1}".format(bin_dir, "{}.meta_info.txt".format(yang_name))

  file_list = ["{0}/{1}".format(src_dir, yang_name + ".yang"),
               "{0}/{1}".format(bin_dir, fxs_name  + ".fxs"),
               "{0}/{1}".format(bin_dir, pc_name   + ".pc"),
               "{0}/{1}".format(bin_dir, typelib_name + ".typelib"),
               "{0}/{1}".format(bin_dir, vapi_name + ".vapi"),
               "{0}/{1}".format(bin_dir, "lib{}.so".format(lib_name))]

  with open(op_file, "w") as f:
    op = [ get_sha1(fname) for fname in file_list ]
    f.write('\n'.join(op))
