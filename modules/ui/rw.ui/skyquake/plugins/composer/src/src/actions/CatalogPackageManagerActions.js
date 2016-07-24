
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import alt from '../alt';

class CatalogPackageManagerActions {

	constructor() {
		this.generateActions('downloadCatalogPackage', 'downloadCatalogPackageStatusUpdated', 'downloadCatalogPackageError', 'uploadCatalogPackage', 'uploadCatalogPackageStatusUpdated', 'uploadCatalogPackageError', 'removeCatalogPackage');
	}

}

export default alt.createActions(CatalogPackageManagerActions);
