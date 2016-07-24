#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

##
# @file rwlib_test.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 07/20/2013
# @brief RIFT utilities for backtrace. 
# 
# @details These utilities are written on top of libunwind.
 
import shlex
from subprocess import call, Popen
import subprocess
import sys

## @class RLIBTest
# @brief Class to test RLIB functionality.
#
# Provides various method to test RLIB functionality. Each test typically
# creates a small "c" program, compiles it and executes it to verify the
# expected behaviour.
class RLIBTest:

    ## Returns the string without non ASCII characters
    # @param[in] string
    #
    # @returns string without non ASCII characters
    def strip_non_ascii(self, string):
        return ''.join([c for c in string if 0 < ord(c) < 127])

    ## Test RW_STATIC_ASSERT
    def testStaticAssert(self):
        fname_pfx = "test_static_assert"
        fname = "test_static_assert.c"
        f = open(fname, "w")
        f.write("#include <stdio.h>\n"
                "#include \"rwlib.h\"\n"
                "\n"
                "int main(int argc, char **argv)\n"
                "{\n"
                "  RW_STATIC_ASSERT(4==1);\n"
                "  return 0;\n"
                "}\n")
        f.close()
        cflags = "C_INCLUDES"
        cmd = "gcc " + cflags + " -o " + fname_pfx + " " + fname
        args = shlex.split(cmd)
        proc = Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        output, error = proc.communicate()
        err = self.strip_non_ascii(error)
        
        if "error: static assertion failed" in err:
            rc = 0
        else:
            print(err)
            rc = -1

        return rc

if __name__ == "__main__":
    test = RLIBTest()
    rc = test.testStaticAssert()
    sys.exit(rc)
