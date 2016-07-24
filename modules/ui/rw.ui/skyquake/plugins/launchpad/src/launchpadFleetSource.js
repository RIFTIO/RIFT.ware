
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
let API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
let HOST = API_SERVER;
let NODE_PORT = require('utils/rw.js').getSearchParams(window.location).api_port || 3000;
let DEV_MODE = require('utils/rw.js').getSearchParams(window.location).dev_mode || false;

if (DEV_MODE) {
    HOST = window.location.protocol + '//' + window.location.hostname;
}

var isSocketOff = true;
var FleetActions = require('./launchpadFleetActions.js');
var Utils = require('utils/utils.js');
import $ from 'jquery';


module.exports = function(Alt) {
  //NEW SOURCE
  return {
    nsrControl: {
      remote: function(state, method, url, value) {
        return new Promise(function(resolve, reject) {
          // $.ajax({});
          // console.log(method + 'ing: "' + value + '" to "' + url + '"');
          resolve(method + 'ing: "' + value + '" to "' + url + '"')
        });
      },
      success: FleetActions.nsrControlSuccess,
      error: FleetActions.nsrControlError
    },
    getNsrInstances: {
      remote: function() {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr?api_server=' + API_SERVER,
            type: 'GET',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          });
        });
      },
      success: FleetActions.getNsrInstancesSuccess,
      error: FleetActions.getNsrInstancesError
    },
    deleteNsrInstance: {
      remote: function(d, id) {
        console.log(id)
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr/' + id + '?api_server=' + API_SERVER,
            type: 'DELETE',
            beforeSend: Utils.addAuthorizationStub,
            success: function(data) {
              resolve(data);
            }
          });
        });
      },
      success: FleetActions.deleteNsrInstanceSuccess,
      error: FleetActions.deleteNsrInstancesError
    }  ,
    openNSRSocket: {
      remote: function(state) {
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
              url: HOST + ':' + NODE_PORT + '/launchpad/nsr?api_server=' + API_SERVER
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
      loading: Alt.actions.global.openNSRSocketLoading,
      success: FleetActions.openNSRSocketSuccess,
      error: FleetActions.openNSRError
    },
    setNSRStatus: {
      remote: function(state, id, status) {
        return new Promise(function(resolve, reject) {
          $.ajax({
            url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr/' + id + '/admin-status?api_server=' + API_SERVER ,
            type:'PUT',
            beforeSend: Utils.addAuthorizationStub,
            data: {
              status: status
            },
            success: function(data, textStatus, jqXHR) {

            }
          });
        });
      },
      success: FleetActions.setNSRStatusSuccess,
      error: FleetActions.setNSRStatusError
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
      success: FleetActions.getLaunchpadConfigSuccess,
      error: FleetActions.getLaunchpadConfigError
    }
  }
};


// module.exports = {
//   //NEW SOURCE
//   nsrControl: function(state) {
//     return {
//       remote: function(state, method, url, value) {
//         return new Promise(function(resolve, reject) {
//           // $.ajax({});
//           // console.log(method + 'ing: "' + value + '" to "' + url + '"');
//           resolve(method + 'ing: "' + value + '" to "' + url + '"')
//         });
//       },
//       success: FleetActions.nsrControlSuccess,
//       error: FleetActions.nsrControlError
//     };
//   },
//   getNsrInstances: function() {
//     return {
//       remote: function() {
//         return new Promise(function(resolve, reject) {
//           $.ajax({
//             url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr?api_server=' + API_SERVER,
//             type: 'GET',
//             beforeSend: Utils.addAuthorizationStub,
//             success: function(data) {
//               resolve(data);
//             }
//           });
//         });
//       },
//       success: FleetActions.getNsrInstancesSuccess,
//       error: FleetActions.getNsrInstancesError
//     };
//   },
//   deleteNsrInstance: function(d, id) {
//     return {
//       remote: function(d, id) {
//         console.log(id)
//         return new Promise(function(resolve, reject) {
//           $.ajax({
//             url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr/' + id + '?api_server=' + API_SERVER,
//             type: 'DELETE',
//             beforeSend: Utils.addAuthorizationStub,
//             success: function(data) {
//               resolve(data);
//             }
//           });
//         });
//       },
//       success: FleetActions.deleteNsrInstanceSuccess,
//       error: FleetActions.deleteNsrInstancesError
//     };
//   },
//   openNSRSocket: function(Alt) {
//     console.log(arguments)
//     return {
//       remote: function(state) {
//         return new Promise(function(resolve, reject) {
//           //If socket connection already exists, eat the request.
//           if(state.socket) {
//             return resolve(false);
//           }
//            $.ajax({
//             url: '//' + window.location.hostname + ':' + NODE_PORT + '/socket-polling?api_server=' + API_SERVER ,
//             type: 'POST',
//             beforeSend: Utils.addAuthorizationStub,
//             data: {
//               url: HOST + ':' + NODE_PORT + '/launchpad/nsr?api_server=' + API_SERVER
//             },
//             success: function(data, textStatus, jqXHR) {
//               var url = Utils.webSocketProtocol() + '//' + window.location.hostname + ':' + data.port + data.socketPath;
//               var ws = new WebSocket(url);
//               resolve(ws);
//             }
//           }).fail(function(xhr){
//             //Authentication and the handling of fail states should be wrapped up into a connection class.
//             Utils.checkAuthentication(xhr.status);
//           });;
//         });
//       },
//       loading: FleetActions.openNSRSocketLoading,
//       success: FleetActions.openNSRSocketSuccess,
//       error: FleetActions.openNSRError
//     };
//   },
//   setNSRStatus: function() {
//     return {
//       remote: function(state, id, status) {
//         return new Promise(function(resolve, reject) {
//           $.ajax({
//             url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/nsr/' + id + '/admin-status?api_server=' + API_SERVER ,
//             type:'PUT',
//             beforeSend: Utils.addAuthorizationStub,
//             data: {
//               status: status
//             },
//             success: function(data, textStatus, jqXHR) {

//             }
//           });
//         });
//       },
//       success: FleetActions.setNSRStatusSuccess,
//       error: FleetActions.setNSRStatusError
//     };
//   },
//   getLaunchpadConfig: function() {
//     return {
//       remote: function() {
//         return new Promise(function(resolve, reject) {
//           $.ajax({
//             url: '//' + window.location.hostname + ':' + NODE_PORT + '/launchpad/config?api_server=' + API_SERVER,
//             type: 'GET',
//             beforeSend: Utils.addAuthorizationStub,
//             success: function(data) {
//               resolve(data);
//             }
//           });
//         });
//       },
//       success: FleetActions.getLaunchpadConfigSuccess,
//       error: FleetActions.getLaunchpadConfigError
//     };
//   }
// };

Object.size = function(obj) {
  var size = 0,
    key;
  for (key in obj) {
    if (obj.hasOwnProperty(key)) size++;
  }
  return size;
};
