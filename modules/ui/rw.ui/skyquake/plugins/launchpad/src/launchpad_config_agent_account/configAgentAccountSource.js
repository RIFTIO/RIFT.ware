
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
var NODE_PORT = 3000;
var createConfigAgentAccountActions = require('./configAgentAccountActions.js');
var Utils = require('utils/utils.js');
import $ from 'jquery';
var createConfigAgentAccountSource = {
  /**
   * Creates a new Config Agent Account
   * @param  {object} state Reference to parent store state.
   * @param  {object} configAgentAccount Config Agent Account payload. Should resemble the following:
   *                               {
   *                                 "name": "Config-Agent-Account-One",
   *                                 "config-agent-type":"type",
   *                                 "type (juju)": {
   *                                   "Type specific options"
   *                                 }
   *                               }
   * @return {[type]}              [description]
   */
  create: function() {

    return {
      remote: function(state, configAgentAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/launchpad/config-agent-account?api_server=' + API_SERVER,
            type:'POST',
            beforeSend: Utils.addAuthorizationStub,
            data: JSON.stringify(configAgentAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            }
            ,error: function(error) {
              console.log("There was an error creating the config agent account: ", arguments);
            }
          }).fail(function(xhr){
            console.log('checking authentication');
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject()
          });

        });
      },
      success: createConfigAgentAccountActions.createConfigAccountSuccess,
      loading: createConfigAgentAccountActions.createConfigAccountLoading,
      error: createConfigAgentAccountActions.createConfigAccountFailed
    }
  },

  /**
   * Updates a Config Agent Account
   * @param  {object} state Reference to parent store state.
   * @param  {object} configAgentAccount Config Agent Account payload. Should resemble the following:
   *                               {
   *                                 "name": "Config-Agent-Account-One",
   *                                 "config-agent-type":"type",
   *                                 "type (juju)": {
   *                                   "Type specific options"
   *                                 }
   *                               }
   * @return {[type]}              [description]
   */
  update: function() {

    return {
      remote: function(state, configAgentAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/launchpad/config-agent-account/' + configAgentAccount.name + '?api_server=' + API_SERVER,
            type:'PUT',
            beforeSend: Utils.addAuthorizationStub,
            data: JSON.stringify(configAgentAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error updating the config agent account: ", configAgentAccount.name, error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject();
          });
        });
      },
      success: createConfigAgentAccountActions.updateSuccess,
      loading: createConfigAgentAccountActions.updateLoading,
      error: createConfigAgentAccountActions.updateFail
    }
  },

  /**
   * Deletes a Config Agent Account
   * @param  {object} state Reference to parent store state.
   * @param  {object} configAgentAccount configAgentAccount to delete
   * @return {[type]}              [description]
   */
  delete: function() {

    return {
      remote: function(state, configAgentAccount, cb) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/launchpad/config-agent-account/' + configAgentAccount + '?api_server=' + API_SERVER,
            type:'DELETE',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve({data:data, cb:cb});
            },
            error: function(error) {
              console.log("There was an error deleting the config agent account: ", configAgentAccount, error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject(arguments);
          });
        });
      },
      success: createConfigAgentAccountActions.deleteSuccess,
      loading: createConfigAgentAccountActions.updateLoading,
      error: createConfigAgentAccountActions.deleteFail
    }
  },
  /**
   * Get a config agent account
   *
   * @return {Promise}
   */
  getConfigAgentAccount: function() {
    return {
      remote: function(state, configAgentAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/launchpad/config-agent-account/' + configAgentAccount + '?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve({
                configAgentAccount: data
              });
            },
            error: function(error) {
              console.log('There was an error getting configAgentAccount', error);
              reject(error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });
        });
      },
      success: createConfigAgentAccountActions.getConfigAgentAccountSuccess,
      error: createConfigAgentAccountActions.getConfigAgentAccountFail
    }
  },
  getConfigAgentAccounts: function() {
    return {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/launchpad/config-agent-account?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(configAgentAccounts) {
              resolve(configAgentAccounts);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });
        });
      },
      success: createConfigAgentAccountActions.getConfigAgentAccountsSuccess,
      error: createConfigAgentAccountActions.getConfigAgentAccountsFail
    }
  }
};

module.exports = createConfigAgentAccountSource;
