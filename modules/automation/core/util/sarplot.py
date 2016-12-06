"""
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

@file sarplot.py
@date 07/22/2016
@brief map and plot sar output
"""

import json
import sys
import re
import itertools
import subprocess
import numpy as np
import matplotlib
matplotlib.use('Agg')
from matplotlib.backends.backend_pdf import PdfPages
import matplotlib.pyplot as plt


def sarplot(
        lines,
        keys,
        fields,
        key_filters=[],
        plot=None,
        title=None,
        xlabel=None,
        ylabel=None,
        xticklabels=None,
        yticklabels=None,
        xmin=None,
        xmax=None,
        ymin=None,
        ymax=None,
        use_average=False,
        ):

    next_line_is_header = True
    data = {}

    max_value = 0

    no_keys = False
    if not keys:
        no_keys = True

    for line in lines:
        line_is_blank = re.match('^\s*$', line)
        if line_is_blank:
            next_line_is_header = True
            continue

        if not use_average:
            line_is_average = re.match('^Average:', line)
            if line_is_average:
                continue

        if next_line_is_header:
            next_line_is_header = False
            line_fields = line.split()
            if line_fields:
                map_data = True
                for key in keys:
                    if key not in line_fields:
                        map_data = False
                        break
                for field in fields:
                    if field not in line_fields:
                        map_data = False
                        break
            continue

        if not map_data:
            continue


        values = line.split()
        if not values:
            continue

        if use_average:
            line_is_average = re.match('^Average:', line)
            if not line_is_average:
                continue


        row = dict(zip(line_fields, values))
        if key_filters:
            discard = True
            for key_filter in key_filters:
                if row[key_filter['key']] == key_filter['value']:
                    discard = False
                    break
            if discard:
                continue

        if no_keys:
            for field in fields:
                if field not in data:
                    data[field] = []
                data[field].append(row[field])
        else:
            key_string = ','.join(["%s:%s" % (key, row[key]) for key in keys])
            for field in fields:
                if (key_string,field) not in data:
                    data[(key_string,field)] = []
                data[(key_string,field)].append(row[field])

    if plot:
        fig, ax = plt.subplots()
        rect_containers = []

        colors = itertools.cycle('bgrcmyk')
        if title:
            ax.set_title(title)

        if xlabel:
            ax.set_xlabel(xlabel)

        if ylabel:
            ax.set_ylabel(ylabel)

        keys = data.keys()
        if keys:
            width = .75 / len(keys)
        else:
            width = .75

        if xticklabels:
            n = np.arange(len(xticklabels))
            ax.set_xticklabels(xticklabels)
        else:
            n = np.arange(len(data[keys[0]]))
            ax.set_xticklabels(range(0, len(data[keys[0]])))
        ax.set_xticks(n + width*len(keys)/2.)


        for idx, key in enumerate(keys):
            values = data[key]
            if ymax is None:
                for value in values:
                    value = float(value)
                    if value > max_value:
                        max_value = value
            values = [float(value) for value in values]
            rect_container = ax.bar(n+width*idx, values, width, color=next(colors))
            rect_containers.append(rect_container)

        if xmax is not None:
            if xmin is None:
                xmin = 0
            ax.set_xlim([xmin, xmax])

        if ymax is not None:
            if ymin is None:
                ymin = 0
            ax.set_ylim([float(ymin), float(ymax)])
        else:
            if ymin is None:
                ymin = 0
            if max_value == ymin:
                max_value = 1
            ax.set_ylim([0, max_value*1.4])

        def autolabel(rect_container):
            for rect in rect_container:
                height = rect.get_height()
                ax.text(rect.get_x() + rect.get_width()/2., 1.05*height,
                        '%.2f' % float(height), rotation=90,
                        ha='center', va='bottom')

        for rect_container in rect_containers:
            autolabel(rect_container)

        legend = ax.legend([rect_container[0] for rect_container in rect_containers], keys, loc='center left', bbox_to_anchor=(1, 0.5))
        plt.savefig(plot, bbox_extra_artists=(legend,), bbox_inches='tight')

    else:
        for key, values in data.items():
            key_str = ':'.join(key)
            value_str = ','.join(values)
            print(json.dumps({key_str:value_str}))

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--keys', default=None, action='store', help="Fields to treat as keys in mapping process")
    parser.add_argument('--fields', default="", action='store', help="Fields to treat as values in mapping process")
    parser.add_argument('--key-filter', default=[], action='append', help="Include only rows that contain particular key value pairs.  e.g. key:value")
    parser.add_argument('--plot', default=None, action='store', help='Plot data to this file')
    parser.add_argument('--title', default=None, action='store', help='Title of generated plot')
    parser.add_argument('--xlabel', default=None, action='store', help='x axis label') 
    parser.add_argument('--ylabel', default=None, action='store', help='y axis label')
    parser.add_argument('--xticklabels', default=None, action='store', help='csv of labels for x tick marks')
    parser.add_argument('--yticklabels', default=None, action='store', help='csv of labels for y tick marks')
    parser.add_argument('--xmin', default=None, action='store', help='minimum value of x in plot')
    parser.add_argument('--xmax', default=None, action='store', help='maximum value of x in plot')
    parser.add_argument('--ymin', default=None, action='store', help='minimum value of y in plot')
    parser.add_argument('--ymax', default=None, action='store', help='maximum value of y in plot')
    parser.add_argument('--use-average', default=False, action='store', help='Use average values from sar output instead of samples')

    args = parser.parse_args()

    if args.keys:
        keys = args.keys.split(',')
    else:
        keys = []
    fields = args.fields.split(',')
    key_filters = []
    for key_filter in args.key_filter:
        key, value = key_filter.split(':')
        key_filters.append({'key':key, 'value':value})

    if not keys and not fields:
        print('No keys or fields specified, nothing to map', sys.stderr)
        exit(1)

    xticklabels=None
    if args.xticklabels:
        xticklabels = args.xticklabels.split(',')

    yticklabels=None
    if args.yticklabels:
        yticklabels = args.yticklabels.split(',')

    sarplot(
            sys.stdin.readlines(),
            keys,
            fields,
            key_filters=key_filters,
            plot=args.plot,
            title=args.title,
            xlabel=args.xlabel,
            ylabel=args.ylabel,
            xticklabels=xticklabels,
            yticklabels=yticklabels,
            xmin=args.xmin,
            xmax=args.xmax,
            ymin=args.ymin,
            ymax=args.ymax,
            use_average=args.use_average,
    )
