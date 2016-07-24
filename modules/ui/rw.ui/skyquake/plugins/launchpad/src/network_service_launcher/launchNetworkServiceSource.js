
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import $ from 'jquery';
let Utils = require('utils/utils.js');
let rw = require('utils/rw.js');
const API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
const API_PORT = require('utils/rw.js').getSearchParams(window.location).api_port;
const NODE_PORT = API_PORT || 3000;
export default function(Alt){
  const Actions = Alt.actions.global;
  return {
  getCatalog: {
      remote (state) {
        return new Promise((resolve,reject) => {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/decorated-catalog?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function (data) {
              resolve(
                      typeof(data) == "string" ? JSON.parse(data):data
                      );
            }
          }).fail(function(xhr){
            console.log(xhr)
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });
        });
      },
      success: Alt.actions.global.getCatalogSuccess,
      error: Alt.actions.global.getCatalogError
  },
  getCloudAccount:{
      remote (state, cb) {
        return new Promise((resolve, reject) => {
          $.ajax({
            url: '//' + window.location.hostname + ':' +
              NODE_PORT + '/launchpad/cloud-account?api_server=' +
              API_SERVER,
              type: 'GET',
              beforeSend: Utils.addAuthorizationStub,
              success: function (data) {
                resolve(data);
                if(cb) {
                  cb();
                }
              }
          })
        })
      },
      success: Alt.actions.global.getLaunchCloudAccountSuccess,
      error: Alt.actions.global.getLaunchCloudAccountError
  },
  getDataCenters:{
      remote () {
        return new Promise((resolve, reject) => {
          $.ajax({
            url: '//' + window.location.hostname + ':' +
              NODE_PORT + '/launchpad/data-centers?api_server=' +
              API_SERVER,
              type: 'GET',
              beforeSend: Utils.addAuthorizationStub,
              success: function (data) {
                resolve(data);
              }
          })
        })
      },
      success: Alt.actions.global.getDataCentersSuccess,
      error: Alt.actions.global.getDataCentersError
  },
  getVDU: {
      remote (state, VNFDid) {
        return new Promise((resolve,reject) => {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/vnfd?api_server=' + API_SERVER,
            type: 'POST',
            beforeSend: Utils.addAuthorizationStub,
            dataType:'json',
            data: {
              data: VNFDid
            },
            success: function (data) {
              resolve(
                typeof(data) == "string" ? JSON.parse(data):data
              );
            }
          })
        });
      },
      success: Alt.actions.global.getVDUSuccess,
      error: Alt.actions.global.getVDUError
  },
  launchNSR: {
      remote (state, NSR) {
        return new Promise((resolve, reject) => {
          console.log('Attempting to instantiate NSR:', NSR)
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr?api_server=' + API_SERVER,
            type: 'POST',
            beforeSend: Utils.addAuthorizationStub,
            dataType:'json',
            data: {
              data: NSR
            },
            success: function (data) {
              resolve(
                      typeof(data) == "string" ? JSON.parse(data):data
                      );
            },
            error: function (err) {
              console.log('There was an error launching')
            }
          }).fail(function(xhr){
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
            reject();
          });
        })
      },
      loading: Alt.actions.global.launchNSRLoading,
      success: Alt.actions.global.launchNSRSuccess,
      error: Alt.actions.global.launchNSRError
  },
  getInputParams:{
      remote(state, NSDId) {
        return new Promise((resolve, reject) => {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsd/' + NSDId + '/input-param?api_server=' + API_SERVER,
            type: 'GET',
              beforeSend: Utils.addAuthorizationStub,
              success: function (data) {
                resolve(data);
              }
          });
        });
      }
  },
  getLaunchpadConfig: {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/config?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          });
        });
      },
      success: Alt.actions.global.getLaunchpadConfigSuccess,
      error: Alt.actions.global.getLaunchpadConfigError
  }
}
}
