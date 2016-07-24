
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/11/15.
 */
import alt from '../alt';

class CatalogDataSourceActions {

	constructor() {
		this.generateActions('loadCatalogsSuccess', 'loadCatalogsError', 'deleteCatalogItemSuccess', 'deleteCatalogItemError', 'saveCatalogItemSuccess', 'saveCatalogItemError');
	}

}

export default alt.createActions(CatalogDataSourceActions);
