#!/usr/bin/env python
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file generate_composite_yang.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 04/06/2015
# @brief Composite Yang Generation
# @details This script is responsible for generating the composite yang file
# from a directory that contains a bunch of existin yang files.
"""

import os
import sys
import argparse

COMPOSITE_MODULE_NAME = "rw-composite"

def parse_directory(directory):
    """ Validate the parameter is a valid directory

    Arguments:
        directory - a directory path

    Raises:
        argparse.ArgumentTypeError if argument is not a directory
    """
    if not os.path.isdir(directory):
        raise argparse.ArgumentTypeError("{} is not a valid directory".format(directory))

    return directory


def get_yang_filenames_from_directory(directory):
    """ Get yang filenames (without path) from a directory

    Arguments:
        directory - Directory path which contains .yang files
    """
    yang_filenames = []
    for entry in os.listdir(directory):
        if entry.endswith(".yang"):
            yang_filenames.append(entry)

    return yang_filenames


def write_import_lines_from_yang_filenames(file_hdl, yang_filenames):
    """ Write yang import statements from a list of yang file names

    Arguments:
        file_hdl - The file to write the import lines to.
        yang_filenames - The list of yang filenames to write import lines for.
    """
    yang_names = []
    for filename in yang_filenames:
        filename_no_ext = filename.replace(".yang", "")

        # Ensure that the composite doesn't end up including itself for some reason.
        if filename_no_ext == COMPOSITE_MODULE_NAME:
            continue

        # yuma_* yang files should not be imported.
        if filename_no_ext.startswith("yuma"):
            continue

        if filename_no_ext.startswith("yangdump"):
            continue

        yang_names.append(filename_no_ext)

    for yang_name in sorted(yang_names):
        import_line = "  import {name} {{ prefix {name}; }}\n".format(name=yang_name)
        file_hdl.write(import_line)


def write_header(file_hdl):
    header = \
"""/* THIS FILE IS GENERATED, DO NOT MODIFY. */

/**
 * @brief A compsite YANG file that imports all other modules loaded by
 * rwuagent. This YANG file gives the uagent a global schema view
 */

module {module_name}
{{

  namespace "http://riftio.com/ns/riftware-1.0/{module_name}";
  prefix "rwcomp";

"""

    file_hdl.write(header.format(module_name=COMPOSITE_MODULE_NAME))


def write_footer(file_hdl):
    # Write the final brace and newline
    file_hdl.write("}\n")


def parse_args(argv=sys.argv[1:]):
    """ Parse the command line arguments

    Arguments:
        argv - The list of command line arguments (default: sys.argv)
    """
    parser = argparse.ArgumentParser()

    parser.add_argument('--output-file',
                        type=argparse.FileType(mode="w"),
                        help='generated composite yang output file')

    parser.add_argument('--yang-dir',
                        required=True,
                        type=parse_directory,
                        help='directory to scan for yang files')

    args = parser.parse_args(args=argv)

    return args


def main():
    args = parse_args()
    file_hdl = args.output_file
    # If the --output-file argument wasn't provided, use stdout
    if file_hdl is None:
        file_hdl = sys.stdout

    try:
        write_header(file_hdl)
        yang_filenames = get_yang_filenames_from_directory(args.yang_dir)
        write_import_lines_from_yang_filenames(file_hdl, yang_filenames)
        write_footer(file_hdl)

    finally:
        file_hdl.flush()
        file_hdl.close()


if __name__ == "__main__":
    main()
