/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from './skyquakeAltInstance.js';
import $ from 'jquery';
import SkyquakeContainerActions from './skyquakeContainerActions'

let Utils = require('utils/utils.js');
let API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
let HOST = API_SERVER;
let NODE_PORT = require('utils/rw.js').getSearchParams(window.location).api_port || 3000;
let DEV_MODE = require('utils/rw.js').getSearchParams(window.location).dev_mode || false;
let RW_REST_API_PORT = require('utils/rw.js').getSearchParams(window.location).rw_rest_api_port || 8008;

if (DEV_MODE) {
    HOST = window.location.protocol + '//' + window.location.hostname;
}

export default {
    getNav() {
        return {
            remote: function() {
                return new Promise(function(resolve, reject) {
                    $.ajax({
                        url: '/nav',
                        type: 'GET',
                        // beforeSend: Utils.addAuthorizationStub,
                        success: function(data) {
                            resolve(data);
                        }
                    })
                })
            },
            success: SkyquakeContainerActions.getSkyquakeNavSuccess
        }
    },

    getEventStreams() {
        return {
            remote: function(state, recordID) {
                return new Promise(function(resolve, reject) {
                    $.ajax({
                        url: '//' + window.location.hostname + ':' + NODE_PORT + '/api/operational/restconf-state/streams?api_server=' + API_SERVER,
                        type: 'GET',
                        beforeSend: Utils.addAuthorizationStub,
                        success: function(data) {
                            resolve(data);
                        }
                    }).fail(function(xhr) {
                        //Authentication and the handling of fail states should be wrapped up into a connection class.
                        Utils.checkAuthentication(xhr.status);
                    });;
                });
            },
            loading: SkyquakeContainerActions.getEventStreamsLoading,
            success: SkyquakeContainerActions.getEventStreamsSuccess,
            error: SkyquakeContainerActions.getEventStreamsError
        }
    },

    openNotificationsSocket() {
        return {
            remote: function(state, location, streamSource) {
                return new Promise((resolve, reject) => {
                    $.ajax({
                        url: '//' + window.location.hostname + ':' + NODE_PORT + '/socket-polling?api_server=' + API_SERVER,
                        type: 'POST',
                        beforeSend: Utils.addAuthorizationStub,
                        data: {
                            url: location
                        },
                        success: (data) => {
                            var url = Utils.webSocketProtocol() + '//' + window.location.hostname + ':' + data.port + data.socketPath;
                            var ws = new WebSocket(url);
                            resolve({
                                ws: ws,
                                streamSource: streamSource
                            });
                        }
                    });
                });
            },
            loading: SkyquakeContainerActions.openNotificationsSocketLoading,
            success: SkyquakeContainerActions.openNotificationsSocketSuccess,
            error: SkyquakeContainerActions.openNotificationsSocketError
        }
    }
}

