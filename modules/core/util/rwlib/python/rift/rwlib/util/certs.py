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

    rift_install = os.environ["RIFT_INSTALL"]
    rift_cert = os.path.join(rift_install, "etc", "ssl", "current.cert")
    rift_key = os.path.join(rift_install, "etc", "ssl", "current.key")

    if os.path.isfile(rift_cert) and os.path.isfile(rift_key):
        return USE_SSL, rift_cert, rift_key

    raise BootstrapSslMissingException()

