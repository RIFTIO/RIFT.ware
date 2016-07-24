#!/bin/bash

tmp_dir=$(mktemp -d)
echo "Generating packages in temporary directory: ${tmp_dir}"

#6WindTR1.1.2 VNF
mkdir -p ${tmp_dir}/6wind_vnf/vnfd
cp -f rift_vnfs/6WindTR1.1.2.yaml ${tmp_dir}/6wind_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} 6wind_vnf


# mwc16-pe.yaml
mkdir -p ${tmp_dir}/mwc16_pe_ns/nsd
mkdir -p ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/scripts
mkdir -p ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/config
mkdir -p ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/charms/trusty
cp -f rift_scenarios/mwc16-pe.yaml ${tmp_dir}/mwc16_pe_ns/nsd
cp -f rift_scenarios/config/mwc16-pe.yaml ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/config
cp -f rift_scenarios/config/1.yaml ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/config
cp -f rift_scenarios/config/2.yaml ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/config
cp -f rift_scenarios/config/3.yaml ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/config
cp -rf rift_scenarios/charms/vpe-router ${tmp_dir}/mwc16_pe_ns/libs/mwc16-pe/charms/trusty
cp -f rift_scenarios/scripts/add_corporation.py ${tmp_dir}/mwc16_pe_ns/libs/mwc16_pe_ns/scripts
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_pe_ns

# tidgen_mwc16_vnf.yaml
mkdir -p ${tmp_dir}/tidgen_mwc16_vnf/vnfd
cp -f rift_vnfs/mwc16gen1.yaml ${tmp_dir}/tidgen_mwc16_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} tidgen_mwc16_vnf

# mwc16-gen.yaml
mkdir -p ${tmp_dir}/mwc16_gen_ns/nsd
cp -f rift_scenarios/mwc16-gen.yaml ${tmp_dir}/mwc16_gen_ns/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_gen_ns


# IMS-ALLin1_2p.yaml
mkdir -p ${tmp_dir}/ims_allin1_2p_vnf/vnfd
cp -f rift_vnfs/IMS-ALLIN1.yaml ${tmp_dir}/ims_allin1_2p_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} ims_allin1_2p_vnf

# IMS-allin1-corpa.yaml
mkdir -p ${tmp_dir}/ims_allin1_corpa/nsd
mkdir -p ${tmp_dir}/ims_allin1_corpa/libs/ims_allin1_corpa/config
mkdir -p ${tmp_dir}/ims_allin1_corpa/libs/ims_allin1_corpa/charms/trusty
cp -f rift_scenarios/IMS-corpA.yaml ${tmp_dir}/ims_allin1_corpa/nsd
cp -f rift_scenarios/config/IMS-ALLIN1_2p.yaml ${tmp_dir}/ims_allin1_corpa/libs/ims_allin1_corpa/config
cp -rf rift_scenarios/charms/clearwater-aio-proxy ${tmp_dir}/ims_allin1_corpa/libs/ims_allin1_corpa/charms/trusty
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} ims_allin1_corpa


# gw_corpA_PE1.yaml
mkdir -p ${tmp_dir}/gw_corpa_pe1_vnf/vnfd
cp -f rift_vnfs/gw-corpa-pe1.yaml ${tmp_dir}/gw_corpa_pe1_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} gw_corpa_pe1_vnf

# gw_corpA_PE2.yaml
mkdir -p ${tmp_dir}/gw_corpa_pe2_vnf/vnfd
cp -f rift_vnfs/gw-corpa-pe2.yaml ${tmp_dir}/gw_corpa_pe2_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} gw_corpa_pe2_vnf

# gw_corpa_ns.yaml
mkdir -p ${tmp_dir}/gw_corpa_ns/nsd
cp -f rift_scenarios/gwcorpA.yaml ${tmp_dir}/gw_corpa_ns/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} gw_corpa_ns
