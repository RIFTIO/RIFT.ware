#!/bin/bash
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file start_multi_vm_vnf.sh
# @author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
# @date 03/02/2016
# @brief Start or attach with the lead-VNF, a VNF based on its VDU name
#

NORTHBOUND_LISTING="cli_multivm_vnf_schema_listing.txt"
DEMO_DIR=$RIFT_INSTALL/demos
CORE_UTIL_DIR=$RIFT_INSTALL/usr/rift/systemtest/util
DEMO_TEST_DIR=$DEMO_DIR/tests

vnf_name=""
vdu_name=""
ip_list_arg=""
vdu_ip_address=""
lead_vdu_ip_address=""

syntax="Syntax:\n\t$0 [-h|--help] <-n|--vnf-name VNF_NAME> <-d|--vdu-name VDU_NAME> \
<-l|--lead-vdu-ip-address LEAD-VDU-IP-ADDRESS> [-i|--vdu-ip-address VDU-IP-ADDRESS]"

if ! ARGPARSE=$(getopt -o h,n:,d:,i:,l: -l help,vnf-name:,vdu-name:,vdu-ip-address:,lead-vdu-ip-address: -- "$@")
then
    exit 1
fi
eval set -- "$ARGPARSE"

while [ $# -gt 0 ]
do
    case "$1" in
        -h|--help)
             echo -e  ${syntax}
             exit 0
             ;;
        -n|--vnf-name)
             vnf_name="$2"
             shift
             ;;
        -d|--vdu-name)
             vdu_name="$2"
             shift
             ;;
        -i|--vdu-ip-address)
             vdu_ip_address="$2"
             shift
             ;;
        -l|--lead-vdu-ip-address)
             lead_vdu_ip_address="$2"
             shift
             ;;
        --)
             break
             ;;
        -*)
             echo "ERROR: $0: Unrecognized option $1" 1>&2
             exit 1
             ;;
         *)
             echo "ERROR: $0: Not an option $1" 1>&2
             exit 1
             ;;
    esac
    shift
done

# Verify that all required arguments are present at the command line
if [ "$vnf_name" == "" ] ; then
   echo "ERROR: vnf-name is required"
   exit 1
elif [ "$vdu_name" == "" ] ; then
   echo "ERROR: vdu-name is required"
   exit 1
elif [ "$vdu_ip_address" == "" ] ; then
   echo "ERROR: vdu-ip-address is required"
   exit 1
fi

# Command to verify that each component in the system reached to running state
up_cmd="${CORE_UTIL_DIR}/wait_until_system_started.py --max-wait 1000"

# Execute the command based on the vdu-name
case "$vdu_name" in
    # VDU for Lead VM
    "RW.VDU.LEAD.STND"|"RW.VDU.LEAD.DDPL"|"RW.VDU.LEAD.DDPL-IPSEC"|"RW.VDU.LEAD.STND-IPSEC")
        # Select the correct demo-info file based the lead-vdu
        if [ "$vdu_name" == "RW.VDU.LEAD.STND" ] ; then
            DEMO_INFO_FILE_NAME="multi_vm_vnf_std_info.py"
        elif [ "$vdu_name" == "RW.VDU.LEAD.STND-IPSEC" ] ; then
            DEMO_INFO_FILE_NAME="multi_vm_vnf_std_ipsec_info.py"
        elif [ "$vdu_name" == "RW.VDU.LEAD.DDPL-IPSEC" ] ; then
            DEMO_INFO_FILE_NAME="multi_vm_vnf_ddp_ipsec_info.py"
        else
            DEMO_INFO_FILE_NAME="multi_vm_vnf_ddp_info.py"
        fi

        # Set the vnf-name to the colony and cluster name in demo-info
        DEMO_INFO_FILE_TEMPLATE=$DEMO_TEST_DIR/demo_params/$DEMO_INFO_FILE_NAME
        DEMO_INFO_FILE=$(mktemp /tmp/${DEMO_INFO_FILE_NAME}.XXXXXXXXXX)
        sed -e "s/name='vnf_name'/name='${vnf_name}'/g" ${DEMO_INFO_FILE_TEMPLATE} > ${DEMO_INFO_FILE}
        system_cmd="$DEMO_DIR/start_demo.py \
             --expanded \
             --mode ethsim \
             --demoinfo-file $DEMO_INFO_FILE \
             --ip-list $vdu_ip_address \
             --northbound-listing $NORTHBOUND_LISTING \
             --locally-install-kmods \
             --verbose"
             # --use-xml-mode
        $DEMO_TEST_DIR/demo_start_and_configure.sh --system_cmd "${system_cmd}" --up_cmd "${up_cmd}"
        ;;

     # VDU for Non-Lead VM
     "RW.VDU.MEMB.STND"|"RW.VDU.MEMB.DDPL"|"RW.VDU.MEMB.DDPM"|"RW.VDU.MEMB.DDPL-IPSEC")
        # lead-vdu-ip-address is required for non-lead VM
        if [ "$lead_vdu_ip_address" == "" ] ; then
             echo "ERROR: lead-vdu-ip-address is required"
             exit 1
        fi
        up_cmd="${up_cmd} --confd-host $lead_vdu_ip_address"
        system_cmd="$CORE_UTIL_DIR/multi_vm_vnf/multivm_vnf_startvm.py \
             $vnf_name \
             $vdu_name \
             $lead_vdu_ip_address \
             $vdu_ip_address"
        ${system_cmd}
        ${up_cmd}
        ;;

     *) error="ERROR: Invalid vdu-name '${vdu_name}'. Choices:"
        error="${error} RW.VDU.LEAD.STND, RW.VDU.LEAD.DDPL, RW.VDU.MEMB.STND, RW.VDU.MEMB.DDPL, RW.VDU.MEMB.DDPM RW.VDU.MEMB.DDPL-IPSEC RW.VDU.LEAD.DDPL-IPSEC RW.VDU.LEAD.STND-IPSEC"
        echo ${error}
        exit 1
        ;;
esac
