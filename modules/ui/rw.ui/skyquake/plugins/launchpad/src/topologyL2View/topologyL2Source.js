
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
var TopologyL2Actions = require('./topologyL2Actions.js');
var Utils = require('utils/utils.js');
import $ from 'jquery';

if (DEV_MODE) {
    HOST = window.location.protocol + '//' + window.location.hostname;
}

export default {
  openTopologyApiSocket: function() {
    return {
      remote: function(state, id) {
        // TODO: add topology type to the parameter
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
              url: HOST + ':' + NODE_PORT + '/launchpad/network-topology?api_server=' + API_SERVER
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
      loading: TopologyL2Actions.openTopologyApiSocketLoading,
      success: TopologyL2Actions.openTopologyApiSocketSuccess,
      error: TopologyL2Actions.openTopologyApiSocketError
    };
  },
  fetchTopology() {
    return {
      remote() {
        return new Promise(function (resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/network-topology?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error getting the network topology data", error);
              reject(error);
            }
          });
        })
      },
      local() {
        return null;
      },
      success: TopologyL2Actions.getTopologyApiSuccess,
      error: TopologyL2Actions.getTopologyApiError,
      loading: TopologyL2Actions.getTopologyApiLoading
    }
  }
}
