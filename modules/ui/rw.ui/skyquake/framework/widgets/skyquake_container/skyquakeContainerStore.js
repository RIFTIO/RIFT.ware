/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
//This will reach out to the global routes endpoint

import Alt from './skyquakeAltInstance.js';
import SkyquakeContainerSource from './skyquakeContainerSource.js';
import SkyquakeContainerActions from './skyquakeContainerActions';
import loggingActions from '../logging/loggingActions.js';
import LoggingSource from '../logging/loggingSource.js';
import _ from 'lodash';
//Temporary, until api server is on same port as webserver
var rw = require('utils/rw.js');
var API_SERVER = rw.getSearchParams(window.location).api_server;
var UPLOAD_SERVER = rw.getSearchParams(window.location).upload_server;

class SkyquakeContainerStore {
    constructor() {
        this.currentPlugin = getCurrentPlugin();
        this.nav = {};
        this.notifications = [];
        this.socket = null;
        //Notification defaults
        this.notificationMessage = '';
        this.displayNotification = false;
        //Screen Loader default
        this.displayScreenLoader = false;
        //Remove logging once new log plugin is implemented
        this.exportAsync(LoggingSource);
        this.bindActions(loggingActions);

        this.bindActions(SkyquakeContainerActions);
        this.exportAsync(SkyquakeContainerSource);


        this.exportPublicMethods({
            // getNav: this.getNav
        });

    }
    getSkyquakeNavSuccess = (data) => {
        var self = this;
        this.setState({
            nav: decorateAndTransformNav(data, self.currentPlugin)
        })
    }

    closeSocket = () => {
        if (this.socket) {
            this.socket.close();
        }
        this.setState({
            socket: null
        });
    }
    //Remove once logging plugin is implemented
    getSysLogViewerURLSuccess(data){
        window.open(data.url);
    }
    getSysLogViewerURLError(data){
        console.log('failed', data)
    }

    openNotificationsSocketLoading = () => {
        this.setState({
            isLoading: true
        })
    }

    openNotificationsSocketSuccess = (data) => {
        let connection = data.ws;
        let streamSource = data.streamSource;
        console.log('Success opening notification socket for stream ', streamSource);
        var self = this;
        if (!connection) return;
        self.setState({
            socket: connection,
            isLoading: false
        });

        connection.onmessage = (socket) => {
            try {
                var data = JSON.parse(socket.data);
                if (!data.notification) {
                    console.warn('No notification in the received payload: ', data);
                } else {
                    // Temp to test before adding multi-sources
                    data.notification.source = streamSource;
                    if (_.indexOf(self.notifications, data.notification) == -1) {
                        // newly appreared event.
                        // Add to the notifications list and setState
                        self.notifications.unshift(data.notification);
                        self.setState({
                            newNotificationEvent: true,
                            newNotificationMsg: data.notification,
                            notifications: self.notifications,
                            isLoading: false
                        });
                    }
                }
            } catch(e) {
                console.log('Error in parsing data on notification socket');
            }
        };

        connection.onclose = () => {
            self.closeSocket();
        };
    }

    openNotificationsSocketError = (data) => {
        console.log('Error opening notification socket', data);
    }

    getEventStreamsLoading = () => {
        this.setState({
            isLoading: true
        });
    }

    getEventStreamsSuccess = (streams) => {
        console.log('Found streams: ', streams);
        let self = this;

        streams['ietf-restconf-monitoring:streams'] &&
        streams['ietf-restconf-monitoring:streams']['stream'] &&
        streams['ietf-restconf-monitoring:streams']['stream'].map((stream) => {
            stream['access'] && stream['access'].map((streamLocation) => {
                if (streamLocation['encoding'] == 'ws_json') {
                    setTimeout(() => {
                        self.getInstance().openNotificationsSocket(streamLocation['location'], stream['name']);
                    }, 0);
                }
            })
        })

        this.setState({
            isLoading: true,
            streams: streams
        })
    }

    getEventStreamsError = (error) => {
        console.log('Failed to get streams object');
        this.setState({
            isLoading: false
        })
    }

    //Notifications
    showNotification = (msg) => {
        this.setState({
            displayNotification: true,
            notificationMessage: msg
        });
    }
    hideNotification = () => {
        this.setState({
            displayNotification: false
        })
    }
    //ScreenLoader
    showScreenLoader = () => {
        this.setState({
            displayScreenLoader: true
        });
    }
    hideScreenLoader = () => {
        this.setState({
            displayScreenLoader: false
        })
    }

}

/**
 * Receives nav data from routes rest endpoint and decorates the data with internal/external linking information
 * @param  {object} nav           Nav item from /nav endoingpoint
 * @param  {string} currentPlugin Current plugin name taken from url path.
 * @return {array}               Returns list of constructed nav items.
 */
function decorateAndTransformNav(nav, currentPlugin) {
    for ( let k in nav) {
        nav[k].pluginName = k;
            if (k != currentPlugin)  {
                nav[k].routes.map(function(route, i) {
                    if (API_SERVER) {
                        route.route = '/' + k + '/index.html?api_server=' + API_SERVER + '&upload_server=' + UPLOAD_SERVER + '#' + route.route;
                    } else {
                        route.route = '/' + k + '/#' + route.route;
                    }
                    route.isExternal = true;
                })
            }
        }
        return nav;
}

function getCurrentPlugin() {
    var paths = window.location.pathname.split('/');
    var currentPath = null;
    if (paths[0] != "") {
        currentPath = paths[0]
    } else {
        currentPath = paths[1];
    }
    if (currentPath != null) {
        return currentPath;
    } else {
        console.error('Well, something went horribly wrong with discovering the current plugin name - perhaps you should consider moving this logic to the server?')
    }
}

export default Alt.createStore(SkyquakeContainerStore);
