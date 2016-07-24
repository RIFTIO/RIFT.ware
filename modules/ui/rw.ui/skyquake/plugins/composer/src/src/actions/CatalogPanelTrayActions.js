
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/11/15.
 */
import alt from '../alt';

class CatalogPanelTrayActions {

	constructor() {
		this.generateActions('open', 'close', 'toggleOpenClose');
	}

}

export default alt.createActions(CatalogPanelTrayActions);
