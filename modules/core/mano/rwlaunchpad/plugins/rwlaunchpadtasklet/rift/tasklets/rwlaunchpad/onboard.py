import requests

from rift.package import convert
from gi.repository import (
    NsdYang,
    RwNsdYang,
    VnfdYang,
    RwVnfdYang,
)


class OnboardError(Exception):
    pass


class UpdateError(Exception):
    pass


class DescriptorOnboarder(object):
    """ This class is responsible for onboarding descriptors using Restconf"""
    DESC_ENDPOINT_MAP = {
            NsdYang.YangData_Nsd_NsdCatalog_Nsd: "nsd-catalog/nsd",
            RwNsdYang.YangData_Nsd_NsdCatalog_Nsd: "nsd-catalog/nsd",
            VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd: "vnfd-catalog/vnfd",
            RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd: "vnfd-catalog/vnfd",
            }

    DESC_SERIALIZER_MAP = {
            NsdYang.YangData_Nsd_NsdCatalog_Nsd: convert.NsdSerializer(),
            RwNsdYang.YangData_Nsd_NsdCatalog_Nsd: convert.RwNsdSerializer(),
            VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd: convert.VnfdSerializer(),
            RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd: convert.RwVnfdSerializer(),
            }

    HEADERS = {"content-type": "application/vnd.yang.data+json"}
    TIMEOUT_SECS = 5
    AUTH = ('admin', 'admin')

    def __init__(self, log, host="127.0.0.1", port=8008, use_ssl=False, ssl_cert=None, ssl_key=None):
        self._log = log
        self._host = host
        self.port = port
        self._use_ssl = use_ssl
        self._ssl_cert = ssl_cert
        self._ssl_key = ssl_key

        self.timeout = DescriptorOnboarder.TIMEOUT_SECS

    @classmethod
    def _get_headers(cls, auth):
        headers = cls.HEADERS.copy()
        if auth is not None:
            headers['authorization'] = auth

        return headers

    def _get_url(self, descriptor_msg):
        if type(descriptor_msg) not in DescriptorOnboarder.DESC_SERIALIZER_MAP:
            raise TypeError("Invalid descriptor message type")

        endpoint = DescriptorOnboarder.DESC_ENDPOINT_MAP[type(descriptor_msg)]

        url = "{}://{}:{}/api/config/{}".format(
                "https" if self._use_ssl else "http",
                self._host,
                self.port,
                endpoint,
                )

        return url

    def _make_request_args(self, descriptor_msg, auth=None):
        if type(descriptor_msg) not in DescriptorOnboarder.DESC_SERIALIZER_MAP:
            raise TypeError("Invalid descriptor message type")

        serializer = DescriptorOnboarder.DESC_SERIALIZER_MAP[type(descriptor_msg)]
        json_data = serializer.to_json_string(descriptor_msg)
        url = self._get_url(descriptor_msg)

        request_args = dict(
            url=url,
            data=json_data,
            headers=self._get_headers(auth),
            auth=DescriptorOnboarder.AUTH,
            verify=False,
            cert=(self._ssl_cert, self._ssl_key) if self._use_ssl else None,
            timeout=self.timeout,
        )

        return request_args

    def update(self, descriptor_msg, auth=None):
        """ Update the descriptor config

        Arguments:
            descriptor_msg - A descriptor proto-gi msg
            auth - the authorization header

        Raises:
            UpdateError - The descriptor config update failed
        """
        request_args = self._make_request_args(descriptor_msg, auth)
        try:
            response = requests.put(**request_args)
            response.raise_for_status()
        except requests.exceptions.ConnectionError as e:
            msg = "Could not connect to restconf endpoint: %s" % str(e)
            self._log.error(msg)
            raise UpdateError(msg) from e
        except requests.exceptions.HTTPError as e:
            msg = "PUT request to %s error: %s" % (request_args["url"], response.text)
            self._log.error(msg)
            raise UpdateError(msg) from e
        except requests.exceptions.Timeout as e:
            msg = "Timed out connecting to restconf endpoint: %s", str(e)
            self._log.error(msg)
            raise UpdateError(msg) from e

    def onboard(self, descriptor_msg, auth=None):
        """ Onboard the descriptor config

        Arguments:
            descriptor_msg - A descriptor proto-gi msg
            auth - the authorization header

        Raises:
            OnboardError - The descriptor config update failed
        """

        request_args = self._make_request_args(descriptor_msg, auth)
        try:
            response = requests.post(**request_args)
            response.raise_for_status()
        except requests.exceptions.ConnectionError as e:
            msg = "Could not connect to restconf endpoint: %s" % str(e)
            self._log.error(msg)
            raise OnboardError(msg) from e
        except requests.exceptions.HTTPError as e:
            msg = "POST request to %s error: %s" % (request_args["url"], response.text)
            self._log.error(msg)
            raise OnboardError(msg) from e
        except requests.exceptions.Timeout as e:
            msg = "Timed out connecting to restconf endpoint: %s", str(e)
            self._log.error(msg)
            raise OnboardError(msg) from e

