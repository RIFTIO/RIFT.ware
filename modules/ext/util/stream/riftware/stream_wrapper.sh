#!/bin/bash

if [ $# -ne 2 ]
then
 echo "Missing arguments. Usage: $0 <exec-path-for-stream> <Logical CPU string>"
 echo "                   Example: ./stream_wrapper.sh \"usr/local/bin/rw_stream\" \"4,[1-10],2\""
 exit -1
fi

if [ ! -e $1 ] ; then
 echo "Executable $1 does not exist"
 exit -1
fi

echo "Executable :" $1

term_handler() { 
  printf "%s\n" "Stream wrapper caught SIGTERM signal!" 
  kill -TERM $child 2>/dev/null
}

trap term_handler SIGTERM

#Split given string based on comma as delimiter
arr=$(echo $2 | tr "," "\n")

for x in $arr
do
    # Check if string has [x-y] or digits format
    pattern1='\[([0-9]+)-([0-9]+)\]'
    pattern2='^[0-9]*$'
    if [[ ! ($x =~ $pattern1 || $x =~ $pattern2) ]]; then
       echo "Processor pattern does not match"
       echo "       Example: ./stream_wrapper.sh \"usr/local/bin/rw_stream\" \"4,[1-10],2\""
       exit -1
    fi
done

sudo taskset -c $2 $1 &

child=$!
#Wait for child exit
wait "$child"
wait "$child"
echo "Exiting stream wrapper"
exit 0
