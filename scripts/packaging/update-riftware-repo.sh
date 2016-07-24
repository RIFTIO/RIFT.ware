#!/bin/sh

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# About:
#
# The $RIFT_ROOT/scripts/packaging/update-riftware-repo.sh is responsible for:
# 	1. copying/moving the RPMs from the staging area
#	2. update build system by calling $RIFT_ROOT/scripts/packaging/setrepo ${RIFT_BUILD_NUMBER} ${repo}.
#		- The setrepo script will then update the http://charm:8000 build system and mark the build number with it's repo: nightly/testing/release
#	3. and then finally calling the createrepo scripts on the proper release directories.
#
# !!! This script needs to run as jenkins on jenkins(charm) !!!
#
# nate.hudson@riftio.com
#

#####################################################################################################
## Arguments
#####################################################################################################

# $1 is the dir to find rpms
# $2 is the build_number
# $3 is the repo to update, ex. nightly or testing

# not fully implemented!
# $4 is the version: ie 4.0 or 4.1
# ? $5 is the method: copy or move?

if [ $# -lt 3 ]; then
    echo 'ARGS ARE srcdir buildnum repo [ver [copy_or_move] ]'
    echo 'EG /tmp/ 12345 nightly'
    exit 
fi

DIR=$1
BUILD=$2
REPO=$3

# $4 RW version. optional argument
#VER=4.0
VER=$4

# $5 copy or move? optional argument
#CPMV="copy"
CPMV=${5:-copy}

#####################################################################################################
## Arguments
#####################################################################################################

function log()
{
	echo "(`date`) [update-riftware-repo $$] $1 " >> $LOG 2>&1
}


#LOG=/tmp/createrepo-${REPO}.log
LOG=/var/log/createrepo/createrepo.log
ME=`basename "$0"`
#LOCK=/tmp/createrepo-${REPO}.lock.d
LOCK=/tmp/createrepo.lock
# we only want one lock so we don't have a bunch of createrepos running at a time
GLOB=${DIR}/*${BUILD}*.rpm

if [[ $(ls $GLOB) =~ riftware-base-([0-9]+\.[0-9]+)\. ]]; then
    ver=${BASH_REMATCH[1]}
    if [ -z "$VER" ]; then
        VER=$ver
    elif [ "$VER" != "$ver" ]; then
        log "WARNING...specified version $VER does not match product release version $ver"
    fi
fi

if [ -z "$VER" ]; then
    log "ABORTING...VER not specified and cannot be parsed"
    exit 1
fi

REPO_BASE=/net/boson/home1/autobot/www/mirrors/releases/riftware/${VER}/20/x86_64

function update_buildsys()
{
       	log "[update_buildsys] Updating Build System";

       	setrepo="${RIFT_ROOT:-/usr/rift}/scripts/packaging/setrepo"

        if [ -f ${setrepo} ]; then
               	log "[update_buildsys] Calling ${setrepo} ${BUILD} ${REPO} "
                ${setrepo} ${BUILD} ${REPO}
        fi

}

# use a mkdir file lock as mkdir is supposed to be atomic over NFS
# http://unix.stackexchange.com/questions/70/what-unix-commands-can-be-used-as-a-semaphore-lock
create_lock_or_wait () {

	log "[create_lock_or_wait] lock=${LOCK}"

	while true; do
		#log "lockDir: `ls -all ${LOCK}` "

        	if mkdir "${LOCK}"; then
			log "[create_lock_or_wait] got the lock!"
        		break;
	        fi

		log "[create_lock_or_wait] LOCKED! ...sleeping..."
	        sleep 60
	done

}

remove_lock () {
	rmdir "${LOCK}"
	log "[remove_lock] lock=${LOCK} removed lock! "
	#log "lockDir: SHOULD_BE_GONE! `ls -all ${LOCK}` "
}



#####################################################################################################
## START of main
#####################################################################################################

log " "
log "START @ `date` "
log "ME=$ME "
log "ARGS=$1 $2 $3 $4 $5"
log " "
log "GLOB=${GLOB} "
log "REPO=${REPO} "
log "VER=${VER} "
log "CPMV=${CPMV} "
log " "


if [ "${DIR}" == "" ]; then
	MSG="You must pass in the directory as arg1 to find the RPMS. Quitting."
	echo $MSG && log "$MSG"
	exit 1
fi

if [ "${BUILD}" == "" ]; then
	MSG="You must pass in the build_number as arg2. Quitting."
	echo $MSG && log "$MSG"
	exit 1
fi

if [ "${REPO}" == "" ]; then
	MSG="You must pass in the repo as arg3. Quitting."
	echo $MSG && log "$MSG"
	exit 1
fi

# repo can now be a branch, such as osm_mwc RIFT-10926
#if [[ "${REPO}" != "nightly"  && "${REPO}" != "testing"  && "${REPO}" != "release" ]]; then
#	MSG="Repo can only be: nightly/testing/release."
#	echo $MSG && log "$MSG"
#	exit 1
#fi

# really this should only be run by jenkins so the permissions are correct in the repo
if [ `whoami` != 'jenkins' ]; then
	MSG="You must be jenkins to do this. Quitting."
	echo $MSG && log "$MSG"
	exit 1
fi

if ! which createrepo >/dev/null; then
	MSG="createrepo not found! Quitting."
	echo $MSG && log "$MSG"
	exit 1;
fi



#NUM_RUNNING=`pgrep -f $ME -c`


# lock
create_lock_or_wait


# cp/mv glob from staging to real repo spot
if [ "${CPMV}" == "copy" ]; then
	log "CMD: cp -vf ${GLOB} ${REPO_BASE}/${REPO}/ "
	cp -vf ${GLOB} ${REPO_BASE}/${REPO}/ >> $LOG 2>&1
elif [ "${CPMV}" == "move" ]; then
	log "CMD: mv -vf ${GLOB} ${REPO_BASE}/${REPO}/ "
	mv -vf ${GLOB} ${REPO_BASE}/${REPO}/ >> $LOG 2>&1
else
	echo "We only support copy or move"
	exit;
fi

log "CDing to ${REPO_BASE}/${REPO}"
#cd /net/boson/home1/autobot/www/mirrors/releases/riftware/4.0/20/x86_64/${REPO} || exit 1;
cd ${REPO_BASE}/${REPO} || { echo "Failed to cd to ${REPO_BASE}/${REPO}. Exitting."; remove_lock; exit 1; }


#
# http://linux.die.net/man/8/createrepo
#
# --update
# If metadata already exists in the outputdir and an rpm is unchanged (based on file size and mtime) since the metadata was generated, reuse the existing metadata rather than recalculating it.
# In the case of a large repository with only a few new or modified rpms this can significantly reduce I/O and processing time.

# make the simple repo first
#echo "[update-riftware-repo] running createrepo" >> $LOG 2>&1
log "running createrepo basic"
createrepo --update --profile . >> $LOG 2>&1

log "sleeping a bit"
sleep 10

# run setrepo to update the build system
# the full binaries are available now, even if the deltas are not
update_buildsys

# deltas turned OFF for now, they were taking a long time
# then the deltas for testing
#if [ "${REPO}" == "testing" ]; then
#	#echo "[update-riftware-repo] running createrepo with deltas" >> $LOG 2>&1
#	log "running createrepo with deltas"
#	createrepo -v --max-delta-rpm-size 5000000000 --oldpackagedirs . --deltas --num-deltas=5 --cachedir=/tmp/repo-${REPO}.cache --profile . >> $LOG 2>&1
#fi

#unlock
remove_lock

log "END @ `date`"
log " "

