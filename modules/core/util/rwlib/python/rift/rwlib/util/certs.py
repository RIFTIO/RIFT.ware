# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 2/5/16
# 

import os

class BootstrapSslMissingException(Exception):
    pass

# True if the environment variable is unset, otherwise False
USE_SSL = os.environ.get("RIFT_BOOT_WITHOUT_HTTPS", None) is None

def get_bootstrap_cert_and_key():
    '''
    Lookup the bootstrap certificate and key and return their paths
    '''

    user_cert = os.path.join("/", "etc", "ssl", "current.cert")
    user_key = os.path.join("/", "etc", "ssl", "current.key")

    if os.path.isfile(user_cert) and os.path.isfile(user_key):
        return USE_SSL, user_cert, user_key

    rift_install = os.environ["RIFT_ROOT"]
    rift_cert = os.path.join(rift_install, "etc", "ssl", "current.cert")
    rift_key = os.path.join(rift_install, "etc", "ssl", "current.key")

    if os.path.isfile(rift_cert) and os.path.isfile(rift_key):
        return USE_SSL, rift_cert, rift_key

    raise BootstrapSslMissingException()

