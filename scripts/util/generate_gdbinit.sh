#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

if [ -d "$1" ] ; then
  cd "$1"
  ( echo '# Pull this in with eg "gdb -x gdbinit ..." to find symbols from plugins, Gi, etc'
    echo -n "set solib-search-path "
    for p in `find . -name \*so -print | grep -v confd | xargs dirname | sort | uniq ` ; do 
      echo -n "$p:"
    done 
    echo . 
  ) > gdbinit
else
  echo "Usage: $0 \$RIFT_INSTALL"
fi

