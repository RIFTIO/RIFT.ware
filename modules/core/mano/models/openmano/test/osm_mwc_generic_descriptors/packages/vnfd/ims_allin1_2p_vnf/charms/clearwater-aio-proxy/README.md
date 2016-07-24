# Overview

This is a [Juju charm](https://jujucharms.com/about), which allows configuration of the [Project Clearwater](http://projectclearwater.org) IMS core's [all-in-one](http://clearwater.readthedocs.org/en/stable/All_in_one_Images/index.html) node.

This is a proxy charm, meaning that you must spin up the all-in-one VM first, and then point this charm at it to manage it.

Since the all-in-one node does not support scaling up, neither does this charm.

# Deployment

## Initial deployment

The all-in-one VM image should be downloaded from [http://repo.cw-ngv.com/juju-clearwater-2/cw-aio.ova](http://repo.cw-ngv.com/juju-clearwater-2/cw-aio.ova) and deployed onto your virtualization platform.  (You could alternatively try the latest all-in-one VM image from [http://vm-images.cw-ngv.com/](http://vm-images.cw-ngv.com/), but this may not have been tested with this charm - the juju-clearwater-2 version above is known to work.)

The proxy charm should then be deployed, pointing at the all-in-one VM.

# Using the All-in-One Node

Once installed, the all-in-one node will listen for SIP traffic on port 5060 (both TCP and UDP).  You can use a standard SIP client (e.g. Blink, Boghe or X-Lite) to register against the all-in-one VM's public IP and make calls.

Our ["Making your first call" documentation](http://clearwater.readthedocs.org/en/latest/Making_your_first_call/index.html) has more information on this process.

# Configuration

-  `proxied_ip`: The IP address of the All-in-One node to manage
-  `password`: The login password of the All-in-One node to manage (default is
 very likely correct)
-  `home_domain`: The home domain for this service
-  `base_number`: The first number to be allocated in the number range
-  `number_count`: The count of numbers to allocate

# Actions

This proxy charm exposes two actions.

-  `create-update-user`: Creates a user, or updates if they already exist
    -  `number`: The number to provision
    -  `password`: The number's password

-  `delete-user`: Deletes a user
    -  `number`: The number to delete

For example, `juju action do clearwater-aio-proxy/0 create-update-user number=\"1234567890\" password=secret` creates a user.  (Note that the escaped double-quotes are required to avoid juju parsing the number as an integer rather than a string.)

Note that the numbers specified in `create-update-user` and `delete-user` actions need not be in the number range specified in the configuration above.

# Contact and Upstream Project Information

Project Clearwater is an open-source IMS core, developed by [Metaswitch Networks](http://www.metaswitch.com) and released under the [GNU GPLv3](http://www.projectclearwater.org/download/license/). You can find more information about it on [our website](http://www.projectclearwater.org/) or [our documentation site](https://clearwater.readthedocs.org).

Clearwater source code and issue list can be found at https://github.com/Metaswitch/.

If you have problems when using Project Clearwater, read [our troubleshooting documentation](http://clearwater.readthedocs.org/en/latest/Troubleshooting_and_Recovery/index.html) for help, or see [our support page](http://clearwater.readthedocs.org/en/latest/Support/index.html) to find out how to ask mailing list questions or raise issues.
