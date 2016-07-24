import sys
import asyncio
from gi import require_version
require_version('RwcalYang', '1.0')
require_version('RwTypes', '1.0')
require_version('RwCloudYang', '1.0')

from gi.repository import (
        RwTypes,
        RwcalYang,
        RwCloudYang,
        )
import rw_peas

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class PluginLoadingError(Exception):
    pass


class CloudAccountCalError(Exception):
    pass


class CloudAccount(object):
    def __init__(self, log, rwlog_hdl, account_msg):
        self._log = log
        self._account_msg = account_msg.deep_copy()

        self._cal_plugin = None
        self._engine = None

        self._cal = self.plugin.get_interface("Cloud")
        self._cal.init(rwlog_hdl)

        self._status = RwCloudYang.CloudAccount_ConnectionStatus(
                status="unknown",
                details="Connection status lookup not started"
                )

        self._validate_task = None

    @property
    def plugin(self):
        if self._cal_plugin is None:
            try:
                self._cal_plugin = rw_peas.PeasPlugin(
                        getattr(self._account_msg, self.account_type).plugin_name,
                        'RwCal-1.0',
                        )

            except AttributeError as e:
                raise PluginLoadingError(str(e))

            self._engine, _, _ = self._cal_plugin()

        return self._cal_plugin

    def _wrap_status_fn(self, fn, *args, **kwargs):
        ret = fn(*args, **kwargs)
        rw_status = ret[0]
        if rw_status != RwTypes.RwStatus.SUCCESS:
            msg = "%s returned %s" % (fn.__name__, str(rw_status))
            self._log.error(msg)
            raise CloudAccountCalError(msg)

        # If there was only one other return value besides rw_status, then just
        # return that element.  Otherwise return the rest of the return values
        # as a list.
        return ret[1] if len(ret) == 2 else ret[1:]

    @property
    def cal(self):
        return self._cal

    @property
    def name(self):
        return self._account_msg.name

    @property
    def account_msg(self):
        return self._account_msg

    @property
    def cal_account_msg(self):
        return RwcalYang.CloudAccount.from_dict(
                self.account_msg.as_dict(),
                ignore_missing_keys=True,
                )

    def cloud_account_msg(self, account_dict):
        self._account_msg = RwCloudYang.CloudAccount.from_dict(account_dict)

    @property
    def account_type(self):
        return self._account_msg.account_type

    @property
    def connection_status(self):
        return self._status

    def update_from_cfg(self, cfg):
        self._log.debug("Updating parent CloudAccount to %s", cfg)

        # Hack to catch updates triggered from apply_callback when a sdn-account is removed
        # from a cloud-account. To be fixed properly when updates are handled
        if (self.account_msg.name == cfg.name
                and self.account_msg.account_type == cfg.account_type):
            return

        if cfg.has_field("sdn_account"):
            self.account_msg.sdn_account = cfg.sdn_account
        else:
            raise NotImplementedError("Update cloud account not yet supported")

    def create_image(self, image_info_msg):
        image_id = self._wrap_status_fn(
                self.cal.create_image, self.cal_account_msg, image_info_msg
                )

        return image_id

    def get_image_list(self):
        self._log.debug("Getting image list from account: %s", self.name)
        resources = self._wrap_status_fn(
                self.cal.get_image_list, self.cal_account_msg
                )

        return resources.imageinfo_list

    @asyncio.coroutine
    def validate_cloud_account_credentials(self, loop):
        self._log.debug("Validating Cloud Account credentials %s", self._account_msg)
        self._status = RwCloudYang.CloudAccount_ConnectionStatus(
                status="validating",
                details="Cloud account connection validation in progress"
                )
        rwstatus, status = yield from loop.run_in_executor(
                None,
                self._cal.validate_cloud_creds,
                self.cal_account_msg,
                )
        if rwstatus == RwTypes.RwStatus.SUCCESS:
            self._status = RwCloudYang.CloudAccount_ConnectionStatus.from_dict(status.as_dict())
        else:
            self._status = RwCloudYang.CloudAccount_ConnectionStatus(
                    status="failure",
                    details="Error when calling CAL validate cloud creds"
                    )

        self._log.info("Got cloud account validation response: %s", self._status)

    def start_validate_credentials(self, loop):
        if self._validate_task is not None:
            self._validate_task.cancel()
            self._validate_task = None

        self._validate_task = asyncio.ensure_future(
                self.validate_cloud_account_credentials(loop),
                loop=loop
                )

