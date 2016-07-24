
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import createSdnAccountActions from './sdnAccountActions.js';
import $ from 'jquery';
let Utils = require('utils/utils.js');
let rw = require('utils/rw.js');
let API_SERVER = rw.getSearchParams(window.location).api_server;
let HOST = API_SERVER;
let NODE_PORT = rw.getSearchParams(window.location).api_port || 3000;
let DEV_MODE = rw.getSearchParams(window.location).dev_mode || false;

if (DEV_MODE) {
    HOST = window.location.protocol + '//' + window.location.hostname;
}


export default {
  /**
   * Creates a new Sdn Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} sdnAccount   Sdn Account payload. Should resemble the following:
   *                               {
   *                                 "name": "Sdn-Account-One",
   *                                 "account-type":"type",
   *                                 "type (sdnsim/odl/mock)": {
   *                                   "Type specific options"
   *                                 }
   *                               }
   * @return {[type]}              [description]
   */
  create: function() {

    return {
      remote: function(state, sdnAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/sdn-account?api_server=' + API_SERVER,
            type:'POST',
            beforeSend: Utils.addAuthorizationStub,
            data: JSON.stringify(sdnAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error creating the sdn account: ", arguments);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject();
          });

        });
      },
      success: createSdnAccountActions.createSuccess,
      loading: createSdnAccountActions.createLoading,
      error: createSdnAccountActions.createFail
    }
  },

  /**
   * Updates a Sdn Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} sdnAccount   Sdn Account payload. Should resemble the following:
   *                               {
   *                                 "name": "Sdn-Account-One",
   *                                 "account-type":"type",
   *                                 "type (sdnsim/odl/mock)": {
   *                                   "Type specific options"
   *                                 }
   *                               }
   * @return {[type]}              [description]
   */
  update: function() {

    return {
      remote: function(state, sdnAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/sdn-account/' + sdnAccount.name + '?api_server=' + API_SERVER,
            type:'PUT',
            beforeSend: Utils.addAuthorizationStub,
            data: JSON.stringify(sdnAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error updating the sdn account: ", sdnAccount.name, error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject();
          });
        });
      },
      success: createSdnAccountActions.updateSuccess,
      loading: createSdnAccountActions.updateLoading,
      error: createSdnAccountActions.updateFail
    }
  },

  /**
   * Deletes a Sdn Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} sdnAccount   sdnAccount to delete
   * @return {[type]}              [description]
   */
  delete: function() {

    return {
      remote: function(state, sdnAccount, cb) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/sdn-account/' + sdnAccount + '?api_server=' + API_SERVER,
            type:'DELETE',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve({data:data, cb:cb});
            },
            error: function(error) {
              console.log("There was an error deleting the sdn account: ", sdnAccount, error);
              reject(error);
            }
          });
        });
      },
      success: createSdnAccountActions.deleteSuccess,
      loading: createSdnAccountActions.updateLoading,
      error: createSdnAccountActions.deleteFail
    }
  },
  /**
   * Get a sdn account
   *
   * @return {Promise}
   */
  getSdnAccount: function() {
    return {
      remote: function(state, sdnAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/sdn-account/' + sdnAccount + '?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve({
                sdnAccount: data
              });
            },
            error: function(error) {
              console.log('There was an error getting sdnAccount', error);
              reject(error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });;
        });
      },
      success: createSdnAccountActions.getSdnAccountSuccess,
      error: createSdnAccountActions.getSdnAccountFail
    }
  },
  getSdnAccounts: function() {
    return {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/sdn-account?api_server=' + API_SERVER,
            // url: '//' + window.location.hostname + ':3000/mission-control/sdn-account?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(sdnAccounts) {
              resolve(sdnAccounts);
            }
          });
        });
      },
      success: createSdnAccountActions.getSdnAccountsSuccess,
      error: createSdnAccountActions.getSdnAccountsFail
    }
  }
}

