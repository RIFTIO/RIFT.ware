/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from '../alt';
// import AppHeaderActions from 'widgets/header/headerActions.js';
import LaunchNetworkServiceSource from '../network_service_launcher/launchNetworkServiceSource';
import LaunchNetworkServiceActions from '../network_service_launcher/launchNetworkServiceActions';
var CloudAccountSource = require('./cloudAccountSource');
var CloudAccountActions = require('./cloudAccountActions.js');

function createCloudAccountStore() {
    this.exportAsync(CloudAccountSource);
    this.bindAction(CloudAccountActions.CREATE_LOADING, this.createLoading);
    this.bindAction(CloudAccountActions.CREATE_SUCCESS, this.createSuccess);
    this.bindAction(CloudAccountActions.CREATE_FAIL, this.createFail);
    this.bindAction(CloudAccountActions.UPDATE_LOADING, this.updateLoading);
    this.bindAction(CloudAccountActions.UPDATE_SUCCESS, this.updateSuccess);
    this.bindAction(CloudAccountActions.UPDATE_FAIL, this.updateFail);
    this.bindAction(CloudAccountActions.DELETE_SUCCESS, this.deleteSuccess);
    this.bindAction(CloudAccountActions.DELETE_CLOUD_ACCOUNT_FAIL, this.deleteCloudAccountFail);
    this.bindAction(CloudAccountActions.GET_CLOUD_ACCOUNT_SUCCESS, this.getCloudAccountSuccess);
    this.bindAction(CloudAccountActions.GET_CLOUD_ACCOUNTS_SUCCESS, this.getCloudAccountsSuccess);
    this.bindAction(CloudAccountActions.VALIDATE_ERROR, this.validateError);
    this.bindAction(CloudAccountActions.VALIDATE_RESET, this.validateReset);
    this.exportAsync(LaunchNetworkServiceSource);
    this.cloudAccount = {
      name: '',
      'account-type': 'openstack'
    };
    this.descriptorCount = 0;
    this.cloudAccounts = [];
    this.validateErrorMsg = "";
    this.validateErrorEvent = false;
    this.isLoading = true;
    this.params = {
        "aws": [{
            label: "Key",
            ref: 'key'
        }, {
            label: "Secret",
            ref: 'secret'
        }, {
          label: "Availability Zone",
          ref: 'availability-zone'
        }, {
          label: "Default Subnet ID",
          ref: 'default-subnet-id'
        }, {
          label: "Region",
          ref: 'region'
        }, {
          label: "VPC ID",
          ref: 'vpcid'
        }, {
          label: "SSH Key",
          ref: 'ssh-key'
        }],
        "openstack": [{
            label: "Key",
            ref: 'key'
        }, {
            label: "Secret",
            ref: 'secret'
        }, {
            label: "Authentication URL",
            ref: 'auth_url'
        }, {
            label: "Tenant",
            ref: 'tenant'
        }, {
            label: 'Management Network',
            ref: 'mgmt-network'
        }, {
            label: 'Floating IP Pool',
            ref: 'floating-ip-pool',
            optional: true
        }],
        "openmano": [{
            label: "Host",
            ref: 'host'
        }, {
            label: "Port",
            ref: 'port'
        }, {
            label: "Tenant ID",
            ref: 'tenant-id'
        }]
    };
    this.accountType = [{
        "name": "OpenStack",
        "account-type": "openstack",
    }, {
        "name": "Cloudsim",
        "account-type": "cloudsim"
    }, {
        "name": "Open Mano",
        "account-type": "openmano"
    }, {
        "name": "AWS",
        "account-type": "aws"
    }];
    this.cloud = {
        name: '',
        'account-type': 'openstack',
        params: this.params['openstack']
    };
    this.exportPublicMethods({
      updateCloud: this.updateCloud.bind(this),
      resetState: this.resetState.bind(this)
    })
}

createCloudAccountStore.prototype.createLoading = function() {
    this.setState({
        isLoading: true
    });
};
createCloudAccountStore.prototype.createSuccess = function() {
    this.setState({
        isLoading: false
    });
};
createCloudAccountStore.prototype.resetState = function() {
    this.setState({
        cloudAccount: {
          name: '',
          'account-type': 'openstack'
        },
        cloud: {
            name: '',
            'account-type': 'openstack',
            params: this.params['openstack']
        }
    });
};
createCloudAccountStore.prototype.createFail = function() {
    var xhr = arguments[0];
    this.setState({
        isLoading: false
    });
};
createCloudAccountStore.prototype.deleteCloudAccountFail = function() {
    var xhr = arguments[0];
    this.setState({
        isLoading: false
    });
};
createCloudAccountStore.prototype.updateFail = function() {
    var xhr = arguments[0];
    this.setState({
        isLoading: false
    });
};
createCloudAccountStore.prototype.updateLoading = function() {
    this.setState({
        isLoading: true
    });
};
createCloudAccountStore.prototype.updateCloud = function(cloud) {
    this.setState({
        cloud: cloud
    });
};
createCloudAccountStore.prototype.updateSuccess = function() {
    console.log('success', arguments);
    this.setState({
        isLoading: false
    });
};

createCloudAccountStore.prototype.deleteSuccess = function(data) {
    this.setState({
        isLoading: false
    });
    console.log('deteled');
    if (data.cb) {
        data.cb();
    }
};

createCloudAccountStore.prototype.getCloudAccountSuccess = function(data) {
  let cloudAccount = data.cloudAccount;
  let stateObject = {
        cloudAccount: cloudAccount,
        cloud: {
          name: cloudAccount.name,
          'account-type': cloudAccount['account-type'],
          params: cloudAccount[cloudAccount['account-type']]
        },
        isLoading: false
    }
    if(cloudAccount['sdn-account']) {
        stateObject.cloud['sdn-account'] = cloudAccount['sdn-account'];
    }
    this.setState(stateObject);
};
createCloudAccountStore.prototype.getCloudAccountsSuccess = function(data) {
    this.setState({
        cloudAccounts: cloudAccounts || []
    });
};
createCloudAccountStore.prototype.validateError = function(msg) {
    this.setState({
        validateErrorEvent: true,
        validateErrorMsg: msg
    });
};
createCloudAccountStore.prototype.validateReset = function() {
    this.setState({
        validateErrorEvent: false
    });
};
createCloudAccountStore.prototype.getCloudAccountsSuccess = function(cloudAccounts) {
    this.setState({
        cloudAccounts: cloudAccounts || [],
        isLoading: false
    });
};
createCloudAccountStore.prototype.getCloudAccountsFail = function(cloudAccounts) {
    console.log('failed')
    this.setState({
        cloudAccounts: cloudAccounts || [],
        isLoading: false
    });
};

module.exports = Alt.createStore(createCloudAccountStore);;
