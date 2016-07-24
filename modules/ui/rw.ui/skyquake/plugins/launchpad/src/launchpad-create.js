
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
require('../components/form-controls.js');
require('./launchpad-launch-fleet.html');
var createStore = require('./createStore.js');
var createActions = require('./createActions.js');


angular.module('launchpad')
    .controller('launchpadCreateCtrl', function($timeout, $stateParams, $state) {
        var self = this;
        self.createStore =
        // var createChannel = $rw.radio.channel('createFleet');
        var apiServer = self.isOnline = require('utils/rw.js').getSearchParams(window.location).api_server;
        // var federationChannel = $rw.radio.channel('federationChannel');
        self.fleet = {
            template_id: null,
            pool_id: null,
            description: '',
            epa_attributes: {},
            status: "active",
            name: 'NEW FLEET'
        };
        self.slaParams = [];
        self.federation = $stateParams.id;
        createStore.getNetworkServices();
        createStore.getSlaParams();
        createStore.getPools();
        createStore.listen(function(state) {
                $timeout(function() {
                    self.networkServices = state.networkServices;
                    self.slaParams = state.slaParams;
                    self.fleet.pool = state.pools[0];
                        self.pools = state.pools;
                    angular.forEach(self.slaParams, function(v) {
                        if (!v.hasOwnProperty('value')) {
                            v.value = v.options.second;
                        };
                        return v;
                    });
                })
            })
            // federationChannel.request("federation:services").then(function(data) {
            //     $timeout(function() {
            //         // self.fleet.service = 'cag';
            //         self.networkServices = data;
            //         createChannel.request('vnfParams', 'cag').then(function(data) {
            //         $timeout(function() {
            //             self.vnfParams = data;
            //         });
            //     });
            //     });
            // });
            //     federationChannel.request('federation:pools', apiServer).then(function(data) {
            //         $timeout(function() {
            //             console.log('pools:', data)
            //             self.fleet.pool = data[0];
            //             self.pools = data;
            //         })
            //     });

        //         federationChannel.request('federation:sla-params').then(function(data) {
        //             $timeout(function() {
        //                 self.slaParams = data;
        //                 angular.forEach(self.slaParams, function(v) {
        //                     if (!v.hasOwnProperty('value')) {
        //                         v.value = v.options.second;
        //                     };
        //                     return v;
        //                 });
        //             }
        //             );
        //         });

        // federationChannel.on("launchpadCreate", function() {
        //         $state.go('launchpad', null, {reload: false});

        // });

        self.generateServiceImage = function(service) {
            return ('assets/img/svg/' + service.src + (self.isSelectedService(service.id) ? '-active' : '-inactive') + '.svg');
        };
        self.generatePoolImage = function(pool) {
            return ('assets/img/svg/' + self.refsDB.resources.openstackCloud.pools[pool.ref].src + (self.isSelectedPool(pool) ? '-active' : '-inactive') + '.svg');
        };
        self.isSelectedPool = function(id) {
            return id == self.fleet.pool_id;
        };
        self.isSelectedService = function(id) {
            return id == self.fleet.template_id;
        };
        self.launch = function(launch) {
            if (self.fleet.name == "") {
                createActions.validateError('Plase Name the Service')
            }
            createActions.validateReset();
            self.slaParams.forEach(function(v) {
                if (v.value.indexOf("RRC") > -1) {
                    v.value = "RRC";
                }
                self.fleet.epa_attributes[v.ref] = v.value;
            });
            delete self.fleet.pool;
            self.fleet.status = launch ? 'active' : 'inactive';
            createStore.createEnvironment(self.fleet)
        };
        self.selectPool = function(id) {
            self.fleet.pool_id = id;
            // createChannel.command("pool:select", id);
        };
        self.selectService = function(id) {

            self.fleet.template_id = id;
            // createChannel.command("service:select", id);
        };
    });
