#!/bin/bash

echo "1a) checking pip six"
cmd "pip list | grep -i six"
echo "1b) checking pip3 six"
cmd "python3-pip list | grep -i six"

echo "upgrading pip six"
cmd "pip install --upgrade six"

echo "2a) checking pip six"
cmd "pip list | grep -i six"
echo "2b) checking pip3 six"
cmd "python3-pip list | grep -i six"
