
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import glob
import itertools
import os

import boto
import boto.vpc

# TODO:  Pull the lastest of owned instances.
__default_instance_ami__ = 'ami-e421bc8c'

# TODO:  Make VPC's per user?
__default_subnet__ = 'subnet-4b484363'
__default_security_group__ = 'sg-d9da90bc'

__default_instance_type__ = 'm1.medium'
__default_vpc__ = 'vpc-e7ed4482'

class RWEC2(object):
    def __init__(self,  subnet=None, ami=None):
        self._subnet = subnet if subnet is not None else __default_subnet__
        self._ami = ami if ami is not None else __default_instance_ami__

        self._conn = boto.connect_ec2()

    @staticmethod
    def cloud_init_current_user():
        """
        Return user_data configuration suitable for cloud-init that will create a user
        with sudo and ssh key access on the remote instance.

        ssh keys are found with the glob ~/.ssh/*pub*
        """
        user_data = "users:\n"
        user_data += " - name: %s\n" % (os.getlogin(),)
        user_data += "   groups: [wheel, adm, systemd-journal]\n"
        user_data += "   sudo: [\"ALL=(ALL) NOPASSWD:ALL\"]\n"
        user_data += "   shell: /bin/bash\n"
        user_data += "   ssh_authorized_keys:\n"
        for pub_key in glob.glob('%s/.ssh/*pub*' % (os.environ['HOME'],)):
            with open(pub_key) as fp:
                user_data += "    -  %s" % (fp.read(),)

        return user_data


    @staticmethod
    def cloud_init_yum_repos():
        """
        Return a string of user_data commands that can be used to update the yum
        repos to point to the correct location.  They should be added by the caller
        within a 'runcmd:' block.
        """
        ret = " - sed -i -e 's,www\.,,' -e 's,riftio\.com/mirrors,riftio.com:8881,' /etc/yum.repos.d/*.repo\n"
        return ret

    def instances(self, cluster_component, cluster_instance):
        """
        List of instances owned by the given cluster instance

        @param cluster_component  - parent cluster of each instance
        @param cluster_instance   - instance id of the owning cluster
        @param n_instances        - number of requested instances

        @return                   - list of boto.ec2.instance.Instances provisioned
        """
        ret = []
        reservations = self._conn.get_all_instances()
        for instance in [instance for reservation in reservations for instance in reservation.instances]:
            tags = instance.tags
            if (tags.get('parent_component') == cluster_component
                    and tags.get('parent_instance') == cluster_instance):
                ret.append(instance)

        return ret

    def provision_master(self, cluster_component, cluster_instance):
        """
        Provision a master instance in EC2.  The master instance is a special instance with the
        following features:
            - Public IP
            - /home shared over NFS

        @param cluster_component  - parent cluster of each instance
        @param cluster_instance   - instance id of the owning cluster

        @return                   - boto.ec2.instance.Instances provisioned
        """
        vpc = boto.vpc.VPCConnection()
        subnet = vpc.get_all_subnets(subnet_ids=__default_subnet__)[0]
        cidr_block = subnet.cidr_block
        vpc.close()

        user_data = "#cloud-config\n"
        user_data += "runcmd:\n"
        user_data += " - echo '/home %s(rw,root_squash,sync)' >  /etc/exports\n" % (cidr_block,)
        user_data += " - systemctl start nfs-server\n"
        user_data += " - systemctl enable nfs-server\n"
        user_data += self.cloud_init_yum_repos()
        user_data += self.cloud_init_current_user()


        net_if = boto.ec2.networkinterface.NetworkInterfaceSpecification(
                subnet_id=__default_subnet__,
                groups=[__default_security_group__,],
                associate_public_ip_address=True)

        net_ifs = boto.ec2.networkinterface.NetworkInterfaceCollection(net_if)

        new_reservation = self._conn.run_instances(
                image_id=self._ami,
                min_count=1,
                max_count=1,
                instance_type=__default_instance_type__,
                network_interfaces=net_ifs,
                tenancy='default',
                user_data=user_data)
        instance = new_reservation.instances[0]

        instance.add_tag('parent_component', cluster_component)
        instance.add_tag('parent_instance', cluster_instance)
        instance.add_tag('master', 'self')

        return instance


    def provision(self, cluster_component, cluster_instance, n_instances=1, master_instance=None, net_ifs=None):
        """
        Provision a number of EC2 instanced to be used in a cluster.

        @param cluster_component  - parent cluster of each instance
        @param cluster_instance   - instance id of the owning cluster
        @param n_instances        - number of requested instances
        @param master_instance    - if specified, the boto.ec2.instance.Instance that is providing master
                                    services for this cluster

        @return                   - list of boto.ec2.instance.Instances provisioned
        """
        instances = []
        cluster_instance = int(cluster_instance)

        def posess_instance(instance):
            instances.append(instance)
            instance.add_tag('parent_component', cluster_component)
            instance.add_tag('parent_instance', cluster_instance)
            if master_instance is not None:
                instance.add_tag('master', master_instance.id)
            else:
                instance.add_tag('master', 'None')

        user_data = "#cloud-config\n"
        user_data += self.cloud_init_current_user()
        user_data += "runcmd:\n"
        user_data += self.cloud_init_yum_repos()

        if master_instance is not None:
            user_data += " - echo '%s:/home /home nfs rw,soft,sync 0 0' >> /etc/fstab\n" % (
                    master_instance.private_ip_address,)
            user_data += " - mount /home\n"

        if net_ifs is not None:
            kwds = {'subnet_id': __default_subnet__}
        else:
            kwds = {'network_interfaces': net_ifs}
            print net_ifs

        new_reservation = self._conn.run_instances(
            image_id=self._ami,
            min_count=n_instances,
            max_count=n_instances,
            instance_type=__default_instance_type__,
            tenancy='default',
            user_data=user_data,
            network_interfaces=net_ifs)

        _ = [posess_instance(i) for i in new_reservation.instances]

        return instances

    def stop(self, instance_id, free_resources=True):
        """
        Stop the specified instance, freeing all allocated resources (elastic ips, etc) if requested.

        @param instance_id      - name of the instance to stop
        @param free_resource    - If True that all resources that were only owned by this instance
                                  will be deallocated as well.
        """
        self._conn.terminate_instances(instance_ids=[instance_id,])

    def fastpath111(self):
        vpc_conn = boto.vpc.VPCConnection()
        vpc = vpc_conn.get_all_vpcs(vpc_ids=[__default_vpc__,])[0]
        subnet_addrs_split = vpc.cidr_block.split('.')

        networks = {
            'mgmt': [s for s in vpc_conn.get_all_subnets() if s.id == __default_subnet__][0],
            'tg_fabric': None,
            'ts_fabric': None,
            'tg_lb_ext': None,
            'lb_ts_ext': None,
        }

        for i, network in enumerate([n for n, s in networks.items() if s == None]):
            addr = "%s.%s.10%d.0/25" % (subnet_addrs_split[0], subnet_addrs_split[1], i)
            try:
                subnet = vpc_conn.create_subnet(vpc.id, addr)
            except boto.exception.EC2ResponseError, e:
                if 'InvalidSubnet.Conflict' == e.error_code:
                    subnet = vpc_conn.get_all_subnets(filters=[('vpcId', vpc.id), ('cidrBlock', addr)])[0]
                else:
                    raise

            networks[network] = subnet

        def create_interfaces(nets):
            ret = boto.ec2.networkinterface.NetworkInterfaceCollection()

            for i, network in enumerate(nets):
                spec = boto.ec2.networkinterface.NetworkInterfaceSpecification(
                        subnet_id=networks[network].id,
                        description='%s iface' % (network,),
                        groups=[__default_security_group__],
                        device_index=i)
                ret.append(spec)

            return ret

        ret = {}

        ret['cli'] = self.provision_master('fp111', 1)
        ret['cli'].add_tag('Name', 'cli')

        net_ifs = create_interfaces(['mgmt'])
        ret['mgmt'] = self.provision('fp111', 1, master_instance=ret['cli'], net_ifs=net_ifs)[0]
        ret['mgmt'].add_tag('Name', 'mgmt')

        net_ifs = create_interfaces(['mgmt', 'tg_fabric'])
        ret['tg1'] = self.provision('fp111', 1, master_instance=ret['cli'], net_ifs=net_ifs)[0]
        ret['tg1'].add_tag('Name', 'tg1')

        net_ifs = create_interfaces(['mgmt', 'tg_fabric', 'tg_lb_ext'])
        ret['tg2'] = self.provision('fp111', 1, master_instance=ret['cli'], net_ifs=net_ifs)[0]
        ret['tg2'].add_tag('Name', 'tg2')

        net_ifs = create_interfaces(['mgmt', 'ts_fabric'])
        ret['ts1'] = self.provision('fp111', 1, master_instance=ret['cli'], net_ifs=net_ifs)[0]
        ret['ts1'].add_tag('Name', 'ts1')

        net_ifs = create_interfaces(['mgmt', 'ts_fabric', 'lb_ts_ext'])
        ret['ts3'] = self.provision('fp111', 1, master_instance=ret['cli'], net_ifs=net_ifs)[0]
        ret['ts3'].add_tag('Name', 'ts3')

        net_ifs = create_interfaces(['mgmt', 'ts_fabric', 'lb_ts_ext', 'tg_lb_ext'])
        ret['ts2'] = self.provision('fp111', 1, master_instance=ret['cli'], net_ifs=net_ifs)[0]
        ret['ts2'].add_tag('Name', 'ts2')

        return ret

# vim: sw=4
