/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from './skyquakeAltInstance.js';
export default Alt.generateActions(
	'getSkyquakeNavSuccess',
	'openNotificationsSocketLoading',
	'openNotificationsSocketSuccess',
	'openNotificationsSocketError',
	'getEventStreamsLoading',
	'getEventStreamsSuccess',
	'getEventStreamsError',
    //Notifications
    'showNotification',
    'hideNotification',
    //Screen Loader
    'showScreenLoader',
    'hideScreenLoader'
);
