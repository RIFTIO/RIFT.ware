#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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

            unset INSTALLDIR
            unset RIFT_MODE
            unset RIFT_ROOT
            unset RIFT_MODE

            unalias color-cmake
            unalias clang-cmake

	    echo "You have left RIFT_MODE"

    fi

    }

    export INSTALLDIR="$RIFT_ROOT/.install"

    _OLD_PATH="$PATH"
    export PATH="$INSTALLDIR/usr/bin:$INSTALLDIR/usr/local/seagull-1.8.2/bin:${PATH:+:$PATH}"

    _OLD_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
    export LD_LIBRARY_PATH="$INSTALLDIR/usr/lib:$INSTALLDIR/usr/lib64:$INSTALLDIR/usr/local/seagull-1.8.2/bin:$INSTALLDIR/usr/lib/rift/plugins"

    _OLD_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
    export PKG_CONFIG_PATH="$INSTALLDIR/usr/lib/pkgconfig"

    export PYTHONPATH=$RIFT_ROOT/modules/core/automation/framework/common/site-packages:$INSTALLDIR/usr/lib/python2.7/site-packages:$INSTALLDIR/usr/lib64/python2.7/site-packages

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

