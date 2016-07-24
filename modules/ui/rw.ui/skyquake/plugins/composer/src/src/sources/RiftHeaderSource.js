
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import $ from 'jquery'
import alt from '../alt'
import utils from '../libraries/utils'
import RiftHeaderActions from '../actions/RiftHeaderActions'

function getApiServerOrigin() {
	return utils.getSearchParams(window.location).api_server + ':3000';
}
function ajaxRequest(path, catalogPackage, resolve, reject, method = 'GET') {
	$.ajax({
		url: getApiServerOrigin() + path,
		type: method,
		beforeSend: utils.addAuthorizationStub,
		dataType: 'json',
		success: function(data) {
			if (typeof data == 'string') {
				data = JSON.parse(data);
			}
			resolve({
				data: data,
				state: catalogPackage
			});
		},
		error: function(error) {
			if (typeof error == 'string') {
				error = JSON.parse(error);
			}
			reject({
				data: error,
				state: catalogPackage
			});
		}
	});
}

const RiftHeaderSource = {

	requestLaunchpadConfig: function () {
		return {
			remote: function (state, download) {
				return new Promise((resolve, reject) => {
					const path = '/launchpad/config?api_server=' + utils.getSearchParams(window.location).api_server;
					ajaxRequest(path, download, resolve, reject);
				});
			},
			success: RiftHeaderActions.requestLaunchpadConfigSuccess,
			error: RiftHeaderActions.requestLaunchpadConfigError
		};
	},
};

export default RiftHeaderSource;
