#!/usr/bin/env python

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
# Author(s): Austin Cormier
# Creation Date: 09/10/2014
# 
#
# Automation Argparse utilities

import argparse
import imp
import os

def parse_class(module_class_arg):
    module_class_pair = module_class_arg.split(":")
    if len(module_class_pair) != 2:
        raise argparse.ArgumentTypeError("Argument must be a module filename and "
                "class name separated by a :")

    module_path, class_name = module_class_pair

    try:
        if module_path.endswith(".py"):
            module_path = module_path[:-3]
        module_name = os.path.basename(module_path)
        args = imp.find_module(module_path, [os.getcwd()])
        module = imp.load_module(module_name, *args)
        cls = getattr(module, class_name)

    except Exception as e:
        raise argparse.ArgumentTypeError(("Error when attempting to load class "
            "({}) from module ({}): {}").format(class_name, module_name, str(e)))

    return cls


