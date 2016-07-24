
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/14/15.
 */
import alt from '../alt';

class ModalOverlayActions {

	constructor() {
		this.generateActions('showModalOverlay', 'hideModalOverlay');
	}

}

export default alt.createActions(ModalOverlayActions);
