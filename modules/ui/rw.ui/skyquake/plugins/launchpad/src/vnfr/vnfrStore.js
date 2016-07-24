
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import VnfrActions from './vnfrActions.js';
import VnfrSource from './vnfrSource.js';

let alt = require('../alt');
class VnfrStore {
  constructor() {
    this.vnfrs = [];
    this.socket;
    this.isLoading = false;
    this.bindActions(VnfrActions);
    this.exportAsync(VnfrSource);



  }
  openVnfrSocketError() {

  }
  openVnfrSocketLoading() {

  }
  openVnfrSocketSuccess(connection) {
    var self = this;
    if (!connection) return;
    self.setState({
      socket: connection
    });
    connection.onmessage = function(data) {
      try {
        var data = JSON.parse(data.data);
        if (!data) {
          console.warn('NSRS property not present on the payload, check that the api server is functioning correct and that the LP is fully launched. Received: ', data);
          data = [];
        }
        self.setState({
          vnfrs: data,
          isLoading: false
        });
      } catch (e) {

      }
    };
  }
};

export default alt.createStore(VnfrStore)
