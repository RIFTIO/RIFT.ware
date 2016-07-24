#!/bin/sh

# remove .src from all the yang files

# show the commands we will run
for f in *.yang.src; do echo mv "$f" "${f/.src/}"; done

# then run them
for f in *.yang.src; do mv "$f" "${f/.src/}"; done
