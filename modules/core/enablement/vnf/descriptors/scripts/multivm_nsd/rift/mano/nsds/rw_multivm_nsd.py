#!/usr/bin/env python3

import sys
import os
import argparse
import uuid
import rift.vcs.component as vcs
import xml.etree.ElementTree as etree
import subprocess
import shutil
import os
import stat
import gi

gi.require_version('RwNsdYang', '1.0') 
gi.require_version('RwYang', '1.0') 
gi.require_version('RwVnfdYang', '1.0') 
gi.require_version('VnfdYang', '1.0') 
gi.require_version('VldYang', '1.0') 

from gi.repository import (
    RwYang,
    VnfdYang,
    RwVnfdYang,
    RwNsdYang,
    VldYang)

class ManoDescriptor(object):
    def __init__(self, name):
        self.name = name
        self.descriptor = None

    def write_to_file(self, module_list, outdir, output_format):
        model = RwYang.Model.create_libncx()
        for module in module_list:
            model.load_module(module)

        if output_format == 'json':
            with open('%s/%s.json' % (outdir, self.name), "w") as fh:
                fh.write(self.descriptor.to_json(model))
        elif output_format == 'yaml':
            with open('%s/%s.yml' % (outdir, self.name), "w") as fh:
                fh.write(self.descriptor.to_yaml(model))
        elif output_format.strip() == 'xml':
            with open('%s/%s.xml' % (outdir, self.name), "w") as fh:
                fh.write(self.descriptor.to_xml_v2(model, pretty_print=True))
        else:
            raise("Invalid output format for the descriptor")

class NetworkService(ManoDescriptor):
    def __init__(self, name):
        super(NetworkService, self).__init__(name)

    def add_scripts(self, srcfile):
        rift_root_dir = os.environ['RIFT_ROOT']
        scripts_dir = os.path.join(rift_root_dir, 'modules/core/enablement/vnf/descriptors/scripts/multivm_nsd/scripts/')
        copy_dir = os.path.join(self.dirpath, 'scripts')
        if not os.path.exists(copy_dir):
            os.makedirs(copy_dir)
        new_file = os.path.join(copy_dir, srcfile)
        shutil.copyfile(scripts_dir+srcfile, new_file, follow_symlinks=True)
        os.chmod(new_file, stat.S_IRWXU)

    def read_vnf_config(self, filename):
      with open(filename, 'r') as content_file:
        content = content_file.read()
        return content

    def default_config(self, const_vnfd, vnfd, src_dir, vnf_info):
        vnf_config = vnfd.mgmt_interface.vnf_configuration
        
        vnf_config.config_access.username = 'admin'
        vnf_config.config_access.password = 'admin'
        vnf_config.config_attributes.config_priority = 0
        vnf_config.config_attributes.config_delay = 0

        # Select "netconf" configuration
        vnf_config.netconf.target = 'running'
        vnf_config.netconf.port = 2022

        # print("### TBR ### vnfd.name = ", vnfd.name)
        
        if vnf_info['config_file'] != "":
            vnf_config.config_attributes.config_priority = int(vnf_info['config_priority'])
            # First priority config delay will delay the entire NS config delay
            vnf_config.config_attributes.config_delay = int(vnf_info['config_delay'])
            conf_file = src_dir + "/config/" + vnf_info['config_file']
            vnf_config.config_template = self.read_vnf_config(conf_file)

    def get_vnf_index(self, id):
        for const_vnfd in self.nsd.constituent_vnfd:
            if const_vnfd.vnfd_id_ref == id:
                return const_vnfd.member_vnf_index
        return 0


    def compose(self, vnfd_list, src_dir, template_info):
        self.descriptor = RwNsdYang.YangData_Nsd_NsdCatalog()
        self.id = str(uuid.uuid1())
        nsd = self.descriptor.nsd.add()
        nsd.id = self.id
        nsd.name = self.name
        nsd.short_name = self.name
        nsd.vendor = 'RIFT.io'
        nsd.description = '%s NS' % (self.name)
        nsd.version = '1.0'
        nsd.logo = 'riftio.png'

        i = 1
        for key,vnfd in vnfd_list.items():
            constituent_vnfd = nsd.constituent_vnfd.add()
            constituent_vnfd.member_vnf_index = int(template_info[key]['vnf_index'])
            constituent_vnfd.vnfd_id_ref = vnfd.id
            i = i+1

            # Add Configuration defaults
            # self.default_config(constituent_vnfd, vnfd, src_dir, template_info[key])


        self.nsd = nsd
        print(nsd)


    def compose_cpgroup(self, name, desc, cpgroup , vld_info):

        vld = self.nsd.vld.add()
        vld.id = str(uuid.uuid1())
        vld.name = name
        vld.short_name = name
        vld.vendor = 'RIFT.io'
        vld.description = desc
        vld.version = '1.0'
        vld.type_yang = 'ELAN'

        vld.provider_network.physical_network = vld_info['provider']
        vld.provider_network.overlay_type = vld_info['overlay']

        for cp in cpgroup:
            cpref = vld.vnfd_connection_point_ref.add()
            cpref.member_vnf_index_ref = cp[0]
            cpref.vnfd_id_ref = cp[1]
            cpref.vnfd_connection_point_ref = cp[2]


    def write_to_file(self, template_name, outdir, output_format):
        nsd_dirpath = "%s/nsd" % (self.dirpath)
        if not os.path.exists(nsd_dirpath):
            os.makedirs(nsd_dirpath)
        super(NetworkService, self).write_to_file(["nsd", "rw-nsd"],
                                                  "%s/%s_multivm_nsd/nsd" % (outdir, template_name),
                                                  output_format)

    def read_from_file(self, module_list, infile, input_format):
        model = RwYang.Model.create_libncx()
        for module in module_list:
            model.load_module(module)

        vnf = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd()
        if input_format == 'json':
            json_str = open(infile).read()
            vnf.from_json(model, json_str)

        elif input_format == 'yaml':
            yaml_str = open(infile).read()
            vnf.from_yaml(model, yaml_str)

        elif input_format.strip() == 'xml':
            tree = etree.parse(infile)
            root = tree.getroot()
            xmlstr = etree.tostring(root, encoding="unicode")

            vnf.from_xml_v2(model, xmlstr)
        else:
            raise("Invalid input format for the descriptor")

        return vnf

def generate_multivm_nsd_descriptors(fmt="json", write_to_file=False, out_dir="./", src_dir="./", template="", template_info={}):
    # List of connection point groups
    # Each connection point group refers to a virtual link
    # the CP group consists of tuples of connection points
    model = RwYang.Model.create_libncx()
    nsd_catalog = NetworkService(template_info['NS']['nsname'] + "_nsd")

    vnfd = {}
    # Open vnf pkg and read vnfdid
    for key,value in template_info.items():
      if key.startswith('VNF'):
        print("Starts With VNF %s" % key)

        vnf_info = value
        vnf_pkg = vnf_info['pkgname']
        vnf_dir = vnf_pkg.split(".", -1)[0]
        vnfd_dir = "./"+ vnf_dir +"/vnfd/"
        print(vnf_dir)
        cmd = "tar -zxvf {}{}".format(out_dir, '/../multivm_vnfd/' + vnf_pkg)
        print("Cmd is ", cmd)
        subprocess.call(cmd, shell=True)
        file_names = [fn for fn in os.listdir(vnfd_dir) if any(fn.endswith(ext) for ext in ["xml", "json", "yml"])]
        if len(file_names) != 1:
          print("%s have more files than expected" % vnfd_dir)
          exit
        if file_names[0].endswith("xml"):
          vnfd[key] = nsd_catalog.read_from_file(["vnfd"], vnfd_dir + "/" + file_names[0], "xml")
        elif file_names[0].endswith("yml"):
          vnfd[key] = nsd_catalog.read_from_file(["vnfd"], vnfd_dir + "/" + file_names[0], "yaml")
        else:
          vnfd[key] = nsd_catalog.read_from_file(["vnfd"], vnfd_dir + "/" + file_names[0], "json")
        print(vnfd[key])
        subprocess.call('rm -rf ./' + vnf_dir, shell=True)

    nsd_catalog.compose(vnfd, src_dir, template_info)

    for key,value in template_info.items():
      if key.startswith('VLD'):
        print("Starts With VLD %s" % key)
        cpgroup = []
        i = 1
        for cp,cp_info in value.items():
           if cp.startswith('cp'):
             print("Starts With cp %s" % cp)
             sp = cp_info.split(":", -1);
             vnfn = sp[0]
             cpindex = int(sp[1])-1
             cpname = vnfd[vnfn].connection_point[cpindex].name
             print(cpname)
             cpgroup.append((int(template_info[vnfn]['vnf_index']), vnfd[vnfn].id, cpname))
             i = i + 1

        print(cpgroup)
        nsd_catalog.compose_cpgroup(value['linkname'], 'Link', cpgroup, value)

    for key,value in template_info.items():
      if key.startswith('PG'):
        print("Starts With PG %s pgname %s" % (key,value['pgname']))
        group = nsd_catalog.nsd.placement_groups.add()
        group.name = value['pgname']

        try:
          group.strategy = value['strategy']
        except KeyError:
          pass

        try:
          group.requirement = value['requirement']
        except KeyError:
          pass
        
        for v,v_info in value.items():
           if v.startswith('vnf'):
             print("Starts With vnf %s" % v)
             member = group.member_vnfd.add()
             member.vnfd_id_ref = vnfd[v_info].id
             member.member_vnf_index_ref = nsd_catalog.get_vnf_index(member.vnfd_id_ref)

    # This will the directory path where the NSD will be written
    nsd_catalog.dirpath = "%s/%s_multivm_nsd" % (out_dir, template)

    for key,value in template_info.items():
      if key.startswith('NSP'):
        print("Starts With NSP %s script-template %s" % (key,value['nspname']))

        for v,v_info in value.items():
           if v_info.startswith('TrafgenTraffic'):
              print("Starts With TrafgenTraffic %s" % v)
              nsd_catalog.nsd.config_primitive.add().from_dict(
                {
                    "parameter": [
                      {
                          "data_type": "STRING",
                          "name": "Trigger",
                          "default_value": "stop",
                          "mandatory": True
                      },
                      {
                            "data_type": "STRING",
                            "name": "Packet size",
                            "default_value": "512",
                      },
                      {
                            "data_type": "STRING",
                            "name": "Tx rate",
                            "default_value": "5",
                      }
                    ],
                    "name": "Traffic",
                    "user_defined_script": "tgtraffic_startstop.py"
                })
              nsd_catalog.add_scripts("tgtraffic_startstop.py")

           if v_info == 'IkeTraffic_2A':
              print("IkeTraffic2A %s" % v)
              nsd_catalog.nsd.config_primitive.add().from_dict(
                {
                      "parameter": [
                       {
                            "data_type": "STRING",
                            "name": "Trigger",
                            "default_value": "stop",
                            "mandatory": True
                        },
                       {
                            "data_type": "STRING",
                            "name": "Tunnel Count",
                            "default_value": "10",
                            "mandatory": True
                        }
                      ],
                      "name": "IKE Traffic",
                      "user_defined_script": "iketraffic_startstop_2a.py"
                  })
              nsd_catalog.add_scripts("iketraffic_startstop_2a.py")

           if v_info == 'IkeTraffic_2B':
              print("IkeTraffic2B %s" % v)
              nsd_catalog.nsd.config_primitive.add().from_dict(
                {
                      "parameter": [
                       {
                            "data_type": "STRING",
                            "name": "Trigger",
                            "default_value": "stop",
                            "mandatory": True
                        },
                       {
                            "data_type": "STRING",
                            "name": "Tunnel Count",
                            "default_value": "10",
                            "mandatory": True
                        }
                      ],
                      "name": "IKE Traffic",
                      "user_defined_script": "iketraffic_startstop_2b.py"
                  })
              nsd_catalog.add_scripts("iketraffic_startstop_2b.py")

           if v_info == 'IkeTraffic':
              print("IkeTraffic %s" % v)
              nsd_catalog.nsd.config_primitive.add().from_dict(
                {
                      "parameter": [
                       {
                            "data_type": "STRING",
                            "name": "Trigger",
                            "default_value": "stop",
                            "mandatory": True
                        },
                       {
                            "data_type": "STRING",
                            "name": "Tunnel Count",
                            "default_value": "10",
                            "mandatory": True
                        }
                      ],
                      "name": "IKE Traffic",
                      "user_defined_script": "iketraffic_startstop.py"
                  })
              nsd_catalog.add_scripts("iketraffic_startstop.py")

           if v_info.startswith('CAT'):
              print("Starts With CAT %s" % v)
              nsd_catalog.nsd.config_primitive.add().from_dict(
                {
                      "parameter": [
                       {
                            "data_type": "STRING",
                            "name": "Host",
                            "mandatory": True
                        },
                        {
                              "data_type": "STRING",
                              "name": "Trigger",
                              "default_value": "stop",
                              "mandatory": True
                        }
                      ],
                      "name": "Toggle CAT",
                      "user_defined_script": "setreset_cat.py"
                  })
              nsd_catalog.add_scripts("setreset_cat.py")

           if v_info.startswith('Stream'):
              print("Starts With Stream %s" % v)
              nsd_catalog.nsd.config_primitive.add().from_dict(
                {
                      "parameter": [
                       {
                            "data_type": "STRING",
                            "name": "Host",
                            "mandatory": True
                        },
                        {
                              "data_type": "STRING",
                              "name": "Trigger",
                              "default_value": "stop",
                              "mandatory": True
                        }
                      ],
                      "name": "Noisy Neighbor",
                      "user_defined_script": "stream_startstop.py"
                  })
              nsd_catalog.add_scripts("stream_startstop.py")


    print(nsd_catalog.nsd)

    if write_to_file:
        nsd_catalog.write_to_file(template, out_dir, fmt)

    return (nsd_catalog)


def main(argv=sys.argv[1:]):
    global outdir, output_format
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--outdir', default='.')
    parser.add_argument('-s', '--srcdir', default='.')
    parser.add_argument('-f', '--format', default='json')
    parser.add_argument('-t', '--template', default = "" )
    args = parser.parse_args()
    outdir = args.outdir
    output_format = args.format

    template_info={}
    with open(args.srcdir + "/templates/" + args.template ) as fd:
      s = [line.strip().split(None, 1) for line in fd]
      for l in s:
        j = [k.strip().split("=", -1) for k in l[1].strip().split(",",-1)]
        template_info[l[0]] = dict(j)
     # for keys,values in template_info.items():
     #   print(keys)
     #   print(values)
    print(template_info)

    try:
      vnf = template_info['NS']
    except KeyError:
      print("Template does not have NS specified");
      raise

    generate_multivm_nsd_descriptors(args.format, True, args.outdir, args.srcdir, args.template, template_info)

if __name__ == "__main__":
    main()

