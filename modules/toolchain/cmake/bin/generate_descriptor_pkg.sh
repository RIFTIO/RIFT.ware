#!/bin/bash

############################################################################
# Copyright 2016 RIFT.io Inc                                               #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
############################################################################

#
# This shell script is used to create a descriptor package
# The main functions of this script include:
# - Generate checksums.txt file
# - Generate a tar.gz file
# This script can be used to create the required folders for
# a descriptor package and a template descriptor

# Usage: generate_descriptor_pkg.sh <base-directory> <package-directory>

# Descriptor names should be
#   - (nsd|vnfd).(yaml|yml|json|xml)
#   - *_(nsd|vnfd).(yaml|yml|json|xml)
#   - *_(nsd|vnfd)_*.(yaml|yml|json|xml)
#   - (nsd|vnfd)/*.(yaml|yml|json|xml)
#

SCRIPTNAME=`basename $0`

# From https://osm.etsi.org/wikipub/index.php/Release_0_Data_Model_Details
# Supported folders for VNFD
# cloud_init - Rel 4.3, not yet part of OSM
VNFD_FOLDERS=(images scripts icons charms cloud_init misc)

# Supported folders for NSD
# OSM document specifies (ns|vnf)-config folder, while Rel 4.3
# is using (ns|vnf)_config.
NSD_FOLDERS=(scripts icons ns_config vnf_config)

# Other files allowed in the descriptor base directory
ALLOWED_FILES=(README)

DESC_TYPES=(vnfd nsd)
DESC_EXTN=(yml yaml json xml)
CHKSUM='checksums.txt'

VERBOSE=false
DRY_RUN=false
CREATE=false
RM="--remove-files"
DEBUG=false

ARCHIVE=false
CREATE_NSD=false
VENDOR='RIFT.io'
INTF_TYPE='VIRTIO'
VCPU=2
MEMORY=4096
STORAGE=10
CLOUD_INIT='#cloud-config '
INTERFACES=1

function usage() {
    cat <<EOF
Usage:
     $SCRIPTNAME [-t <type>] [-N] [-c] [base-directory] <package-name>

        -h|--help : show this message

        -t|--package-type <nsd|vnfd> : Descriptor package type
                                       is NSD or VNFD. Script will try to
                                       determine the type if not provided.
                                       Default is vnfd for create-folders.

        -d|--destination-dir <destination directory>: Directory to create the
                                                      archived file.
                                                      Default is base-directory

        -N|--no-remove-files : Do not remove the package files after creating
                               archive

        Options specifc for create descriptor:

            -c|--create-folder : Create folder with the structure for the
                                 package type using the base-dir and package-dir
                                 and a descriptor template

            -a|--archive: Create package for the descriptor

            --nsd : Generate NSD descriptor package also.

            --vendor : Vendor name for descriptor. Default RIFT.io

            --interface-type : Interface type [VIRTIO|SR-IOV|PCI-PASSTHROUGH|E1000|OM-MGMT]
                               Default VIRTIO

          VM Flavour options:

            --vcpu : Virtual CPU count. Default 2

            --memory : Memory for VM in MB. Default 4096MB

            --storage : Storage size for VM in GB. Default 10GB

          VDU Parameters:

            --image : Location URI of the image

            --cloud-init-file : Cloud init file

            --cloud-init : Cloud init script. Will be ignored if
                           cloud-init-file is specified

            --interfaces : Number of external interfaces. Default 1.

        End of create descriptor specific options

        -v| --verbose : Generate progress details

        -n| --dry-run : Validate the package dir

        base-dir : Directory where the archive file or folders are created,
                   if destination directory is not specified.
                   Default is current directory

        package-name : The descriptor name (full path if base-dir not specified)
EOF
}

CP_TYPE='VPORT'
function get_cp_type() {
    case ${INTF_TYPE} in
        VIRTIO ) CP_TYPE='VPORT';;
        SR-IOV ) CP_TYPE='VPORT';;
        PCI-PASSTHROUGH ) CP_TYPE='VPORT';;
        OM-MGMT ) CP_TYPE='VPORT';;
        E1000 ) CP_TYPE='VPORT';;
        * ) echo "ERROR: Unknown interface type ${INTF_TYPE}"; exit 1;;
    esac
}

function write_vnfd_tmpl() {
    name=$(basename $1)
    desc_file="${name}.yaml"

    cat >$desc_file <<EOF
vnfd:vnfd-catalog:
    vnfd:vnfd:
    -   vnfd:id: ${name}
        vnfd:name: ${name}
        vnfd:short-name: ${name}
        vnfd:description: Generated by RIFT.io package generator
        vnfd:vendor: ${VENDOR}
        vnfd:version: '1.0'

        # Place the logo as png in icons directory and provide the name here
        # vnfd:logo: <update, optional>

        # Atleast one VDU need to be specified
        vnfd:vdu:
        -   vnfd:id: iovdu_0
            vnfd:name: iovdu_0
            vnfd:count: 1

            # Flavour of the VM to be instantiated for the VDU
            vnfd:vm-flavor:
                vnfd:vcpu-count: ${VCPU}
                vnfd:memory-mb: ${MEMORY}
                vnfd:storage-gb: ${STORAGE}

            # Image including the full path
            vnfd:image: '${IMAGE}'

EOF

    # Add the cloud init file or script
    if [[ -n ${CLOUD_INIT_FILE} ]]; then
        cif=$(basename ${CLOUD_INIT_FILE})
        cat >>$desc_file <<EOF
            # Cloud init file
            vnfd:cloud-init-file: '${cif}'
EOF
    else
        cat >>$desc_file <<EOF
            # Cloud init to use
            vnfd:cloud-init: '${CLOUD_INIT}'
EOF
    fi

    # Add external interfaces
    cat >>$desc_file <<EOF
            external-interface:
            # Specify the external interfaces
            # There can be multiple interfaces defined
EOF

    # Add external interfaces
    for i in `seq 1 ${INTERFACES}`; do
        cat >>$desc_file <<EOF
            -   vnfd:name: eth${i}
                vnfd:virtual-interface:
                    vnfd:type: ${INTF_TYPE}
                vnfd:vnfd-connection-point-ref: ${name}/cp${i}
EOF
    done

    # Add connection points
    cat >>$desc_file <<EOF

        vnfd:connection-point:
EOF

    for i in `seq 1 ${INTERFACES}`; do
        cat >>$desc_file <<EOF
            -   vnfd:name: ${name}/cp${i}
                vnfd:type: ${CP_TYPE}
                vnfd:vnfd-connection-point-ref: ${name}/cp${i}
EOF
    done

    if [ $VERBOSE == true ]; then
        echo "INFO: Created $desc_file"
    fi
}

function write_nsd_tmpl() {
    name=$(basename $1)
    vnfd=$2
    desc_file="${name}.yaml"

    cat >$desc_file <<EOF
nsd:nsd-catalog:
    nsd:nsd:
    -   nsd:id: ${name}
        nsd:name: ${name}
        nsd:short-name: ${name}
        nsd:description: Generated by RIFT.io package generator
        nsd:vendor: ${VENDOR}
        nsd:version: '1.0'

        # Place the logo as png in icons directory and provide the name here
        # nsd:logo: <update, optional>

        # Specify the VNFDs that are part of this NSD
        nsd:constituent-vnfd:
            # The member-vnf-index needs to be unique, starting from 1
            # vnfd-id-ref is the id of the VNFD
            # Multiple constituent VNFDs can be specified
        -   nsd:member-vnf-index: 1
            nsd:vnfd-id-ref: ${vnfd}

        nsd:ip-profiles:
        # IP profiles for the VLDs
EOF

    # Add IP Profiles
    for i in `seq 1 ${INTERFACES}`; do
        cat >>$desc_file <<EOF
            -   nsd:name: IpProfile${i}
                nsd:description: IP Profile ${i}
                nsd:ip-profile-params:
                    nsd:gateway-address: 10.31.3${i}.1
                    nsd:ip-version: ipv4
                    nsd:subnet-address: 10.31.3${i}.0/24
EOF
    done

    cat >>$desc_file <<EOF
        vld:
        # Networks for the VNFs
EOF

    for i in `seq 1 ${INTERFACES}`; do
        cat >>$desc_file <<EOF
            -   nsd:id: ${name}_vld${i}
                nsd:name: ${name}_vld${i}
                nsd:type: ELAN
                nsd:ip-profile-ref: IpProfile${i}
                nsd:vnfd-connection-point-ref:
                # Specify the constituent VNFs
                # member-vnf-index-ref - entry from constituent vnf
                # vnfd-id-ref - VNFD id
                # vnfd-connection-point-ref - connection point name in the VNFD
                -   nsd:member-vnf-index-ref: 1
                    nsd:vnfd-id-ref: ${vnfd}
                    nsd:vnfd-connection-point-ref: ${vnfd}/cp${i}
EOF
        done

    if [ $VERBOSE == true ]; then
        echo "INFO: Created $desc_file"
    fi
}

function write_nsd_config_tmpl() {
    name=$(basename $1)
    cfg_file="ns_config/$name.yaml"

    cat >$cfg_file <<EOF

EOF

    if [ $VERBOSE == true ]; then
        echo "INFO: Created $cfg_file"
    fi
}

cur_dir=`pwd`

# Check if the array contains a specific value
# Taken from
# http://stackoverflow.com/questions/3685970/check-if-an-array-contains-a-value
function contains() {
    local n=$#
    local value=${!n}
    for ((i=1;i < $#;i++)); do
        if [ "${!i}" == "${value}" ]; then
            echo "y"
            return 0
        fi
    done
    echo "n"
    return 1
}

function check_type() {
    type=$1
    if [ $(contains "${DESC_TYPES[@]}" $type) == "y" ]; then
        TYPE=$type
    else
        echo "ERROR: Unknown descriptor type $type!" >&2
        exit 1
    fi
}

function get_expr(){
    # First argument is to specify if this is a negative match or match
    # Rest are filename expressions without extension
    #

    local regex=" "
    local n=$1
    local neg="${1}"
    for ((i=2;i <= $#;i++)); do
        if [ $i -eq 2 ]; then
            if [ $neg == true ]; then
                subexpr='! -name'
            else
                subexpr='-name'
            fi
        else
            if [ $neg == true ]; then
                subexpr=' -a ! -name'
            else
                subexpr=' -o -name'
            fi
        fi

        for extn in ${DESC_EXTN[$@]}; do
            regex="$regex $subexpr ${!i}.$extn"
        done
    done

    if [ $VERBOSE == true ]; then
        echo "INFO: Generate expression: $expr"
    fi

    echo "$expr"
}

function get_expr(){
    # First argument is to specify if this is a negative match or not
    # Rest are filename expressions without extension

    local regex=" "
    local n=$#
    local neg="${1}"
    local first="true"
    for ((i=2;i <= $#;i++)); do
        for extn in "${DESC_EXTN[@]}"; do
            if [ $first == true ]; then
                if [ $neg == true ]; then
                    subexpr='! -name'
                else
                    subexpr='-name'
                fi
                first=false
            else
                if [ $neg == true ]; then
                    subexpr=' -a ! -name'
                else
                    subexpr=' -o -name'
                fi
            fi

            regex="$regex $subexpr ${!i}.$extn"
        done
    done

    echo "$regex"
}

function generate_package(){
    type=$1
    name="${2}_${type}"
    vnfd="${2}_vnfd" # Required for NSD
    dest_dir=$3

    dir="${dest_dir}/${name}"

    # Create the folders for the descriptor
    if [ $VERBOSE == true ]; then
        echo "INFO: Creating folders for $PKG in $dest_dir"
    fi

    # Remove any existing directory
    if [[ -d $dir ]]; then
        rm -rf $dir >/dev/null 2>&1
    fi

    mkdir -p $dir && cd $dir
    if [[ $? -ne 0 ]]; then
        rc=$?
        echo "ERROR: creating directory $dir ($rc)" >&2
        exit $rc
    fi

    if [ $type == 'nsd' ]; then
        folders=("${NSD_FOLDERS[@]}")
    else
        folders=("${VNFD_FOLDERS[@]}")
    fi

    for d in ${folders[@]}; do
        mkdir -p $dir/$d
        if [[ $? -ne 0 ]]; then
            rc=$?
            echo "ERROR: creating directory $dir/$d ($rc)" >&2
            exit $rc
        fi
        if [ $VERBOSE == true ]; then
            echo "Created folder $d in $dir"
        fi
    done

    if [ $VERBOSE == true ]; then
        echo "INFO: Created folders for in $dir"
    fi

    # Write a descriptor template file
    if [ $type == 'vnfd' ]; then

        # Copy cloud init file to correct folder
        if [[ -n ${CLOUD_INIT_FILE} ]]; then
            if [[ -e ${CLOUD_INIT_FILE} ]]; then
                cp ${CLOUD_INIT_FILE} $dir/cloud_init
            else
                echo "ERROR: Unable to find cloud-init-file ${CLOUD_INIT_FILE}"
                exit 1
            fi
        fi

        write_vnfd_tmpl $dir
    else
        write_nsd_tmpl $dir $vnfd
    fi

    if [ $ARCHIVE == true ]; then
        # Create archive of the package
        cd $dest_dir
        if [ $VERBOSE == true ]; then
            tar zcvf ${name}.tar.gz ${name}
            echo "Created package ${name}.tar.gz in $dest_dir"
        else
            tar zcvf ${name}.tar.gz ${name} >/dev/null 2>&1
        fi

        if [[ $? -ne 0 ]]; then
            echo "ERROR: Creating archive for ${name} in $dest_dir" >&2
            exit 1
        fi

        echo "$dest_dir/${name}.tar.gz" >&2

        if [ $RM == true ]; then
            rm -rf ${name}
        fi
    fi
}

OPTS=`getopt -o vhnt:d:caN --long verbose,dry-run,help,package-type:,destination-dir,create-folder,no-remove-files,archive,nsd,vendor:,interface-type:,vcpu:,memory:,storage:,image:,cloud-init-file:,cloud-init:,interfaces:,debug -n $SCRIPTNAME -- "$@"`

if [[ $? != 0 ]] ; then
    echo "ERROR: Failed parsing options ($?)." >&2
    usage
    exit 1
fi

echo "$OPTS"
eval set -- "$OPTS >/dev/null 2>&1"

while true; do
    case "$1" in
        -v | --verbose ) VERBOSE=true; shift ;;
        -h | --help )    usage; exit 0; shift ;;
        -n | --dry-run ) DRY_RUN=true; shift ;;
        -t | --package-type ) check_type "$2"; shift; shift ;;
        -d | --destination-dir ) DEST_DIR=$2; shift; shift;;
        -c | --create-folder ) CREATE=true; shift;;
        -N | --no-remove-files ) RM=''; shift;;
        -a | --archive ) ARCHIVE=true; shift;;
        --nsd ) CREATE_NSD=true; shift;;
        --vendor ) VENDOR=$2; shift; shift;;
        --interface-type ) INTF_TYPE=$2; shift; shift;;
        --vcpu ) VCPU=$2; shift; shift;;
        --memory ) MEMORY=$2; shift; shift;;
        --storage ) STORAGE=$2; shift; shift;;
        --image ) IMAGE=$2; shift; shift;;
        --cloud-init ) CLOUD_INIT=$2; shift; shift;;
        --cloud-init-file ) CLOUD_INIT_FILE=$2; shift; shift;;
        --interfaces ) INTERFACES=$2; shift; shift;;
        --debug ) DEBUG=true; shift;;
        -- ) shift; break ;;
        * ) break ;;
    esac
done

if [ $DEBUG == true ]; then
    echo "INFO: Debugging ON"
    set -x
fi

if [ $VERBOSE == true ]; then
    echo "INFO: Descriptor type: $TYPE"
fi

# Dry run is to validate existing descriptor folders
if [ $DRY_RUN == true ] && [ $CREATE == true ]; then
    echo "ERROR: Option dry-run with create-folders not supported!" >&2
    exit 1
fi

if [[ $# -gt 1 ]]; then
    BASE_DIR=$1
    PKG=$(basename $2)
else
    BASE_DIR=$(dirname $1)
    PKG=$(basename $1)
fi

if [ $VERBOSE == true ]; then
    echo "INFO: Using base dir: $BASE_DIR"
fi

if [ $VERBOSE == true ]; then
    echo "INFO: Using package: $PKG"
fi

if [[ -z "$PKG" ]]; then
    echo "ERROR: Need to specify the package name" >&2
    usage >&2
    exit 1
fi

cd $BASE_DIR
if [[ $? -ne 0 ]]; then
    if [ $CREATE == true ]; then
        mkdir -p $BASE_DIR
        if [[ $? -ne 0 ]]; then
            echo "ERROR: Unable to create base directory $BASE_DIR" >&2
            exit 1
        fi
        cd $BASE_DIR
    else
        echo "ERROR: Unable to change to base directory $BASE_DIR!" >&2
        exit 1
    fi
fi

# Get full base dir path
BASE_DIR=`pwd`
cd $cur_dir

if [[ -z $DEST_DIR ]]; then
    DEST_DIR=$BASE_DIR # Default to base directory
fi

mkdir -p $DEST_DIR

cd $DEST_DIR
if [[ $? -ne 0 ]]; then
    echo "ERROR: Not able to access destination directory $DEST_DIR!" >&2
    exit 1
fi

# Get the full destination dir path
DEST_DIR=`pwd`
cd $cur_dir

dir=${BASE_DIR}/${PKG}

function add_chksum() {
    if [ $VERBOSE == true ]; then
        echo "INFO: Add file $1 to $CHKSUM"
    fi

    md5sum $1 >> $CHKSUM
}

if [ $CREATE == false ]; then
    if [[ ! -d $dir ]]; then
        echo "INFO: Package folder $dir not found!" >&2
        exit 1
    fi

    cd $dir
    if [[ $? -ne 0 ]]; then
        rc=$?
        echo "ERROR: changing directory to $dir ($rc)" >&2
        exit $rc
    fi

    # Remove checksum file, if present
    rm -f $CHKSUM

    # Check if the descriptor file is present
    if [[ -z $TYPE ]]; then
        # Desc type not specified, look for the desc file and guess the type
        # Required for backward compatibility
        for ty in ${DESC_TYPES[@]}; do
            re=$(get_expr false "$ty" "*_$ty" "*_${ty}_*")
            desc=$(find * -maxdepth 0 -type f $re 2>/dev/null)

            if [[ -z $desc ]] || [[ ${#desc[@]} -eq 0 ]]; then
                # Check the vnfd|nsd folder
                if [[ ! -d $ty ]]; then
                    continue
                fi
                re=$(get_expr false "*")
                desc=$(find $ty/* -maxdepth 0 -type f $re 2>/dev/null)
                if [[ -z $desc ]] || [[ ${#desc[@]} -eq 0 ]]; then
                    continue
                elif [[ ${#desc[@]} -gt 1 ]]; then
                    echo "ERROR: Found multiple descriptor files: ${desc[@]}" >&2
                    exit 1
                fi
                # Descriptor sub directory
                desc_sub_dir=$ty
            fi

            TYPE=$ty
            if [ $TYPE == 'nsd' ]; then
                folders=("${NSD_FOLDERS[@]}")
            else
                folders=("${VNFD_FOLDERS[@]}")
            fi

            if [ $VERBOSE == true ]; then
                echo "INFO: Determined descriptor is of type $TYPE"
            fi
            break
        done

        if [[ -z $TYPE ]]; then
            echo "ERROR: Unable to determine the descriptor type!" >&2
            exit 1
        fi
    else
        if [ $TYPE == 'nsd' ]; then
            folders=("${NSD_FOLDERS[@]}")
        else
            folders=("${VNFD_FOLDERS[@]}")
        fi

        # Check for descriptor of type provided on command line
        re=$(get_expr false "$TYPE" "*_${TYPE}" "*_${TYPE}_*" "*_${TYPE}*")
        desc=$(find * -maxdepth 0 -type f $re 2>/dev/null)

        if [[ -z $desc ]] || [[ ${#desc[@]} -eq 0 ]]; then
            # Check if it is under vnfd/nsd subdirectory
            # Backward compatibility support
            re=$(get_expr false "*")
            desc=$(find $TYPE/* -maxdepth 0 -type f $re 2>/dev/null)
            if [[ -z $desc ]] || [[ ${#desc[@]} -eq 0 ]]; then
                echo "ERROR: Did not find descriptor file of type $TYPE" \
                     " in $dir" >&2
                exit 1
            fi
            desc_sub_dir=$ty
        fi
    fi

    if [[ ${#desc[@]} -gt 1 ]]; then
        echo "ERROR: Found multiple files of type $TYPE in $dir: $desc" >&2
        exit 1
    fi

    descriptor=${desc[0]}

    # Check if there are files not supported
    files=$(find * -maxdepth 0 -type f ! -name $descriptor 2>/dev/null)

    for f in ${files[@]}; do
        if [ $(contains "${ALLOWED_FILES[@]}" $f)  == "n" ]; then
            echo "WARN: Unsupported file $f found"
        fi
    done

    if [ $VERBOSE == true ]; then
        echo "INFO: Found descriptor package: ${desc_sub_dir} ${descriptor}"
    fi

    if [ $DRY_RUN == false ]; then
        add_chksum ${descriptor}
    fi

    # Check the folders are supported ones
    dirs=$( find * -maxdepth 0 -type d )

    for d in ${dirs[@]}; do
        if [ $(contains "${folders[@]}" $d) == "y" ]; then
            if [ $DRY_RUN == false ]; then
                find $d/* -type f  2>/dev/null|
                    while read file; do
                        add_chksum $file
                    done
            fi
        elif [[ -z $desc_sub_dir ]] || [[ $d != $desc_sub_dir ]]; then
            echo "WARN: $d is not part of standard folders " \
                 "for descriptor type $TYPE in $PKG"
        fi
    done

    if [ $VERBOSE == true ]; then
        echo "INFO: Creating archive for $PKG"
    fi

    cd $BASE_DIR
    if [ $DRY_RUN == false ]; then
        tar zcvf "$DEST_DIR/$PKG.tar.gz" "${PKG}" ${RM}
        if [[ $? -ne 0 ]]; then
            rc=$?
            echo "ERROR: creating archive for $PKG ($rc)" >&2
            exit $rc
        fi
    fi
else
    # Create, default to VNFD if no type is defined
    if [[ -z $TYPE ]]; then
        TYPE=vnfd
        if [ $VERBOSE == true ]; then
            echo "WARNING: Defaulting to descriptor type $TYPE"
        fi
    fi

    if [ $TYPE == 'vnfd' ]; then
        if [[ -z $IMAGE ]]; then
            echo "ERROR: Image file need to be specified for VNF"
            exit 1
        fi
        generate_package vnfd $PKG $DEST_DIR
    fi

    if [ $TYPE == 'nsd' -o  $CREATE_NSD == true ]; then
        generate_package nsd $PKG $DEST_DIR
    fi

fi

cd $cur_dir
