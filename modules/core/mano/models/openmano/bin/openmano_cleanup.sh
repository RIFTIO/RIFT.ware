#!/bin/bash
# Run this on openmano VM to clean up all instances, scenarios and vnfs.

./openmano instance-scenario-list | cut -d " " -f1 | while read line; do
./openmano instance-scenario-delete $line -f
done

./openmano scenario-list | cut -d " " -f1 | while read line; do
./openmano scenario-delete $line -f
done

./openmano vnf-list | cut -d " " -f1 | while read line; do
./openmano vnf-delete $line -f
done
