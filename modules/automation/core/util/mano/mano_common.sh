# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

# file mano_common.sh
# author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
# date 03/20/2016
# brief file contains common functions and variables used by mano tests
#

DEMO_DIR="${RIFT_INSTALL}/demos"
DEMO_TEST_DIR=$DEMO_DIR/tests
SYSTEM_DIR="${RIFT_INSTALL}/usr/bin"
SYSTEM_TEST_DIR="${RIFT_INSTALL}/usr/rift/systemtest"
PYTEST_DIR="${SYSTEM_TEST_DIR}/pytest"
SYSTEM_TEST_UTIL_DIR="${RIFT_INSTALL}/usr/rift/systemtest/util"
VNF_TOOL_DIR="${RIFT_INSTALL}/demos/vnftools"
ALL_IMAGE_DIR="/net/sharedfiles/home1/common/vnf/Rift/"

SCRIPT_SYSTEST_WRAPPER="${SYSTEM_TEST_UTIL_DIR}/systest_wrapper.sh"
SCRIPT_SYSTEM=""
LOG_FILE=""
SCRIPT_BOOT_OPENSTACK="${SYSTEM_TEST_UTIL_DIR}/test_openstack_wrapper.py"
SCRIPT_TEST=""
REBOOT_SCRIPT_TEST=""
LONG_OPTS="ha-mode:,cloud-host:,test-name:,valgrind:,cloud-type:,dts-trace:,filter:,mark:,wait,repeat:,repeat-system:,wait-system:,repeat-keyword:,repeat-mark:,fail-on-error,sysinfo,ssl,tenant:,tenants:,use-existing,mvv,user:,use-xml-mode,remote,restconf,netconf,expanded"
SHORT_OPTS=",h:,,c:,d:,k:,m:,w,r:,,,,,,,,,,,,,,,,"

cmdargs="${@}"
cloud_type='lxc'
test_name=""
valgrind_args=""
cloud_host="127.0.0.1"
repeat=1
filter=""
mark=""
repeat_keyword=""
repeat_mark=""
repeat_system=1
wait_system=1200
fail_on_error=false
teardown_wait=false
no_cntr_mgr=true
remote=false
restconf=false
netconf=false
sysinfo=false
ssl=false
use_xml_mode=false
tenant=()
tenants=""
collapsed_mode=true
use_existing=false
mvv=false
mvv_image=""
user=${USER}
ha_mode=""

echo "Command lines out: $cmdargs"
echo

system_args="\
    --mode ethsim"

function append_arglist()
{
    local args_list=$1
    local arg="$2"
    shift
    shift
    local val=("$@")

    # if val is true " --switch" will be appended
    # if val has non-zero lenght and not a boolean " --argument value" will be appended
    if [ "$val" ] ; then
        if [ "$val" = true ] ; then
            eval $args_list+="' --$arg'"
        elif [ "$val" = false ] ; then
            :
        else
            for item in ${val[@]}; do
                cmd=" --$arg $item"
                # Escape any double quotes. Why? bash can handle both
                # double & single quotes within a double quote("). The
                # same is not true within a single quote(')
                cmd=${cmd//\"/\\\"}
                eval $args_list+="\"$cmd\""
            done
        fi
    fi
}

function append_args()
{
    local args_list=$1
    local arg="$2"
    local val="$3"

    # if val is true " --switch" will be appended
    # if val has non-zero lenght and not a boolean " --argument value" will be appended
    if [ "$val" ] ; then
        if [ "$val" = true ] ; then
            eval $args_list+="' --$arg'"
        elif [ "$val" = false ] ; then
            :
        else
            cmd=" --$arg $val"
            # Escape any double quotes. Why? bash can handle both
            # double & single quotes within a double quote("). The
            # same is not true within a single quote(')
            cmd=${cmd//\"/\\\"}
            eval $args_list+="\"$cmd\""
        fi
    fi
}


function update_up_cmd()
{
    up_cmd="$SYSTEM_TEST_UTIL_DIR/wait_until_system_started.py "
    append_args up_cmd max-wait $wait_system
    append_args up_cmd confd-host $confd_host
}

function construct_test_args()
{
    base_name=${test_name}_${cloud_type}
    JUNITXML_FILE=${RIFT_MODULE_TEST}/${base_name}.xml
    JUNIT_PREFIX=${base_name^^}

    append_args test_args remote ${remote}
    append_args test_args restconf ${restconf}
    append_args test_args netconf ${netconf}
    append_args test_args repeat ${repeat}
    append_args test_args cloud-host ${cloud_host}
    append_args test_args tenants ${tenants}

    append_args test_args junitprefix $JUNIT_PREFIX
    append_args test_args junitxml $JUNITXML_FILE
    append_args test_args fail-on-error ${fail_on_error}
    append_args test_args repeat-keyword ${repeat_keyword}
    append_args test_args repeat-mark ${repeat_mark}
    append_args test_args filter ${filter}
    append_args test_args mark ${mark}
    append_args test_args dts-trace ${dts_trace}
    append_args test_args log-stdout ${log_stdout}
    append_args test_args log-stderr ${log_stderr}
    append_args test_args user ${user}
    append_arglist test_args tenant ${tenant[@]}

    if [ "$cloud_type" ]; then
        test_args+=" --${cloud_type} "
    fi
}

function create_mvv_image_file()
{
    if [ "${mvv}" = true ] ; then
        img_src_file="${ALL_IMAGE_DIR}/rift-root-latest-multivm-vnf.qcow2"
        mvv_image="${RIFT_ROOT}/.images/rift-root-latest-multivm-vnf.qcow2"
        # create the image file if not already exists
        if ! [ -a "${mvv_image}" ] ; then
           # create the image file if it is not in the common location or make a symlink to it
           if [ -a "${img_src_file}" ] ; then
               mkdir ${RIFT_ROOT}/.images 2>/dev/null
               cd ${RIFT_ROOT}/.images
               ln -s ${img_src_file}
               cd -
           else
               ${VNF_TOOL_DIR}/prepare_multivm_qcow.sh
           fi
        fi
    fi
}

function construct_openstack_system_args()
{
    append_args system_args collapsed "${collapsed_mode}"
}

function construct_test_command()
{
    if [ "${SCRIPT_SYSTEM}" == "" ]; then
        if [ -z "$HOME_RIFT" ] ; then
            LAUNCHPAD_HOME=$RIFT_INSTALL
        else
            LAUNCHPAD_HOME=$HOME_RIFT
        fi
        SCRIPT_SYSTEM="${LAUNCHPAD_HOME}/demos/launchpad.py"
        log_stdout="${RIFT_ARTIFACTS}/launchpad_stdout.log"
        log_stderr="${RIFT_ARTIFACTS}/launchpad_stderr.log"
    fi

    append_args system_args use-xml-mode "${use_xml_mode}"
    append_args system_args test-name "${test_name}"
    append_args system_args valgrind "${valgrind_args}"
    append_args test_args use-xml-mode "${use_xml_mode}"
    append_args systest_args wait "${teardown_wait}"
    append_args systest_args sysinfo "${sysinfo}"

    construct_test_args

    case "${cloud_type}" in
        lxc)

            # Set up cloudsim start, stop command
            append_args systest_args pre_test_cmd "\"cloudsim start\""
            append_args systest_args post_test_cmd "\"cloudsim stop\""

            update_up_cmd
            systest_args+=" --log_stdout \"${log_stdout}\""
            systest_args+=" --log_stderr \"${log_stderr}\""
            systest_args+=" --up_cmd \"${up_cmd}\""
            test_cmd="${SCRIPT_SYSTEST_WRAPPER} ${systest_args}"
            append_args test_cmd system_cmd "\"${SCRIPT_SYSTEM} ${system_args}\""
            append_args test_cmd test_cmd "\"${SCRIPT_TEST} ${test_args}\""
            if [ "$REBOOT_SCRIPT_TEST" ] ; then
                append_args test_cmd post_restart_test_cmd "\"${REBOOT_SCRIPT_TEST} ${test_args}\""
            fi
        ;;
        openstack)
            confd_host="CONFD_HOST"
            update_up_cmd


            construct_openstack_system_args

            test_cmd="${SCRIPT_BOOT_OPENSTACK}"
            append_arglist test_cmd tenant ${tenant[@]}
            append_args test_cmd use_existing "\"${use_existing}\""
            append_args test_cmd log-stdout "\"${log_stdout}\""
            append_args test_cmd log-stderr "\"${log_stderr}\""
            append_args test_cmd up-cmd "\"${up_cmd}\""
            append_args test_cmd cloud-host "\"${cloud_host}\""
            append_args test_cmd tenants "\"${tenants}\""
            append_args test_cmd systest-script "\"${SCRIPT_SYSTEST_WRAPPER}\""
            append_args test_cmd systest-args "\"${systest_args}\""
            append_args test_cmd system-script "\"${SCRIPT_SYSTEM}\""
            append_args test_cmd system-args "\"${system_args}\""
            append_args test_cmd test-script "\"${SCRIPT_TEST}\""
            append_args test_cmd test-args "\"${test_args}\""
            append_args test_cmd mvv-image "${mvv_image}"
            append_args test_cmd user ${user}
            append_args test_cmd ha-mode "${ha_mode}"

            if [ "$REBOOT_SCRIPT_TEST" ] ; then
                append_args test_cmd post-restart-test-script "\"${REBOOT_SCRIPT_TEST}\""
                append_args test_cmd post-restart-test-args "\"${test_args}\""
            fi

        ;;
        *)
            echo "error - unrecognized cloud-type type: ${cloud_type}"
           exit 1
        ;;
    esac
}

function parse_args()
{
    if ! ARGPARSE=$(getopt -o ${SHORT_OPTS} -l ${LONG_OPTS} -- "$@")
    then
        exit 1
    fi
    eval set -- "$ARGPARSE"

    while [ $# -gt 0 ]
    do
        case "$1" in
        -h|--cloud-host) cloud_host="$2"
          shift;;
        --ha-mode) ha_mode="$2"
          shift;;
        --expanded) collapsed_mode=false
          shift;;
        -c|--cloud-type) cloud_type="$2"
          shift;;
        -t|--test-name) test_name="$2"
          shift;;
        -t|--valgrind) valgrind_args="$2"
          shift;;
        -d|--dts-trace) dts_trace="${DEMO_DIR}/dts_trace.py -v --client $2"
          shift;;
        --fail-on-error) fail_on_error=true
          ;;
        -k|--filter) filter="$2"
          shift;;
        --tenants) tenants="$2"
          shift;;
        -m|--mark) mark="$2"
          shift;;
        --use-xml-mode) use_xml_mode=true
          ;;
        -w|--wait) teardown_wait=true
          ;;
        --wait-system) wait_system=$2
          shift;;
        --netconf) netconf=true
          ;;
        -r|--repeat) repeat=$((${2}+1))
          shift;;
        --repeat-system) repeat_system=$((${2}+1))
          shift;;
        --repeat-keyword) repeat_keyword=$2
          shift;;
        --repeat-mark) repeat_mark=$2
          shift;;
        --remote) remote=true
          ;;
        --restconf) restconf=true
          ;;
        --sysinfo) sysinfo=true
          ;;
        --tenant) tenant+=("$2")
          shift;;
        --use-existing) use_existing=true
          ;;
        --user) user=$2
          shift;;
        --mvv) mvv=true
          ;;
        --) shift
          break;;
        -*) echo "$0: error - unrecognized option $1" 1>&2
          exit 1;;
        *) echo "$0: error - not an option $1" 1>&2
          exit 1;;
        esac
        shift
    done

    if [ $restconf != true ] && [ $netconf != true ]; then
        restconf=true
    fi
}

function pretty_print_junit_xml()
{
    xmllint --format --output ${JUNITXML_FILE} ${JUNITXML_FILE} 2>/dev/null
}
