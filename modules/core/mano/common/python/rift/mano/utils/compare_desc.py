#
# Copyright 2016 RIFT.io Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


import argparse
import json
import logging
import logging.config
import pprint

from deepdiff import DeepDiff

from rift.mano.yang_translator.common.utils import _

import yaml


class CompareDescShell(object):

    SUPPORTED_TYPES = ['yaml', 'json']
    INDENT = 2

    DIFF_KEYS = (
        REMOVED_ITEMS,
        ADDED_ITEMS,
        ITER_ITEM_ADDED,
        ITER_ITEM_REM,
        TYPE_CHANGES,
        VALUES_CHANGED,
    ) = (
        'dic_item_removed',
        'dic_item_added',
        'iterable_item_added',
        'iterable_item_removed',
        'type_changes',
        'values_changed',
    )

    DIFF_MAP = {
        REMOVED_ITEMS: 'Items removed',
        ADDED_ITEMS: 'Items added',
        ITER_ITEM_ADDED: 'Items added to list',
        ITER_ITEM_REM: 'Items removed from list',
        TYPE_CHANGES: 'Change in types',
        VALUES_CHANGED: 'Change in values',
    }

    # Changes in following items are error
    ERROR_ITEMS = [REMOVED_ITEMS, ADDED_ITEMS, ITER_ITEM_ADDED,
                   ITER_ITEM_REM, TYPE_CHANGES, ]

    @classmethod
    def compare_dicts(cls, generated, expected, log=None):
        """Compare two dictionaries and generate error if required"""
        if log:
            log.debug(_("Generated: {0}").format(generated))
            log.debug(_("Expected: {0}").format(expected))

        diff = DeepDiff(expected, generated)
        if log:
            log.debug(_("Keys in diff: {0}").format(diff.keys()))
            log.info(_("Differences:\n"))

        if log:
            d = pprint.pformat(diff, indent=cls.INDENT)
            log.info("Differences:\n{0}".format(d))

        if len(set(cls.ERROR_ITEMS).intersection(diff.keys())):
            diff_str = pprint.pformat(diff)
            msg = _("Found item changes: {0}").format(diff_str)
            if log:
                log.error(msg)
            raise ValueError(msg)

    def main(self, log, args):
        self.log = log
        self.log.debug(_("Args: {0}").format(args))
        if args.type not in self.SUPPORTED_TYPES:
            self.log.error(_("Unsupported file type {0}").
                           format(args.type))
            exit(1)

        with open(args.generated) as g:
            gen_data = g.read()
            if args.type == 'yaml':
                y_gen = yaml.load(gen_data)
            else:
                y_gen = json.loads(gen_data)

        with open(args.expected) as e:
            exp_data = e.read()
            if args.type == 'yaml':
                y_exp = yaml.load(exp_data)
            else:
                y_exp = json.loads(exp_data)

        self.compare_dicts(y_gen, y_exp, log=self.log)


def main(args=None, log=None):
    parser = argparse.ArgumentParser(
        description='Validate descriptors by comparing')
    parser.add_argument(
        "-g",
        "--generated",
        required=True,
        help="Generated descriptor file")
    parser.add_argument(
        "-e",
        "--expected",
        required=True,
        help="Descriptor file to compare")
    parser.add_argument(
        "-t",
        "--type",
        default='yaml',
        help="File type. Default yaml")
    parser.add_argument(
        "--debug",
        help="Enable debug logging",
        action="store_true")

    if args:
        args = parser.parse_args(args)
    else:
        args = parser.parse_args()

    if log is None:
        if args.debug:
            logging.basicConfig(level=logging.DEBUG)
        else:
            logging.basicConfig(level=logging.ERROR)
        log = logging.getLogger("rwmano-translator")

    CompareDescShell().main(log, args)


if __name__ == '__main__':
    main()
