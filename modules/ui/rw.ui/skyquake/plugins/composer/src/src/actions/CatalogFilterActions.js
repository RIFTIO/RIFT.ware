
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/11/15.
 */
import alt from '../alt';

class CatalogFilterActions {

	constructor() {
		this.generateActions('filterByType');
	}

}

export default alt.createActions(CatalogFilterActions);
