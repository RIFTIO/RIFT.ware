
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
let getSearchParams = require('utils/rw.js').getSearchParams;

var API_SERVER = getSearchParams(window.location).api_server;
let HOST = API_SERVER;
let NODE_PORT = getSearchParams(window.location).api_port || 3000;
let DEV_MODE = getSearchParams(window.location).dev_mode || false;

if (DEV_MODE) {
    HOST = window.location.protocol + '//' + window.location.hostname;
}


var TopologyActions = require('./topologyActions.js');
var Utils = require('utils/utils.js');
import $ from 'jquery';

export default {
  openNSRTopologySocket: function() {
    return {
      remote: function(state, id) {
        return new Promise(function(resolve, reject) {
          //If socket connection already exists, eat the request.
          if(state.socket) {
            return resolve(false);
          }
           $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/socket-polling?api_server=' + API_SERVER ,
            type: 'POST',
            beforeSend: Utils.addAuthorizationStub,
            data: {
              url: HOST + ':' + NODE_PORT + '/launchpad/nsr/' + id + '/compute-topology?api_server=' + API_SERVER
            },
            success: function(data, textStatus, jqXHR) {
              var url = Utils.webSocketProtocol() + '//' + window.location.hostname + ':' + data.port + data.socketPath;
              var ws = new WebSocket(url);
              resolve(ws);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });;
        });
      },
      loading: TopologyActions.openNSRTopologySocketLoading,
      success: TopologyActions.openNSRTopologySocketSuccess,
      error: TopologyActions.openNSRTopologySocketError
    };
  },
  getNSRTopology() {
    return {
        remote: function(state, id) {
        id = 0;
        return new Promise(function (resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr/' + id + '/compute-topology?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error getting the compute topology data", error);
              reject(error);
            }
          });
        });
      },
      local() {
        return null;
      },
      success: TopologyActions.getNSRTopologySuccess,
      error: TopologyActions.getNSRTopologyError,
      loading: TopologyActions.getNSRTopologyLoading
    }
  },
  getRawVNFR() {
    return {
      remote: function(state, vnfrID) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/operational/vnfr-catalog/vnfr/' + vnfrID + '?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          });
        })
      },
      loading: TopologyActions.getRawLoading,
      success: TopologyActions.getRawSuccess,
      error: TopologyActions.getRawError
    }
  },
  getRawNSR() {
    return {
      remote: function(state, nsrID) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/operational/ns-instance-opdata/nsr/' + nsrID + '?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          });
        })
      },
      loading: TopologyActions.getRawLoading,
      success: TopologyActions.getRawSuccess,
      error: TopologyActions.getRawError
    }
  },
  getRawVDUR() {
    return {
      remote: function(state, vdurID, vnfrID) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/operational/vnfr-catalog/vnfr/' + vnfrID + '/vdur/' + vdurID + '?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          });
        })
      },
      loading: TopologyActions.getRawLoading,
      success: TopologyActions.getRawSuccess,
      error: TopologyActions.getRawError
    }
  },
}
