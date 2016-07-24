import asyncio
import rift.tasklets

from gi.repository import(
        RwCloudYang,
        RwDts as rwdts,
        )

class CloudAccountNotFound(Exception):
    pass


class CloudAccountDtsOperdataHandler(object):
    def __init__(self, dts, log, loop):
        self._dts = dts
        self._log = log
        self._loop = loop

        self.cloud_accounts = {}

    def add_cloud_account(self, account):
        self.cloud_accounts[account.name] = account
        account.start_validate_credentials(self._loop)

    def delete_cloud_account(self, account_name):
        del self.cloud_accounts[account_name]

    def get_saved_cloud_accounts(self, cloud_account_name):
        ''' Get Cloud Account corresponding to passed name, or all saved accounts if name is None'''
        saved_cloud_accounts = []

        if cloud_account_name is None or cloud_account_name == "":
            cloud_accounts = list(self.cloud_accounts.values())
            saved_cloud_accounts.extend(cloud_accounts)
        elif cloud_account_name in self.cloud_accounts:
            account = self.cloud_accounts[cloud_account_name]
            saved_cloud_accounts.append(account)
        else:
            errstr = "Cloud account {} does not exist".format(cloud_account_name)
            raise KeyError(errstr)

        return saved_cloud_accounts

    @asyncio.coroutine
    def create_notification(self, account):
        xpath = "N,/rw-cloud:cloud-notif"
        ac_status = RwCloudYang.YangNotif_RwCloud_CloudNotif()
        ac_status.name = account.name
        ac_status.message = account.connection_status.details

        yield from self._dts.query_create(xpath, rwdts.XactFlag.ADVISE, ac_status)
        self._log.info("Notification called by creating dts query: %s", ac_status)


    def _register_show_status(self):
        def get_xpath(cloud_name=None):
            return "D,/rw-cloud:cloud/account{}/connection-status".format(
                    "[name='%s']" % cloud_name if cloud_name is not None else ''
                    )

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            path_entry = RwCloudYang.CloudAccount.schema().keyspec_to_entry(ks_path)
            cloud_account_name = path_entry.key00.name
            self._log.debug("Got show cloud connection status request: %s", ks_path.create_string())

            try:
                saved_accounts = self.get_saved_cloud_accounts(cloud_account_name)
                for account in saved_accounts:
                    connection_status = account.connection_status
                    self._log.debug("Responding to cloud connection status request: %s", connection_status)
                    xact_info.respond_xpath(
                            rwdts.XactRspCode.MORE,
                            xpath=get_xpath(account.name),
                            msg=account.connection_status,
                            )
            except KeyError as e:
                self._log.warning(str(e))
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        yield from self._dts.register(
                xpath=get_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare),
                flags=rwdts.Flag.PUBLISHER,
                )

    def _register_validate_rpc(self):
        def get_xpath():
            return "/rw-cloud:update-cloud-status"

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            if not msg.has_field("cloud_account"):
                raise CloudAccountNotFound("Cloud account name not provided")

            cloud_account_name = msg.cloud_account
            try:
                account = self.cloud_accounts[cloud_account_name]
            except KeyError:
                raise CloudAccountNotFound("Cloud account name %s not found" % cloud_account_name)

            account.start_validate_credentials(self._loop)

            yield from self.create_notification(account)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        yield from self._dts.register(
                xpath=get_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare
                    ),
                flags=rwdts.Flag.PUBLISHER,
                )

    @asyncio.coroutine
    def register(self):
        yield from self._register_show_status()
        yield from self._register_validate_rpc()
