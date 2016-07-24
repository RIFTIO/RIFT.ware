#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import io
import logging
import os
import random
import string
import sys
import unittest
import tempfile
import xmlrunner

from rift.tasklets.rwlaunchpad.extract import (
        boundary_search,
        extract_package,
        )


message_template = """
----------------------------------------
POST /{url} HTTP/1.1
User-Agent: curl/7.32.0
Host: localhost:1337
Accept: */*
Content-Length: {length}
Expect: 100-continue
Content-Type: multipart/form-data; boundary={boundary}

--{boundary}
Content-Disposition: form-data; name=descriptor

{binary}
--{boundary}--
"""

def random_string(ncharacters):
        refs = string.ascii_lowercase + '\n'
        return ''.join(random.choice(refs) for _ in range(ncharacters))

class TestBoundarySearch(unittest.TestCase):
    """
    The boundary_search function is used to efficiently search for a boundary
    string of a message that has been saved to file. In searches the file,
    without loading it all into memory.

    """
    def setUp(self):
        self.log = logging.getLogger('test')
        self.boundary = "----------------------test-boundary"

    def test(self):
        """
        Create a message that contains 3 instance of the boundary interspersed
        with random characters. The message is presented to the boundary_search
        function as a BytesIO so that it can treat it was a file.

        """
        # Construct the message
        message = self.boundary
        message += random_string(32)
        message += self.boundary
        message += random_string(64)
        message += self.boundary

        # Search for the boundaries
        indices = boundary_search(io.BytesIO(message.encode()), self.boundary)

        # Check the results
        self.assertEqual(0, indices[0])
        self.assertEqual(32 + len(self.boundary), indices[1])
        self.assertEqual(96 + 2 * len(self.boundary), indices[2])


class TestExtractPackage(unittest.TestCase):
    def setUp(self):
        self.log = logging.getLogger('devnull')
        self.log.addHandler(logging.NullHandler())
        self.boundary = "----------------------test-boundary"
        self.pkgfile = tempfile.NamedTemporaryFile().name
        self.package = random_string(128)
        self.url = "api/upload"

    def test(self):
        """
        This test takes a known message (form-data) and extract the 'package'
        data from it.

        """
        try:
            message = message_template.format(
                    length=len(self.package),
                    boundary=self.boundary,
                    binary=self.package,
                    url=self.url,
                    )

            extract_package(
                    self.log,
                    io.BytesIO(message.encode()),
                    self.boundary,
                    self.pkgfile,
                    )

            # Read the package file that is extracted to disk, and compare it with
            # the expected data.
            with open(self.pkgfile) as fp:
                for u, v in zip(fp.readline(), self.package):
                    self.assertEqual(u, v)

        finally:
            # Cleanup possible files
            if os.path.exists(self.package):
                os.remove(self.package)

            if os.path.exists(self.package + ".partial"):
                os.remove(self.package + ".partial")


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')

    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
