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
# Author(s): Tom Seidenberg
# Creation Date: 2014/02/18
#
# General script for setting environment variables and invoking a
# program for cmake add_custom_command().  This script works similar to
# env, but with the following syntax:
#
#    envset.sh [VAR=[=|:-|:+|++|+-]VALUE]... [-e] CMD [ARG...]
#
# Variable settings come first.  They allow (path-like) concatentation
# as well as pure setting.  All such settings are exported to the shell
# environment.  The syntax for variable settings contains at least one
# '=', with the following alternatives:
#
#   "VAR==VAL": No special interpretation of VAL - just set
#   "VAR=:-VAL": VAR is colon-separated path, prepend VAL (with colon only if VAR already set)
#   "VAR=:+VAL": VAR is colon-separated path, append VAL (with colon only if VAR already set)
#   "VAR=+-VAL": prepend VAL to VAR
#   "VAR=++VAL": append VAL ro VAR
#   "VAR=VAL": set VAR to VAL - this is potentially ambiguous, if VAL is arbitrary string!
#
# Options:
#   -e   If present, forces an argument that has an '=' in it to be
#        instead inerpretted as the command.
#   CMD  The command to invoke.
#   ARG  An argument to the command.
#
# Environment:
#   VERBOSE   If set to '1', the changed varirables are echoed prior to invoking CMD
#   GDB       If set to '1' or the basename of CMD, CMD is invoked via gdb

envset_verbose="VERBOSE|GDB"

if [ -z "$RIFT_ROOT" ]; then
  echo "Error: You must be in a RIFT_SHELL to run this script."
  exit 1
fi

while [[ $1 ]]
do
  # Eat separator
  if [[ " $1" = " -e" ]]; then
    shift
    break
  fi

  # Stop if not assignment
  [[ "${1%%=*}" = "$1" ]] && break

  envset_var="${1%%=*}"
  envset_verbose="$envset_verbose|$envset_var"
  envset_op_val="${1#*=}"
  shift
  #echo "v='$envset_var' o='$envset_op_val'"

  # "VAR==VAL"
  if [[ " $envset_op_val" != " ${envset_op_val#=}" ]]; then
    export "$envset_var"="${envset_op_val#=}"
    continue
  fi

  # "VAR=:-VAL"
  if [[ " $envset_op_val" != " ${envset_op_val#:-}" ]]; then
    eval 'envset_orig=$'"$envset_var"
    if [[ $envset_orig ]]; then
      export "$envset_var"="${envset_op_val#:-}:$envset_orig"
    else
      export "$envset_var"="${envset_op_val#:-}"
    fi
    continue
  fi

  # "VAR=:+VAL"
  if [[ " $envset_op_val" != " ${envset_op_val#:+}" ]]; then
    eval 'envset_orig=$'"$envset_var"
    if [[ $envset_orig ]]; then
      export "$envset_var"="$envset_orig:${envset_op_val#:+}"
    else
      export "$envset_var"="${envset_op_val#:+}"
    fi
    continue
  fi

  # "VAR=+-VAL"
  if [[ " $envset_op_val" != " ${envset_op_val#+-}" ]]; then
    eval 'envset_orig=$'"$envset_var"
    export "$envset_var"="${envset_op_val#+-}$envset_orig"
    continue
  fi

  # "VAR=++VAL"
  if [[ " $envset_op_val" != " ${envset_op_val#++}" ]]; then
    eval 'envset_orig=$'"$envset_var"
    export "$envset_var"="$envset_orig${envset_op_val#++}"
    continue
  fi

  # Assume "VAR=VAL"
  export "$envset_var"="$envset_op_val"
done

envset_prog="$1"
shift

if [[ "$envset_prog" = "" ]]; then
  echo "envset.sh: No program specified!"
  exit 1
fi

if [[ "$VERBOSE" = 1 || "$GDB" ]]; then
  echo "envset.sh:"
  env | perl -ne "/^(${envset_verbose})=/ and print \"  \$_\""
fi

if [[ "$GDB" = 1 ]]; then
  echo "  Running gdb due to GDB=1"
  echo "  exec: gdb --args $envset_prog $*"
  exec gdb --args "$envset_prog" "$@"
fi

if [[ "$GDB" = "${envset_prog##*/}" ]]; then
  echo "  Running gdb due to GDB=$GDB"
  echo "  exec: gdb --args $envset_prog $*"
  exec gdb --args "$envset_prog" "$@"
fi

if [[ "$VERBOSE" = 1 ]]; then
  echo "  exec: $envset_prog $*"
fi
exec "$envset_prog" "$@"

