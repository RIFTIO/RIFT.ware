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

@file conftest.py
@author Varun Prasad (varun.prasad@riftio.com)
@date 21/01/2016

"""

import pytest


def pytest_addoption(parser):
    parser.addoption("--wait", dest="wait", action="store_true", default=False)


@pytest.fixture(scope="session")
def wait_for_exit(request):
    return request.config.option.wait
