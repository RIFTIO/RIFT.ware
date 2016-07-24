#!/usr/bin/env python3

import os

import pytest

dirpath = os.path.dirname(__file__)

options = '-v'
pytest.main([options, os.path.join(dirpath, 'test_mission_control_negative_cloud_account.py')])
pytest.main([options, os.path.join(dirpath, 'test_mission_control_negative_mgmt_domain.py')])
pytest.main([options, os.path.join(dirpath, 'test_mission_control_negative_vmpool.py')])

