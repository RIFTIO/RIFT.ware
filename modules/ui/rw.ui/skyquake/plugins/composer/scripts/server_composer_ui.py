#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

from http.server import BaseHTTPRequestHandler, HTTPServer, SimpleHTTPRequestHandler
import socketserver
import mimetypes
import argparse
import sys
import os
import ssl

PORT = 9000

enable_https = False
keyfile_path = None
certfile_path = None

DEFAULT_ENABLE_HTTPS = False
DEFAULT_KEYFILE_PATH = None
DEFAULT_CERTFILE_PATH = None

def start_server(
		enable_https=DEFAULT_ENABLE_HTTPS,
		keyfile_path=DEFAULT_KEYFILE_PATH,
		certfile_path=DEFAULT_CERTFILE_PATH):
    Handler = SimpleHTTPRequestHandler
    Handler.extensions_map['.svg'] = 'image/svg+xml'
    httpd = socketserver.TCPServer(('', PORT), Handler)

    if enable_https:

        httpd.socket = ssl.wrap_socket(httpd.socket, 
                                        server_side=True,
                                        certfile=certfile_path,
                                        keyfile=keyfile_path)

    print("Serving at port: {}. HTTPS Enabled: {}".format(PORT, enable_https))
    httpd.serve_forever()


def main(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port",
    					default=PORT,
    					help="Run on the given port")
    parser.add_argument("-e", "--enable-https",
    					action="store_true",
    					default=False,
    					help="Enable HTTPS. Make sure certfile-path and keyfile-path are also specified")
    parser.add_argument("-k", "--keyfile-path",
    					default=DEFAULT_KEYFILE_PATH,
    					help="Path to the key file")
    parser.add_argument("-c", "--certfile-path",
    					default=DEFAULT_CERTFILE_PATH,
    					help="Path to the cert file")

    args = parser.parse_args()

    # When you want to use the debugger, unremark this before the line you want
    #import pdb; pdb.set_trace()

    if args.enable_https:
        if not (args.keyfile_path and args.certfile_path):
            parser.print_help()
            sys.exit(2)
    
    start_server(args.enable_https, args.keyfile_path, args.certfile_path)


if __name__ == '__main__':
    main()

