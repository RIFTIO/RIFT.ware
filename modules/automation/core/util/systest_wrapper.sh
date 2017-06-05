#!/bin/bash
# STANDARD_RIFT_IO_COPYRIGHT
# Author(s): Austin Cormier
# Creation Date: 2015/04/28
#
# Script for invoking and managing separate system, configuration and test processes.
#
#    systest_wrapper.sh --system_cmd "<system_cmd> --test_cmd "<test_cmd>" [--config_cmd "<config_cmd>"]
#                       [--dts_cmd "<dts_cmd>"] [--sink_cmd "<sink_cmd>"] [--wait] [--sysinfo]
#
# The system is command is run in the background while the config command
# (if provided) and test commands are run sequentially in the foreground.
# Since the config/test commands are run immediately after the system is started,
# they must be capable of determining when the system is ready to be configured/tested.
#
# Arguments:
#   system_cmd - The command which starts the system (which runs in the background).
#   log_stdout - Log file which captures a duplicate of the console stdout
#   log_stderr - Log file which captures a duplicate of the console stderr
#   config_cmd - The command which configures the running system in preparation to be tested.
#   pre_test_cmd - The command that runs before the test starts.
#   test_cmd   - The command which tests the now started an configured system.
#   post_test_cmd - The command that runs after the test finishes.
#   dts_cmd    - The command which enables dts tracing for a client, such as uAgent.
#   sink_cmd   - The command which is responsible for setting up the logging sink.
#   wait       - Block after test until receiving interrupt or kill signal
#   sysinfo    - The option to "show sysinfo" when the test fails.
#   confd_ip   - IP address of confd, optional. Defaults to localhost.
#   post_restart_test_cmd - The command which tests the re-started system. If specified the
#                           system is restarted.
#   post_reboot_test_cmd - The command which is executed after rebooting the Master VM.
#                          The provided command/test must have the logic for connecting to
#                          the standby VM.
#   reboot_cmd - Command which does the VM reboot physically or logically.
#
# Exit Code:
#   0 if the system started successfully and the provided config/test commands.
#   The exit code of the config_cmd if unsuccessful, otherwise the test_cmd exit code.

# If wait is specified, block until receiving an INT or TERM signal
# before continuing on to shut down the system

system_pid=0
curr_pid=0
curr_rc=0
config_cmd=""
confd_ip="127.0.0.1"
pre_test_cmd=""
test_cmd=""
post_test_cmd=""
post_restart_test_cmd=""
post_reboot_test_cmd=""
reboot_cmd=""
system_cmd=""
log_stdout="${RIFT_ARTIFACTS}/systemtest_stdout.log"
log_stderr="${RIFT_ARTIFACTS}/systemtest_stderr.log"
up_cmd=""
dts_cmd=""
sink_cmd=""
teardown_wait=false
sysinfo=false
show_sysinfo_cmd=":"
systest_pipe=""
tmp_dir=""

mypid=$$

###
# If teardown_wait is specified, wait till interrupt is received
# Globals:
#     teardown_wait: flag
###
function handle_teardown_wait(){
    if [ ${teardown_wait} = true ]; then
        wait_cond=false
        done_wait() {
            wait_cond=true
        }
        trap done_wait SIGTERM SIGINT
        echo "Scheduled system tests complete. Waiting for interrupt to resume teardown"
        while [ ${wait_cond} != true ]; do
            read
        done < ${systest_pipe}
    fi
}

###
# Wait till specified timeout for the pid to get killed
# Arguments:
#     pid: PID
###
function wait_for_killed_process_termination()
{
    local pid2kill=$1
    local max_kill_time=60
    local time=0

    while [ -d /proc/${pid2kill} ] ; do
       if [ $time -ge $max_kill_time ] ; then
           echo "WARNING: Unable to kill pid: ${pid2kill}"
           # self-terminate if $pid2kill is still alive
           kill -9 $mypid 2>/dev/null
       fi
       sleep 1
       time=$((time+1))
    done
}

###
# Send kill signal to the given PID and wait for it to exit
# Arguments:
#     pid: PID
###
function kill_pid()
{
    local pid2kill=$1
    # run in the background the command waiting for termination of pid:$pid2kill
    wait_for_killed_process_termination $pid2kill &

    kill -9 $pid2kill 2>/dev/null
    wait $pid2kill
    echo "Process killed"
}

###
# Clear temp files and shut down the system.
# Globals:
#     systest_pipe: Temp file
#     tmp_dir: Temp dir
#     curr_rc: exit code
#     curr_pid: Currently executing PID
#     system_pid: PID of the system cmd
###
function handle_exit()
{
    # Give the system a chance to shutdown, and send it a SIGKILL to ensure
    # it gets killed if it decides to hang around.
    handle_teardown_wait

    trap "" EXIT SIGINT SIGTERM SIGHUP
    trap - EXIT SIGINT SIGTERM SIGHUP

    # cleanup temporaries
    rm "${systest_pipe}" 2>/dev/null
    rmdir ${tmp_dir} 2>/dev/null

    if [ "$1" -eq 0 ]; then
       pid2kill=$system_pid
    else
       curr_rc=1
       echo "Exiting with system_rc: $curr_rc"
       pid2kill=$curr_pid
    fi

    # Prevent the system_cmd monitor killing us if it detects the system has been shut down
    # RIFT-13789
    trap "" SIGHUP
    kill_pid $pid2kill

    exit ${curr_rc}
}


###
# Run the given command
# Globals:
#     curr_pid: The pid of the process created
#     curr_rc: The exit status of the cmd
# Arguments:
#     cmd: Command to run.
###
function run_cmd()
{
    local cmd=$1
    
    echo "DEBUG: Running cmd: ${cmd}"
    # This helps us to run command with quote in them!
    eval "{ set -o monitor; cat /dev/null | ${cmd} & }"

    # Global variable
    curr_pid=$!
    wait $curr_pid

    curr_rc=$?
    echo "DEBUG: Got rc: $curr_rc for $cmd"

    return $curr_rc
}

###
# Start the system and set up traps and kill signals
# Globals:
#     system_pid
# Arguments:
#     system_test: Command to start the system
###
function start_system()
{
    local system_test=$1
    # We want to launch the system within a seperate process group in the
    # background so the reaper doesn't affect us.  Pipe /dev/null into the
    # system to give it a "usable" stdin.  Otherwise we've seen ssh fail.
    trap "echo \"Received SIGHUP\"; handle_exit 1" SIGHUP
    trap "echo \"Received EXIT\"; handle_exit 0" EXIT
    { set -o monitor; cat /dev/null | ${system_test} > >(tee ${log_stdout}) 2> >(tee ${log_stderr} >&2) & }

    # Set the global variable
    system_pid=$!

    # Set to receive SIGINT signal upon abnormal termination of the system command
    ( while [ -d /proc/${system_pid} ] ; do sleep 1 ; done ; kill -SIGHUP $$ 2>/dev/null )&
}

###
# Wait till the system is UP and running!
# Globals:
#     None
# Arguments:
#     up_cmd: Command to run to check the system UP condition.
###
function wait_for_system_start()
{
    local up_cmd=$1
    run_cmd "$up_cmd"

    local rc=$?
    if [[ ${rc} -ne 0 ]]; then
        echo "Exiting with up_cmd_rc: $rc"
        exit ${rc}
    fi
}

###
# Sets the RIFT_VAR_ROOT env variable required by system_test scripts
# Globals:
#   None
# Argument:
#   sys_cmd: The system command
##
function set_rift_var_root()
{
    local sys_cmd=$1
    local test_name_found=false
    local test_name=""

    # Get the test-name
    for token in ${sys_cmd}
    do
        if [[ "${token}" == "--test-name" ]]; then
            test_name_found=true
        elif [[ "${test_name_found}" == true ]]; then
            test_name=${token}
            break
        fi
    done

    if [[ -z "${test_name}" ]]; then
        # If test-name not set assume default 
        export RIFT_VAR_ROOT="${RIFT_INSTALL}/var/rift"
    else
        # Get the first directory under var/rift starting with test-name
        export RIFT_VAR_ROOT=`find ${RIFT_INSTALL}/var/rift/* -maxdepth 0 -name "${test_name}*" -a -type d | head -1`
    fi
}

# Uncomment if you do not wish core files to be created
# ulimit -c 0

while [[ $# > 1 ]]
do
  key="$1"

  case $key in
    -c|--config_cmd)
      config_cmd="$2"
      shift
      ;;
    --confd_ip)
      confd_ip="$2"
      shift
      ;;
    -l|--log_stdout)
      log_stdout="$2"
      shift
      ;;
    -e|--log_stderr)
      log_stderr="$2"
      shift
      ;;
    --pre_test_cmd)
      pre_test_cmd="$2"
      shift
      ;;
    -t|--test_cmd)
      test_cmd="$2"
      shift
      ;;
    --post_test_cmd)
      post_test_cmd="$2"
      shift
      ;;
    --post_restart_test_cmd)
      post_restart_test_cmd="$2"
      shift
      ;;
    --post_reboot_test_cmd)
      post_reboot_test_cmd="$2"
      shift
      ;;
    --reboot_cmd)
      reboot_cmd="$2"
      shift
      ;;
    -u|--up_cmd)
     up_cmd="$2"
      shift
      ;;
    -s|--system_cmd)
      system_cmd="$2"
      shift
      ;;
    -d|--dts_cmd)
      dts_cmd="$2"
      shift
      ;;
    -s|--sink_cmd)
      sink_cmd="$2"
      shift
      ;;
    --sysinfo)
      sysinfo=true
      ;;
    --wait)
      teardown_wait=true
      ;;
    *)
      echo "ERROR: Got an unknown option: $key"
      exit 1
      ;;
  esac
  shift
done

echo "my pid: $$"


if [ "${system_cmd}" == "" ]; then
    echo "ERROR: system_cmd was not provided."
    exit 1
fi

if [ "${up_cmd}" == "" ]; then
    echo "ERROR: up_cmd was not provided."
    exit 1
fi

if [ "${test_cmd}" == "" ]; then
    echo "ERROR: test_cmd was not provided."
    exit 1
fi

if [ "${sysinfo}" == true ]; then
    show_sysinfo_cmd="$RIFT_INSTALL/usr/rift/systemtest/util/show_sysinfo.py --confd-host $confd_ip"
fi

# Preload the xerces ssl workaround
PRELOAD="LD_PRELOAD=${RIFT_INSTALL}/usr/lib/rift/preloads/librwxercespreload.so"

echo "Launching system using command: ${system_cmd} > >(tee ${log_stdout}) 2> >(tee ${log_stderr} >&2)"
echo "-------------------------------------------------------"

# We want to launch the system within a separate process group in the background
# so the reaper doesn't affect us.  Pipe /dev/null into the system to give it a
# "usable" stdin.  Otherwise we've seen ssh fail.
start_system "$system_cmd"
wait_for_system_start "${up_cmd}"

# Set the RIFT_VAR_ROOT env that can be used by other scripts
set_rift_var_root "$system_cmd"

# If a command for sink configuration is provided, run it and proceed with the
# system test regardless of the return value.
if [ "${sink_cmd}" != "" ]; then
    run_cmd "${sink_cmd}"
fi

# If a dts command was provided, run it in the foreground and capture the return value.
# The system test will continue regardless of the dts_cmd's return value.
if [ "${dts_cmd}" != "" ]; then
    run_cmd "${dts_cmd}"
fi

# If a separate config command was provided, run it in the foreground
# and capture the return value.
if [ "${config_cmd}" != "" ]; then
    run_cmd "${PRELOAD} ${config_cmd}"
    if [[ ${curr_rc} -ne 0 ]]; then
        $show_sysinfo_cmd
        echo "Exiting with config_rc: $curr_rc"
        exit ${curr_rc}
    fi
fi

# Save log output to temporaries so that we can output the test results after
# the system has been shutdown
# cleanup temporaries on EXIT
tmp_dir=$(mktemp -d)
systest_pipe="${tmp_dir}/systest_pipe"
mkfifo ${systest_pipe}

# Test the system in the foreground
if [ "${test_cmd}" != "" ]; then

    if [ "${pre_test_cmd}" != "" ]; then
        run_cmd "${PRELOAD} ${pre_test_cmd}"
    fi

    run_cmd "${PRELOAD} ${test_cmd}"
    test_rc=${curr_rc}
    if [[ ${test_rc} -ne 0 ]]; then
        echo "Running system command - $show_sysinfo_cmd"
        $show_sysinfo_cmd
    fi

    if [ "${post_test_cmd}" != "" ]; then
        run_cmd "${PRELOAD} ${post_test_cmd}"
    fi

    # Reset the curr_rc to whatever test_rc was so that becomes the exit
    # code of this wrapper script.
    curr_rc=${test_rc}

fi

if [[ ${curr_rc} -eq 0 ]]; then
    if [ "${post_restart_test_cmd}" != "" ]; then
        echo "Restarting the system."

        # Restart sequence: Nullify any existing trap and kill system cmd.
        trap "" SIGHUP EXIT
        kill_pid "${system_pid}"

        # Sleep for 5s before starting, so that the SIGUP trap is not overwritten
        # causing the restarted system to exit.
        sleep 5

        start_system "${system_cmd}"
        wait_for_system_start "$up_cmd"
        run_cmd "${PRELOAD} ${post_restart_test_cmd}"
    fi
    if [ "${post_reboot_test_cmd}" != "" ]; then
        echo "Rebooting the system"
        # Restart sequence: Nullify any existing trap and kill system cmd.
        trap "" SIGHUP EXIT
        echo "DEBUG: Running cmd: ${reboot_cmd}"
        `${reboot_cmd}`

        run_cmd "${PRELOAD} ${post_reboot_test_cmd}"

    fi
fi

echo "Exiting with test_rc: $curr_rc"
handle_exit 0
