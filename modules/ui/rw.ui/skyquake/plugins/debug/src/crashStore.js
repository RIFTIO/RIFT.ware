
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from 'widgets/skyquake_container/skyquakeAltInstance';
function crashStore () {
  this.exportAsync(require('./crashSource.js'));
  this.bindActions(require('./crashActions.js'));
}

crashStore.prototype.getCrashDetailsSuccess = function(list) {
  this.setState({
    crashList:list
  })
  console.log('success', list)
};
crashStore.prototype.getCrashDetailsLoading = function(info) {
  console.log('Loading crash details...', info)
};
crashStore.prototype.getCrashDetailsFailure = function(info) {
  console.log('Failed to retrieve crash/debug details', info)
};

module.exports = Alt.createStore(crashStore);;

