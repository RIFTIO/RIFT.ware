#!/bin/bash


git xinit -s modules/tools/CI
exec modules/tools/CI/jenkins/run_steps_and_archive.sh "$@"
