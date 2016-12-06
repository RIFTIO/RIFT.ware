
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

# this script is designed to set all of the workspace 
# independent variables needed

PATH=${PATH//\/usr\/local\/git-submodule-tools:/}
PATH=${PATH//\/usr\/local\/rift-submodule-tools:/}


if [[ ":$PATH:" != *":/usr/rift/bin:"* ]]; then
    PATH=$PATH:/usr/rift/bin
fi
if [[ ":$PATH:" != *":./git/rift-submodule-tools:"* ]]; then
    PATH=$PATH:./git/rift-submodule-tools
fi
if [[ ":$PATH:" != *":/usr/sbin:"* ]]; then
    PATH=$PATH:/usr/sbin
fi
export PYTHONPATH
export PATH
if [ -z "$CCACHE_DIR" ]; then
    if [ -e /localdisk/${USER} ]; then
        mkdir -p /localdisk/${USER}/.ccache
        if [ $? -eq 0 ]; then
            export CCACHE_DIR=/localdisk/${USER}/.ccache
        fi
    fi
fi
if [ -z "$TZ" ]; then
    export TZ=America/New_York
fi
if [ -z "$PROMPT_COMMAND" ]; then
    setps()
    {
            PS1='[\u@\h \W]\$ '
    }
    PROMPT_COMMAND=setps
fi

# causing LOTs of IO for jenkins and regular users and making logins SLOW
#if [ -f /bin/ccache ]; then
#    if [ $USER == "jenkins" ]; then
#        /bin/ccache -M 50G >/dev/null
#    else
#        /bin/ccache -M 10G >/dev/null
#    fi
#fi

