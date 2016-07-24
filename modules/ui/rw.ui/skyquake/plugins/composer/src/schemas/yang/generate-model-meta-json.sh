#!/bin/sh

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

./src-remove.sh

./yang2json.sh

./src-append.sh

node confd2model.js >./model-meta.json
