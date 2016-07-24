#!/bin/sh

# append .src to all the yang files

# show the commands we will run
for f in *.yang; do echo mv "$f" "$f.src"; done

# then do it
for f in *.yang; do mv "$f" "$f.src"; done
