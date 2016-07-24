#!/usr/bin/env python
# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008-2010 Johan Dahlin
# Copyright (C) 2009 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import errno
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

from giscanner import message
from giscanner.annotationparser import AnnotationParser
from giscanner.ast import Include, Namespace
from giscanner.dumper import compile_introspection_binary
from giscanner.gdumpparser import GDumpParser, IntrospectionBinary
from giscanner.introspectablepass import IntrospectablePass
from giscanner.girparser import GIRParser
from giscanner.girwriter import GIRWriter
from giscanner.maintransformer import MainTransformer
from giscanner.shlibs import resolve_shlibs
from giscanner.sourcescanner import SourceScanner
from giscanner.transformer import Transformer
from . import utils

def get_preprocessor_option_group(parser):
    group = optparse.OptionGroup(parser, "Preprocessor options")
    group.add_option("-I", help="Pre-processor include file",
                     action="append", dest="cpp_includes",
                     default=[])
    group.add_option("-D", help="Pre-processor define",
                     action="append", dest="cpp_defines",
                     default=[])
    group.add_option("-U", help="Pre-processor undefine",
                     action="append", dest="cpp_undefines",
                     default=[])
    group.add_option("-p", dest="", help="Ignored")
    return group

def get_windows_option_group(parser):
    group = optparse.OptionGroup(parser, "Machine Dependent Options")
    group.add_option("-m", help="some machine dependent option",
                     action="append", dest='m_option',
                     default=[])

    return group

def _get_option_parser():
    parser = optparse.OptionParser('%prog [options] sources')
    parser.add_option('', "--quiet",
                      action="store_true", dest="quiet",
                      default=False,
                      help="If passed, do not print details of normal" \
                          + " operation")
    parser.add_option("", "--format",
                      action="store", dest="format",
                      default="gir",
                      help="format to use, one of gidl, gir")
    parser.add_option("-i", "--include",
                      action="append", dest="includes", default=[],
                      help="Add specified gir file as dependency")
    parser.add_option("", "--include-uninstalled",
                      action="append", dest="includes_uninstalled", default=[],
                      help=("""A file path to a dependency; only use this "
                            "when building multiple .gir files inside a "
                            "single module."""))
    parser.add_option("", "--add-include-path",
                      action="append", dest="include_paths", default=[],
                      help="include paths for other GIR files")
    parser.add_option("", "--program",
                      action="store", dest="program", default=None,
                      help="program to execute")
    parser.add_option("", "--program-arg",
                      action="append", dest="program_args", default=[],
                      help="extra arguments to program")
    parser.add_option("", "--libtool",
                      action="store", dest="libtool_path", default=None,
                      help="full path to libtool")
    parser.add_option("", "--no-libtool",
                      action="store_true", dest="nolibtool", default=False,
                      help="do not use libtool")
    parser.add_option("", "--external-library",
                      action="store_true", dest="external_library", default=False,
                      help=("""If true, the library is located on the system,""" +
                            """not in the current directory"""))
    parser.add_option("-l", "--library",
                      action="append", dest="libraries", default=[],
                      help="libraries of this unit")
    parser.add_option("-L", "--library-path",
                      action="append", dest="library_paths", default=[],
                      help="directories to search for libraries")
    parser.add_option("", "--header-only",
                      action="store_true", dest="header_only", default=[],
                      help="If specified, just generate a GIR for the given header files")
    parser.add_option("-n", "--namespace",
                      action="store", dest="namespace_name",
                      help=("name of namespace for this unit, also "
                            "used to compute --identifier-prefix and --symbol-prefix"))
    parser.add_option("", "--nsversion",
                      action="store", dest="namespace_version",
                      help="version of namespace for this unit")
    parser.add_option("", "--strip-prefix",
                      action="store", dest="strip_prefix",
                      help="""Option --strip-prefix is deprecated, please see --identifier-prefix
and --symbol-prefix.""")
    parser.add_option("", "--identifier-prefix",
                      action="append", dest="identifier_prefixes", default=[],
                      help="""Remove this prefix from C identifiers (structure typedefs, etc.).
May be specified multiple times.  This is also used as the default for --symbol-prefix if
the latter is not specified.""")
    parser.add_option("", "--symbol-prefix",
                      action="append", dest="symbol_prefixes", default=[],
                      help="Remove this prefix from C symbols (function names)")
    parser.add_option("", "--accept-unprefixed",
                      action="store_true", dest="accept_unprefixed", default=False,
                      help="""If specified, accept symbols and identifiers that do not
match the namespace prefix.""")
    parser.add_option("", "--add-init-section",
                      action="append", dest="init_sections", default=[],
            help="add extra initialization code in the introspection program")
    parser.add_option("-o", "--output",
                      action="store", dest="output", default="-",
                      help="output filename to write to, defaults to - (stdout)")
    parser.add_option("", "--pkg",
                      action="append", dest="packages", default=[],
                      help="pkg-config packages to get cflags from")
    parser.add_option("", "--pkg-export",
                      action="append", dest="packages_export", default=[],
                      help="Associated pkg-config packages for this library")
    parser.add_option('', "--warn-all",
                      action="store_true", dest="warn_all", default=False,
                      help="If true, enable all warnings for introspection")
    parser.add_option('', "--warn-error",
                      action="store_true", dest="warn_fatal",
                      help="Turn warnings into fatal errors")
    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose",
                      help="be verbose")
    parser.add_option("", "--c-include",
                      action="append", dest="c_includes", default=[],
                      help="headers which should be included in C programs")

    group = get_preprocessor_option_group(parser)
    parser.add_option_group(group)

    if os.environ.get('MSYSTEM') == 'MINGW32':
        group = get_windows_option_group(parser)
        parser.add_option_group(group)

    # Private options
    parser.add_option('', "--generate-typelib-tests",
                      action="store", dest="test_codegen", default=None,
                      help=optparse.SUPPRESS_HELP)
    parser.add_option('', "--passthrough-gir",
                      action="store", dest="passthrough_gir", default=None,
                      help=optparse.SUPPRESS_HELP)
    parser.add_option('', "--reparse-validate",
                      action="store_true", dest="reparse_validate_gir", default=False,
                      help=optparse.SUPPRESS_HELP)
    parser.add_option("", "--typelib-xml",
                      action="store_true", dest="typelib_xml",
                      help=optparse.SUPPRESS_HELP)

    return parser


def _error(msg):
    raise SystemExit('ERROR: %s' % (msg, ))

def passthrough_gir(path, f):
    parser = GIRParser()
    parser.parse(path)

    writer = GIRWriter(parser.get_namespace(),
                       parser.get_shared_libraries(),
                       parser.get_includes(),
                       parser.get_pkgconfig_packages(),
                       parser.get_c_includes())
    f.write(writer.get_xml())

def test_codegen(optstring):
    (namespace, out_h_filename, out_c_filename) = optstring.split(',')
    if namespace == 'Everything':
        from .testcodegen import EverythingCodeGenerator
        gen = EverythingCodeGenerator(out_h_filename, out_c_filename)
        gen.write()
    else:
        _error("Invaild namespace %r" % (namespace, ))
    return 0

def process_options(output, allowed_flags):
    for option in output.split():
        for flag in allowed_flags:
            if not option.startswith(flag):
                continue
            yield option
            break

def process_packages(options, packages):
    args = ['pkg-config', '--cflags']
    args.extend(packages)
    output = subprocess.Popen(args,
                              stdout=subprocess.PIPE).communicate()[0]
    if output is None:
        # the error output should have already appeared on our stderr,
        # so we just exit
        return 1
    # Some pkg-config files on Windows have options we don't understand,
    # so we explicitly filter to only the ones we need.
    options_whitelist = ['-I', '-D', '-U', '-l', '-L']
    filtered_output = list(process_options(output, options_whitelist))
    parser = _get_option_parser()
    pkg_options, unused = parser.parse_args(filtered_output)
    options.cpp_includes.extend(pkg_options.cpp_includes)
    options.cpp_defines.extend(pkg_options.cpp_defines)
    options.cpp_undefines.extend(pkg_options.cpp_undefines)

def extract_filenames(args):
    filenames = []
    for arg in args:
        # We don't support real C++ parsing yet, but we should be able
        # to understand C API implemented in C++ files.
        if (arg.endswith('.c') or arg.endswith('.cpp') or
            arg.endswith('.cc') or arg.endswith('.cxx') or
            arg.endswith('.h') or arg.endswith('.hpp') or
            arg.endswith('.hxx')):
            if not os.path.exists(arg):
                _error('%s: no such a file or directory' % (arg, ))
            # Make absolute, because we do comparisons inside scannerparser.c
            # against the absolute path that cpp will give us
            filenames.append(os.path.abspath(arg))
    return filenames

def create_namespace(options):
    if options.strip_prefix:
        print """g-ir-scanner: warning: Option --strip-prefix has been deprecated;
see --identifier-prefix and --symbol-prefix."""
        options.identifier_prefixes.append(options.strip_prefix)

    # We do this dance because the empty list has different semantics from
    # None; if the user didn't specify the options, we want to use None so
    # the Namespace constructor picks the defaults.
    if options.identifier_prefixes:
        identifier_prefixes = options.identifier_prefixes
    else:
        identifier_prefixes = None
    if options.symbol_prefixes:
        for prefix in options.symbol_prefixes:
            # See Transformer._split_c_string_for_namespace_matches() for
            # why this check is needed
            if prefix.lower() != prefix:
                _error("Values for --symbol-prefix must be entirely lowercase")
        symbol_prefixes = options.symbol_prefixes
    else:
        symbol_prefixes = None

    return Namespace(options.namespace_name,
                     options.namespace_version,
                     identifier_prefixes=identifier_prefixes,
                     symbol_prefixes=symbol_prefixes)

def create_transformer(namespace, options):
    transformer = Transformer(namespace,
                              accept_unprefixed=options.accept_unprefixed)
    transformer.set_include_paths(options.include_paths)
    if options.passthrough_gir:
        transformer.disable_cache()
        transformer.set_passthrough_mode()

    shown_include_warning = False
    for include in options.includes:
        if os.sep in include:
            _error("Invalid include path %r" % (include, ))
        try:
            include_obj = Include.from_string(include)
        except:
            _error("Malformed include %r\n" % (include, ))
        transformer.register_include(include_obj)
    for include_path in options.includes_uninstalled:
        transformer.register_include_uninstalled(include_path)

    return transformer

def create_binary(transformer, options, args):
    # Transform the C AST nodes into higher level
    # GLib/GObject nodes
    gdump_parser = GDumpParser(transformer)

    # Do enough parsing that we have the get_type() functions to reference
    # when creating the introspection binary
    gdump_parser.init_parse()

    if options.program:
        args=[options.program]
        args.extend(options.program_args)
        binary = IntrospectionBinary(args)
    else:
        binary = compile_introspection_binary(options,
                                              gdump_parser.get_get_type_functions(),
                                              gdump_parser.get_error_quark_functions())

    shlibs = resolve_shlibs(options, binary, options.libraries)
    gdump_parser.set_introspection_binary(binary)
    gdump_parser.parse()
    return shlibs

def create_source_scanner(options, args):
    filenames = extract_filenames(args)

    # Run the preprocessor, tokenize and construct simple
    # objects representing the raw C symbols
    ss = SourceScanner()
    ss.set_cpp_options(options.cpp_includes,
                       options.cpp_defines,
                       options.cpp_undefines)
    ss.parse_files(filenames)
    ss.parse_macros(filenames)
    return ss

def write_output(data, options):
    if options.output == "-":
        output = sys.stdout
    elif options.reparse_validate_gir:
        main_f, main_f_name = tempfile.mkstemp(suffix='.gir')
        main_f = os.fdopen(main_f, 'w')
        main_f.write(data)
        main_f.close()

        temp_f, temp_f_name = tempfile.mkstemp(suffix='.gir')
        temp_f = os.fdopen(temp_f, 'w')
        passthrough_gir(main_f_name, temp_f)
        temp_f.close()
        if not utils.files_are_identical(main_f_name, temp_f_name):
            _error("Failed to re-parse gir file; scanned=%r passthrough=%r" % (
                main_f_name, temp_f_name))
        os.unlink(temp_f_name)
        try:
            shutil.move(main_f_name, options.output)
        except OSError, e:
            if e.errno == errno.EPERM:
                os.unlink(main_f_name)
                return 0
            raise
        return 0
    else:
        try:
            output = open(options.output, "w")
        except IOError, e:
            _error("opening output for writing: %s" % (e.strerror, ))

    try:
        output.write(data)
    except IOError, e:
        _error("while writing output: %s" % (e.strerror, ))

def scanner_main(args):
    parser = _get_option_parser()
    (options, args) = parser.parse_args(args)

    if options.passthrough_gir:
        passthrough_gir(options.passthrough_gir, sys.stdout)
    if options.test_codegen:
        return test_codegen(options.test_codegen)

    if len(args) <= 1:
        _error('Need at least one filename')

    if not options.namespace_name:
        _error('Namespace name missing')

    if options.format == 'gir':
        from giscanner.girwriter import GIRWriter as Writer
    else:
        _error("Unknown format: %s" % (options.format, ))

    if not (options.libraries
            or options.program
            or options.header_only):
        _error("Must specify --program or --library")

    namespace = create_namespace(options)
    logger = message.MessageLogger.get(namespace=namespace)
    if options.warn_all:
        logger.enable_warnings(True)
    transformer = create_transformer(namespace, options)

    packages = set(options.packages)
    packages.update(transformer.get_pkgconfig_packages())
    if packages:
        exit_code = process_packages(options, packages)
        if exit_code:
            return exit_code

    ss = create_source_scanner(options, args)

    ap = AnnotationParser()
    blocks = ap.parse(ss.get_comments())

    # Transform the C symbols into AST nodes
    transformer.set_annotations(blocks)
    transformer.parse(ss.get_symbols())

    if not options.header_only:
        shlibs = create_binary(transformer, options, args)
    else:
        shlibs = []

    main = MainTransformer(transformer, blocks)
    main.transform()

    utils.break_on_debug_flag('tree')

    final = IntrospectablePass(transformer, blocks)
    final.validate()

    warning_count = logger.get_warning_count()
    if options.warn_fatal and warning_count > 0:
        message.fatal("warnings configured as fatal")
        return 1
    elif warning_count > 0 and options.warn_all is False:
        print ("g-ir-scanner: %s: warning: %d warnings suppressed (use --warn-all to see them)"
               % (transformer.namespace.name, warning_count, ))

    # Write out AST
    if options.packages_export:
        exported_packages = options.packages_export
    else:
        exported_packages = options.packages

    writer = Writer(transformer.namespace, shlibs, transformer.get_includes(),
                    exported_packages, options.c_includes)
    data = writer.get_xml()

    write_output(data, options)

    return 0
