#! /bin/sh
# Copyright (C) 1998,1999,2001,2003,2004,2006,2007,2008,2009
# Free Software Foundation, Inc.
# This file is part of the GNU C Library.
# Contributed by Ulrich Drepper <drepper@cygnus.com>, 1998.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA.

if test $# -eq 0; then
  echo "$0: missing program name" >&2
  echo "Try \`$0 --help' for more information." >&2
  exit 1
fi

prog="$1"
shift

if test $# -eq 0; then
  case "$prog" in
    --h | --he | --hel | --help)
      echo 'Usage: catchsegv PROGRAM ARGS...'
      echo '  --help      print this help, then exit'
      echo '  --version   print version number, then exit'
      echo "For bug reporting instructions, please see:"
      echo "<http://www.debian.org/Bugs/>."
      exit 0
      ;;
    --v | --ve | --ver | --vers | --versi | --versio | --version)
      echo 'catchsegv (Ubuntu EGLIBC 2.11.1-0ubuntu7.8) 2.11.1'
      echo 'Copyright (C) 2009 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
Written by Ulrich Drepper.'
      exit 0
      ;;
    *)
      ;;
  esac
fi

segv_output=`mktemp ${TMPDIR:-/tmp}/segv_output.XXXXXX` || exit

# Redirect stderr to avoid termination message from shell.
(exec 3>&2 2>/dev/null
LD_PRELOAD=${LD_PRELOAD:+${LD_PRELOAD}:}/\$LIB/libSegFault.so \
SEGFAULT_USE_ALTSTACK=1 \
SEGFAULT_OUTPUT_NAME=$segv_output \
"$prog" ${1+"$@"} 2>&3 3>&-)
exval=$?

# Check for output.  Even if the program terminated correctly it might
# be that a minor process (clone) failed.  Therefore we do not check the
# exit code.
if test -s "$segv_output"; then
  # The program caught a signal.  The output is in the file with the
  # name we have in SEGFAULT_OUTPUT_NAME.  In the output the names of
  # functions in shared objects are available, but names in the static
  # part of the program are not.  We use addr2line to get this information.
  case $prog in
  */*) ;;
  *)
    old_IFS=$IFS
    IFS=:
    for p in $PATH; do
      test -n "$p" || p=.
      if test -f "$p/$prog"; then
	prog=$p/$prog
      break
      fi
    done
    IFS=$old_IFS
    ;;
  esac
  sed '/Backtrace/q' "$segv_output"
  sed '1,/Backtrace/d' "$segv_output" | sed '/Memory map:/q' |
  (while read line; do
     if echo -n "$line" | grep -Fq "$prog"; then
       exe=`echo -n "$line" | sed 's/([^(]\{1,\})//' | sed 's/\[0x[[:xdigit:]]\{1,\}\]//'`
       exe=`readlink --canonicalize-existing "$exe" 2>/dev/null`
       if test $? -eq 0; then
         if test -f "$exe" -a -e "$exe"; then
           addr=`echo -n "$line" | sed 's/.*\[\(0x[[:xdigit:]]\{1,\}\)\]$/\1/'`
           addr2line=`addr2line --exe="$exe" --functions --demangle $addr 2>/dev/null`
           if test $? -eq 0; then
             if echo -n "$addr2line" | grep -Eq '^\?\?:0'; then
               :
             else
               func=`echo "$addr2line" | head --lines=1`
               fileline=`echo "$addr2line" | tail --lines=1`
               file=`echo -n "$fileline" | sed 's/:[[:digit:]]\{1,\}$//'`
               if test -f "$file"; then
                 line="$fileline: $func"
               fi
             fi
           fi
         fi
       fi
     else
       exe=`echo -n "$line" | sed 's/([^(]\{1,\})//' | sed 's/\[0x[[:xdigit:]]\{1,\}\]//'`
       exe=`readlink --canonicalize-existing "$exe" 2>/dev/null`
       if test $? -eq 0; then
         if test -f "$exe" -a -e "$exe"; then
           addr=`echo -n "$line" | sed 's/.*\[\(0x[[:xdigit:]]\{1,\}\)\]$/\1/'`
           addr=`printf '%d' $addr 2>/dev/null`
           if test $? -eq 0; then
             mmap=`grep -F "$exe" "$segv_output" | grep -E ' 0+ '`
             if test $? -eq 0; then
               baseaddr=`echo -n "$mmap" | grep -Eo '^[[:xdigit:]]+'`
               baseaddr=`printf '%d' 0x$baseaddr 2>/dev/null`
               if test $? -eq 0; then
                 addr=`expr $addr - $baseaddr`
                 addr=`printf '%x' $addr`
                 addr2line=`addr2line --exe="$exe" --functions --demangle $addr 2>/dev/null`
                 if test $? -eq 0; then
                   if echo -n "$addr2line" | grep -Eq '^\?\?:0'; then
                     :
                   else
                     func=`echo "$addr2line" | head --lines=1`
                     fileline=`echo "$addr2line" | tail --lines=1`
                     file=`echo -n "$fileline" | sed 's/:[[:digit:]]\{1,\}$//'`
                     if test -f "$file"; then
                       line="$fileline: $func"
                     fi
                   fi
                 fi
               fi
             fi
           fi
         fi
       fi
     fi
     echo "$line"
   done)
  sed '1,/Memory map:/d' "$segv_output"
fi
rm -f "$segv_output"

exit $exval
