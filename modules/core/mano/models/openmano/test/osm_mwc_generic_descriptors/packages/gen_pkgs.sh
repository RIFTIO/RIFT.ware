#!/bin/bash

set -o nounset
set -x

tmp_dir=$(mktemp -d)
echo "Generating packages in temporary directory: ${tmp_dir}"

# Create any missing directories/files so each package has
# a complete hierachy
nsd_dirs=( ns_config vnf_config icons scripts )
nsd_files=( README )
for nsd_dir in nsd/*/; do
    tmp_nsd_dir=${tmp_dir}/${nsd_dir}
    mkdir -p ${tmp_nsd_dir}
    cp -rf ${nsd_dir}/* ${tmp_nsd_dir}
    for sub_dir in ${nsd_dirs[@]}; do
        dir_path=${tmp_nsd_dir}/${sub_dir}
        mkdir -p ${dir_path}
    done

    for file in ${nsd_files[@]}; do
        file_path=${tmp_nsd_dir}/${file}
        touch ${file_path}
    done
done

vnfd_dirs=( charms icons scripts images )
vnfd_files=( README )
for vnfd_dir in vnfd/*/; do
    tmp_vnfd_dir=${tmp_dir}/${vnfd_dir}
    mkdir -p ${tmp_vnfd_dir}
    cp -rf ${vnfd_dir}/* ${tmp_vnfd_dir}
    for sub_dir in ${vnfd_dirs[@]}; do
        dir_path=${tmp_vnfd_dir}/${sub_dir}
        mkdir -p ${dir_path}
    done

    for file in ${vnfd_files[@]}; do
        file_path=${tmp_vnfd_dir}/${file}
        touch ${file_path}
    done
done


# Create the packages
for package_dir in ${tmp_dir}/nsd/*/; do
    ${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir}/nsd $(basename ${package_dir})
done

for package_dir in ${tmp_dir}/vnfd/*/; do
    ${RIFT_ROOT}/bin/generate_descriptor_pkg.sh ${tmp_dir}/vnfd $(basename ${package_dir})
done
