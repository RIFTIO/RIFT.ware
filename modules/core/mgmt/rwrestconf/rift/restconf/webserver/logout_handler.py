import asyncio

import tornado.gen
import tornado.web
import tornado.ioloop
import tornado.platform.asyncio

class LogoutHandler(tornado.web.RequestHandler):
    def initialize(self, logger, connection_manager, asyncio_loop, ):
        # ATTN: can't call it _log because the base class has a _log instance already
        self.__log = logger
        self._asyncio_loop = asyncio_loop
        self._connection_manager = connection_manager
        self._return_type_json = "application/vnd.yang.data+json"
        self._return_type_xml = "application/vnd.yang.data+xml"

    @tornado.gen.coroutine
    def post(self, *args, **kwargs):

        def impl():
            accept_type = self.request.headers.get("Accept")
            if accept_type is None:
                accept_type = self._return_type_json

            auth_header = self.request.headers.get("Authorization")        
            if auth_header is None:
                if "json" in accept_type:
                    response = "{'error':'must supply Authorization'}"
                    return_type = self._return_type_json
                else:
                    response = "<error>must supply Authorization<error>"
                    return_type = self._return_type_xml

                return 405, default_return, response

            if not auth_header.startswith("Basic "):
                if "json" in accept_type:
                    response = "{'error':'must supply BasicAuth'}"
                    return_type = self._return_type_json
                else:
                    response = "<error>must supply BasicAuth</error>"
                    return_type = self._return_type_xml

                return 405, default_return, response

            auth_header = auth_header[6:]

            if not self._connection_manager.is_logged_in(auth_header):
                if "json" in accept_type:
                    response = "{'logout':'user is not logged in'}"
                    return_type = self._return_type_json
                else:
                    return_type = self._return_type_xml
                    response = "<logout>user is not logged in</logout>"

                return 200, return_type, response

            try:
                self._connection_manager.logout(auth_header)
            except Exception as e:
                self.__log.error(str(e))
                if "json" in accept_type:
                    response = "{'error':'couldn't logout user'}"
                    return_type = self._return_type_json
                else:
                    response = "<error>couldn't logout user</error>"
                    return_type = self._return_type_xml

                return 500, return_type, response

            if "json" in accept_type:
                response = "{'logout':'success'}"
            else:
                response = "<logout>success</logout>"

            return_code = 200

            return return_code, accept_type, response
        
        response = impl()

        http_code, content_type, message = response[:]
        self.clear()
        self.set_status(http_code)
        self.set_header("Content-Type", content_type)
        self.write(message)
