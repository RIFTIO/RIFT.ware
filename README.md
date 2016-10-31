RIFT.ware
=========

# RIFT.ware version 4.3.3.0 Release Notes
October 31, 2016

RIFT.ware version 4.3.3.0 runs on Ubuntu 16.04 and Fedora 20.  

See https://open.riftio.com for the latest downloads and documentation. 

Visit https://support.riftio.com to post questions or raise issues.

## What’s New in RIFT.ware 4.2
RIFT.ware version 4.3.3 introduces many enhancements to prepare the MANO subsystem for production deployments and to improve the user experience. See the RIFT.ware 4.3 New and Changed Features Guide at http://open.riftio.com/webdocs/RIFTware-4.3/a/new/new-features-changes-riftware-4_3.htm

## Important Notes
RIFT.ware 4.3 is the last release of Fedora 20 support. RIFT.io strongly recommends that you use Ubuntu 16.04 for your RIFT.ware installation. 

If you are running RIFT.ware on Fedora 20, upgrade instructions are coming soon.

## Fixed Issues
* RIFT-12354  The Launchpad interface fails to return a message when cloud accounts are created with invalid information. This feature has been resolved by the new “Check Connectivity” feature on the Launchpad Accounts page. 
* RIFT-13673 DTS was unable to complete the transaction, causing the transaction to fail.

## Known Issues
* RIFT-11010  If you delete a network service and immediately go to the Launchpad Catalog page, the Catalog still shows the network service in use with a "(1)" appended to the descriptor name. The Catalog does not automatically check if the number of network
service records (NSR) using that network service descriptor (NSD) has changed. Therefore, it continues to show (1) and prevents the deletion until you navigate away and come back or refresh the page.
* RIFT-11095  An exported image name does not include the NFV object type as a prefix, such as VNFD or NSD. 
* RIFT-11300  If an element is removed from a valid descriptor, the onboarding operation succeeds when it should error. The failure occurs during network service instantiation.
* RIFT-11305  Adding an unknown attribute to the ping-pong VNF descriptors and network descriptor (ping_vnfd.xml, ping_pong_nsd.xml) results in an onboard failure.
* RIFT-11371  RIFT.ware does not yet support Account Connectivity Status checking with Amazon Web Services (AWS) VIM accounts.
* RIFT-11488  After starting the network service and terminating the ping VM from the OpenStack Horizon dashboard, the system fails to report that the ping VM has been terminated.
* RIFT-11489  The RIFT.ware Launchpad fails to detect that a virtual link between ping_vnfd and pong_vnfd has been deleted.
* RIFT-13652  The minimum qualified resolution for the RIFT.ware User Interface (RW.UI) is 1440px x 900px, which is the resolution of a HiDPI screen. RW.UI generally works on MDPI screens (1280px x 800px), but you might encounter viewing issues on some pages. If problems occur with an MDPI or lower resolution screen, use your browser controls to zoom out until the entire page becomes visible so you can perform the tasks you need to do.
* RIFT-14080  Due to glance library incompatibilities, you will not be able to use the command line glance client to interact with the installed glance server.
* RIFT-14589, RIFT-14998  Some of the recovery operations following a task failure is unsuccessful and RIFT.ware is unable to continue. Workaround: Restart the launchpad service by running the following series of commands:
$ sudo systemctl stop launchpad.service
$ sudo systemctl start launchpad.service 
* RIFT-14848  In limited scenarios, RIFT.ware does not properly release OpenStack resources following an instantiation failure.
* RIFT-14889  On the Launchpad UI Logging page, the Syslog Viewer URL defaults to 127.0.0.1. Workaround: Edit the URL by replacing the localhost IP address (127.0.0.1) with the management IP address of the Launchpad VM or container.
* RIFT-14910  A filter added to syslog sink cannot be deleted using either CLI or REST API. Workaround: Set severity level to Error to delete the filter.
* RIFT-14962  If confd fails to come up on a corrupt Launchpad, you can log in to Launchpad using any username/password combination.
* RIFT-14982, RIFT-14970  When trying to instantiate a new network service, certain failure scenarios (not defining a VIM account, choosing a duplicate network service name) will not allow instantiation but do not provide immediate feedback to the operator, beyond refusing to begin instantiation. Workaround: Cancel the instantiation and check the logs for the failure.
* RIFT-14989  On the RIFT.ware UI, if you click Logout from the RO Config page and log in again, the RO Config page displays details for the non-default account (openmano). If you click Cancel, other pages on the UI may not render properly. Workarounds: (1) Do not click Cancel from the RO Config page. (2) Update the URL to https://<IP-ADDR>:8443/ and delete the rest of URL to view the RO Config default view (rift-ro).
* RIFT-15001  RIFT.ware running on Ubuntu 16.04 occasionally fails with heartbeat timeouts after instantiating more than 5 Network Services.
