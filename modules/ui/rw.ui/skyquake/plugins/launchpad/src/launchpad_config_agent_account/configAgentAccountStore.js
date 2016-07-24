
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from '../alt';
import AppHeaderActions from 'widgets/header/headerActions.js';
import LaunchNetworkServiceSource from '../network_service_launcher/launchNetworkServiceSource';
import LaunchNetworkServiceActions from '../network_service_launcher/launchNetworkServiceActions';
var ConfigAgentAccountSource = require('./configAgentAccountSource');
var ConfigAgentAccountActions = require('./configAgentAccountActions');

function createconfigAgentAccountStore () {
  this.exportAsync(ConfigAgentAccountSource);
  this.bindAction(ConfigAgentAccountActions.CREATE_CONFIG_ACCOUNT_SUCCESS, this.createConfigAccountSuccess);
  this.bindAction(ConfigAgentAccountActions.CREATE_CONFIG_ACCOUNT_LOADING, this.createConfigAccountLoading);
  this.bindAction(ConfigAgentAccountActions.CREATE_CONFIG_ACCOUNT_FAILED, this.createConfigAccountFailed);
  this.bindAction(ConfigAgentAccountActions.UPDATE_SUCCESS, this.updateSuccess);
  this.bindAction(ConfigAgentAccountActions.UPDATE_LOADING, this.updateLoading);
  this.bindAction(ConfigAgentAccountActions.UPDATE_FAIL, this.updateFail);
  this.bindAction(ConfigAgentAccountActions.DELETE_SUCCESS, this.deleteSuccess);
  this.bindAction(ConfigAgentAccountActions.DELETE_FAIL, this.deleteFail);
  this.bindAction(ConfigAgentAccountActions.GET_CONFIG_AGENT_ACCOUNT_SUCCESS, this.getConfigAgentAccountSuccess);
  this.bindAction(ConfigAgentAccountActions.GET_CONFIG_AGENT_ACCOUNT_FAIL, this.getConfigAgentAccountFail);
  this.bindAction(ConfigAgentAccountActions.GET_CONFIG_AGENT_ACCOUNTS_SUCCESS, this.getConfigAgentAccountsSuccess);
  // this.bindAction(ConfigAgentAccountActions.GET_CONFIG_AGENT_ACCOUNTS_FAIL, this.getConfigAgentAccountsFail);
  this.exportAsync(LaunchNetworkServiceSource);
  this.exportPublicMethods({
    resetState: this.resetState.bind(this),
    updateConfigAgentAccount: this.updateConfigAgentAccount.bind(this)
  })
  this.configAgentAccount = {};
  this.configAgentAccounts = [];
  var self = this;
  this.isLoading = true;
  this.params = {
    "juju": [{
        label: "IP Address",
        ref: 'ip-address'
    }, {
        label: "Port",
        ref: 'port',
        optional: true
    }, {
        label: "Username",
        ref: 'user',
        optional: true
    }, {
        label: "Secret",
        ref: 'secret',
        optional: true
    }]
  };

  this.accountType = [{
      "name": "JUJU",
      "account-type": "juju"
  }];

  this.configAgentAccount = {
      name: '',
      'account-type': 'juju',
      params: this.params['juju']
  };
}

createconfigAgentAccountStore.prototype.resetState = function() {
  this.setState({
    configAgentAccount: {
      name: '',
      'account-type': 'juju',
      params: this.params['juju']
    }
  });
};
createconfigAgentAccountStore.prototype.validateReset = function() {
  this.setState({
    validateErrorEvent: false
  });
};
createconfigAgentAccountStore.prototype.createConfigAccountLoading = function() {
  this.setState({
    isLoading: true
  });
};
createconfigAgentAccountStore.prototype.createConfigAccountSuccess = function() {
  this.setState({
    isLoading: false
  });
};
createconfigAgentAccountStore.prototype.createConfigAccountFailed = function(msg) {
  console.log('failed')
  var xhr = arguments[0];
  this.setState({
    isLoading: false
  });
};
createconfigAgentAccountStore.prototype.updateLoading = function() {
  this.setState({
    isLoading: true
  });
};
createconfigAgentAccountStore.prototype.updateSuccess = function() {
  this.setState({
    isLoading: false
  });
};
createconfigAgentAccountStore.prototype.updateFail = function() {
  var xhr = arguments[0];
  this.setState({
    isLoading: false
  });
};

createconfigAgentAccountStore.prototype.updateConfigAgentAccount = function(configAgentAccount) {
  this.setState({
    configAgentAccount: configAgentAccount
  })
};

createconfigAgentAccountStore.prototype.deleteSuccess = function(data) {
  this.setState({
    isLoading: false
  });
  if(data.cb) {
    data.cb();
  }
};

createconfigAgentAccountStore.prototype.deleteFail = function(data) {
  this.setState({
    isLoading: false
  });
  if(data.cb) {
    data.cb();
  }
};

createconfigAgentAccountStore.prototype.getConfigAgentAccountSuccess = function(data) {

  let configAgentAccount = data.configAgentAccount;
  configAgentAccount.params = configAgentAccount[configAgentAccount['account-type']];
  let stateObject = {
    configAgentAccount: configAgentAccount,
    isLoading: false
  }
  this.setState(stateObject);
};

createconfigAgentAccountStore.prototype.getConfigAgentAccountFail = function(data) {
  this.setState({
    isLoading:false
  });
};
createconfigAgentAccountStore.prototype.getConfigAgentAccountsSuccess = function(configAgentAccounts) {
  this.setState({
    configAgentAccounts: configAgentAccounts || [],
    isLoading:false
  });
};

module.exports = Alt.createStore(createconfigAgentAccountStore);;

