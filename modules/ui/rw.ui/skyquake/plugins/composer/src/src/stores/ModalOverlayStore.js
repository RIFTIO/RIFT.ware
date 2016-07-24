
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/14/15.
 */
'use strict';

import alt from '../alt'
import React from 'react'
import ModalOverlayActions from '../actions/ModalOverlayActions'
import LoadingIndicator from '../components/LoadingIndicator'

class ModalOverlayStore {

	constructor() {
		this.ui = <LoadingIndicator/>;
		this.visible = false;
		this.bindActions(ModalOverlayActions);
	}

	showModalOverlay(ui = <LoadingIndicator/>) {
		this.setState({visible: true, ui: ui});
	}

	hideModalOverlay() {
		this.setState({visible: false, ui: null});
	}

}

ModalOverlayStore.config = {
	onSerialize: function() {
		return {};
	}
};

export default alt.createStore(ModalOverlayStore, 'ModalOverlayStore');
