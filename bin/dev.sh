#!/bin/bash
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#

root=$(realpath $(dirname $0)/../)
log=${root}/.install/.undev
verbose=false

undev() {
  local found_dirs=false
  local links=$(find ${root}/.install/ -type l)
  local f

  for f in ${links}; do
    fp=$(readlink ${f})
    if [[ ${fp} = *.build* ]]; then
      if [ -d ${fp} ]; then
        echo "ERROR: ${f} points to a directory"
        found_dirs=true
      fi
    fi

    if [[ ${fp} = *@* || ${f} = *@* ]]; then
      echo "Cannot deal with @ in paths"
      exit 1
    fi
  done

  if ${found_dirs}; then
    echo "Cannot proceed with linked directories"
    exit 1
  fi

  if [ -s ${log} ]; then
    echo "Previous undev found, appending"
    echo > ${log}
  fi

  for f in ${links}; do
    fp=$(readlink ${f})
    if [[ ${fp} = *.build* ]]; then
      rm -f ${f}

      pushd $(dirname ${f}) >/dev/null
      cp -p ${fp} ${f}
      popd >/dev/null

      echo "${f}@${fp}" >> ${log}
      if ${verbose}; then
        echo "Replacing ${f} with ${fp}"
      fi
    fi
  done

  echo 'Converted all symlinks to real files'
}

redev() {
  local line
  local src
  local dest

  if [ ! -s "${log}" ]; then
    echo "Cannot redev something that was not undeved"
    exit 1
  fi

  for line in $(<${log}); do
    dest=${line%@*}
    src=${line#*@}

    rm -f ${dest}
    ln -s ${src} ${dest}
    if ${verbose}; then
      echo "Linking ${src} at ${dest}"
    fi
  done

  rm -f ${log}
}

_chksum() {
  echo $(md5sum ${1} | cut -d' ' -f1)
}

verify() {
  local line
  local src
  local dest

  if [ ! -s "${log}" ]; then
    echo "Nothing to verify"
    exit 1
  fi

  for line in $(<${log}); do
    dest=${line%@*}
    src=${line#*@}

    pushd $(dirname ${dest}) >/dev/null
    if [ "$(_chksum ${dest})" != "$(_chksum ${src})" ]; then
      echo "${dest} does not match ${src}"
    fi
    popd > /dev/null
  done
}

usage() {
  echo "$(basename $0) ARGUMENTS"
  echo
  echo "ARGUMENTS:"
  echo "  -r,--redev    re-dev the install tree"
  echo "  -u,--undev    un-dev the install tree"
  echo "  -V,--verify   verify an un-deved tree"
  echo "  -v,--verbose  verbose logging"
  echo "  -h,--help     this screen"
}

action=

while [ $# -gt 0 ]; do
  case $1 in
    -r|--redev)
      action='redev'
      ;;
    -u|--undev)
      action='undev'
      ;;
    -V|--verify)
      action='verify'
      ;;
    -v|--verbose)
      verbose=true;
      ;;
    -h|--help)
      usage
      exit 0
      ;;
  esac
  shift
done

if [ -z "${action}" ]; then
  echo "No action specified"
  exit 1
fi

${action}
