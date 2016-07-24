RIFT.ware
=========

# RIFT.ware version 4.2.2.0 Release Notes
June 17, 2016

RIFT.ware version 4.2.2.0 is available as RPMs. See https://open.riftio.com for the latest downloads and documentation. 

Visit https://support.riftio.com to post questions or raise issues.

## What’s New in RIFT.ware 4.2
Significant changes introduced in 4.2 are described below. To see a complete list of new and changed features in this release, see the RIFT.ware 4.2 New Features and Changes Guide at http://open.riftio.com/webdocs/RIFTware-4.2/Authoring/NewFeatures/NewRiftware4_2.htm

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



## Fixed Issues
* RIFT-11507  Onboarding a malformed ping_vnfd package with incorrect checksum.txt content succeeds when it should fail with error. 
* RIFT-11508  Onboarding a malformed ping_vnfd package with incorrect checksum.txt value succeeds when it should fail with error. 
* RIFT-12460  Launchpad should support updates of Cloud/SDN/Config Agent accounts.
* RIFT-13287  Deleting and recreating the same OpenStack account causes an exception.


## Known Issues
* RIFT-11010  On the Launchpad interface, descriptors cannot be deleted on the Catalog page. Workaround: Either refresh the Catalog page or navigate to the Launchpad Dashboard and back to the Catalog to remove a descriptor.
* RIFT-11095  An image exported from the RIFT.ware UI does not include the type of NFV object in its name, such as VNFD or NSD.
* RIFT-11300  After element `<vnfd:internal-connection-point-ref>` is removed from an, otherwise, valid descriptor (ping_vnfd.xml), the onboarding step incorrectly succeeds. This element is an internal reference for the VNFD. The omission is discovered during network service instantiation only. 
* RIFT-11305  Adding an unknown attribute to the ping-pong VNF descriptors and network descriptor (ping_vnfd.xml, ping_pong_nsd.xml) causes an onboard failure.
* RIFT-11488  After starting the network service and terminating the ping VM from the OpenStack dashboard, the system fails to report that the ping VM has been terminated.
* RIFT-11489  The RIFT.ware Launchpad fails to detect that a virtual link between ping_vnfd and pong_vnfd has been deleted.
* RIFT-12354  The Launchpad interface fails to return a message when cloud accounts are created with invalid information.
* RIFT-13652  The minimum qualified resolution for the RIFT.ware User Interface (RW.UI) is 1440px x 900px, which is the resolution of a HiDPI screen. RW.UI generally works on MDPI screens (1280px x 800px), but you might encounter viewing issues on some pages. If problems occur with an MDPI or lower resolution screen, use your browser controls to zoom out until the entire page becomes visible so you can perform the tasks you need to do.
* RIFT-13673 DTS was unable to complete the transaction, causing the transaction to fail.
