
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var alt = require('./alt');
var Utils = require('utils/utils.js');
var API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
var NODE_PORT = 3000;
var createActions = require('./createActions.js');
import $ from 'jquery';

module.exports = {
  getNetworkServices: function() {
    return {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/launchpad/network-service?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          })
        })
      },
      success: createActions.getNetworkServicesSuccess,
      error: createActions.getNetworkServicesError
    }
  },
  createEnvironment: function() {
    return {
      remote: function(state, environment) {
        return $.ajax({
          url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/launchpad/environment?api_server=' + API_SERVER,
          type: 'POST',
          beforeSend: Utils.addAuthorizationStub,
          dataType: 'json',
          data: JSON.stringify(environment),
          contentType: 'application/json',
          accept: 'application/json'
        })

      },
      success: createActions.createEnvironmentSuccess,
      error: createActions.createEnvironmentsError
    }
  },
  getPools: function() {
    return {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/launchpad/pools?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          })
        })
      },
      success: createActions.getPoolsSuccess,
      error: createActions.getPoolsError
    }
  },
  getSlaParams: function() {
    return {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/launchpad/sla-params?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          })
        })
      },
      success: createActions.getSlaParamsSuccess,
      error: createActions.getSlaParamsError
    }
  }
}
