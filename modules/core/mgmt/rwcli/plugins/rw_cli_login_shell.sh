#!/bin/sh

# STANDARD_RIFT_IO_COPYRIGHT


if [ -z "${RW_VAR_RIFT}" ]; then
    rw_env_location=/var/rift
else
    rw_env_location=${RW_VAR_RIFT}
fi

netconf_port_number=$(cat ${rw_env_location}/env.d/NETCONF_PORT_NUMBER)
rwvm_instance_id=$(cat ${rw_env_location}/env.d/RWVM_INSTANCE_ID)

cli_exe="${RIFT_INSTALL}/usr/bin/zsh --netconf_port ${netconf_port_number}"

${cli_exe}

exit

