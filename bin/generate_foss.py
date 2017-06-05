#!/usr/bin/env python3
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file generate_foss.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 04/09/2015
# @brief Foss Documenation Generation
# @details This script is used to combine the installed submodule foss.txt
# files into a single document.
"""

import os
import sys
import argparse
import logging

FOSS_FILENAME = "foss.txt"

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


def find_filename_within_directory(directory, find_filename):
    """ Return a list of file paths of filenames that live within directory

    Arguments:
        directory - Directory path which contains .yang files
        file_name - file name to search for
    """
    filepaths = []
    for dirpath, dirnames, filenames in os.walk(directory):
        for filename in filenames:
            if filename == find_filename:
                filepath = os.path.join(dirpath, filename)
                filepaths.append(filepath)

    return filepaths


class InvalidFossEntry(Exception):
    pass

class FossEntry(object):
    def __init__(self, component, name, description, license, url):
        self.component = component
        self.name = name
        self.description = description
        self.license = license
        self.url = url

    @classmethod
    def from_foss_line(cls, line):

        fields = line.split(",")
        strip_fields = [field.strip() for field in fields]
        if len(strip_fields) != 5:
            raise InvalidFossEntry("Expecting 5 fields in entry: {}".format(strip_fields))

        return cls(*strip_fields)


def write_header(file_hdl):
    header_html = """
<html>
    <head>
        <style>
          .demo {
            border:1px solid #C0C0C0;
            border-collapse:collapse;
            padding:5px;
          }
          .demo th {
            border:1px solid #C0C0C0;
            padding:5px;
            background:#F0F0F0;
          }
          .demo td {
            border:1px solid #C0C0C0;
            padding:5px;
          }
          .demo caption {
            font-size: 26px;
            color: #000000;
            padding:5px;
            padding-top: 25px;
          }
        </style>
    </head>
    <body>
"""
    file_hdl.write(header_html)


def write_entry_table(file_hdl, foss_entries):
    table_hdr = """
        <table class="demo">
          <caption>RIFT IO FOSS Usage</caption>
          <thead>
          <tr>
            <th>Component</th>
            <th>SourceDir</th>
            <th>Description</th>
            <th>License</th>
            <th>URL</th>
          </tr>
          </thead>
          <tbody>
"""
    file_hdl.write(table_hdr)

    for entry in foss_entries:
        row_template = """
          <tr>
            <td>{}</td>
            <td>{}</td>
            <td>{}</td>
            <td>{}</td>
            <td>{}</td>
          </tr>
"""
        row = row_template.format(entry.component, entry.name, entry.description, entry.license, entry.url)
        file_hdl.write(row)

    table_ftr = """
    </tbody>
    </table>
"""

    file_hdl.write(table_ftr)

def write_license_total_table(file_hdl, license_totals):
    table_hdr = """
        <table class="demo">
          <caption>FOSS License Totals</caption>
          <thead>
          <tr>
            <th>License</th>
            <th>Total</th>
          </tr>
          </thead>
          <tbody>
"""
    file_hdl.write(table_hdr)

    for license in sorted( license_totals, key=license_totals.get, reverse=True ):
        row_template = """
          <tr>
            <td>{}</td>
            <td>{}</td>
          </tr>
"""
        row = row_template.format( license, license_totals[license] )
        file_hdl.write(row)

    table_ftr = """
    </tbody>
    </table>
"""

    file_hdl.write(table_ftr)


def write_unrecognized_foss_lines(file_hdl, unrecognized_lines):
    file_hdl.write("<br><h5> Unrecognized lines from foss.txt files </h5>\n")
    file_hdl.write("<hr>\n")
    for line in unrecognized_lines:
        file_hdl.write("<pre>{}</pre><br>\n".format(line))
    file_hdl.write("<br>\n")

def write_footer(file_hdl):
    footer_html = """
    </body>
</html>
"""
    file_hdl.write(footer_html)

def create_foss_html_from_entries(file_hdl, foss_entries, invalid_lines, license_totals):
    write_header(file_hdl)
    write_entry_table(file_hdl, foss_entries)
    write_license_total_table(file_hdl, license_totals)
    write_unrecognized_foss_lines(file_hdl, invalid_lines)
    #write_license_totals(file_hdl, license_totals)
    write_footer(file_hdl)

def create_foss_entries(foss_filepaths):
    foss_lines = []
    for path in foss_filepaths:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path) as hdl:
            foss_lines.extend(hdl.readlines())

    invalid_lines = []
    foss_entries = []
    license_totals = {}

    for line in foss_lines:

        # allow blank lines
        if line.strip() == "":
            continue

        # allow # comments also on a line
        if line[0:1] == "#":
            continue

        try:
            entry = FossEntry.from_foss_line(line)
            foss_entries.append(entry)

            if license_totals.get(entry.license) == None:
                license_totals[entry.license]=0
            license_totals[entry.license] += 1

        except InvalidFossEntry as e:
            logging.warning("Could not create Foss Entry from line: %s", str(e))
            invalid_lines.append(line)

    return foss_entries, invalid_lines, license_totals

def parse_args(argv=sys.argv[1:]):
    """ Parse the command line arguments

    Arguments:
        argv - The list of command line arguments (default: sys.argv)
    """
    parser = argparse.ArgumentParser()

    parser.add_argument('--output-file',
                        type=argparse.FileType(mode="w"),
                        help='generated composite yang output file')

    parser.add_argument('--foss-dir',
                        required=True,
                        type=parse_directory,
                        help='directory to scan for foss.txt files')

    parser.add_argument('--foss-files',
                        nargs="+",
                        type=argparse.FileType('r'),
                        help='extra foss text file paths')

    args = parser.parse_args(args=argv)

    return args


def main():
    args = parse_args()
    file_hdl = args.output_file
    # If the --output-file argument wasn't provided, use stdout
    if file_hdl is None:
        file_hdl = sys.stdout

    try:
        foss_paths = find_filename_within_directory(args.foss_dir, FOSS_FILENAME)
        if args.foss_files:
            foss_file_names = [foss_hdl.name for foss_hdl in args.foss_files]
            foss_paths.extend(foss_file_names)

        foss_entries, invalid_lines, license_totals = create_foss_entries(foss_paths)

        foss_entries.sort(key=lambda x: x.name)
        create_foss_html_from_entries(file_hdl, foss_entries, invalid_lines, license_totals)

    finally:
        file_hdl.flush()
        file_hdl.close()


if __name__ == "__main__":
    main()
