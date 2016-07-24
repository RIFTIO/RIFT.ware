
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
require('./launchpad-fleet-card-params.html');
var React = require('react');
var LaunchpadCard = require('./launchpadCard.jsx');
// var LaunchpadCard = require('../../components/dashboard_card/dashboard_card.jsx');
angular.module('launchpad')
  .directive('fleetCard', function($state, $stateParams) {
    return {
      restrict: 'AE',
      replace: true,
      template: '<div></div>',
      scope: {
        fleet: '=data',
        $index: '=',
        metric: '=?',
        slaParam: '=?',
        nfviMetric: '=?'
      },

      bindToController: true,
      controllerAs: 'card',
      link: function(scope, element, attributes) {

      },
      controller: function($timeout, $interval, $scope, $rootScope, $element) {
        var self = this;

        console.log(self.fleet)
         $scope.$watch(function() {
          return self.fleet}, reactRender)
        function reactRender() {
          console.log('rendering', self.fleet)
          React.render(
            React.createElement(LaunchpadCard, {className:'launchpadCard'}
            ),
           $element[0]
          );
        }
        //FOR WAG ONLY
        //   self.isOn = false;

        //   //END WAG ONLY
        //   //Remove for testing only
        //   $scope.$watch(function() {
        //     return self.fleet;
        //   }, function() {});

        //   // var fleetChannel = $rw.radio.channel('fleetChannel');
        //   var FleetStore = require('../launchpadFleetStore.js');
        //   self.valueFormat = {
        //     "int": 1,
        //     "dec": 0
        //   };
        //   self.federationID = $stateParams.id;
        //   //if this is true, set gauges to start
        //   if (require('utils/rw.js').getSearchParams(window.location).api_server) {
        //     self.fleet.started = true;
        //   }
        //   self.apiServer = require('utils/rw.js').getSearchParams(window.location).api_server;
        //   self.isNoisy = false;
        //   var rateChanged = function(rate) {
        //     self.rate = rate;
        //     fleetChannel.command('fleet:change:rate', rate)
        //   }
        //   var packetSizeChanged = function(packetSize) {
        //     self.packetSize = packetSize
        //     fleetChannel.command('fleet:change:packetSize', packetSize)
        //   };
        //   self.openFleet = function(index) {
        //     var params = require('utils/rw.js').getSearchParams(window.location);
        //     if (params.api_server) {
        //       if (params.api_server == "http://10.0.201.25:5000") {
        //         var newLoc = window.location.origin + window.location.pathname + '?config=' + params.config + '&api_server=' + 'http://10.0.201.25:5050' + '#/wag';
        //         window.open(newLoc);
        //         return;
        //       }
        //       if (self.fleet.api == "10.0.117.22") self.fleet.api = "10.0.117.17";
        //       var newLoc = window.location.origin + window.location.pathname + '?config=' + params.config + '&api_server=' + 'http://' + self.fleet.api + ':5050' + '&name=' + self.fleet.name + '&env_id=' + self.fleet.id + '#/dashboard/' + $stateParams.id + '/' + index;;
        //       console.log('Opening new window at: %s', newLoc)
        //       window.open(newLoc);
        //     } else {
        //       if (self.fleet.id == 'wag-fleet') {
        //         var newLoc = window.location.origin + window.location.pathname + '?config=' + params.config + '#/wag';
        //         window.open(newLoc);
        //       }
        //       window.open('#/dashboard/' + $stateParams.id + '/' + index);
        //     }
        //   };
        //   self.openConsole = function(index) {
        //     console.log(self);
        //     var params = require('utils/rw.js').getSearchParams(window.location);
        //     if (params.api_server) {
        //       if (self.fleet.api == "10.0.117.22") self.fleet.api = "10.0.117.17";
        //       var newLoc = self.apiServer + '/api/environments/' + self.fleet.id + '/console';
        //       window.open(newLoc);
        //     } else {
        //       window.open('#/dashboard/' + $stateParams.id + '/' + index);
        //     }
        //   };


        //   self.noisyToggle = function() {
        //     var action;
        //     if (self.isNoisy) {
        //       action = 'stop';
        //       self.isNoisy = false;
        //     } else {
        //       action = 'start';
        //       self.isNoisy = true;
        //     }
        //     $.ajax({
        //       url: "http://" + self.fleet.api + ':5050/api/operations/' + action + '-stream',
        //       type: "POST",
        //       headers: {
        //         "Content-Type": "application/vnd.yang.operation+json"
        //       },
        //       dataType: "json",
        //       data: JSON.stringify({
        //         "input": {
        //           "now": ""
        //         }
        //       })
        //     });
        //   };

        //   self.toggleStatus = function() {
        //     var state;
        //     var status = self.fleet.status;
        //     console.log(self.fleet)
        //     if (status == "starting" || status == "stopping") {
        //       return;
        //     }
        //     if (status == "active") {
        //       state = "inactive";
        //     } else {
        //       state = "active";
        //     }
        //     self.fleet.status = state;
        //     FleetStore.setFleetState(self.fleet.id, state);
        //   };
        //   //CAT + NOISY NEIGHBOR ONLY

        //   self.catStarted = false;
        //   self.catToggle = function() {
        //     var action;
        //     if (self.catStarted) {
        //       action = 'stop';
        //       self.catStarted = false;
        //     } else {
        //       action = 'start';
        //       self.catStarted = true;
        //     }
        //     $.ajax({
        //       url: "http://" + self.fleet.api + ':5050/api/operations/' + action + '-cat',
        //       type: "POST",
        //       headers: {
        //         "Content-Type": "application/vnd.yang.operation+json"
        //       },
        //       dataType: "json",
        //       data: JSON.stringify({
        //         "input": {
        //           "now": ""
        //         }
        //       })
        //     });
        //   };

        //   self.deleteFleet = function() {
        //     if (confirm("Do you really want to delete this fleet?")) {
        //       // if (confirm("Seriously, you REALLY want to delete this?")) {
        //         // fleetChannel.request('fleet:delete', self.fleet);
        //         FleetStore.deleteFleet(self.fleet.id)
        //       // }
        //     }
        //   };


        //   /**
        //    *  WAG Code Start
        //    **/
        //   // var wagChannel = $rw.radio.channel('wag');

        //   //If Wag page Offline
        //   if (!self.apiServer && self.fleet.id == 'wag-fleet') {
        //     self.offline = setInterval(function() {
        //       if (self.fleet.started) {
        //         self.fleet.wagpage.clientsim['traffic-gen']['rx-pps'] = 100 * (Math.random() - .5) + 200;
        //         self.fleet.wagpage.clientsim['traffic-gen']['tx-pps'] = 100 * (Math.random() - .5) + 200;
        //       }
        //     }, 2000);
        //   }

        //   // If Wag page Online
        //   if (self.fleet.template_name == 'Wireless Access Gateway') {
        //     wagChannel.command('wag-poll');
        //     wagChannel.on('wag-update', function(data) {
        //       $timeout(function() {
        //         self.fleet.wagpage = {};
        //         self.fleet.wagpage.clientsim = data.clientsim
        //           // self.data.clientsim['traffic-sink']['rx-pps'] / 1000000;
        //         self.isOn = data.clientsim['traffic-gen-status'].running;
        //       });
        //     });
        //   }

        //   $rootScope.$on('$stateChangeStart', function() {
        //     wagChannel.command('wag-poll:kill');
        //   });

        //   /**
        //    *  WAG Code End
        //    **/


      }
    };
  })

.directive('launchpadFleetCardParams', function() {
  return {
    restrict: 'AE',
    replace: true,
    templateUrl: '/modules/launchpad/launchpad_card/launchpad-fleet-card-params.html',
    controllerAs: 'params',
    scope: {
      $index: "=",
      cardData: '=',
      slaParam: '=?',
      nfviMetric: '=?'
    },
    bindToController: true,
    controller: function($scope, $stateParams, $timeout, $interval, $rootScope) {
      //remove watch
      $scope.$watch(function() {
        return self.cardData;
      }, function() {})
      var self = this;
      var apiServer = require('utils/rw.js').getSearchParams(window.location).api_server;
      self.apiServer = apiServer;
      // self.refs = refsDB;


      // var LaunchpadFleetActions = require('../launchpadFleetActions.js');
      // var LaunchpadFleetStore = require('../launchpadFleetStore.js');
      //var LaunchpadFleetActions = require('./launchpadFleetActions.js');

      self.rate = 40;
      self.packetSize = 256;
      self.refParam = $rw.refParams;
      var currentNFVIRef;

      this.visible = 'default';
      if (self.cardData.id == "wag-fleet") {
        self.cardData.started = self.cardData.wagpage.clientsim['traffic-gen-status'].running;
      }


      self.selectedNFVI = 0;
      self.selectedSLAParam = {};
      self.refPools = $rw.refPools;

      // Event Listeners
      // View Actions
      self.load = function(panel) {
        self.visible = panel;
      };
      self.filterSLA = function(ref) {
        // fleetChannel.command('filter:SLAParams', ref);
        FleetActions.filterSLAParams(ref);
      };
      self.filterNFVI = function(ref) {
        FleetActions.filterNfviMetrics(ref)
          // fleetChannel.command('filter:NFVIMetrics', ref);
      };
      self.serviceToggle = function(fleet, control) {
        if (apiServer) {
          var vnfrChannel = $rw.radio.channel('vnfr');
          switch (control.ref) {
            case "trafgen":
              var action = (control.started) ? 'stop' : 'start';
              vnfrChannel.command("vnf:command", action, control.data.api, "http://" + fleet.api + ':5050');
              break;
            case "iot_army":
              var action = (control.started) ? 'stop' : 'start';
              vnfrChannel.command("vnf:command", action, control.data.api, "http://" + fleet.api + ':5050');
              break;
          }
        } else {
          fleetChannel.command('fleet:start', $stateParams.id, self.$index);
          if ($stateParams.id == 'wag-federation') {
            if (!self.cardData.wagpage.clientsim['traffic-gen-status']) {
              self.cardData.wagpage.clientsim['traffic-gen-status'] = {};
            }
            self.cardData.wagpage.clientsim['traffic-gen-status'].running = !self.cardData.wagpage.clientsim['traffic-gen-status'].running;
          }
        }

      };
      // Init Params


      //WAG ONLY
      if (self.apiServer == "http://10.0.201.25:5000") {
        self.isWag = true;
      }
      self.toggleOn = function(e) {
        // self.setPacket(e, self.packetSize);
        // self.setRate(e, self.rate);
        // self.setSubscribers(e, self.subscribers);
        // self.setAP(e, self.ap);
        var wagServer = ""
        if (self.apiServer) {
          if (self.isOn) {
            $.ajax({
              type: "POST",
              url: "http://10.0.201.25:5050/api/running/stop-device-group/",
              success: (function() {
                console.log('stahp post sent')
              })
            });
            $.ajax({
              type: "POST",
              url: "http://10.0.201.25:5050/api/running/stop-sink-server/",
              success: (function() {
                console.log('stahp post sent')
              })
            });
          } else {
            $.ajax({
              type: "POST",
              url: "http://10.0.201.25:5050/api/running/start-sink-server/",
              success: (function() {
                console.log('start post sent')
              })
            });
            $.ajax({
              type: "POST",
              url: "http://10.0.201.25:5050/api/running/start-device-group/",
              success: (function() {
                console.log('start post sent')
              })
            });
          }
        }
        self.isOn = !self.isOn;
      }

      //END WAG ONLY

    },
    link: function(s, e, a) {}
  }
});
