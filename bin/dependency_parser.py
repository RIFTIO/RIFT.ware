#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

# This program parses the depndency digraph to get the 
# list of dependencies recursively
#
# This is used by cmake to get the submodule dependencies
# CMAKE has terrible support for recirsive functions, 
# and hence this is written in python

import sys
import re
import os
from hashlib import sha1
import argparse
import shlex
import subprocess
import StringIO

THIS_DIR = os.path.dirname(os.path.realpath(__file__))

STATIC_DEPENDENCIES = ['modules/toolchain']

def get_dependencies(dep_file, submodule):
    """Parse the dependency file and determine all the dependencies
    recursively.
    """
    deps = []
    # Save the file position so we can restore before returning
    dep_file.seek(0)

    for line in dep_file.readlines():
        line = line.strip()
        regex = r'"' + re.escape(submodule) + '"[ ]*->[ ]*"(.*)"'
        m = re.match(regex, line)
        if not m:
            continue

        if m.group(1) not in deps:
            deps.append(m.group(1))

        temp = get_dependencies(dep_file, m.group(1))
        for i in temp:
            if i not in deps:
                deps.append(i)

    # Add the static dependencies to the list of submodule dependencies
    deps.extend(STATIC_DEPENDENCIES)
    if submodule in deps:
        deps.remove(submodule)

    # Restore the file position to the same spot before this function was called
    return deps

def reverse_dependency_file(dep_file):
    """ Create a in-memory reversed dependency file

    For all lines that match the "sub_a" -> "sub_b" regex,
    reverse the dependency such that "sub_b" -> "sub_a".  This allows
    us to reverse the dependency search to get dependents.
    """

    reversed_file = StringIO.StringIO()
    for line in dep_file:
        line = line.strip()
        if line.startswith("//"):
            continue

        regex = r'"(.*)"[ ]*->[ ]*"(.*)"'
        m = re.match(regex, line)
        if m is None:
            reversed_file.write(line + "\n")
            continue

        reversed_line = '"{}" -> "{}"\n'.format(m.group(2), m.group(1))
        reversed_file.write(reversed_line)

    reversed_file.flush()

    reversed_file.seek(0)

    return reversed_file


def submodule_hash(filename,  submodule, outdir):
    """This function calculates the submodule hash to be
    be used when caching the aritifacts. The submodule hash
    depends on the hash of the submodule, as well as on the
    hashes of all the submodules that the current submodule
    depends on. For example if a submodule B depends on A,
    the hash for submodule B, should include the hashes
    of both submodules A and B. In otherwords, if submodule
    A changes, the submodule B needs recompilation eventhough
    the contents of submodule B didn't change.
    """
    deps = get_dependencies(filename, submodule)
    out_fname = outdir + "/" + submodule.replace("/", "_").replace("modules_", "") + ".hash"
    out_f = open(out_fname, "w")

    mylist = deps
    mylist.append(submodule)
    for i in mylist:
        cmd = "git submodule status " + i
        args = shlex.split(cmd) 
        p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = p.communicate()
        out_f.write(out.split()[0].lstrip(" +-")  + " " + out.split()[1] + "\n")

    out_f.close()
    filesize_bytes = os.path.getsize(out_f.name)
    s = sha1()
    s.update(("blob %u\0" % filesize_bytes).encode('utf-8'))

    with open(out_fname, 'rb') as f:
        s.update(f.read())

    return s.hexdigest()

def get_output_dir():
    build = os.environ["RIFT_BUILD"]
    if os.path.exists(build):
        return build

    return "/tmp"


def main():
    ##
    # Command line argument specification
    ##
    desc= """This script helps in getting the submodule dependency information"""
    parser = argparse.ArgumentParser(description=desc)

    parser.add_argument('-o', '--output-dir',
            default=get_output_dir(),
            help='Directory for the output files (default: %(default)s)'
            )

    parser.add_argument('-f', '--dependency-file',
            type=argparse.FileType('r'),
            required=True,
            default='.gitmodules.deps',
            help='Name of the file with dependencies in DAG format (default: %(default)s)'
            )

    parser.add_argument('-s', '--submodule',
            default='modules/core/util',
            help='Name of the submodule (default: %(default)s'
            )

    parser.add_argument('-d', '--print-dependency',
            action='store_true',
            help='Print the dependency information for the submodule'
            )

    parser.add_argument('--print-dependents',
            action='store_true',
            help='Print the dependent information for the submodule'
            )

    parser.add_argument('-x', '--print-hash',
            dest='print_hash',
            action='store_true',
            help='Print the combined hash for the submodule and its dependencies'
            )

    cmdargs = parser.parse_args()

    if cmdargs.print_dependency:
        deps = get_dependencies(cmdargs.dependency_file, cmdargs.submodule)
        # output as a list for cmake
        for i in deps:
            sys.stdout.write(i+";")

    elif cmdargs.print_dependents:
        # In order to get the dependents (instead of dependencies), reverse
        # the dependency file (a -> b becomes b -> a) and reuse the
        # get_dependencies() function.
        reverse_dep_file = reverse_dependency_file(cmdargs.dependency_file)
        deps = get_dependencies(reverse_dep_file, cmdargs.submodule)
        # output as a list for cmake
        for i in deps:
            sys.stdout.write(i+";")

    if cmdargs.print_hash:
        h=submodule_hash(cmdargs.dependency_file, cmdargs.submodule, cmdargs.output_dir)
        sys.stdout.write(h)


if __name__ == "__main__":
    main()

