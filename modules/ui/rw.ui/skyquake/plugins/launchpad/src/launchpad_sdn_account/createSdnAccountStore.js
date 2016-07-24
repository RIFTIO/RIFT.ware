/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from 'widgets/skyquake_container/skyquakeAltInstance';

/*

  Create and Updated:
  {"name":"odltwo","account-type":"odl","odl":{"username":"un","Password":"pw","url":"url"}}

  {"account-type":"odl","name":"odltwo","odl":{"username":"un","url":"url"}}
 */
import LaunchNetworkServiceActions from '../network_service_launcher/launchNetworkServiceActions.js'
function createSdnAccountStore() {
    this.exportAsync(require('./createSdnAccountSource.js'));
    this.exportAsync(require('../network_service_launcher/launchNetworkServiceSource.js'));
    this.bindActions(require('./createSdnAccountActions.js'));
        this.bindListeners({
        getCatalogSuccess: LaunchNetworkServiceActions.getCatalogSuccess
    });
    this.sdnAccount = {};
    this.descriptorCount = 0;
    this.validateErrorEvent = false;
    this.validateErrorMsg = "";
    this.isLoading = false;
    this.params = {
        "odl": [{
            label: "Username",
            ref: 'username'
        }, {
            label: "Password",
            ref: 'password'
        }, {
            label: "URL",
            ref: 'url'
        }],
        "static": [

        ],
        "mock":[
            {
                label: "Username",
                ref: "username"
            }
        ]
    };
    this.accountType = [{
        "name": "ODL",
        "account-type": "odl",
    }, {
        "name": "Mock",
        "account-type": "mock",
    }];
    this.sdn = {
        name: '',
        'account-type': 'odl',
        'odl': {
            username: '',
            password: '',
            url: ''
        }
    };
    this.exportPublicMethods({
      updateName: this.updateName.bind(this),
      updateAccount: this.updateAccount.bind(this)
    })
}

createSdnAccountStore.prototype.createSuccess = function() {
    console.log('success', arguments);
    this.setState({
        isLoading: false
    });
};
createSdnAccountStore.prototype.createLoading = function() {
    console.log('success', arguments);
    this.setState({
        isLoading: true
    });
};
createSdnAccountStore.prototype.updateLoading = function() {
    console.log('success', arguments);
    this.setState({
        isLoading: true
    });
};
createSdnAccountStore.prototype.updateName = function(sdn) {
    this.setState({
        sdn:sdn
    });
};
createSdnAccountStore.prototype.updateAccount = function(sdn) {
    this.setState({
        sdn:sdn
    });
};
createSdnAccountStore.prototype.updateSuccess = function() {
    console.log('success', arguments);
    this.setState({
        isLoading: false
    });
};

createSdnAccountStore.prototype.deleteSuccess = function() {
    console.log('success', arguments);
};

createSdnAccountStore.prototype.getSdnAccountSuccess = function(data) {
    console.log('success', arguments);
    this.setState({
      sdn: data.sdnAccount
    });
};
createSdnAccountStore.prototype.validateError = function(msg) {
    this.setState({
        validateErrorEvent: true,
        validateErrorMsg: msg
    });
};
createSdnAccountStore.prototype.validateReset = function() {
    this.setState({
        validateErrorEvent: false
    });
};
createSdnAccountStore.prototype.getSdnAccountsSuccess = function(sdnAccounts) {
    let data = null;
    if(sdnAccounts.statusCode == 204) {
        data = null;
    } else {
        data = sdnAccounts;
    }
    this.setState({
        sdnAccounts: data || [],
        isLoading: false
    })
};
createSdnAccountStore.prototype.getCatalogSuccess = function(data) {
  var self = this;
  var descriptorCount = 0;
  data.forEach(function(catalog) {
    descriptorCount += catalog.descriptors.length;
  });

  self.setState({
    descriptorCount: descriptorCount
  });
};

module.exports = Alt.createStore(createSdnAccountStore);;
