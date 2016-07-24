
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/27/15.
 */

import guid from '../libraries/guid'
import DropZone from 'dropzone'
import Utils from '../libraries/utils'
import CatalogPackageManagerActions from '../actions/CatalogPackageManagerActions'

/**
 * This class is responsible for wiring the DropZone.js to our React actions.
 */

const ACTIONS = {
	onboard: 'onboard',
	update: 'update'
};

function getCatalogPackageManagerServerOrigin() {
	return Utils.getSearchParams(window.location).upload_server + ':4567';
}

function initializeDropZone(element = '#dropzone', button = false, action = ACTIONS.onboard) {
	DropZone.autoDiscover = false;
	return new DropZone(element, {
		paramName: 'descriptor',
		url() {
			if (action === ACTIONS.update) {
				return getCatalogPackageManagerServerOrigin() + '/api/update';
			}
			return getCatalogPackageManagerServerOrigin() + '/api/upload';
		},
		maxFilesize: 10000000000,
		clickable: button,
		acceptedFiles: 'application/octet-stream,.gz,.tar.gz,.tar,.qcow,.qcow2,.iso,application/yaml,.yaml,application/json,.json,application/zip,.zip,application/x-rar-compressed,.rar,application/x-7z-compressed,.7z,application/x-bzip,.bz,application/x-bzip2,.bz2,application/x-gtar,.gtar',
		autoProcessQueue: true,
		previewTemplate: '',
		sending(file, xhr, formData) {
			// NOTE ie11 does not get this form data
			formData.append('id', file.id);
		},
		error(file, errorMessage) {
			const response = {
				state: file,
				data: {
					status: 'upload-error',
					message: errorMessage
				}
			};
			CatalogPackageManagerActions.uploadCatalogPackageError(response);
		},
		success(file) {
			const data = JSON.parse(file.xhr.responseText);
			data.status = 'upload-success';
			const response = {
				state: file,
				data: data
			};
			CatalogPackageManagerActions.uploadCatalogPackageStatusUpdated(response);
		},
		addedfile(file) {
			file.id = file.id || guid();
			file.riftAction = action;
			CatalogPackageManagerActions.uploadCatalogPackage(file);
		},
		thumbnail(file, dataUrl) {
			const response = {
				state: file,
				data: {
					status: 'upload-thumbnail',
					dataUrl: dataUrl
				}
			};
			CatalogPackageManagerActions.uploadCatalogPackageStatusUpdated(response);
		},
		uploadprogress(file, progress, bytesSent) {
			const response = {
				state: file,
				data: {
					status: 'upload-progress',
					progress: progress,
					bytesSent: bytesSent
				}
			};
			CatalogPackageManagerActions.uploadCatalogPackageStatusUpdated(response);
		}
	});
}

export default class CatalogPackageManagerUploadDropZone {

	constructor(element, button, action) {
		this.dropZone = initializeDropZone(element, button, action);
	}

	static get ACTIONS() {
		return ACTIONS;
	}

	on(eventName, eventCallback) {
		this.dropZone.on(eventName, eventCallback);
	}

}
