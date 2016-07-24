
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import _ from 'lodash'
import alt from '../alt'
import guid from '../libraries/guid'
import numeral from 'numeral'
import moment from 'moment'
import utils from '../libraries/utils'
import CatalogPackageManagerSource from '../sources/CatalogPackageManagerSource'
import CatalogPackageManagerActions from '../actions/CatalogPackageManagerActions'
import CatalogDataSource from '../sources/CatalogDataSource'

import imgDownload from '../../../node_modules/open-iconic/svg/cloud-download.svg'
import imgOnboard from '../../../node_modules/open-iconic/svg/cloud-upload.svg'
import imgUpdate from '../../../node_modules/open-iconic/svg/data-transfer-upload.svg'

const defaults = {
	downloadPackage: {
		id: '',
		name: '',
		icon: imgDownload,
		catalogItems: [],
		transactionId: '',
		progress: 0,
		message: 'Requesting catalog package export...',
		pending: false,
		success: false,
		error: false,
		url: '',
		urlValidUntil: ''
	},
	checkStatusDelayInSeconds: 2,
	downloadUrlTimeToLiveInMinutes: 5
};

const exception = function ignoreException() {};

const packagePropertyNames = Object.keys(defaults.downloadPackage);

function getCatalogPackageManagerServerOrigin() {
	return utils.getSearchParams(window.location).upload_server + ':4567';
}

function delayStatusCheck(statusCheckFunction, catalogPackage) {
	if (!catalogPackage.checkStatusTimeoutId) {
		const delayCallback = function () {
			delete catalogPackage.checkStatusTimeoutId;
			statusCheckFunction(catalogPackage).catch(exception);
		};
		catalogPackage.checkStatusTimeoutId = _.delay(delayCallback, defaults.checkStatusDelayInSeconds * 1000);
	}
}

class CatalogPackageManagerStore {

	constructor() {

		this.packages = [];

		this.registerAsync(CatalogDataSource);
		this.registerAsync(CatalogPackageManagerSource);
		this.bindAction(CatalogPackageManagerActions.REMOVE_CATALOG_PACKAGE, this.removeCatalogPackage);
		this.bindAction(CatalogPackageManagerActions.DOWNLOAD_CATALOG_PACKAGE, this.downloadCatalogPackage);
		this.bindAction(CatalogPackageManagerActions.DOWNLOAD_CATALOG_PACKAGE_STATUS_UPDATED, this.onDownloadCatalogPackageStatusUpdated);
		this.bindAction(CatalogPackageManagerActions.DOWNLOAD_CATALOG_PACKAGE_ERROR, this.onDownloadCatalogPackageError);
		this.bindAction(CatalogPackageManagerActions.UPLOAD_CATALOG_PACKAGE, this.uploadCatalogPackage);
		this.bindAction(CatalogPackageManagerActions.UPLOAD_CATALOG_PACKAGE_STATUS_UPDATED, this.onUploadCatalogPackageStatusUpdated);
		this.bindAction(CatalogPackageManagerActions.UPLOAD_CATALOG_PACKAGE_ERROR, this.onUploadCatalogPackageError);

	}

	addPackage(catalogPackage) {
		const packages = [catalogPackage].concat(this.packages);
		this.setState({packages: packages});
	}

	updatePackage(catalogPackage) {
		const packages = this.packages.map(d => {
			if (d.id === catalogPackage.id) {
				return Object.assign({}, d, catalogPackage);
			}
			return d;
		});
		this.setState({packages: packages});
	}

	removeCatalogPackage(catalogPackage) {
		const packages = this.packages.filter(d => d.id !== catalogPackage.id);
		this.setState({packages: packages});
	}

	uploadCatalogPackage(file) {
		file.id = file.id || guid();
		const catalogPackage = _.pick(file, packagePropertyNames);
		catalogPackage.icon = file.riftAction === 'onboard' ? imgOnboard : imgUpdate;
		catalogPackage.type = 'upload';
		this.addPackage(catalogPackage);
		// note DropZone.js handles the async upload so we don't have to invoke any async action creators
	}

	onUploadCatalogPackageStatusUpdated(response) {
		const upload = updateStatusInfo(response);
		this.updatePackage(upload);
		// if pending with no transaction id - do nothing
		// bc DropZone.js will notify upload progress
		if (upload.pending && upload.transactionId) {
			delayStatusCheck(this.getInstance().requestCatalogPackageUploadStatus, upload);
		} else if (upload.success) {
			this.getInstance().loadCatalogs();
		}
	}

	onUploadCatalogPackageError(response) {
		console.warn('onUploadCatalogPackageError', response);
		const catalogPackage = updateStatusInfo(response);
		this.updatePackage(catalogPackage);
	}

	downloadCatalogPackage(data) {
		let catalogItems = data['selectedItems'] || [];
		let format = data['selectedFormat'] || 'mano';
		if (catalogItems.length) {
			const catalogPackage = Object.assign({}, defaults.downloadPackage, {id: guid()});
			catalogPackage.name = catalogItems[0].name;
			catalogPackage.type = 'download';
			if (catalogItems.length > 1) {
				catalogPackage.name += ' (' + catalogItems.length + ' items)';
			}
			catalogPackage.ids = catalogItems.map(d => d.id).sort().toString();
			catalogPackage.catalogItems = catalogItems;
			this.addPackage(catalogPackage);
			this.getInstance().requestCatalogPackageDownload(catalogPackage, format).catch(exception);
		}
	}

	onDownloadCatalogPackageStatusUpdated(response) {
		const download = updateStatusInfo(response);
		this.updatePackage(download);
		if (download.pending) {
			delayStatusCheck(this.getInstance().requestCatalogPackageDownloadStatus, download);
		}
	}

	onDownloadCatalogPackageError(response) {
		console.warn('onDownloadCatalogPackageError', response);
		const catalogPackage = updateStatusInfo(response);
		this.updatePackage(catalogPackage);
	}

}

function calculateUploadProgressMessage(size = 0, progress = 0, bytesSent = 0) {
	const amount = parseFloat(progress) || 0;
	const loaded = amount === 100 ? size : size * amount / 100;
	let progressText;
	if (amount === 100) {
		progressText = numeral(loaded).format('0.0b') + ' loaded ';
	} else if (typeof amount === 'number' && amount != 0) {
		progressText = numeral(bytesSent).format('0.0b') + ' out of ' + numeral(size).format('0.0b');
	} else {
		progressText = progress;
	}
	return progressText;
}

function updateStatusInfo(response) {
	// returns the catalogPackage object with the status fields updated based on the server response
	const statusInfo = {
		pending: false,
		success: false,
		error: false
	};
	const responseData = response.data;
	const catalogPackage = response.state;
	switch(responseData.status) {
	case 'upload-progress':
		statusInfo.pending = true;
		statusInfo.progress = parseFloat(responseData.progress) || 0;
		statusInfo.message = calculateUploadProgressMessage(catalogPackage.size, responseData.progress, responseData.bytesSent);
		break;
	case 'upload-success':
		statusInfo.pending = true;
		statusInfo.progress = 100;
		statusInfo.message = 'Upload completed.';
		statusInfo.transactionId = responseData.transaction_id;
		break;
	case 'upload-error':
		statusInfo.error = true;
		statusInfo.message = responseData.message;
		break;
	case 'download-requested':
		statusInfo.pending = true;
		statusInfo.progress = 25;
		statusInfo.transactionId = responseData.transaction_id;
		break;
	case 'pending':
		statusInfo.pending = true;
		statusInfo.progress = 50;
		statusInfo.message = responseData.events[responseData.events.length - 1].text;
		break;
	case 'success':
		statusInfo.success = true;
		statusInfo.progress = 100;
		statusInfo.message = responseData.events[responseData.events.length - 1].text;
		if (catalogPackage.type === 'download') {
			statusInfo.urlValidUntil = moment().add(defaults.downloadUrlTimeToLiveInMinutes, 'minutes').toISOString();
			statusInfo.url = getCatalogPackageManagerServerOrigin() + '/api/export/' + catalogPackage.transactionId + '.tar.gz';
		}
		break;
	case 'failure':
		statusInfo.error = true;
		statusInfo.message = responseData.errors[0].value;
		break;
	default:
		throw new ReferenceError('a status of "request", "success", "failure", "pending", "upload-completed", "upload-error", "download-requested", "upload-progress", "upload-action" is required');
	}
	return Object.assign({}, catalogPackage, statusInfo);
}

export default alt.createStore(CatalogPackageManagerStore, 'CatalogPackageManagerStore');
