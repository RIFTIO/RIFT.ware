#!/usr/bin/env python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import uuid
import sys

import json
import requests
import tornado.ioloop
import tornado.options
from tornado.options import options
import tornado.web
import tornado.escape



class FederationHandler(tornado.web.RequestHandler):
    def get(self):
        headers = {'content-type': 'application/vnd.yang.data+json'}
        name = str(uuid.uuid4().hex)
        auth = ('admin', 'admin')
        data = json.dumps({'federation': {'name': name}})
        url = "http://{host}:8008/api/config".format(host=options.host)

        response = requests.post(url, headers=headers, auth=auth, data=data)
        if not response.ok:
            print(response.status_code, response.reason)
            print(response.text)


class OperationalHandler(tornado.web.RequestHandler):
    def get(self):
        headers = {'content-type': 'application/vnd.yang.operational+json'}
        auth = ('admin', 'admin')
        url = "http://{host}:8008/api/operational/federation".format(host=options.host)

        response = requests.get(url, headers=headers, auth=auth)
        if not response.ok:
            print(response.status_code, response.reason)
            print(response.text)


class ConfigHandler(tornado.web.RequestHandler):
    def get(self):
        headers = {'content-type': 'application/vnd.yang.config+json'}
        auth = ('admin', 'admin')
        url = "http://{host}:8008/api/config/".format(host=options.host)

        response = requests.get(url, headers=headers, auth=auth)
        if not response.ok:
            print(response.status_code, response.reason)
            print(response.text)



def main():
    tornado.options.define("host")

    try:
        tornado.options.parse_command_line()

        if options.host is None:
            raise tornado.options.Error('A host must be specified')

        app = tornado.web.Application([
            (r"/federation", FederationHandler),
            (r"/operational", OperationalHandler),
            (r"/config", ConfigHandler),
            ])

        app.listen(8888)
        tornado.ioloop.IOLoop.current().start()

    except tornado.options.Error as e:
        print("{}\n\n".format(str(e)))
        tornado.options.print_help()
        sys.exit(1)

    except Exception as e:
        print(str(e))
        sys.exit(1)

    except (KeyboardInterrupt, SystemExit):
        pass

    finally:
        tornado.ioloop.IOLoop.current().stop()

if __name__ == "__main__":
    main()
