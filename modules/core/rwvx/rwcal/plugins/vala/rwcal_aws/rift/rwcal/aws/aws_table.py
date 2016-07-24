#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


"""
Sizes must be hardcoded, because Amazon doesn't provide an API to fetch them.
From http://aws.amazon.com/ec2/instance-types/
max_inst From http://aws.amazon.com/ec2/faqs/#How_many_instances_can_I_run_in_Amazon_EC2 
paravirt from https://aws.amazon.com/amazon-linux-ami/instance-type-matrix/
"""
INSTANCE_TYPES = {
    'm4.large': {
        'id': 'm4.large',
        'name': 'Large Instance',
        'ram': 8*1024,
        'vcpu': 2,
        'disk': 0,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'm4.xlarge': {
        'id': 'm4.xlarge',
        'name': 'Large Instance',
        'ram': 16*1024,
        'vcpu': 4,
        'disk': 0,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'm4.2xlarge': {
        'id': 'm4.2xlarge',
        'name': 'Large Instance',
        'ram': 32*1024,
        'vcpu': 8,
        'disk': 0,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'm4.4xlarge': {
        'id': 'm4.4xlarge',
        'name': 'Large Instance',
        'ram': 64*1024,
        'vcpu': 16,
        'disk': 0,
        'bandwidth': None,
        'max_inst': 10,
        'sriov': True,
        'paravirt': False
    },
    'm4.10xlarge': {
        'id': 'm4.10xlarge',
        'name': 'Large Instance',
        'ram': 160*1024,
        'vcpu': 40,
        'disk': 0,
        'bandwidth': None,
        'max_inst': 5,
        'sriov': True,
        'paravirt': False
    },
    'm3.medium': {
        'id': 'm3.medium',
        'name': 'Medium Instance',
        'ram': 3.75*1024, #3840
        'vcpu': 1,
        'disk': 4,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': True
    },
    'm3.large': {
        'id': 'm3.large',
        'name': 'Large Instance',
        'ram': 7.5*1024, #7168
        'vcpu': 2,
        'disk': 32,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': True
    },
    'm3.xlarge': {
        'id': 'm3.xlarge',
        'name': 'Extra Large Instance',
        'ram': 15*1024,#15360
        'vcpu': 4,
        'disk': 80,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': True
    },
    'm3.2xlarge': {
        'id': 'm3.2xlarge',
        'name': 'Double Extra Large Instance',
        'ram': 30*1024, #30720
        'vcpu': 8,
        'disk': 160,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': True
    },
    'g2.2xlarge': {
        'id': 'g2.2xlarge',
        'name': 'Cluster GPU G2 Double Extra Large Instance',
        'ram': 15000,
        'disk': 60,
        'vcpu': 5,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': False
    },
    'g2.8xlarge': {
        'id': 'g2.8xlarge',
        'name': 'Cluster GPU G2 Double Extra Large Instance',
        'ram': 60000,
        'disk': 240,
        'vcpu': 2,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': False
    },
    # c4 instances have 2 SSDs of the specified disk size
    'c4.large': {
        'id': 'c4.large',
        'name': 'Compute Optimized Large Instance',
        'ram': 3750,
         'vcpu':2,
        'disk': 0,  #EBS-only
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'c4.xlarge': {
        'id': 'c4.xlarge',
        'name': 'Compute Optimized Extra Large Instance',
        'ram': 7500,
         'vcpu':4,
        'disk': 0,  #EBS-only
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'c4.2xlarge': {
        'id': 'c4.2xlarge',
        'name': 'Compute Optimized Double Extra Large Instance',
        'ram': 15000,
         'vcpu':8,
        'disk': 0,  #EBS-only
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'c4.4xlarge': {
        'id': 'c4.4xlarge',
        'name': 'Compute Optimized Quadruple Extra Large Instance',
        'ram': 30000,
         'vcpu':16,
        'disk': 0,  #EBS-only
        'bandwidth': None,
        'max_inst': 10,
        'sriov': True,
        'paravirt': False
    },
    'c4.8xlarge': {
        'id': 'c4.8xlarge',
        'name': 'Compute Optimized Eight Extra Large Instance',
        'ram': 60000,
         'vcpu':36,
        'disk': 0,  #EBS-only
        'bandwidth': None,
        'max_inst': 5,
        'sriov': True,
        'paravirt': False
    },
    # c3 instances have 2 SSDs of the specified disk size
    'c3.large': {
        'id': 'c3.large',
        'name': 'Compute Optimized Large Instance',
        'ram': 3750,
         'vcpu':2,
        'disk': 32,  # x2
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': True
    },
    'c3.xlarge': {
        'id': 'c3.xlarge',
        'name': 'Compute Optimized Extra Large Instance',
        'ram': 7500,
        'vcpu':4,
        'disk': 80,  # x2
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': True
    },
    'c3.2xlarge': {
        'id': 'c3.2xlarge',
        'name': 'Compute Optimized Double Extra Large Instance',
        'ram': 15000,
        'vcpu':8,
        'disk': 160,  # x2
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': True
    },
    'c3.4xlarge': {
        'id': 'c3.4xlarge',
        'name': 'Compute Optimized Quadruple Extra Large Instance',
        'ram': 30000,
        'vcpu':16,
        'disk': 320,  # x2
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': True
    },
    'c3.8xlarge': {
        'id': 'c3.8xlarge',
        'name': 'Compute Optimized Eight Extra Large Instance',
        'ram': 60000,
        'vcpu':32,
        'disk': 640,  # x2
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': True
    },
    # i2 instances have up to eight SSD drives
    'i2.xlarge': {
        'id': 'i2.xlarge',
        'name': 'High Storage Optimized Extra Large Instance',
        'ram': 31232,
         'vcpu': 4,
        'disk': 800,
        'bandwidth': None,
        'max_inst': 8,
        'sriov': True,
        'paravirt': False
    },
    'i2.2xlarge': {
        'id': 'i2.2xlarge',
        'name': 'High Storage Optimized Double Extra Large Instance',
        'ram': 62464,
        'vcpu': 8,
        'disk': 1600,
        'bandwidth': None,
        'max_inst': 8,
        'sriov': True,
        'paravirt': False
    },
    'i2.4xlarge': {
        'id': 'i2.4xlarge',
        'name': 'High Storage Optimized Quadruple Large Instance',
        'ram': 124928,
        'vcpu': 16,
        'disk': 3200,
        'bandwidth': None,
        'max_inst': 4,
        'sriov': True,
        'paravirt': False
    },
    'i2.8xlarge': {
        'id': 'i2.8xlarge',
        'name': 'High Storage Optimized Eight Extra Large Instance',
        'ram': 249856,
        'vcpu': 32,
        'disk': 6400,
        'bandwidth': None,
        'max_inst': 2,
        'sriov': True,
        'paravirt': False
    },
    'd2.xlarge': {
        'id': 'd2.xlarge',
        'name': 'High Storage Optimized Extra Large Instance',
        'ram': 30050,
        'vcpu': 4,
        'disk': 6000,  # 3 x 2 TB
        'max_inst': 20,
        'bandwidth': None,
        'sriov': True,
        'paravirt': False
    },
    'd2.2xlarge': {
        'id': 'd2.2xlarge',
        'name': 'High Storage Optimized Double Extra Large Instance',
        'ram': 61952,
        'vcpu': 8,
        'disk': 12000,  # 6 x 2 TB
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'd2.4xlarge': {
        'id': 'd2.4xlarge',
        'name': 'High Storage Optimized Quadruple Extra Large Instance',
        'ram': 122000,
        'vcpu': 16,
        'disk': 24000,  # 12 x 2 TB
        'bandwidth': None,
        'max_inst': 10,
        'sriov': True,
        'paravirt': False
    },
    'd2.8xlarge': {
        'id': 'd2.8xlarge',
        'name': 'High Storage Optimized Eight Extra Large Instance',
        'ram': 244000,
        'vcpu': 36,
        'disk': 48000,  # 24 x 2 TB
        'bandwidth': None,
        'max_inst': 5,
        'sriov': True,
        'paravirt': False
    },
    # 1x SSD
    'r3.large': {
        'id': 'r3.large',
        'name': 'Memory Optimized Large instance',
        'ram': 15000,
        'vcpu': 2,
        'disk': 32,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'r3.xlarge': {
        'id': 'r3.xlarge',
        'name': 'Memory Optimized Extra Large instance',
        'ram': 30500,
        'vcpu': 4,
        'disk': 80,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'r3.2xlarge': {
        'id': 'r3.2xlarge',
        'name': 'Memory Optimized Double Extra Large instance',
        'ram': 61000,
        'vcpu': 8,
        'disk': 160,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': True,
        'paravirt': False
    },
    'r3.4xlarge': {
        'id': 'r3.4xlarge',
        'name': 'Memory Optimized Quadruple Extra Large instance',
        'ram': 122000,
        'vcpu': 16,
        'disk': 320,
        'bandwidth': None,
        'max_inst': 10,
        'sriov': True,
        'paravirt': False
    },
    'r3.8xlarge': {
        'id': 'r3.8xlarge',
        'name': 'Memory Optimized Eight Extra Large instance',
        'ram': 244000,
        'vcpu': 32,
        'disk': 320,  # x2
        'bandwidth': None,
        'max_inst': 5,
        'sriov': True,
        'paravirt': False
    },
    't2.micro': {
        'id': 't2.micro',
        'name': 'Burstable Performance Micro Instance',
        'ram': 1024,
        'disk': 0,  # EBS Only
        'vcpu': 1,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': False,
        'extra': {
            'cpu': 6
        }
    },
    # Burstable Performance General Purpose
    't2.small': {
        'id': 't2.small',
        'name': 'Burstable Performance Small Instance',
        'ram': 2048,
        'vcpu': 1,
        'disk': 0,  # EBS Only
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': False,
        'extra': {
            'cpu': 12
        }
    },
    't2.medium': {
        'id': 't2.medium',
        'name': 'Burstable Performance Medium Instance',
        'ram': 4096,
        'disk': 0,  # EBS Only
        'vcpu': 2,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': False,
        'extra': {
            'cpu': 24
        }
    },
    't2.large': {
        'id': 't2.large',
        'name': 'Burstable Performance Large Instance',
        'ram': 8192,
        'disk': 0,  # EBS Only
        'vcpu': 2,
        'bandwidth': None,
        'max_inst': 20,
        'sriov': False,
        'paravirt': False,
        'extra': {
            'cpu': 36
        }
    }
}

