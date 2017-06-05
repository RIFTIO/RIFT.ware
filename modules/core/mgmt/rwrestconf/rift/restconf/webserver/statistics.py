# RIFT_IO_STANDARD_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 9/11/2015
# RIFT_IO_STANDARD_COPYRIGHT_HEADER(END)

def hash_method_and_code(method, code):
    return "%s-%d" % (method, code)

class Statistics(object):
    def __init__(self):

        self.del_200_rsp = 0
        self.del_404_rsp = 0
        self.del_405_rsp = 0
        self.del_500_rsp = 0
        self.del_req = 0

        self.get_200_rsp = 0
        self.get_204_rsp = 0
        self.get_404_rsp = 0
        self.get_500_rsp = 0
        self.get_req = 0

        self.post_200_rsp = 0
        self.post_201_rsp = 0
        self.post_404_rsp = 0
        self.post_405_rsp = 0
        self.post_409_rsp = 0
        self.post_500_rsp = 0
        self.post_req = 0

        self.put_200_rsp = 0
        self.put_201_rsp = 0
        self.put_404_rsp = 0
        self.put_405_rsp = 0
        self.put_409_rsp = 0
        self.put_500_rsp = 0
        self.put_req = 0

        self.req_401_rsp = 0

        # can't use the result of has_method_and_code to generate the keys for an unknown reason
        self._stat_map = {
            "DELETE-200" : self.del_200_rsp,
            "DELETE-404" : self.del_404_rsp,
            "DELETE-405" : self.del_405_rsp,
            "DELETE-500" : self.del_500_rsp,
            "DELETE" : self.del_req,
            "GET-200" : self.get_200_rsp,
            "GET-204" : self.get_204_rsp,
            "GET-404" : self.get_404_rsp,
            "GET-500" : self.get_500_rsp,
            "GET" : self.get_req,
            "POST-200" : self.post_200_rsp,
            "POST-201" : self.post_201_rsp,
            "POST-404" : self.post_404_rsp,
            "POST-405" : self.post_405_rsp,
            "POST-409" : self.post_409_rsp,
            "POST-500" : self.post_500_rsp,
            "POST" : self.post_req,
            "PUT-200" : self.put_200_rsp,
            "PUT-200" : self.put_201_rsp,
            "PUT-200" : self.put_404_rsp,
            "PUT-200" : self.put_405_rsp,
            "PUT-200" : self.put_409_rsp,
            "PUT-200" : self.put_500_rsp,
            "PUT" : self.put_req,
            "401-RSP" : self.req_401_rsp,
        }

        # Event Source specific statistics
        self.websocket_stream_open = 0
        self.websocket_stream_close = 0
        self.websocket_events = 0
        self.http_stream_open = 0
        self.http_stream_close = 0
        self.http_events = 0


    def increment_http_code(self, method, code=None):
        if code is not None:
            # silently consume improper method/code pair by defaulting to 0 for increment
            stat_reference = self._stat_map.get(hash_method_and_code(method, code), 0) 
        else:
            # silently consume improper method by defaulting to 0 for increment
            stat_reference = self._stat_map.get(method, 0) 

        stat_reference += 1
