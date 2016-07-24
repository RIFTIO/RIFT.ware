
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
var NODE_PORT = 3000;
var createSdnAccountActions = require('./createSdnAccountActions.js');
var Utils = require('../../../../framework/utils/utils.js');
import $ from 'jquery';
var createSdnAccountSource = {
  /**
   * Creates a new Cloud Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} cloudAccount Cloud Account payload. Should resemble the following:
   *                               {
   *                                 "name": "Cloud-Account-One",
   *                                 "cloud-type":"type",
   *                                 "type (openstack/aws)": {
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
            url: '//' + window.location.hostname + ':3000/mission-control/sdn-account?api_server=' + API_SERVER,
            type:'POST',
            beforeSend: Utils.addAuthorizationStub,
            dataType: "json",
            data: JSON.stringify(sdnAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error creating the sdn account: ", error);
              reject(error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });

        });
      },
      success: createSdnAccountActions.createSuccess,
      loading: createSdnAccountActions.createLoading,
      error: createSdnAccountActions.createFail
    }
  },

  /**
   * Updates a Cloud Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} cloudAccount Cloud Account payload. Should resemble the following:
   *                               {
   *                                 "name": "Cloud-Account-One",
   *                                 "cloud-type":"type",
   *                                 "type (openstack/aws)": {
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
            url: '//' + window.location.hostname + ':3000/mission-control/sdn-account/' + sdnAccount.name + '?api_server=' + API_SERVER,
            type:'PUT',
            beforeSend: Utils.addAuthorizationStub,
            dataType: "json",
            data: JSON.stringify(sdnAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error updating the sdn account: ", sdnAccount.name, error);
              reject(error);
            }
          });
        });
      },
      success: createSdnAccountActions.updateSuccess,
      loading: createSdnAccountActions.updateLoading,
      error: createSdnAccountActions.updateFail
    }
  },

  /**
   * Deletes a Cloud Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} cloudAccount cloudAccount to delete
   * @return {[type]}              [description]
   */
  delete: function() {

    return {
      remote: function(state, cloudAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/mission-control/sdn-account/' + cloudAccount + '?api_server=' + API_SERVER,
            type:'DELETE',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error deleting the sdn account: ", sdnAccount, error);
              reject(error);
            }
          });
        });
      },
      success: createSdnAccountActions.deleteSuccess,
      error: createSdnAccountActions.deleteFail
    }
  },
  /**
   * Get a cloud account
   *
   * @return {Promise}
   */
  getSdnAccount: function() {
    return {
      remote: function(state, sdnAccount) {
        console.log('requesting sdnAccount, ', sdnAccount)
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/mission-control/sdn-account/' + sdnAccount + '?api_server=' + API_SERVER,
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
            url: '//' + window.location.hostname + ':3000/mission-control/sdn-account?api_server=' + API_SERVER,
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
};

module.exports = createSdnAccountSource;
