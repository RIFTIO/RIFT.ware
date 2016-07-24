#!/usr/bin/env python3
import os
import json
from gi.repository import NsdYang, RwYang

def add_pe_vnf(nsd, vnf_index, intf_ip_pairs):
    const_vnfd = nsd.constituent_vnfd.add()
    const_vnfd.vnfd_id_ref = "b7a3d170-c448-11e5-8795-fa163eb18cb8"
    const_vnfd.member_vnf_index = vnf_index

    vnf_config = const_vnfd.vnf_configuration
    vnf_config.config_attributes.config_priority = 0
    vnf_config.config_attributes.config_delay = 0

    # Select "script" configuration
    vnf_config.juju.charm = 'vpe-router'

    # Set the initital-config
    init_config = NsdYang.InitialConfigPrimitive.from_dict({
            "seq": 1,
            "name": "config",
            "parameter": [
                {"name": "vpe-router", "value": "<rw_mgmt_ip>"},
                {"name": "user", "value": "root"},
                {"name": "pass", "value": "6windos"}
            ]
        })
    vnf_config.initial_config_primitive.append(init_config)

    for seq, (intf, cidr) in enumerate(intf_ip_pairs, start=2):
        params = [{"name": "iface-name", "value": intf}]
        if cidr is not None:
            params.append(
                {"name": "cidr", "value": cidr}
            )

        vnf_config.initial_config_primitive.add().from_dict({
                "seq": seq,
                "name": "configure-interface",
                "parameter": params
            })


nsd = NsdYang.YangData_Nsd_NsdCatalog_Nsd()
add_pe_vnf(nsd, 1,
    [
        ("eth1", "10.10.10.9/30"),
        ("eth2", "10.10.10.1/30"),
        ("eth3", None),
    ]
)

add_pe_vnf(nsd, 2,
    [
        ("eth1", "10.10.10.10/30"),
        ("eth2", "10.10.10.6/30"),
        ("eth3", None),
    ]
)

add_pe_vnf(nsd, 3,
    [
        ("eth1", "10.10.10.2/30"),
        ("eth2", "10.10.10.5/30"),
        ("eth3", None),
        ("eth4", None),
    ]
)

ns_cfg_prim = nsd.config_primitive.add()
ns_cfg_prim.name = "Add SP Test Corporation"
ns_cfg_prim.user_defined_script = "/home/rift/.install/usr/bin/add_corporation.py"

ns_cfg_prim.parameter.add().from_dict({
    "name": "Corporation Name",
    "data_type": "string",
    "mandatory": True,
    })

ns_cfg_prim.parameter.add().from_dict({
        "name": 'Tunnel Key',
        "data_type": "integer",
        "mandatory": True,
        "default_value": "10",
    })

ns_cfg_prim.parameter_group.add().from_dict({
    "name": "PE1",
    "mandatory": False,
    "parameter": [
            {
                "name": 'Vlan ID',
                "data_type": "integer",
                "mandatory": True,
                "default_value": "3000",
            },
            {
                "name": 'Interface Name',
                "data_type": "string",
                "mandatory": True,
                "default_value": "eth3",
            },
            {
                "name": 'Corp. Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.1.0/24",
            },
            {
                "name": 'Corp. Gateway',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.1.1",
            },
            {
                "name": 'Local Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.255.255.0/24",
            },
            {
                "name": 'Local Network Area',
                "data_type": "string",
                "mandatory": True,
                "default_value": "0",
            },
        ]
    })

ns_cfg_prim.parameter_group.add().from_dict({
    "name": "PE2",
    "mandatory": False,
    "parameter": [
            {
                "name": 'Vlan ID',
                "data_type": "integer",
                "mandatory": True,
                "default_value": "3000",
            },
            {
                "name": 'Interface Name',
                "data_type": "string",
                "mandatory": True,
                "default_value": "eth3",
            },
            {
                "name": 'Corp. Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.2.0/24",
            },
            {
                "name": 'Corp. Gateway',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.2.1",
            },
            {
                "name": 'Local Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.255.255.0/24",
            },
            {
                "name": 'Local Network Area',
                "data_type": "string",
                "mandatory": True,
                "default_value": "0",
            },
        ]
    })

ns_cfg_prim.parameter_group.add().from_dict({
    "name": "PE3",
    "mandatory": False,
    "parameter": [
            {
                "name": 'Vlan ID',
                "data_type": "integer",
                "mandatory": True,
                "default_value": "3000",
            },
            {
                "name": 'Interface Name',
                "data_type": "string",
                "mandatory": True,
                "default_value": "eth3",
            },
            {
                "name": 'Corp. Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.3.0/24",
            },
            {
                "name": 'Corp. Gateway',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.3.1",
            },
            {
                "name": 'Local Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.255.255.0/24",
            },
            {
                "name": 'Local Network Area',
                "data_type": "string",
                "mandatory": True,
                "default_value": "0",
            },
        ]
    })

ns_cfg_prim = nsd.config_primitive.add()
ns_cfg_prim.name = "Add Corporation"
ns_cfg_prim.user_defined_script = "/home/rift/.install/usr/bin/add_corporation.py"

ns_cfg_prim.parameter.add().from_dict({
    "name": "Corporation Name",
    "data_type": "string",
    "mandatory": True,
    })

ns_cfg_prim.parameter.add().from_dict({
        "name": 'Tunnel Key',
        "data_type": "integer",
        "mandatory": True,
        "default_value": "1",
    })

ns_cfg_prim.parameter_group.add().from_dict({
    "name": "PE1",
    "mandatory": False,
    "parameter": [
            {
                "name": 'Vlan ID',
                "data_type": "integer",
                "mandatory": True,
                "default_value": "101",
            },
            {
                "name": 'Interface Name',
                "data_type": "string",
                "mandatory": True,
                "default_value": "eth3",
            },
            {
                "name": 'Corp. Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.1.0/24",
            },
            {
                "name": 'Corp. Gateway',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.1.1",
            },
            {
                "name": 'Local Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.255.255.0/24",
            },
            {
                "name": 'Local Network Area',
                "data_type": "string",
                "mandatory": True,
                "default_value": "0",
            },
        ]
    })

ns_cfg_prim.parameter_group.add().from_dict({
    "name": "PE2",
    "mandatory": False,
    "parameter": [
            {
                "name": 'Vlan ID',
                "data_type": "integer",
                "mandatory": True,
                "default_value": "102",
            },
            {
                "name": 'Interface Name',
                "data_type": "string",
                "mandatory": True,
                "default_value": "eth3",
            },
            {
                "name": 'Corp. Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.2.0/24",
            },
            {
                "name": 'Corp. Gateway',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.2.1",
            },
            {
                "name": 'Local Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.255.255.0/24",
            },
            {
                "name": 'Local Network Area',
                "data_type": "string",
                "mandatory": True,
                "default_value": "0",
            },
        ]
    })

ns_cfg_prim.parameter_group.add().from_dict({
    "name": "PE3",
    "mandatory": False,
    "parameter": [
            {
                "name": 'Vlan ID',
                "data_type": "integer",
                "mandatory": True,
                "default_value": "108",
            },
            {
                "name": 'Interface Name',
                "data_type": "string",
                "mandatory": True,
                "default_value": "eth4",
            },
            {
                "name": 'Corp. Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.4.0/24",
            },
            {
                "name": 'Corp. Gateway',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.0.4.1",
            },
            {
                "name": 'Local Network',
                "data_type": "string",
                "mandatory": True,
                "default_value": "10.255.255.0/24",
            },
            {
                "name": 'Local Network Area',
                "data_type": "string",
                "mandatory": True,
                "default_value": "0",
            },
        ]
    })

model = RwYang.Model.create_libncx()
model.load_module("nsd")
print(nsd.to_xml_v2(model, pretty_print=True))

print("\n\n")
print(json.dumps(nsd.as_dict(), indent=4, separators=(',', ': ')))
