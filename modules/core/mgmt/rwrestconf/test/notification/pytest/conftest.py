"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
