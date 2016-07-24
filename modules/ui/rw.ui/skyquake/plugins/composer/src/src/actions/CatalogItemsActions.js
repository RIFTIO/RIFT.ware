
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/11/15.
 */
import alt from '../alt';

/*
 This class manages Catalog Data State
 */

class CatalogItemsActions {

	constructor() {
		this.generateActions('catalogItemMetaDataChanged', 'catalogItemDescriptorChanged', 'createCatalogItem', 'editCatalogItem', 'duplicateSelectedCatalogItem', 'selectCatalogItem', 'deleteSelectedCatalogItem', 'cancelCatalogItemChanges', 'saveCatalogItem', 'exportSelectedCatalogItems');
	}

}

export default alt.createActions(CatalogItemsActions);
