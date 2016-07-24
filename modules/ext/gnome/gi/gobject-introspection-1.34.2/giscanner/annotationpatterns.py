# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2012 Dieter Verfaillie <dieterv@optionexplicit.be>
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


'''
This module provides regular expression programs that can be used to identify
and extract useful information from different parts of GTK-Doc comment blocks.
These programs are built to:
 - match (or substitute) a single comment block line at a time;
 - support MULTILINE mode and should support (but remains untested)
   LOCALE and UNICODE modes.
'''


import re


# Program matching the start of a comment block.
#
# Results in 0 symbolic groups.
COMMENT_START_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    /                                        # 1 forward slash character
    \*{2}                                    # exactly 2 asterisk characters
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching the end of a comment block.
#
# Results in 0 symbolic groups.
COMMENT_END_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    \*+                                      # 1 or more asterisk characters
    /                                        # 1 forward slash character
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching the "*" at the beginning of every
# line inside a comment block.
#
# Results in 0 symbolic groups.
COMMENT_ASTERISK_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    \*                                       # 1 asterisk character
    [^\S\n\r]?                               # 0 or 1 whitespace characters
                                             #   Carefull: removing more would
                                             #   break embedded example program
                                             #   indentation
    ''',
    re.VERBOSE)

# Program matching an empty line.
#
# Results in 0 symbolic groups.
EMPTY_LINE_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or 1 whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching SECTION identifiers.
#
# Results in 2 symbolic groups:
#   - group 1 = colon
#   - group 2 = section_name
SECTION_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    SECTION                                  # SECTION
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<colon>:?)                            # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<section_name>\w\S+)?                 # section name
    [^\S\n\r]*                               # 0 or more whitespace characters
    $
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching symbol (function, constant, struct and enum) identifiers.
#
# Results in 3 symbolic groups:
#   - group 1 = symbol_name
#   - group 2 = colon
#   - group 3 = annotations
SYMBOL_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<symbol_name>[\w-]*\w)                # symbol name
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<colon>:?)                            # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<annotations>(?:\(.*?\)[^\S\n\r]*)*)  # annotations
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching property identifiers.
#
# Results in 4 symbolic groups:
#   - group 1 = class_name
#   - group 2 = property_name
#   - group 3 = colon
#   - group 4 = annotations
PROPERTY_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<class_name>[\w]+)                    # class name
    [^\S\n\r]*                               # 0 or more whitespace characters
    :{1}                                     # required colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<property_name>[\w-]*\w)              # property name
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<colon>:?)                            # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<annotations>(?:\(.*?\)[^\S\n\r]*)*)  # annotations
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching signal identifiers.
#
# Results in 4 symbolic groups:
#   - group 1 = class_name
#   - group 2 = signal_name
#   - group 3 = colon
#   - group 4 = annotations
SIGNAL_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<class_name>[\w]+)                    # class name
    [^\S\n\r]*                               # 0 or more whitespace characters
    :{2}                                     # 2 required colons
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<signal_name>[\w-]*\w)                # signal name
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<colon>:?)                            # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<annotations>(?:\(.*?\)[^\S\n\r]*)*)  # annotations
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching parameters.
#
# Results in 4 symbolic groups:
#   - group 1 = parameter_name
#   - group 2 = annotations
#   - group 3 = colon
#   - group 4 = description
PARAMETER_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    @                                        # @ character
    (?P<parameter_name>[\w-]*\w|\.\.\.)      # parameter name
    [^\S\n\r]*                               # 0 or more whitespace characters
    :{1}                                     # required colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<annotations>(?:\(.*?\)[^\S\n\r]*)*)  # annotations
    (?P<colon>:?)                            # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<description>.*?)                     # description
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching old style "Description:" tag.
#
# Results in 0 symbolic groups.
DESCRIPTION_TAG_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    Description:                             # 'Description:' literal
    ''',
    re.VERBOSE | re.MULTILINE)

# Program matching tags.
#
# Results in 4 symbolic groups:
#   - group 1 = tag_name
#   - group 2 = annotations
#   - group 3 = colon
#   - group 4 = description
TAG_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<tag_name>virtual|since|stability|
                 deprecated|returns|
                 return\ value|attributes|
                 rename\ to|type|
                 unref\ func|ref\ func|
                 set\ value\ func|
                 get\ value\ func|
                 transfer|value)             # tag name
    [^\S\n\r]*                               # 0 or more whitespace characters
    :{1}                                     # required colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<annotations>(?:\(.*?\)[^\S\n\r]*)*)  # annotations
    (?P<colon>:?)                            # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<description>.*?)                     # description
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE | re.IGNORECASE)

# Program matching multiline annotation continuations.
# This is used on multiline parameters and tags (but not on the first line) to
# generate warnings about invalid annotations spanning multiple lines.
#
# Results in 3 symbolic groups:
#   - group 2 = annotations
#   - group 3 = colon
#   - group 4 = description
MULTILINE_ANNOTATION_CONTINUATION_RE = re.compile(r'''
    ^                                        # start
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<annotations>(?:\(.*?\)[^\S\n\r]*)*)  # annotations
    (?P<colon>:)                             # colon
    [^\S\n\r]*                               # 0 or more whitespace characters
    (?P<description>.*?)                     # description
    [^\S\n\r]*                               # 0 or more whitespace characters
    $                                        # end
    ''',
    re.VERBOSE | re.MULTILINE)


if __name__ == '__main__':
    import unittest

    identifier_section_tests = [
        (SECTION_RE, 'TSIEOCN',
             None),
        (SECTION_RE, 'section',
             None),
        (SECTION_RE, 'section:',
             None),
        (SECTION_RE, 'section:test',
             None),
        (SECTION_RE, 'SECTION',
             {'colon': '',
              'section_name': None}),
        (SECTION_RE, 'SECTION  \t   ',
             {'colon': '',
              'section_name': None}),
        (SECTION_RE, '   \t  SECTION  \t   ',
             {'colon': '',
              'section_name': None}),
        (SECTION_RE, 'SECTION:   \t ',
             {'colon': ':',
              'section_name': None}),
        (SECTION_RE, 'SECTION   :   ',
             {'colon': ':',
              'section_name': None}),
        (SECTION_RE, '   SECTION : ',
             {'colon': ':',
              'section_name': None}),
        (SECTION_RE, 'SECTION:gtkwidget',
             {'colon': ':',
              'section_name': 'gtkwidget'}),
        (SECTION_RE, 'SECTION:gtkwidget  ',
             {'colon': ':',
              'section_name': 'gtkwidget'}),
        (SECTION_RE, '  SECTION:gtkwidget',
             {'colon': ':',
              'section_name': 'gtkwidget'}),
        (SECTION_RE, '  SECTION:gtkwidget\t  ',
             {'colon': ':',
              'section_name': 'gtkwidget'}),
        (SECTION_RE, 'SECTION:    gtkwidget   ',
             {'colon': ':',
              'section_name': 'gtkwidget'}),
        (SECTION_RE, 'SECTION   :  gtkwidget',
             {'colon': ':',
              'section_name': 'gtkwidget'}),
        (SECTION_RE, 'SECTION    gtkwidget \f  ',
             {'colon': '',
              'section_name': 'gtkwidget'})]

    identifier_symbol_tests = [
        (SYMBOL_RE, 'GBaseFinalizeFunc:',
             {'colon': ':',
              'symbol_name': 'GBaseFinalizeFunc',
              'annotations': ''}),
        (SYMBOL_RE, 'gtk_widget_show  ',
             {'colon': '',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, '  gtk_widget_show',
             {'colon': '',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, '  gtk_widget_show  ',
             {'colon': '',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, 'gtk_widget_show:',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, 'gtk_widget_show :',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, 'gtk_widget_show:  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, 'gtk_widget_show :  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, '  gtk_widget_show:',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, '  gtk_widget_show :',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, '  gtk_widget_show:  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, '  gtk_widget_show :  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': ''}),
        (SYMBOL_RE, 'gtk_widget_show (skip)',
             {'colon': '',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, 'gtk_widget_show: (skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, 'gtk_widget_show : (skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, 'gtk_widget_show:  (skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, 'gtk_widget_show :  (skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, '  gtk_widget_show:(skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, '  gtk_widget_show :(skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, '  gtk_widget_show:  (skip)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)'}),
        (SYMBOL_RE, '  gtk_widget_show :  (skip)    \t    ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)    \t    '}),
        (SYMBOL_RE, '  gtk_widget_show  :  (skip)   \t    ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)   \t    '}),
        (SYMBOL_RE, 'gtk_widget_show:(skip)(test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)(test1)'}),
        (SYMBOL_RE, 'gtk_widget_show (skip)(test1)',
             {'colon': '',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)(test1)'}),
        (SYMBOL_RE, 'gtk_widget_show: (skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, 'gtk_widget_show : (skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, 'gtk_widget_show:  (skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, 'gtk_widget_show :  (skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, '  gtk_widget_show:(skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, '  gtk_widget_show :(skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, '  gtk_widget_show:  (skip) (test1)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)'}),
        (SYMBOL_RE, '  gtk_widget_show :  (skip) (test1)  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1)  '}),
        (SYMBOL_RE, 'gtk_widget_show: (skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, 'gtk_widget_show : (skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, 'gtk_widget_show:  (skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, 'gtk_widget_show :  (skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, '  gtk_widget_show:(skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, '  gtk_widget_show :(skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, '  gtk_widget_show:  (skip) (test1) (test-2)',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)'}),
        (SYMBOL_RE, '  gtk_widget_show :  (skip) (test1) (test-2)  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip) (test1) (test-2)  '}),
        (SYMBOL_RE, '  gtk_widget_show  :  (skip)  (test1)  (test-2)  ',
             {'colon': ':',
              'symbol_name': 'gtk_widget_show',
              'annotations': '(skip)  (test1)  (test-2)  '}),
        # constants
        (SYMBOL_RE, 'MY_CONSTANT:',
             {'colon': ':',
              'symbol_name': 'MY_CONSTANT',
              'annotations': ''}),
        # structs
        (SYMBOL_RE, 'FooWidget:',
             {'colon': ':',
              'symbol_name': 'FooWidget',
              'annotations': ''}),
        # enums
        (SYMBOL_RE, 'Something:',
             {'colon': ':',
              'symbol_name': 'Something',
              'annotations': ''})]

    identifier_property_tests = [
        # simple property name
        (PROPERTY_RE, 'GtkWidget:name (skip)',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': '',
              'annotations': '(skip)'}),
        (PROPERTY_RE, 'GtkWidget:name',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, ' GtkWidget :name',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, 'GtkWidget: name ',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, '  GtkWidget  :  name  ',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, 'GtkWidget:name:',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'GtkWidget:name:  ',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, '  GtkWidget:name:',
             {'class_name': 'GtkWidget',
              'property_name': 'name',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'Something:name:',
             {'class_name': 'Something',
              'property_name': 'name',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'Something:name:  ',
             {'class_name': 'Something',
              'property_name': 'name',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, '  Something:name:',
             {'class_name': 'Something',
              'property_name': 'name',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'Weird-thing:name:',
             None),
        (PROPERTY_RE, 'really-weird_thing:name:',
             None),
        (PROPERTY_RE, 'GWin32InputStream:handle:',
             {'class_name': 'GWin32InputStream',
              'property_name': 'handle',
              'colon': ':',
              'annotations': ''}),
        # property name that contains a dash
        (PROPERTY_RE, 'GtkWidget:double-buffered (skip)',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': '',
              'annotations': '(skip)'}),
        (PROPERTY_RE, 'GtkWidget:double-buffered',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, ' GtkWidget :double-buffered',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, 'GtkWidget: double-buffered ',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, '  GtkWidget  :  double-buffered  ',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': '',
              'annotations': ''}),
        (PROPERTY_RE, 'GtkWidget:double-buffered:',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'GtkWidget:double-buffered:  ',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, '  GtkWidget:double-buffered:',
             {'class_name': 'GtkWidget',
              'property_name': 'double-buffered',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'Something:double-buffered:',
             {'class_name': 'Something',
              'property_name': 'double-buffered',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'Something:double-buffered:  ',
             {'class_name': 'Something',
              'property_name': 'double-buffered',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, '  Something:double-buffered:',
             {'class_name': 'Something',
              'property_name': 'double-buffered',
              'colon': ':',
              'annotations': ''}),
        (PROPERTY_RE, 'Weird-thing:double-buffered:',
             None),
        (PROPERTY_RE, 'really-weird_thing:double-buffered:',
             None),
        (PROPERTY_RE, ' GMemoryOutputStream:realloc-function: (skip)',
             {'class_name': 'GMemoryOutputStream',
              'property_name': 'realloc-function',
              'colon': ':',
              'annotations': '(skip)'})]

    identifier_signal_tests = [
        # simple property name
        (SIGNAL_RE, 'GtkWidget::changed: (skip)',
             {'class_name': 'GtkWidget',
              'signal_name': 'changed',
              'colon': ':',
              'annotations': '(skip)'}),
        (SIGNAL_RE, 'GtkWidget::changed:',
             {'class_name': 'GtkWidget',
              'signal_name': 'changed',
              'colon': ':',
              'annotations': ''}),
        (SIGNAL_RE, 'Something::changed:',
             {'class_name': 'Something',
              'signal_name': 'changed',
              'colon': ':',
              'annotations': ''}),
        (SIGNAL_RE, 'Weird-thing::changed:',
             None),
        (SIGNAL_RE, 'really-weird_thing::changed:',
             None),
        # property name that contains a dash
        (SIGNAL_RE, 'GtkWidget::hierarchy-changed: (skip)',
             {'class_name': 'GtkWidget',
              'signal_name': 'hierarchy-changed',
              'colon': ':',
              'annotations': '(skip)'}),
        (SIGNAL_RE, 'GtkWidget::hierarchy-changed:',
             {'class_name': 'GtkWidget',
              'signal_name': 'hierarchy-changed',
              'colon': ':',
              'annotations': ''}),
        (SIGNAL_RE, 'Something::hierarchy-changed:',
             {'class_name': 'Something',
              'signal_name': 'hierarchy-changed',
              'colon': ':',
              'annotations': ''}),
        (SIGNAL_RE, 'Weird-thing::hierarchy-changed:',
             None),
        (SIGNAL_RE, 'really-weird_thing::hierarchy-changed:',
             None)]

    parameter_tests = [
        (PARAMETER_RE, '@Short_description: Base class for all widgets  ',
             {'parameter_name': 'Short_description',
              'annotations': '',
              'colon': '',
              'description': 'Base class for all widgets'}),
        (PARAMETER_RE, '@...: the value of the first property, followed optionally by more',
             {'parameter_name': '...',
              'annotations': '',
              'colon': '',
              'description': 'the value of the first property, followed optionally by more'}),
        (PARAMETER_RE, '@widget: a #GtkWidget',
             {'parameter_name': 'widget',
              'annotations': '',
              'colon': '',
              'description': 'a #GtkWidget'}),
        (PARAMETER_RE, '@widget_pointer: (inout) (transfer none): '
                       'address of a variable that contains @widget',
             {'parameter_name': 'widget_pointer',
              'annotations': '(inout) (transfer none)',
              'colon': ':',
              'description': 'address of a variable that contains @widget'}),
        (PARAMETER_RE, '@weird_thing: (inout) (transfer none) (allow-none) (attribute) (destroy) '
                       '(foreign) (inout) (out) (transfer) (skip) (method): some weird @thing',
             {'parameter_name': 'weird_thing',
              'annotations': '(inout) (transfer none) (allow-none) (attribute) (destroy) '
                                '(foreign) (inout) (out) (transfer) (skip) (method)',
              'colon': ':',
              'description': 'some weird @thing'}),
        (PARAMETER_RE, '@data: a pointer to the element data. The data may be moved as elements '
                       'are added to the #GByteArray.',
             {'parameter_name': 'data',
              'annotations': '',
              'colon': '',
              'description': 'a pointer to the element data. The data may be moved as elements '
                                'are added to the #GByteArray.'}),
        (PARAMETER_RE, '@a: a #GSequenceIter',
             {'parameter_name': 'a',
              'annotations': '',
              'colon': '',
              'description': 'a #GSequenceIter'}),
        (PARAMETER_RE, '@keys: (array length=n_keys) (element-type GQuark) (allow-none):',
             {'parameter_name': 'keys',
              'annotations': '(array length=n_keys) (element-type GQuark) (allow-none)',
              'colon': ':',
              'description': ''})]

    tag_tests = [
        (TAG_RE, 'Since 3.0',
             None),
        (TAG_RE, 'Since: 3.0',
             {'tag_name': 'Since',
              'annotations': '',
              'colon': '',
              'description': '3.0'}),
        (TAG_RE, 'Attributes: (inout) (transfer none): some note about attributes',
             {'tag_name': 'Attributes',
              'annotations': '(inout) (transfer none)',
              'colon': ':',
              'description': 'some note about attributes'}),
        (TAG_RE, 'Rename to: something_else',
             {'tag_name': 'Rename to',
              'annotations': '',
              'colon': '',
              'description': 'something_else'}),
        (TAG_RE, '@Deprecated: Since 2.8, reference counting is done atomically',
             None),
        (TAG_RE, 'Returns %TRUE and does weird things',
             None),
        (TAG_RE, 'Returns: a #GtkWidget',
             {'tag_name': 'Returns',
              'annotations': '',
              'colon': '',
              'description': 'a #GtkWidget'}),
        (TAG_RE, 'Return value: (transfer none): The binary data that @text responds. '
                 'This pointer',
             {'tag_name': 'Return value',
              'annotations': '(transfer none)',
              'colon': ':',
              'description': 'The binary data that @text responds. This pointer'}),
        (TAG_RE, 'Return value: (transfer full) (array length=out_len) (element-type guint8):',
             {'tag_name': 'Return value',
              'annotations': '(transfer full) (array length=out_len) (element-type guint8)',
              'colon': ':',
              'description': ''}),
        (TAG_RE, 'Returns: A boolean value, but let me tell you a bit about this boolean.  It',
             {'tag_name': 'Returns',
              'annotations': '',
              'colon': '',
              'description': 'A boolean value, but let me tell you a bit about this boolean.  '
                             'It'}),
        (TAG_RE, 'Returns: (transfer container) (element-type GObject.ParamSpec): a',
             {'tag_name': 'Returns',
              'annotations': '(transfer container) (element-type GObject.ParamSpec)',
              'colon': ':',
              'description': 'a'}),
        (TAG_RE, 'Return value: (type GLib.HashTable<utf8,GLib.HashTable<utf8,utf8>>) '
                 '(transfer full):',
             {'tag_name': 'Return value',
              'annotations': '(type GLib.HashTable<utf8,GLib.HashTable<utf8,utf8>>) '
                             '(transfer full)',
              'colon': ':',
              'description': ''})]


    def create_tests(cls, test_name, testcases):
        for (index, testcase) in enumerate(testcases):
            real_test_name = '%s_%03d' % (test_name, index)

            test_method = cls.__create_test__(testcase)
            test_method.__name__ = real_test_name
            setattr(cls, real_test_name, test_method)


    class TestProgram(unittest.TestCase):
        @classmethod
        def __create_test__(cls, testcase):
            def do_test(self):
                (program, text, expected) = testcase

                match = program.search(text)

                if expected is None:
                    msg = 'Program matched text but shouldn\'t:\n"%s"'
                    self.assertTrue(match is None, msg % (text))
                else:
                    msg = 'Program should match text but didn\'t:\n"%s"'
                    self.assertTrue(match is not None, msg % (text))

                    for key, value in expected.items():
                        msg = 'expected "%s" for "%s" but match returned "%s"'
                        msg = msg % (value, key, match.group(key))
                        self.assertEqual(match.group(key), value, msg)

            return do_test


    # Create tests from data
    create_tests(TestProgram, 'test_identifier_section', identifier_section_tests)
    create_tests(TestProgram, 'test_identifier_symbol', identifier_symbol_tests)
    create_tests(TestProgram, 'test_identifier_property', identifier_property_tests)
    create_tests(TestProgram, 'test_identifier_signal', identifier_signal_tests)
    create_tests(TestProgram, 'test_parameter', parameter_tests)
    create_tests(TestProgram, 'test_tag', tag_tests)

    # Run test suite
    unittest.main()
