#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import boto3
import botocore
from . import aws_table
from . import exceptions

import logging
logger = logging.getLogger('rwcal.aws.drv')
logger.setLevel(logging.DEBUG)

class AWSDriver(object):
    """
    Driver for AWS
    """
    def __init__(self, key, secret, region,ssh_key=None,vpcid = None,availability_zone = None,default_subnet_id = None):
        """
          Constructor for AWSDriver
          Arguments:
             key    : AWS user access key
             secret : AWS user access secret
             region : AWS region
             ssh_key: Name of key pair to connect to EC2 instance
             vpcid  : VPC ID for the resources
             availability_zone: Avaialbility zone to allocate EC2 instance.
             default_subnet_id: Default subnet id to be used for the EC2 instance interfaces at instance creation time
          Returns: AWS Driver Object 
        """
        self._access_key    = key
        self._access_secret = secret
        self._region        = region
        self._availability_zone =  availability_zone
        self._ssh_key       = ssh_key
        
        self._sess  = boto3.session.Session(aws_access_key_id = self._access_key,
                                            aws_secret_access_key = self._access_secret,
                                            region_name = self._region)
        self._ec2_resource_handle = self._sess.resource(service_name = 'ec2')
        self._s3_handle  = self._sess.resource(service_name = 's3')
        self._iam_handle = self._sess.resource(service_name = 'iam')

        self._acct_arn = self._iam_handle.CurrentUser().arn
        self._account_id = self._acct_arn.split(':')[4]
        # If VPC id is not passed; use default VPC for the account 
        if vpcid is None:
            self._vpcid = self._default_vpc_id
        else:
            self._vpcid  = vpcid

        self._default_subnet_id = default_subnet_id 
        # If default_subnet_is is not passed; get default subnet for AZ.
        # We use this to create first network interface during instance creation time. This subnet typically should have associate public address 
        # to get public address.  
        if default_subnet_id is None:
            self._default_subnet_id = self._get_default_subnet_id_for_az 
           
       
    @property
    def default_subnet_id(self):
        """
           Returns default subnet id for account
        """
        return self._default_subnet_id

    @property
    def _ec2_client_handle(self):
        """
        Low level EC2 client connection handle
           Arguments: None
           Returns: EC2 Client Connection Handle
        """
        return self._ec2_resource_handle.meta.client

    @property
    def _default_vpc_id(self):
        """
        Method to get Default VPC ID
          Arguments: None
          Returns: Default EC2.Vpc Resource ID for AWS account
        """
        return self._default_vpc.vpc_id

    @property
    def _default_vpc(self):
        """
        Method to get Default VPC Resource Object
           Arguments: None
           Returns: Default EC2.Vpc Resource for AWS account
        """
        try:
           response = list(self._ec2_resource_handle.vpcs.all())
        except Exception as e:
            logger.error("AWSDriver: Get of Default VPC failed with exception: %s" %(repr(e)))
            raise
        default_vpc = [vpc for vpc in response if vpc.is_default]
        assert(len(default_vpc) == 1)
        return default_vpc[0]

    def _get_vpc_info(self,VpcId):
        """
        Get Vpc resource for specificed VpcId
          Arguments:
            - VpcId (String) : VPC ID  
          Returns: EC2.Vpc Resouce
        """ 
        VpcIds = list()
        VpcIds.append(VpcId)
        response = list(self._ec2_resource_handle.vpcs.filter(
                                               VpcIds = VpcIds))
        if response:
            assert(len(response) == 1)
            return response[0]
        return None


    def upload_image(self, **kwargs):
        """
        Upload image to s3
          Arguments: **kwargs -- dictionary
               {
                 'image_path'          : File location for the image,
                 'image_prefix'        : Name-Prefix of the image on S3 
                 'public_key'          : The path to the user's PEM encoded RSA public key certificate file,
                 'private_key'         : The path to the user's PEM encoded RSA private key file,
                 'arch'                : One of ["i386", "x86_64"],
                 's3_bucket'           : Name of S3 bucket where this image should be uploaded
                                         (e.g. 'Rift.Cal' or 'Rift.VNF' or 'Rift.3rdPartyVM' etc)
                 'kernelId'            : Id of the default kernel to launch the AMI with (OPTIONAL)
                 'ramdiskId'           : Id of the default ramdisk to launch the AMI with (OPTIONAL)
                 'block_device_mapping : block_device_mapping string  (OPTIONAL)
                                         Default block-device-mapping scheme to launch the AMI with. This scheme
                                         defines how block devices may be exposed to an EC2 instance of this AMI
                                         if the instance-type of the instance is entitled to the specified device.
                                         The scheme is a comma-separated list of key=value pairs, where each key
                                         is a "virtual-name" and each value, the corresponding native device name
                                         desired. Possible virtual-names are:
                                         - "ami": denotes the root file system device, as seen by the instance.
                                         - "root": denotes the root file system device, as seen by the kernel.
                                         - "swap": denotes the swap device, if present.
                                         - "ephemeralN": denotes Nth ephemeral store; N is a non-negative integer.
                                          Note that the contents of the AMI form the root file system. Samples of
                                          block-device-mappings are:
                                          '"ami=sda1","root=/dev/sda1","ephemeral0=sda2","swap=sda3"'
                                          '"ami=0","root=/dev/dsk/c0d0s0","ephemeral0=1"'
               }
          Returns: None
        """
        import subprocess
        import tempfile
        import os
        import shutil
        
        CREATE_BUNDLE_CMD  = 'ec2-bundle-image --cert {public_key} --privatekey {private_key} --user {account_id} --image {image_path} --prefix {image_prefix} --arch {arch}'
        UPLOAD_BUNDLE_CMD  = 'ec2-upload-bundle --bucket {bucket} --access-key {key} --secret-key {secret} --manifest {manifest} --region {region} --retry'
        
        cmdline = CREATE_BUNDLE_CMD.format(public_key    = kwargs['public_key'],
                                           private_key   = kwargs['private_key'],
                                           account_id    = self._account_id,
                                           image_path    = kwargs['image_path'],
                                           image_prefix  = kwargs['image_prefix'],
                                           arch          = kwargs['arch'])
        
        if 'kernelId' in kwargs:
            cmdline += (' --kernel ' + kwargs['kernelId'])

        if 'ramdiskId' in kwargs:
            cmdline += (' --ramdisk ' + kwargs['ramdiskId'])
            
        if 'block_device_mapping' in kwargs:
            cmdline += ' --block-device-mapping ' + kwargs['block_device_mapping']

        ### Create Temporary Directory
        try:
            tmp_dir = tempfile.mkdtemp()
        except Exception as e:
            logger.error("Failed to create temporary directory. Exception Details: %s" %(repr(e)))
            raise

        cmdline += (" --destination " + tmp_dir)
        logger.info('AWSDriver: Executing ec2-bundle-image command. Target directory name: %s. This command may take a while...\n' %(tmp_dir))
        result = subprocess.call(cmdline.split())
        if result == 0:
            logger.info('AWSDriver: ec2-bundle-image command succeeded')
        else:
            logger.error('AWSDriver: ec2-bundle-image command failed. Return code %d. CMD: %s'%(result, cmdline))
            raise OSError('AWSDriver: ec2-bundle-image command failed. Return code %d' %(result))
        
        logger.info('AWSDriver: Initiating image upload. This may take a while...')

        cmdline = UPLOAD_BUNDLE_CMD.format(bucket   = kwargs['s3_bucket'],
                                           key      = self._access_key,
                                           secret   = self._access_secret,
                                           manifest = tmp_dir+'/'+kwargs['image_prefix']+'.manifest.xml',
                                           region   = self._region)
        result = subprocess.call(cmdline.split())
        if result == 0:
            logger.info('AWSDriver: ec2-upload-bundle command succeeded')
        else:
            logger.error('AWSDriver: ec2-upload-bundle command failed. Return code %d. CMD: %s'%(result, cmdline))
            raise OSError('AWSDriver: ec2-upload-bundle command failed. Return code %d' %(result))
        ### Delete the temporary directory
        logger.info('AWSDriver: Deleting temporary directory and other software artifacts')
        shutil.rmtree(tmp_dir, ignore_errors = True)
        
                     
    def register_image(self, **kwargs):
        """
        Registers an image uploaded to S3 with EC2
           Arguments: **kwargs -- dictionary
             {
                Name (string)         : Name of the image
                ImageLocation(string) : Location of image manifest file in S3 (e.g. 'rift.cal.images/test-img.manifest.xml')
                Description(string)   : Description for the image (OPTIONAL)
                Architecture (string) : Possible values 'i386' or 'x86_64' (OPTIONAL)
                KernelId(string)      : Kernel-ID Refer: http://docs.aws.amazon.com/AWSEC2/latest/UserGuide/UserProvidedKernels.html#AmazonKernelImageIDs (OPTIONAL)
                RamdiskId(string)     : Ramdisk-ID Refer: http://docs.aws.amazon.com/AWSEC2/latest/UserGuide/UserProvidedKernels.html#AmazonKernelImageIDs (OPTIONAL)
                RootDeviceName(string): The name of the root device (for example, /dev/sda1 , or /dev/xvda ) (OPTIONAL)
                BlockDeviceMappings(list) : List of dictionary of block device mapping (OPTIONAL)
                                            [
                                               {
                                                 'VirtualName': 'string',
                                                 'DeviceName': 'string',
                                                 'Ebs': {
                                                    'SnapshotId': 'string',
                                                    'VolumeSize': 123,
                                                    'DeleteOnTermination': True|False,
                                                    'VolumeType': 'standard'|'io1'|'gp2',
                                                    'Iops': 123,
                                                    'Encrypted': True|False
                                                 },
                                                 'NoDevice': 'string'
                                              },
                                            ]
                VirtualizationType(string): The type of virtualization (OPTIONAL)
                                           Default: paravirtual
                SriovNetSupport(string): (OPTIONAL)
                       Set to ``simple`` to enable enhanced networking for the AMI and any instances that are launched from the AMI.
                       This option is supported only for HVM AMIs. Specifying this option with a PV AMI can make instances launched from the AMI unreachable.
        
          Returns:
             image_id: UUID of the image
        """

        kwargs['DryRun'] = False
        try:
            response = self._ec2_client_handle.register_image(**kwargs)
        except Exception as e:
            logger.error("AWSDriver: List image operation failed with exception: %s" %(repr(e)))
            raise
        assert response['ResponseMetadata']['HTTPStatusCode'] == 200
        return response['ImageId']
        

    def deregister_image(self, ImageId):
        """
        DeRegisters image from EC2.
          Arguments:
            - ImageId (string): ImageId generated by AWS in register_image call
          Returns: None
        """
        try:
            response = self._ec2_client_handle.deregister_image(
                                                         ImageId = ImageId)
        except Exception as e:
            logger.error("AWSDriver: deregister_image operation failed with exception: %s" %(repr(e)))
            raise
        assert response['ResponseMetadata']['HTTPStatusCode'] == 200
        
    def get_image(self, ImageId):
        """
        Returns a dictionary object describing the Image identified by ImageId
        """
        try:
            response = list(self._ec2_resource_handle.images.filter(ImageIds = [ImageId]))
        except Exception as e:
            logger.error("AWSDriver: List image operation failed with exception: %s" %(repr(e)))
            raise
        return response[0]
        
    def list_images(self):
        """
        Returns list of dictionaries. Each dictionary contains attributes associated with image
           Arguments: None
           Returns: List of dictionaries.
        """
        try:
            response = list(self._ec2_resource_handle.images.filter(Owners = [self._account_id]))
        except Exception as e:
            logger.error("AWSDriver: List image operation failed with exception: %s" %(repr(e)))
            raise
        return response

    def create_image_from_instance(self,InstanceId,ImageName,VolumeSize = 16):
        """
        Creates AWS AMI from the instance root device Volume and registers the same
        Caller is expected to stop the instance and restart the instance if required 
        Arguments:
           - InstanceId (String) : AWS EC2 Instance Id
           - ImageName (String)  : Name for AMI
         Returns
           - AWS AMI Image Id
        """

        try:
            inst = self.get_instance(InstanceId)
            # Find Volume Id of Root Device
            if inst.root_device_type == 'ebs':
                for dev in inst.block_device_mappings:
                    if inst.root_device_name == dev['DeviceName']:
                        volume_id = dev['Ebs']['VolumeId']
                        break

                rsp=self._ec2_resource_handle.create_snapshot(VolumeId=volume_id)
                snapshot_id = rsp.id

                #Wait for the snapshot to be completed
                attempts = 0
                while attempts < 2:
                    try:
                        attempts = attempts + 1
                        waiter = self._ec2_client_handle.get_waiter('snapshot_completed')
                        waiter.wait(SnapshotIds=[snapshot_id])
                    except botocore.exceptions.WaiterError as e:
                        logger.error("AWSDriver: Create Snapshot for image still not completed. Will wait for another iteration") 
                        continue
                    except Exception as e:
                        logger.error("AWSDriver: Createing Snapshot for instance failed during image creation: %s", (repr(e)))
                        raise
                    break
                  
                logger.debug("AWSDriver: Snapshot %s completed successfully from instance %s",snapshot_id,InstanceId)
                image_id = self.register_image(Name=ImageName,VirtualizationType='hvm',
                                               RootDeviceName='/dev/sda1',SriovNetSupport='simple',
                                               BlockDeviceMappings=[{'DeviceName':'/dev/sda1',
                                               'Ebs':{'SnapshotId':snapshot_id,'VolumeSize': VolumeSize,
                                               'VolumeType': 'standard', 'DeleteOnTermination': True}}],
                                               Architecture='x86_64')
                return image_id
            else:
                logger.error("AWSDriver: Create Image failed as Instance Root device Type should be ebs to create image") 
                raise exceptions.RWErrorFailure("AWSDriver: Create Image failed as Instance Root device Type should be ebs to create image")
        except Exception as e:
            logger.error("AWSDriver: Createing image from instance failed with exception: %s", (repr(e)))
            raise
        
    def list_instances(self):
        """
        Returns list of resource object representing EC2 instance.
           Arguments: None
           Returns:  List of EC2.Instance object
        """
        instance_list = []
        try:
            # Skip Instances in terminated state
            response = self._ec2_resource_handle.instances.filter(
                                                           Filters = [
                                                               { 'Name': 'instance-state-name',
                                                                 'Values': ['pending',
                                                                            'running',
                                                                            'shutting-down',
                                                                            'stopping',
                                                                            'stopped']
                                                            }
                                                           ])
        except Exception as e:
            logger.error("AWSDriver: List instances operation failed with exception: %s" %(repr(e)))
            raise
        for instance in response:
             instance_list.append(instance)
        return instance_list

    def get_instance(self, InstanceId):
        """
        Returns a EC2 resource Object describing the Instance identified by InstanceId
           Arguments:
             - InstnaceId (String) : MANDATORY, EC2 Instance Id
           Returns: EC2.Instance object
        """

        try:
            instance = list(self._ec2_resource_handle.instances.filter(
                                                           InstanceIds = [InstanceId]))
        except Exception as e:
            logger.error("AWSDriver: Get instances operation failed with exception: %s" %(repr(e)))
            raise
        if len(instance) == 0:
            logger.error("AWSDriver: instance with id %s not avaialble" %InstanceId)
            raise exceptions.RWErrorNotFound("AWSDriver: instance with id %s not avaialble" %InstanceId)
        elif len(instance) > 1:
            logger.error("AWSDriver: Duplicate instances with id %s is avaialble" %InstanceId)
            raise exceptions.RWErrorDuplicate("AWSDriver: Duplicate instances with id %s is avaialble" %InstanceId)
        return instance[0] 

    def create_instance(self,**kwargs):
        """
         Create an EC2instance.
            Arguments: **kwargs -- dictionary
               {
                  ImageId (string): MANDATORY, Id of AMI to create instance 
                  SubetId (string): Id of Subnet to start EC2 instance. EC2 instance will be started in VPC subnet resides. 
                                    Default subnet from account used if not present
                  InstanceType(string): AWS Instance Type name. Default: t2.micro
                  SecurityGroupIds: AWS Security Group Id to associate with the instance. Default from VPC used if not present
                  KeyName (string): Key pair name. Default key pair from account used if not present 
                  MinCount (Integer): Minimum number of instance to start. Default: 1
                  MaxCount (Integer): Maximum number of instance to start. Default: 1
                  Placement (Dict) : Dictionary having Placement group details
                                     {AvailabilityZone (String): AZ to create the instance}
                  UserData (string) : cloud-init config file 
               }
            Returns: List of EC2.Instance object
        """ 

        if 'ImageId' not in kwargs:
            logger.error("AWSDriver: Mandatory parameter ImageId not available during create_instance")
            raise AttributeError("Mandatory parameter ImageId not available during create_instance")

        #Validate image exists and is avaialble
        try:
            image_res = self._ec2_resource_handle.Image(kwargs['ImageId'])
            image_res.load() 
        except Exception as e:
            logger.error("AWSDriver: Image with id %s not available and failed with exception: %s",kwargs['ImageId'],(repr(e)))
            raise AttributeError("AWSDriver: Image with id %s not available and failed with exception: %s",kwargs['ImageId'],(repr(e)))
        if image_res.state != 'available':
            logger.error("AWSDriver: Image state is not available for image with id %s; Current state is %s",
                         image_res.id,image_res.state)
            raise AttributeError("ImageId is not valid")

        # If MinCount or MaxCount is not passed set them to default of 1
        if 'MinCount' not in kwargs:
            kwargs['MinCount'] = 1  
        if 'MaxCount' not in kwargs:
            kwargs['MaxCount'] = kwargs['MinCount'] 

        if 'KeyName' not in kwargs:
            if not self._ssh_key:
                logger.error("AWSDriver: Key not available during create_instance to allow SSH")
            else:
                kwargs['KeyName'] = self._ssh_key

        if 'Placement' not in kwargs and self._availability_zone is not None:
            placement = {'AvailabilityZone':self._availability_zone}
            kwargs['Placement'] = placement

        if 'SubnetId' not in kwargs and 'NetworkInterfaces' not in kwargs:
            if self._default_subnet_id:
                kwargs['SubnetId'] = self._default_subnet_id
            else: 
                logger.error("AWSDriver: Valid subnetid not present during create instance")
                raise AttributeError("Valid subnet not present during create instance")

        if self._availability_zone and 'SubnetId' in kwargs:
            subnet = self.get_subnet(SubnetId= kwargs['SubnetId']) 
            if not subnet:
                logger.error("AWSDriver: Valid subnet not found for subnetid %s",kwargs['SubnetId'])
                raise AttributeError("Valid subnet not found for subnetid %s",kwargs['SubnetId'])
            if subnet.availability_zone != self._availability_zone:
                logger.error("AWSDriver: AZ of Subnet %s %s doesnt match account AZ %s",kwargs['SubnetId'],
                                       subnet.availability_zone,self._availability_zone)
                raise AttributeError("AWSDriver: AZ of Subnet %s %s doesnt match account AZ %s",kwargs['SubnetId'],
                                       subnet.availability_zone,self._availability_zone)

        # If instance type is not passed; use t2.micro as default
        if 'InstanceType' not in kwargs or kwargs['InstanceType'] is None:
               kwargs['InstanceType'] = 't2.micro'
        inst_type =  kwargs['InstanceType']
        if inst_type not in aws_table.INSTANCE_TYPES.keys():
            logger.error("AWSDriver: Invalid instance type %s used",inst_type)
            raise AttributeError('InstanceType %s is not valid' %inst_type)

        #validate instance_type for AMI 
        if image_res.sriov_net_support == 'simple':
            if image_res.virtualization_type != 'hvm':
                logger.error("AWSDriver: Image with id %s has SRIOV net support but virtualization type is not hvm",kwargs['ImageId'])
                raise AttributeError('Invalid Image with id %s' %kwargs['ImageId'])
            if aws_table.INSTANCE_TYPES[inst_type]['sriov'] is False:
                logger.warning("AWSDriver: Image %s support SR-IOV but instance type %s does not support HVM",kwargs['ImageId'],inst_type)

        if image_res.virtualization_type == 'paravirtual' and aws_table.INSTANCE_TYPES[inst_type]['paravirt'] is False:  # Need to check virt type str for PV
            logger.error("AWSDriver: Image %s requires PV support but instance %s does not support PV",kwargs['ImageId'],inst_type)
            raise AttributeError('Image %s requires PV support but instance %s does not support PV',kwargs['ImageId'],inst_type)

        if image_res.root_device_type == 'instance-store' and aws_table.INSTANCE_TYPES[inst_type]['disk'] ==  0: 
            logger.error("AWSDriver: Image %s uses instance-store root device type that is not supported by instance type %s",kwargs['ImageId'],inst_type) 
            raise AttributeError("AWSDriver: Image %s uses instance-store root device type that is not supported by instance type %s",kwargs['ImageId'],inst_type)


        # Support of instance type varies across regions and also based on account. So we are not validating it
        #if inst_type not in aws_table.REGION_DETAILS[self._region]['instance_types']:
        #    logger.error("AWSDriver: instance type %s not supported in region %s",inst_type,self._region)
        #    raise AttributeError("AWSDriver: instance type %s not supported in region %s",inst_type,self._region)

        try:
            instances = self._ec2_resource_handle.create_instances(**kwargs)
        except Exception as e:
            logger.error("AWSDriver: Creating instance failed with exception: %s" %(repr(e)))
            raise  
        return instances

    def terminate_instance(self,InstanceId):
        """
        Termintae an EC2 instance
           Arguments:
            - InstanceId (String): ID of EC2 instance
           Returns: None
        """ 

        InstanceIds = InstanceId
        if type(InstanceIds) is not list:
            InstanceIds = list()
            InstanceIds.append(InstanceId)

        try:
            response = self._ec2_client_handle.terminate_instances(InstanceIds=InstanceIds)
        except Exception as e:
            logger.error("AWSDriver: Terminate instance failed with exception: %s" %(repr(e)))
            raise  
        return response 

    def stop_instance(self,InstanceId):
        """
        Stop an EC2 instance. Stop is supported only for EBS backed instance
           Arguments:
            - InstanceId (String): ID of EC2 instance
           Returns: None
        """ 

        InstanceIds = InstanceId
        if type(InstanceIds) is not list:
            InstanceIds = list()
            InstanceIds.append(InstanceId)

        try:
            response = self._ec2_client_handle.stop_instances(InstanceIds=InstanceIds)
        except Exception as e:
            logger.error("AWSDriver: Stop for instance %s failed with exception: %s",InstanceId,repr(e))
            raise  
        return response 

    def start_instance(self,InstanceId):
        """
        Start an EC2 instance. Start is supported only for EBS backed instance
           Arguments:
            - InstanceId (String): ID of EC2 instance
           Returns: None
        """ 

        InstanceIds = InstanceId
        if type(InstanceIds) is not list:
            InstanceIds = list()
            InstanceIds.append(InstanceId)

        try:
            response = self._ec2_client_handle.start_instances(InstanceIds=InstanceIds)
        except Exception as e:
            logger.error("AWSDriver: Start for instance %s failed with exception: %s",InstanceId,repr(e))
            raise  
        return response 
       
    @property
    def _get_default_subnet_id_for_az(self):
        """
        Get default subnet id for AWS Driver registered Availability Zone 
          Arguments: None
          Returns: SubnetId (String)
        """ 

        if self._availability_zone:
            subnet = self._get_default_subnet_for_az(self._availability_zone)
            return subnet.id
        else:
            return None

    def _get_default_subnet_for_az(self,AvailabilityZone):
        """
        Get default Subnet for Avaialbility Zone
           Arguments:
              - AvailabilityZone (String) : EC2 AZ
           Returns: EC2.Subnet object
        """

        AvailabilityZones = [AvailabilityZone]
        try:
            response = list(self._ec2_resource_handle.subnets.filter(
                                                              Filters = [
                                                               {'Name':'availability-zone',
                                                                 'Values': AvailabilityZones}]))
        except Exception as e:
            logger.error("AWSDriver: Get default subnet for Availability zone failed with exception: %s" %(repr(e)))
            raise
        default_subnet = [subnet for subnet in response if subnet.default_for_az is True and subnet.vpc_id == self._vpcid]
        assert(len(default_subnet) == 1)
        return default_subnet[0]
        
    def get_subnet_list(self,VpcId=None):
        """
        List all the subnets
          Arguments:
           - VpcId (String) - VPC ID to filter the subnet list
        Returns: List of EC2.Subnet Object
        """

        try:
            VpcIds = VpcId
            if VpcId is not None:
                if type(VpcIds) is not list:
                    VpcIds = list()
                    VpcIds.append(VpcId)
                response = list(self._ec2_resource_handle.subnets.filter(
					       Filters = [
					       { 'Name': 'vpc-id',
					       'Values': VpcIds}]))
            else:
                response = list(self._ec2_resource_handle.subnets.all())
        except Exception as e:
            logger.error("AWSDriver: List subnets operation failed with exception: %s" %(repr(e)))
            raise
        return response 

    def get_subnet(self,SubnetId):
        """
	Get the subnet for specified SubnetId
          Arguments:
             - SubnetId (String) - MANDATORY
          Returns: EC2.Subnet Object
	"""

        try:
            response = list(self._ec2_resource_handle.subnets.filter(SubnetIds=[SubnetId]))
        except botocore.exceptions.ClientError as e:
           if e.response['Error']['Code'] == 'InvalidSubnetID.NotFound':
                logger.error("AWSDriver: Get Subnet Invalid SubnetID %s",SubnetId)
                raise exceptions.RWErrorNotFound("AWSDriver: Delete Subnet Invalid SubnetID %s",SubnetId)
           else:
               logger.error("AWSDriver: Creating network interface failed with exception: %s",(repr(e)))
               raise
        except Exception as e:
            logger.error("AWSDriver: Get subnet operation failed with exception: %s" %(repr(e)))
            raise
        if len(response) == 0:
            logger.error("AWSDriver: subnet with id %s is not avaialble" %SubnetId)
            raise exceptions.RWErrorNotFoun("AWSDriver: subnet with id %s is not avaialble" %SubnetId)
        elif len(response) > 1: 
            logger.error("AWSDriver: Duplicate subnet with id %s is avaialble" %SubnetId)
            raise exceptions.RWErrorDuplicate("AWSDriver: Duplicate subnet with id %s is avaialble" %SubnetId)
        return response[0] 

    def create_subnet(self,**kwargs):
        """
        Create a EC2 subnet based on specified CIDR
          Arguments:
             - CidrBlock (String): MANDATORY. CIDR for subnet. CIDR should be within VPC CIDR
             - VpcId (String): VPC ID to create the subnet. Default AZ from AWS Driver registration used if not present. 
             - AvailabilityZone (String): Availability zone to create subnet. Default AZ from AWS Driver registration used
                                          if not present
          Returns: EC2.Subnet Object 
        """

        if 'CidrBlock' not in kwargs:
            logger.error("AWSDriver: Insufficent params for create_subnet. CidrBlock is mandatory parameter")
            raise AttributeError("AWSDriver: Insufficent params for create_subnet. CidrBlock is mandatory parameter")

        if 'VpcId' not in kwargs:
            kwargs['VpcId'] = self._vpcid
        if 'AvailabilityZone' not in kwargs and self._availability_zone is not None:
            kwargs['AvailabilityZone'] = self._availability_zone

        vpc = self._get_vpc_info(kwargs['VpcId'])
        if not vpc:
            logger.error("AWSDriver: Subnet creation failed as VpcId %s does not exist", kwargs['VpcId'])
            raise exceptions.RWErrorNotFound("AWSDriver: Subnet creation failed as VpcId %s does not exist", kwargs['VpcId'])
        if vpc.state != 'available':
            logger.error("AWSDriver: Subnet creation failed as VpcId %s is not in available state. Current state is %s", kwargs['VpcId'],vpc.state)
            raise exceptions.RWErrorNotConnected("AWSDriver: Subnet creation failed as VpcId %s is not in available state. Current state is %s", kwargs['VpcId'],vpc.state)
        
        try:
            subnet = self._ec2_resource_handle.create_subnet(**kwargs)
        except botocore.exceptions.ClientError as e:
           if e.response['Error']['Code'] == 'InvalidSubnet.Conflict':
                logger.error("AWSDriver: Create Subnet for ip %s failed due to overalp with existing subnet in VPC %s",kwargs['CidrBlock'],kwargs['VpcId'])
                raise exceptions.RWErrorExists("AWSDriver: Create Subnet for ip %s failed due to overalp with existing subnet in VPC %s",kwargs['CidrBlock'],kwargs['VpcId'])
           elif e.response['Error']['Code'] == 'InvalidSubnet.Range':
                logger.error("AWSDriver: Create Subnet for ip %s failed as it is not in VPC CIDR range for VPC %s",kwargs['CidrBlock'],kwargs['VpcId'])
                raise AttributeError("AWSDriver: Create Subnet for ip %s failed as it is not in VPC CIDR range for VPC %s",kwargs['CidrBlock'],kwargs['VpcId'])
           else:
               logger.error("AWSDriver: Creating subnet failed with exception: %s",(repr(e)))
               raise  
        except Exception as e:
            logger.error("AWSDriver: Creating subnet failed with exception: %s" %(repr(e)))
            raise  
        return subnet

    def modify_subnet(self,SubnetId,MapPublicIpOnLaunch):
        """
        Modify a EC2 subnet
           Arguements: 
               - SubnetId (String): MANDATORY, EC2 Subnet ID
               - MapPublicIpOnLaunch (Boolean): Flag to indicate if subnet is associated with public IP 
        """

        try:
            response = self._ec2_client_handle.modify_subnet_attribute(SubnetId=SubnetId,MapPublicIpOnLaunch={'Value':MapPublicIpOnLaunch})
        except botocore.exceptions.ClientError as e:
           if e.response['Error']['Code'] == 'InvalidSubnetID.NotFound':
                logger.error("AWSDriver: Modify Subnet Invalid SubnetID %s",SubnetId)
                raise exceptions.RWErrorNotFound("AWSDriver: Modify Subnet Invalid SubnetID %s",SubnetId)
           else:
               logger.error("AWSDriver: Modify subnet failed with exception: %s",(repr(e)))
               raise  
        except Exception as e:
            logger.error("AWSDriver: Modify subnet failed with exception: %s",(repr(e)))
            raise


    def delete_subnet(self,SubnetId):
        """
        Delete a EC2 subnet
           Arguements: 
               - SubnetId (String): MANDATORY, EC2 Subnet ID
           Returns: None 
        """

        try:
            response = self._ec2_client_handle.delete_subnet(SubnetId=SubnetId)
        except botocore.exceptions.ClientError as e:
           if e.response['Error']['Code'] == 'InvalidSubnetID.NotFound':
                logger.error("AWSDriver: Delete Subnet Invalid SubnetID %s",SubnetId)
                raise exceptions.RWErrorNotFound("AWSDriver: Delete Subnet Invalid SubnetID %s",SubnetId)
           else:
               logger.error("AWSDriver: Delete subnet failed with exception: %s",(repr(e)))
               raise  
        except Exception as e:
            logger.error("AWSDriver: Delete subnet failed with exception: %s",(repr(e)))
            raise

    def get_network_interface_list(self,SubnetId=None,VpcId=None,InstanceId = None):
        """
        List all the network interfaces
           Arguments:
              - SubnetId (String)
              - VpcId (String)
              - InstanceId (String)
           Returns List of EC2.NetworkInterface  
        """

        try:
            if InstanceId is not None:
                InstanceIds = [InstanceId]
                response = list(self._ec2_resource_handle.network_interfaces.filter(
					       Filters = [
					       { 'Name': 'attachment.instance-id',
                                                 'Values': InstanceIds}]))
            elif SubnetId is not None:
                SubnetIds = SubnetId
                if type(SubnetId) is not list:
                    SubnetIds = list()
                    SubnetIds.append(SubnetId)
                response = list(self._ec2_resource_handle.network_interfaces.filter(
					       Filters = [
					       { 'Name': 'subnet-id',
					       'Values': SubnetIds}]))
            elif VpcId is not None:
                VpcIds = VpcId
                if type(VpcIds) is not list:
                    VpcIds = list()
                    VpcIds.append(VpcId)
                response = list(self._ec2_resource_handle.network_interfaces.filter(
					       Filters = [
					       { 'Name': 'vpc-id',
					       'Values': VpcIds}]))
            else:
                response = list(self._ec2_resource_handle.network_interfaces.all())
        except Exception as e:
            logger.error("AWSDriver: List network interfaces operation failed with exception: %s" %(repr(e)))
            raise
        return response

    def get_network_interface(self,NetworkInterfaceId):
        """
	Get the network interface
          Arguments:
              NetworkInterfaceId (String): MANDATORY, EC2 Network Interface Id
         Returns:  EC2.NetworkInterface Object
	"""

        try:
            response = list(self._ec2_resource_handle.network_interfaces.filter(NetworkInterfaceIds=[NetworkInterfaceId]))
        except Exception as e:
            logger.error("AWSDriver: List Network Interfaces operation failed with exception: %s" %(repr(e)))
            raise
        if len(response) == 0:
            logger.error("AWSDriver: Network interface with id %s is not avaialble" %NetworkInterfaceId)
            raise exceptions.RWErrorNotFound("AWSDriver: Network interface with id %s is not avaialble" %NetworkInterfaceId)
        elif len(response) > 1:
            logger.error("AWSDriver: Duplicate Network interface with id %s is avaialble" %NetworkInterfaceId)
            raise exceptions.RWErrorDuplicate("AWSDriver: Duplicate Network interface with id %s is avaialble" %NetworkInterfaceId)
        return response[0] 

    def create_network_interface(self,**kwargs):
        """
        Create a network interface in specified subnet 
          Arguments:
             - SubnetId (String): MANDATORY, Subnet to create network interface
          Returns: EC2.NetworkInterface Object
        """

        if 'SubnetId' not in kwargs:
            logger.error("AWSDriver: Insufficent params for create_network_inteface . SubnetId is mandatory parameters")
            raise AttributeError("AWSDriver: Insufficent params for create_network_inteface . SubnetId is mandatory parameters")

        try:
            interface = self._ec2_resource_handle.create_network_interface(**kwargs)
        except botocore.exceptions.ClientError as e:
           if e.response['Error']['Code'] == 'InvalidSubnetID.NotFound':
                logger.error("AWSDriver: Create Network interface failed as subnet %s is not found",kwargs['SubnetId'])
                raise exceptions.RWErrorNotFound("AWSDriver: Create Network interface failed as subnet %s is not found",kwargs['SubnetId'])
           else:
               logger.error("AWSDriver: Creating network interface failed with exception: %s",(repr(e)))
               raise
        except Exception as e:
            logger.error("AWSDriver: Creating network interface failed with exception: %s" %(repr(e)))
            raise
        return interface

    def delete_network_interface(self,NetworkInterfaceId):
        """
        Delete a network interface
         Arguments:
            - NetworkInterfaceId(String): MANDATORY
         Returns: None
        """
        try:
            response = self._ec2_client_handle.delete_network_interface(NetworkInterfaceId=NetworkInterfaceId)
        except botocore.exceptions.ClientError as e:
           if e.response['Error']['Code'] == 'InvalidNetworkInterfaceID.NotFound':
                logger.error("AWSDriver: Delete Network interface not found for interface ID  %s",NetworkInterfaceId)
                raise exceptions.RWErrorNotFound("AWSDriver: Delete Network interface not found for interface ID  %s",NetworkInterfaceId)
           else:
               logger.error("AWSDriver: Delete network interface failed with exception: %s",(repr(e)))
               raise  
        except Exception as e:
            logger.error("AWSDriver: Delete network interface failed with exception: %s",(repr(e)))
            raise

    def associate_public_ip_to_network_interface(self,NetworkInterfaceId):
        """
        Allocate a Elastic IP and associate to network interface
          Arguments:
            NetworkInterfaceId (String): MANDATORY
          Returns: None
        """
        try:
            response = self._ec2_client_handle.allocate_address(Domain='vpc')
            self._ec2_client_handle.associate_address(NetworkInterfaceId=NetworkInterfaceId,AllocationId = response['AllocationId'])
        except Exception as e:
             logger.error("AWSDriver: Associating Public IP to network interface %s failed with exception: %s",NetworkInterfaceId,(repr(e)))
             raise
        return response

    def disassociate_public_ip_from_network_interface(self,NetworkInterfaceId):
        """
        Disassociate a Elastic IP from network interface and release the same
          Arguments:
            NetworkInterfaceId (String): MANDATORY
          Returns: None
        """
        try:
            interface = self.get_network_interface(NetworkInterfaceId=NetworkInterfaceId) 
            if interface  and interface.association and 'AssociationId' in interface.association:
                self._ec2_client_handle.disassociate_address(AssociationId = interface.association['AssociationId'])
                self._ec2_client_handle.release_address(AllocationId=interface.association['AllocationId'])
        except Exception as e:
             logger.error("AWSDriver: Associating Public IP to network interface %s failed with exception: %s",NetworkInterfaceId,(repr(e)))
             raise

    def attach_network_interface(self,**kwargs):
        """
        Attach network interface to running EC2 instance. Used to add additional interfaces to instance
          Arguments:
            - NetworkInterfaceId (String):  MANDATORY,
            - InstanceId(String) :  MANDATORY
            - DeviceIndex (Integer): MANDATORY
          Returns: Dict with AttachmentId which is string
        """

        if 'NetworkInterfaceId' not in kwargs or 'InstanceId' not in kwargs or 'DeviceIndex' not in kwargs:
            logger.error('AWSDriver: Attach network interface to instance requires NetworkInterfaceId and InstanceId as mandatory parameters')
            raise AttributeError('AWSDriver: Attach network interface to instance requires NetworkInterfaceId and InstanceId as mandatory parameters')

        try:
            response = self._ec2_client_handle.attach_network_interface(**kwargs)
        except Exception as e:
            logger.error("AWSDriver: Attach network interface failed with exception: %s",(repr(e)))
            raise
        return response

    def detach_network_interface(self,**kwargs):
        """
        Detach network interface from instance 
          Arguments:
            - AttachmentId (String)
          Returns: None 
        """

        if 'AttachmentId' not in kwargs:
            logger.error('AWSDriver: Detach network interface from instance requires AttachmentId as mandatory parameters')
            raise AttributeError('AWSDriver: Detach network interface from instance requires AttachmentId as mandatory parameters')

        try:
            response = self._ec2_client_handle.detach_network_interface(**kwargs)
        except Exception as e:
            logger.error("AWSDriver: Detach network interface failed with exception: %s",(repr(e)))
            raise

    def map_flavor_to_instance_type(self,name,ram,vcpus,disk,inst_types = None):
        """
        Method to find a EC2 instance type matching the requested params
          Arguments:
             - name (String) : Name for flavor
             - ram (Integer) : RAM size in MB
             - vcpus (Integer): VPCU count
             - disk (Integer): Storage size in GB
             - inst_types (List): List of string having list of EC2 instance types to choose from
                                  assumed to be in order of resource size 
          Returns
             InstanceType (String) - EC2 Instance Type
        """
        if inst_types is None:
            inst_types = ['c3.large','c3.xlarge','c3.2xlarge', 'c3.4xlarge', 'c3.8xlarge']
        
        for inst in inst_types:
           if inst in aws_table.INSTANCE_TYPES:
               if ( aws_table.INSTANCE_TYPES[inst]['ram'] > ram and  
                    aws_table.INSTANCE_TYPES[inst]['vcpu'] > vcpus and 
                    aws_table.INSTANCE_TYPES[inst]['disk'] > disk):
                   return inst
        return 't2.micro'  

    def upload_ssh_key(self,key_name,public_key):
        """
        Method to upload Public Key to AWS
          Arguments:
            - keyname (String): Name for the key pair
            - public_key (String): Base 64 encoded public key
          Returns  None
        """
        self._ec2_resource_handle.import_key_pair(KeyName=key_name,PublicKeyMaterial=public_key) 

    def delete_ssh_key(self,key_name):
        """
        Method to delete Public Key from AWS
          Arguments:
            - keyname (String): Name for the key pair
          Returns  None
        """
        self._ec2_client_handle.delete_key_pair(KeyName=key_name) 
 
             
