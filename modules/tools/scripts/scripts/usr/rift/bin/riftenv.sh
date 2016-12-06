#!/bin/bash

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


# Allow source from anywhere
rift_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
rift_root="${rift_root%/bin}"

if [ -f "$rift_root/.gitmodules" ] ; then

    export RIFT_ROOT="$rift_root"

    riftenv-exit() {

    if [ "$RIFT_MODE" ] ; then

	    PS1="$_OLD_PS1" 
	    export PS1
	    unset _OLD_PS1

	    PATH="$_OLD_PATH"
	    export PATH
	    unset _OLD_PATH

	    LD_LIBRARY_PATH="$_OLD_LD_LIBRARY_PATH"
	    export LD_LIBRARY_PATH
	    unset _OLD_LD_LIBRARY_PATH

	    PKG_CONFIG_PATH="$_OLD_PKG_CONFIG_PATH"
	    export PKG_CONFIG_PATH
	    unset _OLD_PKG_CONFIG_PATH

	    #PYTHONPATH="$_OLD_PYTHONPATH"
	    #export PYTHONPATH
	    #unset _OLD_PYTHONPATH

            unset RIFT_MODE
            unset RIFT_ROOT
            unset RIFT_MODE

            unalias color-cmake
            unalias clang-cmake

	    echo "You have left RIFT_MODE"

    fi

    }

    _OLD_PATH="$PATH"
    export PATH="$RIFT_INSTALL/usr/bin:$RIFT_INSTALL/usr/local/seagull-1.8.2/bin:${PATH:+:$PATH}"

    _OLD_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
    export LD_LIBRARY_PATH="$RIFT_INSTALL/usr/lib:$RIFT_INSTALL/usr/lib64:$RIFT_INSTALL/usr/local/seagull-1.8.2/bin:$RIFT_INSTALL/usr/lib/rift/plugins"

    _OLD_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
    export PKG_CONFIG_PATH="$RIFT_INSTALL/usr/lib/pkgconfig"

    export PYTHONPATH=$RIFT_ROOT/modules/core/automation/framework/common/site-packages:$RIFT_INSTALL/usr/lib/python2.7/site-packages:$RIFT_INSTALL/usr/lib64/python2.7/site-packages

    if [ -z "$RIFT_MODE" ] ; then
        _OLD_PS1="$PS1"

        if [[ ! "${RIFT_PRESERVE_PS1+1}" ]]; then
            # Determine the best workspace name
            rift_wsname="${rift_root##*/}"
            if [[ "$rift_wsname" = "rift" ]]; then
                rift_wsname="${rift_root%/rift}"
                rift_wsname="${rift_wsname##*/}"
            fi

            PS1="(riftenv-`basename \"$rift_wsname\"`)$PS1"
            unset rift_wsname
            export PS1
        fi
    fi

    RIFT_MODE=yes

    alias color-cmake='(CC=../misc/gcc-color/cc-color CXX=../misc/gcc-color/c++-color cmake ..)'
    alias clang-cmake='(CC=clang CXX=clang++ cmake ..)'

    # This is what the old setup.env tool did
    #source "$RIFT_ROOT/bin/setupenv.sh"

else
    echo "Failed to verify workspace - .../rift/.gitmodules is missing"
fi

unset rift_root
if [ $# -ne 0 ]; then
        "$@"
fi

