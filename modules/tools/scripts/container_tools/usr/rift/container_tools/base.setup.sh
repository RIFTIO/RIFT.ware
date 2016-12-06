# Install enum34 if we are at python 3.4 or earlier

test -h /usr/rift && rm -f /usr/rift 

if python3 -c 'import sys;assert sys.version_info < (3,5)'; then
    cmd $PIP3_INSTALLER install enum34==1.0.4
fi
