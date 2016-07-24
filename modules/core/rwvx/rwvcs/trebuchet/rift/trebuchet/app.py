
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import getopt
import logging
import os
import signal
import sys

import tornado.httpserver
import tornado.ioloop
import tornado.web

from . import vcs_handlers


class Application(tornado.web.Application):
    """
    Basic web application serving the VCS REST handlers

    Attributes:
        loop    - ioloop being used by the application
        vms     - set of Rift.VMs being managed by this server
        rwmain  - path to rwmain executable
        suid    - boolean representing if the uid field in any
                  request should be respected
    """
    def __init__(self, rwmain, loop=None, suid=True):
        """
        Create an application

        @param rwmain - path to rwmain executable.
        @param loop   - ioloop to user.
        @param suid   - boolean representing if the uid field in any
                        request should be respected
        """
        self.loop = loop
        self.vms = set()
        self.rwmain = rwmain
        self.suid = suid

        if not os.path.exists(rwmain):
            raise ValueError('rwmain path "%s" does not exist' % (rwmain,))

        handlers = [
                (r'/vcs/1.0/ping', vcs_handlers.Ping),
                (r'/vcs/1.0/vms/launch', vcs_handlers.VMLauncher),
                (r'/vcs/1.0/vms/([^-]*)-([0-9]+)/status', vcs_handlers.VMStatus),
                (r'/vcs/1.0/vms/([^-]*)-([0-9]+)/terminate', vcs_handlers.VMTerminator),
        ]

        super(Application, self).__init__(handlers)


__default_port__ = 8679
__default_rwmain__ = './usr/bin/rwmain'

__usage__ = """
trebuchet [ARGUMENTS]

Launch the trebuchet web server.  This will expose REST API's which are
used by RIFT.vcs to launch processes on remote virtual machines.

ARGUMENTS:
    -h, --help            This screen
    -p, --port [PORT]     Port trebuchet should listen on [%d]
    -r, --rwmain [PATH]   Path to the rwmain executable [%s]
    -v, --verbose         Enable debug level logging
        --no-suid         Trebuchet is not running as root and will ignore the uid
                          field of any requests.
""" % (
        __default_port__,
        __default_rwmain__,
)


def main():
    try:
        opts, args = getopt.getopt(
                sys.argv[1:],
                'hp:r:v',
                ['help', 'port=', 'rwmain=', 'verbose', 'no-suid'])
    except getopt.GetoptError as e:
        print(e)
        sys.exit(1)

    port = __default_port__
    rwmain = __default_rwmain__
    suid = True

    for o, a in opts:
        if o in ('-h', '--help'):
            print(__usage__)
            sys.exit(0)
        elif o in ('-p', '--port'):
            port = int(a)
        elif o in ('-r', '--rwmain'):
            rwmain = a
        elif o in ('-v', '--verbose'):
            logging.getLogger().setLevel(logging.DEBUG)
        elif o in ('--no-suid'):
            suid = False


    loop = tornado.ioloop.IOLoop.instance()
    app = Application(rwmain=rwmain, loop=loop, suid=False)
    httpd = tornado.httpserver.HTTPServer(app)
    httpd.listen(port)

    def on_signal(*args):
        logging.debug("Got signal, shutting down")
        loop.add_callback_from_signal(loop.stop)
    signal.signal(signal.SIGTERM, on_signal)


    logging.info('Starting trebuchet on port %d' % (port,))
    loop.start()
    loop.close()



# vim: sw=4
