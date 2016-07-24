
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio

from gi.repository import (
    RwDts,
    RwLaunchpadYang,
)

import rift.openmano.openmano_client as openmano_client
import rift.tasklets


class DataCenterPublisher(object):
    """
    This class is reponsible for exposing the data centers associated with an
    openmano cloud account.
    """

    XPATH = "D,/rw-launchpad:datacenters"

    def __init__(self, tasklet):
        """Creates an instance of a DataCenterPublisher

        Arguments:
            tasklet - the tasklet that this publisher is registered for

        """
        self.tasklet = tasklet
        self.reg = None

    @property
    def dts(self):
        """The DTS instance used by this tasklet"""
        return self.tasklet.dts

    @property
    def log(self):
        """The logger used by this tasklet"""
        return self.tasklet.log

    @property
    def loop(self):
        """The event loop used by this tasklet"""
        return self.tasklet.loop

    @property
    def accounts(self):
        """The known openmano cloud accounts"""
        accounts = list()
        for acc in self.tasklet.cloud_accounts:
            if acc.account_type == "openmano":
                accounts.append(acc.account_msg)

        return accounts

    @asyncio.coroutine
    def register(self):
        """Registers the publisher with DTS"""

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            try:
                # Create a datacenters instance to hold all of the cloud
                # account data.
                datacenters = RwLaunchpadYang.DataCenters()

                # Iterate over the known openmano accounts and populate cloud
                # account instances with the corresponding data center info
                for account in self.accounts:
                    try:
                        cloud_account = RwLaunchpadYang.CloudAccount()
                        cloud_account.name = account.name

                        # Create a client for this cloud account to query for
                        # the associated data centers
                        client = openmano_client.OpenmanoCliAPI(
                                self.log,
                                account.openmano.host,
                                account.openmano.port,
                                account.openmano.tenant_id,
                                )

                        # Populate the cloud account with the data center info
                        for uuid, name in client.datacenter_list():
                            cloud_account.datacenters.append(
                                    RwLaunchpadYang.DataCenter(
                                        uuid=uuid,
                                        name=name,
                                        )
                                    )

                        datacenters.cloud_accounts.append(cloud_account)

                    except Exception as e:
                        self.log.exception(e)

                xact_info.respond_xpath(
                        RwDts.XactRspCode.MORE,
                        'D,/rw-launchpad:datacenters',
                        datacenters,
                        )

                xact_info.respond_xpath(RwDts.XactRspCode.ACK)

            except Exception as e:
                self.log.exception(e)
                raise

        handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare)

        with self.dts.group_create() as group:
            self.reg = group.register(
                    xpath=DataCenterPublisher.XPATH,
                    handler=handler,
                    flags=RwDts.Flag.PUBLISHER,
                    )
