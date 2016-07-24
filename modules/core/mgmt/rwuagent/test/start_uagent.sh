#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
