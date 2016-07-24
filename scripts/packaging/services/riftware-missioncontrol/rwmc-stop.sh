#!/bin/sh

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


SCREEN_NAME='rwmc'

# kill the screen
/usr/bin/screen -S ${SCREEN_NAME} -X quit

