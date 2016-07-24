#!/bin/bash
### Copyright 2010 Manuel Carrasco Moñino. (manolo at apache.org)
###
### Licensed under the Apache License, Version 2.0.
### You may obtain a copy of it at
### http://www.apache.org/licenses/LICENSE-2.0

###
### A library for shell scripts which creates reports in jUnit format.
### These reports can be used in Jenkins, or any other CI.
###
### Usage:
###     - Include this file in your shell script
###     - Use juLog to call your command any time you want to produce a new report
###        Usage:   juLog <options> command arguments
###           options:
###             -name="TestName" : the test name which will be shown in the junit report
###             -error="RegExp"  : a regexp which sets the test as failure when the output matches it
###             -ierror="RegExp" : same as -error but case insensitive
###     - Junit reports are left in the folder 'result' under the directory where the script is executed.
###     - Configure Jenkins to parse junit files from the generated folder
###

asserts=00; errors=0; total=0; content=""
date=`which date`

# create output folder
juDIR=`pwd`/results

# The name of the suite is calculated based in your script name
suite=`basename $0 | sed -e 's/.sh$//' | tr "." "_"`

# A wrapper for the eval method witch allows catching seg-faults and use tee
errfile=/tmp/evErr.$$.log

juSetOutputDir(){
  out_dir=$1
  if [ ! -d "$out_dir" ]; then
    mkdir -p $out_dir || exit
  fi
  juDIR=$out_dir
}

juSetSuiteName(){
  suite="$1"
}

# Method to clean old tests
juLogClean() {
  echo "+++ Removing old junit reports from: $juDIR "
  rm -f "$juDIR"/TEST-*
}

# Execute a command and record its results
juLog() {

  # parse arguments
  ya=""; icase=""
  while [ -z "$ya" ]; do
    case "$1" in
  	  -name=*)   name=$asserts-`echo "$1" | sed -e 's/-name=//'`;   shift;;
      -ierror=*) ereg=`echo "$1" | sed -e 's/-ierror=//'`; icase="-i"; shift;;
      -error=*)  ereg=`echo "$1" | sed -e 's/-error=//'`;  shift;;
      *)         ya=1;;
    esac
  done

  # use first arg as name if it was not given
  if [ -z "$name" ]; then
    name="$asserts-$1"
    shift
  fi

  # eval the command sending output to a file
  outf=/var/tmp/ju$$.txt
  >$outf
  tail -f $outf 2>/dev/null &
  tailpid=$!
  here=`pwd`
  cat <<EOF >>$outf
  
+++ Running case: $name 
+++ working dir: $here
+++ command: $@      
EOF
  ini=`$date +%s.%N`

  # Force monitor mode to ensure we are running the processes within
   # seperate process groups. Otherwise the reaper could take down this script and even Jenkins
  { set -o monitor; $@ >>$outf 2>&1; }
  evErr=$?

  rm -f $errfile
  end=`date +%s.%N`
  echo "+++ exit code: $evErr"        >> $outf

  # RIFT-2842: Strip invalid XML characters from output
  # http://stackoverflow.com/questions/7772430/how-to-remove-invalid-characters-from-an-xml-file-using-sed-or-perl
  #  Assuming UTF-8 XML documents:
  #
  #  perl -CSDA -pe'
  #     s/[^\x9\xA\xD\x20-\x{D7FF}\x{E000}-\x{FFFD}\x{10000}-\x{10FFFF}]+//g;
  #     ' file.xml > file_fixed.xml
  #     If you want to encode the bad bytes instead,
  #
  #     perl -CSDA -pe'
  #        s/([^\x9\xA\xD\x20-\x{D7FF}\x{E000}-\x{FFFD}\x{10000}-\x{10FFFF}])/
  #              "&#".ord($1).";"
  #                 /xeg;
  #                 ' file.xml > file_fixed.xml
  #                 You can call it a few different ways:
  #
  #                 perl -CSDA     -pe'...' file.xml > file_fixed.xml
  #                 perl -CSDA -i~ -pe'...' file.xml     # Inplace with backup
  #                 perl -CSDA -i  -pe'...' file.xml     # Inplace without backup
  kill $tailpid
  wait $tailpid 2>/dev/null
  perl -CSDA -i -pe's/[^\x9\xA\xD\x20-\x{D7FF}\x{E000}-\x{FFFD}\x{10000}-\x{10FFFF}]+//g;' $outf

  # set the appropriate error, based in the exit code and the regex
  [ $((evErr+0)) -ne 0 ] && err=1 || err=0
  out=`cat $outf | sed -e 's/^\([^+]\)/| \1/g'`
  if [ $err -eq 0 -a -n "${ereg:-}" ]; then
      H=`echo "$out" | egrep $icase "$ereg"`
      [ -n "${H:-}" ] && err=1
  fi
  echo "+++ error: $err"
  rm -f $outf

  # calculate vars
  asserts=`expr $asserts + 1`
  asserts=`printf "%.2d" $asserts`
  errors=`expr $errors + $err`
  time=`echo "$end - $ini" | bc -l`
  total=`echo "$total + $time" | bc -l`

  # write the junit xml report
  ## failure tag
  [ $err = 0 ] && failure="" || failure="
      <failure type=\"ScriptError\" message=\"Script Error\"></failure>
  "
  ## testcase tag
  content="$content
    <testcase assertions=\"1\" name=\"$name\" time=\"$time\">
    $failure
    <system-out>
<![CDATA[
$out
]]>
    </system-out>
    </testcase>
  "
  ## testsuite block
  mkdir -p "$juDIR" || exit

  cat <<EOF > "$juDIR/TEST-$suite.xml"
  <testsuite failures="0" assertions="$assertions" name="$suite" tests="1" errors="$errors" time="$total">
    $content
  </testsuite>
EOF

  return $err
}

