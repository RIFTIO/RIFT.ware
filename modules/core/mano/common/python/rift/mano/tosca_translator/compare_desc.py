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

from rift.mano.tosca_translator.common.utils import _


class CompareDescShell(object):

    SUPPORTED_TYPES = ['json']
    INDENT = 2
    DIFF_KEYS = (REMOVED_ITEMS, ADDED_ITEMS, TYPE_CHANGES, VALUES_CHANGED) = \
                ('dic_item_removed', 'dic_item_added', 'type_changes',
                 'values_changed')
    DIFF_MAP = {REMOVED_ITEMS: 'Items removed',
                ADDED_ITEMS: 'Items added',
                TYPE_CHANGES: 'Changes in types',
                VALUES_CHANGED: 'Changes in values'}
    # Currently considering changes in removed keys or type changes
    # as error.
    ERROR_ITEMS = [REMOVED_ITEMS, TYPE_CHANGES]

    def main(self, log, args):
        self.log = log
        print("Args: {}".format(args))
        self.log.debug(_("Args: {0}").format(args))
        if args.type not in self.SUPPORTED_TYPES:
            self.log.error(_("Unsupported file type {0}").
                           format(args.type))
            exit(1)

        with open(args.generated_file) as g:
            gen_data = g.read()
            json_gen = json.loads(gen_data)
            self.log.debug(_("Generated: {0}").format(json_gen))

        with open(args.expected_file) as e:
            exp_data = e.read()
            json_exp = json.loads(exp_data)
            self.log.debug(_("Expected: {0}").format(json_exp))

        diff = DeepDiff(json_exp, json_gen)
        self.log.debug(_("Keys in diff: {0}").format(diff.keys()))
        self.log.info(_("Differences:\n"))

        d = pprint.pformat(diff, indent=self.INDENT)
        self.log.info("Differences:\n{0}".format(d))

        if len(set(self.ERROR_ITEMS).intersection(diff.keys())):
            diff_str = pprint.pformat(diff)
            msg = _("Found item changes: {0}").format(diff_str)
            self.log.error(msg)
            raise ValueError(msg)


def main(args=None):
    parser = argparse.ArgumentParser(
        description='Validate descriptors by comparing')
    parser.add_argument(
        "-g",
        "--generated-file",
        required=True,
        help="Generated descriptor file")
    parser.add_argument(
        "-e",
        "--expected-file",
        required=True,
        help="Descriptor to compare")
    parser.add_argument(
        "-t",
        "--type",
        default='json',
        help="File type. Default json")
    parser.add_argument(
        "--debug",
        help="Enable debug logging",
        action="store_true")
    if args:
        args = parser.parse_args(args)
    else:
        args = parser.parse_args()

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.ERROR)
    log = logging.getLogger("rwmano-translator")

    CompareDescShell().main(log, args)


if __name__ == '__main__':
    main()
