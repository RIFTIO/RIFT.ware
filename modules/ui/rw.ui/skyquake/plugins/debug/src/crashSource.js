
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var rw = require('utils/rw.js');
var API_SERVER = rw.getSearchParams(window.location).api_server;
var NODE_PORT = 3000;
var crashActions = require('./crashActions.js');
var Utils = require('utils/utils.js');
import $ from 'jquery';
var crashSource = {

  get: function() {
    return {
      remote: function(state) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/mission-control/crash-details?api_server=' + API_SERVER,
            type:'GET',
            beforeSend: Utils.addAuthorizationStub,
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error getting the crash details: ", error);
              reject(error);
            }
          }).fail(function(xhr){
            console.log('fail')
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });
        }).catch(function(err){console.log(err)});
      },
      success: crashActions.getCrashDetailsSuccess,
      loading: crashActions.getCrashDetailsLoading,
      error: crashActions.getCrashDetailsFail
    }
  }
}
module.exports = crashSource;
