
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var CloudAccountCreateController = function($timeout, $state, $stateParams) {
  var self = this;
  var CloudAccountStore = require('../missioncontrol/cloud_account/cloudAccountStore')
  var CloudAccountActions = require('../missioncontrol/cloud_account/cloudAccountActions')
  self.CloudAccountStore = CloudAccountStore;
  self.accountType = [
    // {
    //   "name":"AWS",
    //   "account-type":"aws",
    // },
    {
      "name":"OpenStack",
      "account-type":"openstack",
    },{
      "name":"Cloudsim",
      "account-type":"cloudsim"
    },{
      "name":"OpenMano",
      "account-type":"openmano"
    }
  ];
  self.params = {
    "aws": [
        {
          label: "Key",
          ref: 'key'
        },
        {
          label: "Secret",
          ref: 'secret'
        },
        {
          label: "Availability Zone",
          ref: 'availability-zone'
        },
        {
          label: "Default Subnet ID",
          ref: 'default-subnet-id'
        },
        {
          label: "Region",
          ref: 'region'
        },
        {
          label: "VPC ID",
          ref: 'vpcid'
        },
        {
          label: "SSH Key",
          ref: 'ssh-key'
        }
      ],
    "openstack": [
        {
          label: "Key",
          ref: 'key'
        },
        {
          label: "Secret",
          ref: 'secret'
        },
        {
          label: "Authentication URL",
          ref: 'auth_url'
        },
        {
          label: "Tenant",
          ref: 'tenant'
        },
        {
          label: 'Management Network',
          ref: 'mgmt-network'
        }
      ],
    "openmano": [
        {
          label: "Host",
          ref: 'host'
        },
        {
          label: "Port",
          ref: 'port'
        },
        {
          label: "Tenant ID",
          ref: 'tenant-id'
        }
      ]
  }
  self.cloud = {
    name: '',
    'account-type': 'openstack'
  };
  self.loadDashboard = function() {
     $state.go('/launchpad', {management_domain: $stateParams.management_domain});
  }
  self.openLog = function() {
    console.log('openLog')
    var MissionControlStore = require('./launchpadFleetStore.js');
    MissionControlStore.getSysLogViewerURL('lp');
  }
    self.openAbout = function() {
      $state.go('lp-about', {management_domain: $stateParams.management_domain});
    }
    self.openDebug = function() {
      $state.go('lp-debug', {management_domain: $stateParams.management_domain});
    }
  self.create = function() {
    if (self.cloud.name == "") {
      CloudAccountActions.validateError("Please give the cloud account a name");
      return;
    } else {
      var type = self.cloud['account-type'];
      if (typeof(self.params[type]) != 'undefined') {
        var params = self.params[type];
        for (var i = 0; i < params.length; i++) {
          var param = params[i].ref;
          if (typeof(self.cloud[type]) == 'undefined' || typeof(self.cloud[type][param]) == 'undefined' || self.cloud[type][param] == "") {

            CloudAccountActions.validateError("Please fill all account details");
            return;
          }
        }
      }
    }
    CloudAccountActions.validateReset();
     CloudAccountStore.create(self.cloud).then(function() {
      $state.go('/launchpad',{management_domain: $stateParams.management_domain});
     });
  }
  self.cancel = function() {
    $state.go('/launchpad',{management_domain: $stateParams.management_domain});
  }
  self.store = CloudAccountStore;
  require('../utils/utils.js').isNotAuthenticated(window.location, function() {
      $state.go('login');
    });
};

var CloudAccountEditController = function($timeout, $state, $stateParams) {
  var self = this;
  var CloudAccountStore = require('../missioncontrol/cloud_account/cloudAccountStore');
  self.CloudAccountStore = CloudAccountStore;
  var cloud_account_name = $stateParams.cloud_account;

  // Mark this is edit
  self.edit = true;

  self.accountType = [
    // {
    //   "name":"AWS",
    //   "account-type":"aws",
    // },
    {
      "name":"OpenStack",
      "account-type":"openstack",
    },{
      "name":"Cloudsim",
      "account-type":"cloudsim"
    },{
      "name":"OpenMano",
      "account-type":"openmano"
    }
  ];
  self.params = {
    "aws": [
        {
          label: "Key",
          ref: 'key'
        },
        {
          label: "Secret",
          ref: 'secret'
        },
        {
          label: "Availability Zone",
          ref: 'availability-zone'
        },
        {
          label: "Default Subnet ID",
          ref: 'default-subnet-id'
        },
        {
          label: "Region",
          ref: 'region'
        },
        {
          label: "VPC ID",
          ref: 'vpcid'
        },
        {
          label: "SSH Key",
          ref: 'ssh-key'
        }
      ],
    "openstack": [
        {
          label: "Key",
          ref: 'key'
        },
        {
          label: "Secret",
          ref: 'secret'
        },
        {
          label: "Authentication URL",
          ref: 'auth_url'
        },
        {
          label: "Tenant",
          ref: 'tenant'
        },
        {
          label: 'Management Network',
          ref: 'mgmt-network'
        }
      ],
    "openmano": [
        {
          label: "Host",
          ref: 'host'
        },
        {
          label: "Port",
          ref: 'port'
        },
        {
          label: "Tenant ID",
          ref: 'tenant'
        },
        {
          label: "Data Center ID",
          ref: 'datacenter'
        }
      ]
  };
  self.cloud = {
    name: ''
  };

  // Setup listener
  var listener = function(data) {
    $timeout(function() {
      console.log('updating', data);
      self.cloud.name = data.cloudAccount.name;
      var accountType = self.cloud['account-type'] = data.cloudAccount['account-type'];
      if (data.cloudAccount[accountType]) {
        // There are extra params.
        // Initialize object
        self.cloud[accountType] = {};
        // Iterate over them and assign to proper keys
        for (var k in data.cloudAccount[accountType]) {
          self.cloud[accountType][k] = data.cloudAccount[accountType][k];
        }
      }
    });
  };

  CloudAccountStore.listen(listener);
  require('../utils/utils.js').isNotAuthenticated(window.location, function() {
      CloudAccountStore.unlisten(listener);
      $state.go('login')
    });
  // Get the cloud account
  CloudAccountStore.getCloudAccount($stateParams.cloud_account);
  self.store = CloudAccountStore;
  self.update = function() {
    if (self.cloud.name == "") {
      console.log('pop')
      return;
    }
     CloudAccountStore.update(self.cloud).then(function() {
      CloudAccountStore.unlisten(listener);
      $state.go('/launchpad',{management_domain: $stateParams.management_domain});
     });
  };
  self.loadDashboard = function() {
     $state.go('/launchpad', {management_domain: $stateParams.management_domain});
  }
  self.openLog = function() {
    console.log('openLog')
    var MissionControlStore = require('./launchpadFleetStore.js');
    MissionControlStore.getSysLogViewerURL('lp');
  }
      self.openAbout = function() {
      $state.go('lp-about', {management_domain: $stateParams.management_domain});
    }
    self.openDebug = function() {
      $state.go('lp-debug', {management_domain: $stateParams.management_domain});
    }
  self.delete = function() {
    console.log('Deleting cloud account', cloud_account_name);
    CloudAccountStore.delete(cloud_account_name).then(function() {
      CloudAccountStore.unlisten(listener);
      $state.go('/launchpad',{management_domain: $stateParams.management_domain});
    });
  };
  self.cancel = function() {
    CloudAccountStore.unlisten(listener);
   $state.go('/launchpad',{management_domain: $stateParams.management_domain});
  }
};

module.exports = {
  CloudAccountCreateController: CloudAccountCreateController,
  CloudAccountEditController: CloudAccountEditController
};
