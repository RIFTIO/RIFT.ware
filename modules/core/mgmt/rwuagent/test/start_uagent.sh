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


# This is a temporary shell script until the rwvcs
# components can be started directly from python scripts


source ../../../../.././.install/usr/data/exec_env.sh

trgen=1
ldbn=2
trnk=3

if [[ $# -ne 2 ]] ; then
    echo "Usage: $0 <collapsed> <confd|native>"
    echo "    For example:"
    echo "      $0 collapsed confd"
        exit 1
fi

if [[ $# -ne 3 ]] ; then
	if [ $1 == "small" ] ; then
		trgen=1
		ldbn=1
		trnk=1
	fi
fi

targ=""
txml=""

if [ $1 == "collapsed" ] ; then
 	if [ $2 == "native" ] ; then
		targ="UAgentTests.test_uagent_collapsed_native"
		txml="collapsed_uagent_native.xml"
	fi
 	if [ $2 == "confd" ] ; then
		targ="UAgentTests.test_uagent_collapsed_confd"
		txml="collapsed_uagent_confd.xml"
	fi
fi

./uagent_tests.py --only_gen_xml $targ

if [ ! -f $txml ]; then
	echo manifest file not found
	exit 1
fi
CURR_DIR=`pwd`
cd $INSTALLDIR
gdb --args $INSTALLDIR/usr/bin/rwmain load manifest file $CURR_DIR/$txml
