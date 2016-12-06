#!/usr/bin/python3
import tornado.websocket as ws
import tornado.gen
import tornado.ioloop
import tornado.httpclient

# For XML content
#WS_URL = "ws://127.0.0.1:8888/ws_streams/NETCONF"

# For Json content
WS_URL = "wss://127.0.0.1:8888/ws_streams/NETCONF-JSON"
HTTP_REQ = tornado.httpclient.HTTPRequest(
              url = WS_URL,
              auth_username = "admin",
              auth_password = "admin",
              validate_cert = False)


@tornado.gen.coroutine
def websocket_client():
  websock = yield ws.websocket_connect(HTTP_REQ)
  print('=== Websocket connected')
  while True:
    msg = yield websock.read_message()
    if msg is None: break
    print("Received Message: {}".format(msg))


tornado.ioloop.IOLoop.current().run_sync(websocket_client)

