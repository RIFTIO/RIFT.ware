
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

import logging
import sys
import time

logger = logging.getLogger(__name__)


def print_with_underline(string):
    print(string)
    print(("-" * len(string)))


def add_color(string, color):
    colors = {
            "black":   0,
            "red":     1,
            "green":   2,
            "yellow":  3,
            "blue":    4,
            "magenta": 5,
            "cyan":    6,
            "white":   7,
            }

    prefix = "\033[1;3{}m"
    suffix = "\033[0m"

    if color not in colors:
        return string

    output = "{}{}{}".format(prefix.format(colors[color]), string, suffix)
    return output


def get_pass_fail_string(is_pass):
    if is_pass:
        result = "PASS"
        color = "green"
    else:
        result = "FAIL"
        color = "red"

    return add_color(result, color)


class TestResult(object):
    def __init__(self, result_name, summary, is_success=None, failure_detail=None):
        self._name = result_name
        self._summary = summary
        self._is_success = is_success
        self._failure_detail = failure_detail

    @property
    def name(self):
        return self._name

    @property
    def summary(self):
        """ A one line summary of a test result """

        return self._summary

    def is_success(self):
        return self._is_success

    def print_summary(self):
        """ Print a one line summary of the test result """

        sys.stdout.write("(" + self.name + "): " + self.summary)
        success = self.is_success()
        if success is not None:
            sys.stdout.write(" (" + get_pass_fail_string(success) + ")")
        sys.stdout.write("\n")

        if self._failure_detail is not None:
            sys.stdout.write("  *" + self._failure_detail + "\n")


class TestReporter(object):
    def __init__(self, test_name):
        self._name = test_name
        self._test_results = []

        self.mark_start_time()
        self.mark_stop_time()

    @property
    def start_time(self):
        return self._start_time

    @property
    def end_time(self):
        return self._stop_time

    def is_success(self):
        for result in self._test_results:
            if not result.is_success():
                return False

        return True

    def mark_start_time(self):
        self._start_time = time.time()

    def mark_stop_time(self):
        self._stop_time = time.time()

    def add_test_result(self, test_result):
        self._test_results.append(test_result)

    def print_report(self):
        print("")
        print_with_underline("\'{}\' Test Suite Summary (".format(self._name) + \
                             get_pass_fail_string(self.is_success()) + ")")

        #time_result = TestTimeTakenResult(self._start_time, self._stop_time)
        #results = [time_result]
        #results.extend(self._test_results)

        for result in self._test_results:
            result.print_summary()

        print("")

