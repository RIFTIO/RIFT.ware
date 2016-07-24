#!/bin/sh
GIT_SHA1=abcdef
GIT_DIRTY=1
BUILD_ID=1
echo "#define REDIS_GIT_SHA1 \"$GIT_SHA1\"" > myrelease.h
echo "#define REDIS_GIT_DIRTY \"$GIT_DIRTY\"" >> myrelease.h
echo "#define REDIS_BUILD_ID \"$BUILD_ID\"" >> myrelease.h

