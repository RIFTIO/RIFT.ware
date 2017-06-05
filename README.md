RIFT.ware
=========

# RIFT.ware version 4.4.1.1 Release Notes
May 23, 2017


RIFT.ware version 4.4 runs on Ubuntu 16.04.  

See https://open.riftio.com for version 4.4 downloads and documentation. 

Visit https://support.riftio.com to post questions or raise issues.


## What’s New in RIFT.ware 4.4
This release offers streamlined integration with ETSI MANO.


## Fixed Issues in RIFT.ware 4.4.1.1
* RIFT-11095 An exported image name does not include the NFV object type as a prefix, such as vnfd_ or nsd_.
* RIFT-11300 If a known descriptor element (such as nsd, vnfd) is removed from a valid descriptor, the onboarding operation succeeds when it should error. The failure occurs during network service instantiation.
* RIFT-14080 Due to glance library incompatibilities, you cannot use the command line glance client to interact with the installed Glance server. This issue is no longer relevant. RIFT.io is not releasing FC20 images, and the previous RPM based install system has been replaced. 
* RIFT-14848 In limited scenarios, RIFT.ware does not properly release OpenStack resources following an instantiation failure.
* RIFT-14889  On the Launchpad UI Logging page, the Syslog Viewer URL defaults to 127.0.0.1. The workaround was to edit the URL to replace the localhost IP address (127.0.0.1) with the management IP address of the Launchpad VM or container.
* RIFT-14910 A filter added to syslog sink cannot be deleted using either the CLI or REST API. Workaround: Set severity level to error and delete filter.
* RIFT-15309  When Launchpad is installed on a bare VM with multiple interfaces, you cannot connect to the REST server on port 8008 if there is no eth0 or or if eth0 is not the management interface. The workaround was to use port 8888.
* RIFT-16155, RIFT-15001 RIFT.ware running on Ubuntu 16.04 occasionally fails with heartbeat timeouts after instantiating more than 5 network services.   


## Known Issues in RIFT.ware 4.4.1.1
* RIFT-11010 If you delete a network service and immediately go to the Launchpad Catalog page, the Catalog still shows the network service in use with a "(1)" appended to the descriptor name. The Catalog does not automatically check if the number of network service records (NSR) using that network service descriptor (NSD) has changed. Therefore, it continues to show (1) and prevents the deletion until you navigate away from the page and come back or refresh the page.
* RIFT-11305 Adding an unknown attribute to the ping-pong VNF descriptors and network descriptor (ping_vnfd.xml, ping_pong_nsd.xml) results in an onboard failure.
* RIFT-11371 RIFT.ware does not yet support account connectivity status for Amazon Web Services (AWS) VIM accounts.
* RIFT-11488 After starting the network service and terminating the ping VM from the OpenStack Horizon dashboard, the system fails to report that the ping VM has been terminated.
* RIFT-11489 The RIFT.ware Launchpad fails to detect that a virtual link between ping_vnfd and pong_vnfd has been deleted.
* RIFT-14589, RIFT-14998 Some of the recovery operations following a task failure is unsuccessful and RIFT.ware is unable to continue. Workaround: Restart the launchpad service by running the following series of commands: sudo systemctl stop launchpad.service ; sudo systemctl start launchpad.service
* RIFT-14962 If confD fails to come up on a corrupt Launchpad, you can log in to Launchpad using any username/password combination.
* RIFT-14982, RIFT-14970 When trying to instantiate a new network service, certain failure scenarios (not defining a VIM account, choosing a duplicate network service name) does not allow instantiation but does not provide immediate feedback to the operator, beyond failing to begin instantiation. Workaround: Cancel the instantiation operation and check the logs for the failure.
* RIFT-16186  In rare cases, Launchpad times out during an export operation on a previously-onboarded descriptor package.
* RIFT-16502  A descriptor export operation fails to include the original onboarded image in the package.



# RIFT.ware version 4.3.3.1 Release Notes
December 6, 2016


RIFT.ware version 4.3.3.1 runs on Ubuntu 16.04 and Fedora 20.  

See https://open.riftio.com for version 4.3 downloads and documentation. 

Visit https://support.riftio.com to post questions or raise issues.


## What’s New in RIFT.ware 4.3.3.1
This release offers streamlined options to build RIFT.ware from source. See the new Developer Guide for instructions.


## Important Notes
Before you install RIFT.ware, run the following commands to disable the Ubuntu automatic update system. (These updates can cause conflicts with RIFT.ware by installing newer packages that do not contain RIFT.ware enhancements.)
sudo systemctl disable apt-daily.service
sudo systemctl disable apt-daily.timer 

RIFT.ware 4.3 is the last release of Fedora 20 support. RIFT.io strongly recommends that you use Ubuntu 16.04 for your RIFT.ware installation. 


## Fixed Issues in RIFT.ware 4.3.3.1
* RIFT-14989  On the RIFT.ware Launchpad interface, if you click Logout from the RO Config page and log in again, the RO Config page displays details for the non-default account (openmano). If you click Cancel, other pages on the UI may not render properly. This issue has been resolved.


## Known Issues in RIFT.ware 4.3.3.1
* RIFT-11010  If you delete a network service and immediately go to the Launchpad Catalog page, the Catalog still shows the network service in use with a "(1)" appended to the descriptor name. The Catalog does not automatically check if the number of network service records (NSR) using that network service descriptor (NSD) has changed. Therefore, it continues to show (1) and prevents the deletion until you navigate away from the page and come back or refresh the page.
* RIFT-11095  An exported image name does not include the NFV object type as a prefix, such as vnfd_ or nsd_. 
* RIFT-11300  If an element is removed from a valid descriptor, the onboarding operation succeeds when it should error. The failure occurs during network service instantiation.
* RIFT-11305  Adding an unknown attribute to the ping-pong VNF descriptors and network descriptor (ping_vnfd.xml, ping_pong_nsd.xml) results in an onboard failure.
* RIFT-11371  RIFT.ware does not yet support account connectivity status for Amazon Web Services (AWS) VIM accounts.
* RIFT-11488  After starting the network service and terminating the ping VM from the OpenStack Horizon dashboard, the system fails to report that the ping VM has been terminated.
* RIFT-11489  The RIFT.ware Launchpad fails to detect that a virtual link between ping_vnfd and pong_vnfd has been deleted.
* RIFT-14080  Due to glance library incompatibilities, you cannot use the command line glance client to interact with the installed Glance server.
* RIFT-14589, RIFT-14998  Some of the recovery operations following a task failure is unsuccessful and RIFT.ware is unable to continue. Workaround: Restart the launchpad service by running the following series of commands: sudo systemctl stop launchpad.service ; sudo systemctl start launchpad.service 
* RIFT-14848  In limited scenarios, RIFT.ware does not properly release OpenStack resources following an instantiation failure.
* RIFT-14889  On the Launchpad UI Logging page, the Syslog Viewer URL defaults to 127.0.0.1. Workaround: Edit the URL to replace the localhost IP address (127.0.0.1) with the management IP address of the Launchpad VM or container.
* RIFT-14910  A filter added to syslog sink cannot be deleted using either the CLI or REST API. Workaround: Set severity level to error and delete filter.
* RIFT-14962  If confD fails to come up on a corrupt Launchpad, you can log in to Launchpad using any username/password combination.
* RIFT-14982, RIFT-14970  When trying to instantiate a new network service, certain failure scenarios (not defining a VIM account, choosing a duplicate network service name) does not allow instantiation but does not provide immediate feedback to the operator, beyond failing to begin instantiation. Workaround: Cancel the instantiation operation and check the logs for the failure.
* RIFT-15001  RIFT.ware running on Ubuntu 16.04 occasionally fails with heartbeat timeouts after instantiating more than 5 network services.
* RIFT-15309  When Launchpad is installed on a bare VM with multiple interfaces, the user will not be able to connect to the REST server on port 8888 if there is no eth0 or if eth0 is not the management interface. Workaround: Use port 8008.



# RIFT.ware version 4.3.3.0 Release Notes
October 31, 2016


RIFT.ware version 4.3.3.0 runs on Ubuntu 16.04 and Fedora 20.  

See https://open.riftio.com for version 4.3 downloads and documentation. 

Visit https://support.riftio.com to post questions or raise issues.


## Important Notes
RIFT.ware 4.3 is the last release of Fedora 20 support. RIFT.io strongly recommends that you use Ubuntu 16.04 for your RIFT.ware installation. 

If you are running RIFT.ware on Fedora 20, upgrade instructions are coming soon.

Source code is coming soon.


## What’s New in RIFT.ware 4.3
See “New and Changed Features in RIFT.ware 4.3” at http://open.riftio.com/webdocs/RIFTware-4.3/a/new/new-features-changes-riftware-4_3.htm


## Fixed Issues in RIFT.ware 4.3.3.0
* RIFT-12354  The Launchpad interface fails to return a message when cloud accounts are created with invalid information. This feature has been resolved by the new “Check Connectivity” feature on the Launchpad Accounts page.
* RIFT-13673 DTS was unable to complete the transaction, causing the transaction to fail.


## Known Issues in RIFT.ware 4.3.3.0
* RIFT-11010  If you delete a network service and immediately go to the Launchpad Catalog page, the Catalog still shows the network service in use with a "(1)" appended to the descriptor name. The Catalog does not automatically check if the number of network service records (NSR) using that network service descriptor (NSD) has changed. Therefore, it continues to show (1) and prevents the deletion until you navigate away and come back or refresh the page.
* RIFT-11095  An exported image name does not include the NFV object type as a prefix, such as vnfd_ or nsd_. 
* RIFT-11300  If an element is removed from a valid descriptor, the onboarding operation succeeds when it should error. The failure occurs during network service instantiation.
* RIFT-11305  Adding an unknown attribute to the ping-pong VNF descriptors and network descriptor (ping_vnfd.xml, ping_pong_nsd.xml) results in an onboard failure.
* RIFT-11371  RIFT.ware does not yet support account connectivity status for Amazon Web Services (AWS) VIM accounts.
* RIFT-11488  After starting the network service and terminating the ping VM from the OpenStack Horizon dashboard, the system fails to report that the ping VM has been terminated.
* RIFT-11489  The RIFT.ware Launchpad fails to detect that a virtual link between ping_vnfd and pong_vnfd has been deleted.
* RIFT-13652  The minimum qualified resolution for the RIFT.ware User Interface (RW.UI) is 1440px x 900px, which is the resolution of a HiDPI screen. RW.UI generally works on MDPI screens (1280px x 800px), but you might encounter viewing issues on some pages. If problems occur with an MDPI or lower resolution screen, use your browser controls to zoom out until the entire page becomes visible so you can perform the tasks you need to do.
* RIFT-14080  Due to glance library incompatibilities, you cannot use the command line glance client to interact with the installed Glance server.
* RIFT-14589, RIFT-14998  Some of the recovery operations following a task failure is unsuccessful and RIFT.ware is unable to continue. Workaround: Restart the launchpad service by running the following series of commands: sudo systemctl stop launchpad.service ; sudo systemctl start launchpad.service 
* RIFT-14848  In limited scenarios, RIFT.ware does not properly release OpenStack resources following an instantiation failure.
* RIFT-14889  On the Launchpad UI Logging page, the Syslog Viewer URL defaults to 127.0.0.1. Workaround: Edit the URL to replace the localhost IP address (127.0.0.1) with the management IP address of the Launchpad VM or container.
* RIFT-14910  A filter added to syslog sink cannot be deleted using either the CLI or REST API. Workaround: Set severity level to error and delete filter.
* RIFT-14962  If confD fails to come up on a corrupt Launchpad, you can log in to Launchpad using any username/password combination.
* RIFT-14982, RIFT-14970  When trying to instantiate a new network service, certain failure scenarios (not defining a VIM account, choosing a duplicate network service name) does not allow instantiation but does not provide immediate feedback to the operator, beyond failing to begin instantiation. Workaround: Cancel the instantiation operation and check the logs for the failure.
* RIFT-14989  On the RIFT.ware UI, if you click Logout from the RO Config page and log in again, the RO Config page displays details for the non-default account (openmano). If you click Cancel, other pages on the UI may not render properly. Workarounds: (1) Do not click Cancel from the RO Config page. (2) Update the URL to https://<IP-ADDR>:8443/ and delete the rest of URL to view the RO Config default view (rift-ro).
* RIFT-15001  RIFT.ware running on Ubuntu 16.04 occasionally fails with heartbeat timeouts after instantiating more than 5 network services.


# RIFT.ware version 4.2.2.0 Release Notes
June 17, 2016

RIFT.ware version 4.2.2.0 is available as RPMs. See https://open.riftio.com for the latest downloads and documentation. 

Visit https://support.riftio.com to post questions or raise issues.

## What’s New in RIFT.ware 4.2
Significant changes introduced in 4.2 are described below. To see a complete list of new and changed features in this release, see New Features and Changes in RIFT.ware 4.2 at http://open.riftio.com/webdocs/RIFTware-4.2/Authoring/NewFeatures/NewRiftware4_2.htm

#### Support for commercial OpenStack VIM suppliers
RIFT.ware supports the following commercial OpenStack VIM suppliers:
- Red Hat Enterprise Linux
- Mirantis OpenStack (Ubuntu 14.04)
- Canonical (Ubuntu 14.04)

#### Event-triggered network service autoscaling
RIFT.ware introduces horizontal scaling of network services. RIFT.ware can receive event triggers from an external system through a REST API to scale a network service to meet demand. The scaling action effected by MANO can also trigger other actions, such as program new flows into a Load Balancer to spread loads across newly-instantiated VNFs.

#### Information model translation
RIFT.ware can read in MANO descriptors that are in TOSCA or YAML format. You can onboard these descriptors in both NS AND VNF packages to the Launchpad.

#### RIFT.ware UI visualization of data center network
The RIFT.ware UI Viewport displays the physical and logical topology of your data center.

#### Enhanced NFV infrastructure metrics
Release 4.2 supports NFVI statistics and threshold crossing alerts into RIFT.ware. RIFT.ware displays notifications for NFVI level alerts that affect RIFT.ware or the VNFs and NS that RIFT.ware manages.

#### Improved error message reporting
The RIFT.ware MANO subsystem generates NETCONF notifications and alarms for significant lifecycle events, such as up, down, or failed network service instantiation. This enhancement is designed to help you more quickly identify and resolve issues.

#### IPv6 support
RIFT.ware is now fully IPv6 capable.

#### Behavior change for accessing the RIFT.ware Launchpad
The RIFT.ware user interface (RW.UI) uses Secure Socket Layer (SSL) to secure HTTPS requests between your browser and the Launchpad. If you do not have a valid root CA signed certificate, see “Avoiding Self-signed Certificate Warnings” in the RIFT.ware 4.2 documentation.

The Launchpad now requires HTTPS to connect from a browser to the RIFT.ware UI. Type the IP address or fully qualified domain name of the Launchpad virtual machine, followed by port 8443. 


## Fixed Issues in RIFT.ware 4.2.2.0
* RIFT-11507  Onboarding a malformed ping_vnfd package with incorrect checksum.txt content succeeds when it should fail with error. 
* RIFT-11508  Onboarding a malformed ping_vnfd package with incorrect checksum.txt value succeeds when it should fail with error. 
* RIFT-12460  Launchpad should support updates of Cloud/SDN/Config Agent accounts.
* RIFT-13287  Deleting and recreating the same OpenStack account causes an exception.


## Known Issues in RIFT.ware 4.2.2.0
* RIFT-11010  On the Launchpad interface, descriptors cannot be deleted on the Catalog page. Workaround: Either refresh the Catalog page or navigate to the Launchpad Dashboard and back to the Catalog to remove a descriptor.
* RIFT-11095  An exported image name does not include the NFV object type as a prefix, such as vnfd_ or nsd_. 
* RIFT-11300  If an element is removed from a valid descriptor, the onboarding operation succeeds when it should error. The failure occurs during network service instantiation.
* RIFT-11305  Adding an unknown attribute to the ping-pong VNF descriptors and network descriptor (ping_vnfd.xml, ping_pong_nsd.xml) results in an onboard failure.
* RIFT-11488  After starting the network service and terminating the ping VM from the OpenStack Horizon dashboard, the system fails to report that the ping VM has been terminated.
* RIFT-11489  The RIFT.ware Launchpad fails to detect that a virtual link between ping_vnfd and pong_vnfd has been deleted.
* RIFT-12354  The Launchpad interface fails to return a message when cloud accounts are created with invalid information.
* RIFT-13652  The minimum qualified resolution for the RIFT.ware User Interface (RW.UI) is 1440px x 900px, which is the resolution of a HiDPI screen. RW.UI generally works on MDPI screens (1280px x 800px), but you might encounter viewing issues on some pages. If problems occur with an MDPI or lower resolution screen, use your browser controls to zoom out until the entire page becomes visible so you can perform the tasks you need to do.
* RIFT-13673 DTS was unable to complete the transaction, causing the transaction to fail.
