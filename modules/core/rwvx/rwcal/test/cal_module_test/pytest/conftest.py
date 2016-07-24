"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file conftest.py
@author Varun Prasad (varun.prasad@riftio.com)
@date 21/01/2016

"""

def pytest_addoption(parser):
    # Openstack related options
    parser.addoption("--os-host", action="store", default="10.66.4.102")
    parser.addoption("--os-user", action="store", default="pluto")
    parser.addoption("--os-password", action="store", default="mypasswd")
    parser.addoption("--os-tenant", action="store", default="demo")
    parser.addoption("--os-network", action="store", default="private")

    # aws related options
    parser.addoption("--aws-user", action="store", default="AKIAIKRDX7BDLFU37PDA")
    parser.addoption("--aws-password", action="store", default="cjCRtJxVylVkbYvOUQeyvCuOWAHieU6gqcQw29Hw")
    parser.addoption("--aws-region", action="store", default="us-east-1")
    parser.addoption("--aws-zone", action="store", default="us-east-1c")
    parser.addoption("--aws-ssh-key", action="store", default="vprasad-sshkey")
