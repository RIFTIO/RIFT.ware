
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var alt = require('../core/alt');
function CreateFleet () {
  this.exportAsync(require('./createSource.js'));
  this.bindActions(require('./createActions.js'));
  this.nsd = [];
  this.vnfd = [];
  this.pnfd = [];
  validateErrorEvent = false;
  validateErrorMsg = "";
};

CreateFleet.prototype.getNetworkServicesSuccess = function(data) {
  this.setState({
    networkServices: data
  })
};
CreateFleet.prototype.getSlaParamsSuccess = function(data) {
  this.setState({
    slaParams: data
  })
};
CreateFleet.prototype.getPoolsSuccess = function(data) {
  this.setState({
    pools: data
  })
};
CreateFleet.prototype.validateError = function(msg) {
  this.setState({
    validateErrorEvent: true,
    validateErrorMsg: msg
  });
};
CreateFleet.prototype.validateReset = function() {
  this.setState({
    validateErrorEvent: false
  });
};

module.exports = alt.createStore(CreateFleet);

