#!/bin/sh

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

