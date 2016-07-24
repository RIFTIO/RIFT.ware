
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var rw = require('utils/rw.js');
var API_SERVER = rw.getSearchParams(window.location).api_server;
var NODE_PORT = 3000;
var aboutActions = require('./aboutActions.js');
var Utils = require('utils/utils.js');
import $ from 'jquery';
var aboutSource = {
  get: function() {
    return {
      remote: function(state) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/mission-control/about?api_server=' + API_SERVER,
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
            console.log(xhr)
            //Authentication and the handling of fail states should be wrapped up into a connection class.
            Utils.checkAuthentication(xhr.status);
          });

        }).catch(function(){});
      },
      success: aboutActions.getAboutSuccess,
      loading: aboutActions.getAboutLoading,
      error: aboutActions.getAboutFail
    }
  },
  createTime: function() {
    return {
      remote: function(state) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':3000/mission-control/create-time?api_server=' + API_SERVER,
            type:'GET',
            beforeSend: Utils.addAuthorizationStub,
            contentType: "application/json",
            success: function(data) {
              resolve(data);
            },
            error: function(error) {
              console.log("There was an error getting the uptime : ", error);
              reject(error);
            }
          })

        }).catch(function(){});;
      },
      success: aboutActions.getCreateTimeSuccess,
      loading: aboutActions.getCreateTimeLoading,
      error: aboutActions.getCreateTimeFail
    }
  }
}
module.exports = aboutSource;
