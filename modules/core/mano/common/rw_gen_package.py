#!/usr/bin/env python3
import sys
import os
import subprocess
import argparse
import shutil
import xml.etree.ElementTree as etree

from gi.repository import (
    RwYang,
    NsdYang,
    RwNsdYang,
    VnfdYang,
    RwVnfdYang,
    VldYang,
    RwVldYang
)

def read_from_file(module_list, infile, input_format, descr_type):
      model = RwYang.Model.create_libncx()
      for module in module_list:
          model.load_module(module)
 
      descr = None
      if descr_type == "nsd": 
        descr = RwNsdYang.YangData_Nsd_NsdCatalog_Nsd()
      else:
        descr = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd()

      if input_format == 'json':
          json_str = open(infile).read()
          descr.from_json(model, json_str)
 
      elif input_format.strip() == 'xml':
          tree = etree.parse(infile)
          root = tree.getroot()
          xmlstr = etree.tostring(root, encoding="unicode")
          descr.from_xml_v2(model, xmlstr)
      else:
          raise("Invalid input format for the descriptor")
 
      return descr

def write_to_file(name, outdir, infile, descr_type):
      dirpath = os.path.join(outdir, name, descr_type)
      if not os.path.exists(dirpath):
          os.makedirs(dirpath)
      shutil.copy(infile, dirpath)

def main(argv=sys.argv[1:]):
      global outdir, output_format
      parser = argparse.ArgumentParser()
      parser.add_argument('-i', '--infile', required=True,
                          type=lambda x: os.path.isfile(x) and x or parser.error("INFILE does not exist"))
      parser.add_argument('-o', '--outdir', default=".",
                          type=lambda x: os.path.isdir(x) and x or parser.error("OUTDIR does not exist"))
      parser.add_argument('-f', '--format', choices=['json', 'xml'], required=True)
      parser.add_argument('-t', '--descriptor-type', choices=['nsd', 'vnfd'], required=True )

      args = parser.parse_args()
      infile = args.infile
      input_format = args.format
      outdir = args.outdir
      dtype = args.descriptor_type
 
      print('Reading file {} in format {}'.format(infile, input_format))
      module_list = ['vld', 'rw-vld']
      if dtype == 'nsd': 
          module_list.extend(['nsd', 'rw-nsd'])
      else:
          module_list.extend(['vnfd', 'rw-vnfd'])

      descr = read_from_file(module_list, args.infile, args.format, dtype)

      print("Creating %s descriptor for {}".format(dtype.upper(), descr.name))
      write_to_file(descr.name, outdir, infile, dtype)
      status = subprocess.call(os.path.join(os.environ["RIFT_ROOT"],  
              "bin/generate_descriptor_pkg.sh %s %s" % (outdir, descr.name)), shell=True)
      print("Status of %s descriptor package creation is: %s" % (dtype.upper(), status))
     

if __name__ == "__main__":
      main()


