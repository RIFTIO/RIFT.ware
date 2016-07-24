#!/bin/bash

tmp_dir=$(mktemp -d)
echo "Generating packages in temporary directory: ${tmp_dir}"

#6WindTR1.1.2 VNF
mkdir -p ${tmp_dir}/6wind_vnf/vnfd
cp -f rift_vnfs/6WindTR1.1.2.xml ${tmp_dir}/6wind_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} 6wind_vnf


# mwc16-pe.yaml
mkdir -p ${tmp_dir}/mwc16_pe_ns/nsd
cp -f rift_scenarios/mwc16-pe.xml ${tmp_dir}/mwc16_pe_ns/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_pe_ns

# mwc16-pe-onevnf.yaml
mkdir -p ${tmp_dir}/mwc16_pe_onevnf_ns/nsd
cp -f rift_scenarios/mwc16-pe-onevnf.xml ${tmp_dir}/mwc16_pe_onevnf_ns/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_pe_onevnf_ns


# mwc16-gen1.yaml
mkdir -p ${tmp_dir}/mwc16_gen1_vnf/vnfd
cp -f rift_vnfs/mwc16gen1.xml ${tmp_dir}/mwc16_gen1_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_gen1_vnf

# mwc16-gen2.yaml
mkdir -p ${tmp_dir}/mwc16_gen2_vnf/vnfd
cp -f rift_vnfs/mwc16gen2.xml ${tmp_dir}/mwc16_gen2_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_gen2_vnf

# mwc16-gen.yaml
mkdir -p ${tmp_dir}/mwc16_gen_ns/nsd
cp -f rift_scenarios/mwc16-gen.xml ${tmp_dir}/mwc16_gen_ns/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} mwc16_gen_ns


# IMS-ALLin1.yaml
mkdir -p ${tmp_dir}/ims_allin1_vnf/vnfd
cp -f rift_vnfs/IMS-ALLIN1.xml ${tmp_dir}/ims_allin1_vnf/vnfd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} ims_allin1_vnf

# IMS-allin1-corpa.yaml
mkdir -p ${tmp_dir}/ims_allin1_corpa/nsd
cp -f rift_scenarios/IMS-corpA.xml ${tmp_dir}/ims_allin1_corpa/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} ims_allin1_corpa

# IMS-allin1-corpb.yaml
mkdir -p ${tmp_dir}/ims_allin1_corpb/nsd
cp -f rift_scenarios/IMS-corpB.xml ${tmp_dir}/ims_allin1_corpb/nsd
${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir} ims_allin1_corpb

