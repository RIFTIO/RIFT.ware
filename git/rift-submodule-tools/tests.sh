#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# This test can be run from within any directory.  It sets the path to use the
# local rift-submodule-tools so it is not dependant on the environment.
# It has been modified to fail on any error and will not delete temporary files to make
# debugging possible.

function log()
{
  echo "*** $@"
  "$@"
}

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SUBMODULE_TOOLS_DIR="$THIS_DIR/../rift-submodule-tools"
SCRIPTS_DIR="$THIS_DIR/../../scripts"
GIT_SCRIPTS_DIR="${SCRIPTS_DIR}/git/repo-mgmt"

source $SCRIPTS_DIR/test/sh2ju.sh

# Clean any existing Junit XML log files
juLogClean

export PATH="$SUBMODULE_TOOLS_DIR:$PATH"

err_trap_enable='trap \"echo Caught error in ${BASH_SOURCE}:${LINENO}; return 1\" ERR'
err_trap_disable='trap "" ERR'

DEFAULT_FILE="README.txt"


function mktemp_repo() {
  path=$(realpath .)
  local repo="$(mktemp -d --tmpdir=$path repoXXX)"
  echo $repo
}

function mkrift_repo() {
  path=$(realpath .)
  local repo="$(mktemp -u -d --tmpdir="." riftXXX)"
  $GIT_SCRIPTS_DIR/create_rift_git_sample_repo.sh $repo >/dev/null || return 1
  echo $repo
}

function init_submodules() {
  [ $# -ge 2 ] || return 1
  repo_root=$1
  shift
  submodules=$@

  pushd $repo_root > /dev/null
  git xinit -s $submodules || return 1
  popd > /dev/null
}

function create_clone() {
  [ $# -ge 2 ] || return 1
  rift_repo=$1
  repo_root=$2
  shift 2
  submodules="modules/core/util modules/core/fpath"
  if [ $# -gt 0 ]; then
    submodules=$@
  fi

  git clone $rift_repo/rift.git $repo_root || return 1
  init_submodules $repo_root $submodules
}

function create_branch() {
  [ $# -eq 2 ] || return 1
  repo_root=$1
  branch=$2

  pushd $repo_root > /dev/null
  git branch $branch || return 1
  popd > /dev/null

}

# Run command and grep for whether the command output contains the string.
#  Arguments
#  $1 - String to except in the command output
#  $2+ - Command to run
#
function run_and_expect_output()
{
  [ $# -lt 3 ] && return 1

  local grep_string=$1

  # Shift commadn to run into "$@"
  shift
  command="$@"

  # Create a process substitution loop to print output and search for a line
  # matching the grep string.  If any line matches, we set
  # rc to 0, otherwise rc will stay at 1.
  local rc=1
  while read line; do
    echo $line
    echo $line | grep -q "$grep_string"
    if [ $? -eq 0 ]; then
      rc=0
      # Lets keep going to print out the rest of the output anyways
    fi
  done < <($@ 2>&1 || true)

  if [ $rc -ne 0 ]; then
    log "ERROR: Did not find string ($grep_string) when running command: $@"
  fi

  return $rc
}

function xcommit() {
  [ $# -ge 2 ] || return 1
  repo_root=$1
  comment=$2

  pushd $repo_root > /dev/null
  git xcommit -a -m "xcommit, Added $file in submodule $sub_dir, repo $repo_root" || return 1
  popd > /dev/null
}

function xpush()
{
  [ $# -eq 1 ] || return 1
  repo_root=$1

  pushd $repo_root > /dev/null
  git xpush || return 1
  popd > /dev/null
}

function xcheckout()
{
  [ $# -eq 2 ] || return 1
  repo_root=$1
  changeset=$2

  pushd $repo_root > /dev/null
  git xcheckout $changeset || return 1
  popd > /dev/null
}

function xpull()
{
  [ $# -ge 1 ] || return 1
  repo_root=$1
  branch=""
  if [ $# -ge 2 ]; then
    branch=$2
  fi

  pushd $repo_root > /dev/null
  git xpull $branch || return 1
  popd > /dev/null
}

function resolve_conflict()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1
  sub_dir=$2
  file=$DEFAULT_FILE
  if [ $# -ge 3 ]; then
    file=$3
  fi

  pushd $repo_root/$sub_dir > /dev/null
  git checkout --theirs $file || return 1
  popd > /dev/null
}

function write_file()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1
  sub_dir=$2
  file=$DEFAULT_FILE
  if [ $# -ge 3 ]; then
    file=$3
  fi


  # goto the fastpath submodule directory and add a new file
  pushd $repo_root/$sub_dir
  echo "Adding line from $sub_dir submodule in repo $repo_root" >> $file || return 1
  popd > /dev/null
}

function add_file()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1
  sub_dir=$2
  file=$DEFAULT_FILE
  if [ $# -ge 3 ]; then
    file=$3
  fi

  pushd $repo_root/$sub_dir
  git add $file || return 1
  popd > /dev/null
}

function write_add()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1
  sub_dir=$2
  file=$DEFAULT_FILE
  if [ $# -ge 3 ]; then
    file=$3
  fi

  write_file $@ || return 1
  add_file $@ || return 1
}

function write_add_commit()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1
  sub_dir=$2
  file=$DEFAULT_FILE
  if [ $# -ge 3 ]; then
    file=$3
  fi

  write_add $@ || return 1

  pushd $repo_root/$sub_dir > /dev/null
   git commit -a -m "NO xcommit, Added $file in submodule $sub_dir, repo $repo_root, no xcommit" || return 1
  popd > /dev/null
}

function write_add_xcommit()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1
  sub_dir=$2
  file=$DEFAULT_FILE
  if [ $# -ge 3 ]; then
    file=$3
  fi

  write_add $@ || return 1
  xcommit $repo_root "xcommit, Added $file in submodule $sub_dir, repo $repo_root" || return 1
}

function write_add_xcommit_xpush()
{
  [ $# -ge 2 ] || return 1
  repo_root=$1

  write_add_xcommit $@

  pushd $repo_root > /dev/null
   git xpush || return 1
  popd > /dev/null
}

function diff_file()
{
  [ $# -ge 3 ] || return 1
  repo_root_a=$1
  repo_root_b=$2
  sub_dir=$3
  file=$DEFAULT_FILE
  if [ $# -ge 4 ]; then
    file=$4
  fi

  diff $repo_root_a/$sub_dir/$file $repo_root_b/$sub_dir/$file || return 1
}

function has_branch()
{
  [ $# -eq 3 ] || return 1
  repo_root=$1
  sub_dir=$2
  branch=$3

  pushd $repo_root/$sub_dir > /dev/null
  git show-ref "$branch" > /dev/null || return 1
  popd > /dev/null
}

function current_branch()
{
    git rev-parse --abbrev-ref HEAD
}

# function tests submodule commands on unmodified repo
# expect nothing abnormal to happen
# Clone a new repo and issue submodule commands
function test_submodule_commands_on_pristine_repo()
{
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)

    # create a temp repository
    local TMPREPO=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO || return 1
    cd $TMPREPO || return 1

    # These should all exit successfully
    git xcommit -a -m"testing submodule commands on pristine repo" || return 1
    git xpull || return 1
    git xpush || return 1

    cd modules
    run_and_expect_output "git-xcommit can only be executed from top" git xcommit -a -m"testing submodule commands on pristine repo" || return 1
    run_and_expect_output "git-xpull can only be executed from top" git xpull || return 1
    run_and_expect_output "git-xpush can only be executed from top" git xpush || return 1

}

# This function tests the basic commit and push in submodules
# This function creates a local repo, adds a new file, and
# rolls up the changes. It clones another repository and
# checks that new added file exists
function test_submodule_xcommit_and_push() {
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a temp repository
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1
    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1

    # create another temp repository
    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1
}

# This function tests submodule pull operation
# This function creates two temporary repositories
# Changes the contents of the first repository and rolls up
# the changes.
function test_submodule_pull() {
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1
    xpull $TMPREPO2
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1
}

# This function tests rollup process when someone else rolledup
# changes in another submodule
function test_submodule_updates_in_different_submodule() {
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1
    write_add_xcommit_xpush $TMPREPO2 modules/core/util || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1

    xpull $TMPREPO1 || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/util || return 1
}

# This function tests rollup process when changes occur in the same
# submodule
function test_submodule_updates_in_same_submodule() {
    echo "Inside ${FUNCNAME[0]}"

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1

    write_add_xcommit_xpush $TMPREPO2 modules/core/fpath foo.c || return 1
    # REPO2 should get the changes in fpath directory in REP01
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1

    xpull $TMPREPO1 || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath foo.c || return 1

}

function test_submodule_conflicting_updates() {
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1
    write_add_xcommit_xpush $TMPREPO1 modules/core/util || return 1

    write_add_xcommit $TMPREPO2 modules/core/fpath || return 1
    write_add_xcommit $TMPREPO2 modules/core/util || return 1

    run_and_expect_output "Merge conflict in README.txt" xpush $TMPREPO2  || return 1
}


# This should fail because committing locally then trying to checkout another
# submodule is sketchy and not supported.
function test_submodule_commit_then_checkout_another(){
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)

    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 modules/core/util || return 1

    # This should work fine since we are in another submodule
    init_submodules $TMPREPO1 modules/core/fpath || return 1

    write_add_commit $TMPREPO1 modules/core/util || return 1

    # This should NOT work since we haven't pushed the change to the submodule
    run_and_expect_output "There are unstaged changes" xcheckout $TMPREPO1 master || return 1

    xcommit $TMPREPO1 "stage the change" || return 1
    xpush $TMPREPO1 || return 1
    xcheckout $TMPREPO1 master || return 1
}

# Checking out a submodule after xcommitting before should be fine.
function test_submodule_xcommit_then_checkout_another(){
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 modules/core/util || return 1

    write_add_xcommit $TMPREPO1 modules/core/util || return 1

    init_submodules $TMPREPO1 modules/core/fpath || return 1
    write_add_xcommit $TMPREPO1 modules/core/fpath

    run_and_expect_output "Unpushed changes in branch" xcheckout $TMPREPO1 master^ || return 1

    xpush $TMPREPO1 || return 1
    xcheckout $TMPREPO1 master^ || return 1
}

function test_create_branch(){
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    create_branch $TMPREPO1 dts || return 1
    xcheckout $TMPREPO1 dts || return 1

    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath

    has_branch $TMPREPO1 modules/core/fpath dts || return 1
    has_branch $TMPREPO1 modules/core/util dts || return 1

    xpull $TMPREPO2 || return 1
    xcheckout $TMPREPO2 dts || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1
}

function test_branch_merge_no_conflicts(){
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    create_branch $TMPREPO1 dts || return 1
    xcheckout $TMPREPO1 dts || return 1
    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1

    # Switch back to master branch, make a commit, pull, and merge dts branch
    xcheckout $TMPREPO2 master || return 1
    write_add_xcommit $TMPREPO2 modules/core/util || return 1
    xpull $TMPREPO2 dts || return 1

    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1

    # Push merged changes
    xpush $TMPREPO2 || return 1

    # Ensure merged changes in master branch make it over
    xcheckout $TMPREPO1 master || return 1
    xpull $TMPREPO1 || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/util || return 1
}

function test_branch_merge_with_conflict(){
    echo "Inside ${FUNCNAME[0]}"
    local TMPDIR=`pwd`

    # create a sample repository with submodules
    local rift_repo=$(mkrift_repo)


    # create a two temp repositories
    local TMPREPO1=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO1 || return 1

    local TMPREPO2=$(mktemp_repo)
    create_clone $rift_repo $TMPREPO2 || return 1

    create_branch $TMPREPO1 dts || return 1
    xcheckout $TMPREPO1 dts || return 1

    write_add_xcommit_xpush $TMPREPO1 modules/core/fpath || return 1

    write_add_xcommit_xpush $TMPREPO2 modules/core/fpath || return 1

    run_and_expect_output "CONFLICT (add/add): Merge conflict in " xpull $TMPREPO2 dts || return 1

    resolve_conflict $TMPREPO2 modules/core/fpath
    xcommit $TMPREPO2 "Resolved merge conflict"

    # Xmerge should now succeed
    xpull $TMPREPO2 dts || return 1
    xpush $TMPREPO2 || return 1

    xpull $TMPREPO1 || return 1
    diff_file $TMPREPO1 $TMPREPO2 modules/core/fpath || return 1
}


# A test wrapper to log output and fail hard
run_test()
{
  test=$(basename $1)

  echo ""
  echo "Running test: $test"
  echo "--------------------------------------------"

  juLog -name=$test $@ 2>&1
  rc=$?
  if [ $rc -eq 0 ]; then
    echo "$test was succesful!"
    echo "--------------------------------------------"
    echo ""
  else
    echo "Error: Test failed: $test"
    exit 1
  fi
}

# Create a temporary
PWD=`pwd`
TEST_TOPDIR=`mktemp -d --tmpdir=${PWD} tmpXXX`
cd ${TEST_TOPDIR}

run_test test_submodule_commands_on_pristine_repo
run_test test_submodule_xcommit_and_push
run_test test_submodule_pull
#
run_test test_submodule_updates_in_different_submodule
run_test test_submodule_updates_in_same_submodule
run_test test_submodule_conflicting_updates
#
run_test test_submodule_commit_then_checkout_another
run_test test_submodule_xcommit_then_checkout_another
#
run_test test_create_branch
run_test test_branch_merge_no_conflicts
run_test test_branch_merge_with_conflict


echo ""
echo "==== All Test Cases Passed ===="


