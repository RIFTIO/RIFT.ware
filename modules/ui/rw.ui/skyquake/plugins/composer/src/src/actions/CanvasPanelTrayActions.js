/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 2/4/16.
 */
import alt from '../alt';

class CanvasPanelTrayActions {

	constructor() {
		this.generateActions('open', 'close', 'toggleOpenClose');
	}

}

export default alt.createActions(CanvasPanelTrayActions);
