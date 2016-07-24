
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import $ from 'jquery'
import alt from '../alt'
import utils from '../libraries/utils'
import CatalogPackageManagerActions from '../actions/CatalogPackageManagerActions'
let Utils = require('utils/utils.js');
function getApiServerOrigin() {
	return utils.getSearchParams(window.location).upload_server + ':4567';
}

function ajaxRequest(path, catalogPackage, resolve, reject, method = 'GET') {
	$.ajax({
		url: getApiServerOrigin() + path,
		type: method,
		beforeSend: Utils.addAuthorizationStub,
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
	}).fail(function(xhr){
			            //Authentication and the handling of fail states should be wrapped up into a connection class.
			            Utils.checkAuthentication(xhr.status);
		          	});
}

const CatalogPackageManagerSource = {

	requestCatalogPackageDownload: function () {
		return {
			remote: function (state, download, format) {
				return new Promise((resolve, reject) => {
					// the server does not add a status in the payload
					// so we add one so that the success handler will
					// be able to follow the flow of this download
					const setStatusBeforeResolve = (response = {}) => {
						response.data.status = 'download-requested';
						resolve(response);
					};
					// RIFT-13485 requires to send type (nsd/vnfd) as a path element.
					// Backend no longer supports mixed multi-package download.
					// Probably does not even support multi-package download of same type.
					// Hence, pick the type from the first element.
					const path = '/api/export/' + download['catalogItems'][0]['uiState']['type'] + '?schema=' + format + '&ids=' + download.ids;
					ajaxRequest(path, download, setStatusBeforeResolve, reject);
				});
			},
			success: CatalogPackageManagerActions.downloadCatalogPackageStatusUpdated,
			error: CatalogPackageManagerActions.downloadCatalogPackageError
		};
	},

	requestCatalogPackageDownloadStatus: function() {
		return {
			remote: function(state, download) {
				const transactionId = download.transactionId;
				return new Promise(function(resolve, reject) {
					const path = '/api/export/' + transactionId + '/state';
					ajaxRequest(path, download, resolve, reject);
				});
			},
			success: CatalogPackageManagerActions.downloadCatalogPackageStatusUpdated,
			error: CatalogPackageManagerActions.downloadCatalogPackageError
		}
	},

	requestCatalogPackageUploadStatus: function () {
		return {
			remote: function (state, upload) {
				const transactionId = upload.transactionId;
				return new Promise(function (resolve, reject) {
					const action = upload.riftAction === 'onboard' ? 'upload' : 'update';
					const path = '/api/' + action + '/' + transactionId + '/state';
					ajaxRequest(path, upload, resolve, reject);
				});
			},
			success: CatalogPackageManagerActions.uploadCatalogPackageStatusUpdated,
			error: CatalogPackageManagerActions.uploadCatalogPackageError
		};
	}

};

export default CatalogPackageManagerSource;
