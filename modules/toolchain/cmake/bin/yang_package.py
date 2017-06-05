#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

import sys
import re
import argparse

def to_camelcase(s):
    temp = re.sub(r'(?!^)[-_]([0-9a-zA-Z])', lambda m: m.group(1).upper(), s)
    return temp[0].upper() + temp[1:]       


if __name__ == '__main__':
    # Parse the command line options
    desc = ("""Extract the package name from a yang file.
            If a package statement is present in the yang file
            it's converted to camel case for package name, otherwise
            package name is inferred from the module name. If
            the module name is not present an error is raised""")
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument('yang_file',
                        metavar='yang-file',
                        help='input yang filename')
    args = parser.parse_args()

    f = open(args.yang_file, 'r')
    module_name = None
    package_name = None
    for line in f:
        m = re.match(r'\s*module\s+([0-9a-zA-Z_-]*)\s*.*', line)
        if m:
            if not module_name:
                module_name = m.group(1).strip()

        m = re.match(r'.*:package-name\s+"(.*)";', line)
        if m:
            package_name = m.group(1)
             
    if not module_name:
        print "Error: module_name not found"
        sys.exit(1)

    if package_name:
        sys.stdout.write(to_camelcase(package_name).strip())
    else:
        sys.stdout.write(to_camelcase(module_name).strip())

