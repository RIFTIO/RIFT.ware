
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
var NODE_PORT = 3000;
var Utils = require('utils/utils.js');
import $ from 'jquery';
var createCloudAccountSource = function(Alt){
  return {
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

    create: {
      remote: function(state, cloudAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/cloud-account?api_server=' + API_SERVER,
            type:'POST',
            beforeSend: Utils.addAuthorizationStub,
            data: JSON.stringify(cloudAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error creating the cloud account: ", arguments);

            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject();
          });

        });
      },
      success: Alt.actions.global.createSuccess,
      loading: Alt.actions.global.createLoading,
      error: Alt.actions.global.createFail
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
  update:  {
      remote: function(state, cloudAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/cloud-account/' + cloudAccount.name + '?api_server=' + API_SERVER,
            type:'PUT',
            beforeSend: Utils.addAuthorizationStub,
            data: JSON.stringify(cloudAccount),
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error updating the cloud account: ", cloudAccount.name, error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject();
          });
        });
      },
      success: Alt.actions.global.updateSuccess,
      loading: Alt.actions.global.updateLoading,
      error: Alt.actions.global.updateFail
  },

  /**
   * Deletes a Cloud Account
   * @param  {object} state        Reference to parent store state.
   * @param  {object} cloudAccount cloudAccount to delete
   * @return {[type]}              [description]
   */
  delete: {

      remote: function(state, cloudAccount, cb) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/cloud-account/' + cloudAccount + '?api_server=' + API_SERVER,
            type:'DELETE',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve({data:data, cb:cb});
            },
            error: function(error) {
              console.log("There was an error deleting the cloud account: ", cloudAccount, error);

            }
          }).fail(function() {
            reject(arguments);
          });
        });
      },
      success: Alt.actions.global.deleteSuccess,
      loading: Alt.actions.global.updateLoading,
      error: Alt.actions.global.deleteCloudAccountFail
  },
  /**
   * Get a cloud account
   *
   * @return {Promise}
   */
  getCloudAccountByName:  {
      remote: function(state, cloudAccount) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/cloud-account/' + cloudAccount + '?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve({
                cloudAccount: data
              });
            },
            error: function(error) {
              console.log('There was an error getting cloudAccount', error);
              reject(error);
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });;
        });
      },
      success: Alt.actions.global.getCloudAccountSuccess,
      error: Alt.actions.global.getCloudAccountFail
  },
  getCloudAccounts: {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/cloud-account?api_server=' + API_SERVER,
            // url: '//' + window.location.hostname + ':3000/mission-control/cloud-account?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(cloudAccounts) {
              resolve(cloudAccounts);
            }
          });
        });
      },
      success: Alt.actions.global.getCloudAccountsSuccess,
      error: Alt.actions.global.getCloudAccountsFail
  }
}
};

module.exports = createCloudAccountSource;
