#!/usr/bin/python
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# This program takes as arguments a list of submodules and a submodule
# dependency file then prints the submodules in topological order.

from os.path import join, dirname, relpath, realpath
from dependency_parser import get_dependencies
import argparse
import re

SCRIPT_DIR=dirname(realpath(__file__))
ROOT_DIR=join(SCRIPT_DIR, "..")
GITMODULES_DEPS_FILEPATH=relpath(join(ROOT_DIR, ".gitmodules.deps"))
GITMODULES_FILEPATH=join(ROOT_DIR, ".gitmodules")


def get_all_submodules():
    submodules = []
    with open(GITMODULES_FILEPATH) as gitmodules_hdl:
        for line in gitmodules_hdl:
            match = re.match('\[submodule "(?P<submodule>.*)"\]', line)
            if not match:
                continue

            submodule = match.group("submodule")
            submodules.append(submodule)

    return submodules

def generate_submodules_depends(submodules, dependency_file):
    submodule_depends = []
    for submodule in submodules:
        depends = get_dependencies(dependency_file, submodule)
        entry = (submodule, depends)
        submodule_depends.append(entry)

    return submodule_depends


def sort_submodules(submodule_depends):
    sorted_depends = []
    unsorted_depends = dict(submodule_depends)

    while unsorted_depends:
        acyclic = False
        for node, edges in unsorted_depends.items():
            for edge in edges:
                if edge in unsorted_depends:
                    break
            else:
                acyclic = True
                del unsorted_depends[node]
                sorted_depends.append((node, edges))

        assert acyclic

    return [submodule[0] for submodule in sorted_depends]


def main():
    ##
    # Command line argument specification
    ##
    desc= """Submodule Topological Sort.  Submodules are written to stdout seperated by spaces."""
    parser = argparse.ArgumentParser(description=desc)

    # User can provide an alternate dependency file.  Default is the .gitmodules.dep at the root of the repo.
    parser.add_argument('-f', '--dependency-file', dest='dependency_file',
                        type=argparse.FileType('r'), default=GITMODULES_DEPS_FILEPATH,
                        help='Name of the file with dependencies in DAG format (default: %(default)s)')

    # User can provide a list of submodules.  Default is all of the submodules.
    parser.add_argument('-s', '--submodule_list', dest='submodule_list', default=get_all_submodules(),
                        choices=get_all_submodules(), type=str, nargs='+',
                        help='Names of the submodule to sort (default: %(default)s')

    parsed_args = parser.parse_args()

    submodule_depends = generate_submodules_depends(parsed_args.submodule_list, parsed_args.dependency_file)
    sorted_submodules = sort_submodules(submodule_depends)

    # print the sorted list of submodules seperated by spaces.
    print " ".join(sorted_submodules)


if __name__ == "__main__":
    main()
