#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


# change to the directory of this script
THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $THIS_DIR
cd ..

echo "Building RW.UI framework"
npm install
echo "RW.UI framework build... done"

echo "Building RW.UI plugins"
cd plugins
for f in *; do
    if [[ -d $f ]]; then
        echo 'Building plugin '$f
        cd $f
        echo 'Fetching third-party node_modules for '$f
        npm install
        echo 'Fetching third-party node_modules for '$f'...done'
        echo 'Packaging '$f' using webpack'
        ./node_modules/.bin/webpack --progress --config webpack.production.config.js
        echo 'Packaging '$f' using webpack... done'
        cd ..
        echo 'Building plugin '$f'... done'
    fi
done

echo "Building RW.UI plugins... done"

