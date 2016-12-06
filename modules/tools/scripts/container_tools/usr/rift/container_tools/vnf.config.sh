if $ONLY_RIFT_PKG_REPOS ; then
    cmd npm set registry https://npm.riftio.com:4873/ --global
fi

cmd npm set strict-ssl false --global
cmd npm set progress=false --global
cmd npm config set cache /tmp/npm-cache --global
