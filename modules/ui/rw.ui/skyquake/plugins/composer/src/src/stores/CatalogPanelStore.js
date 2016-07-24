
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import alt from '../alt'
import CatalogPanelTrayActions from '../actions/CatalogPanelTrayActions'

class CatalogPanelStore {

	constructor() {
		this.isTrayOpen = 0;
		this.bindAction(CatalogPanelTrayActions.OPEN, this.openTray);
		this.bindAction(CatalogPanelTrayActions.CLOSE, this.closeTray);
		this.bindAction(CatalogPanelTrayActions.TOGGLE_OPEN_CLOSE, this.toggleTrayOpenClose);
	}

	openTray() {
		// note incrementing integer will force a state change needed to redraw tray drop zones
		this.setState({isTrayOpen: this.isTrayOpen + 1});
	}

	closeTray() {
		this.setState({isTrayOpen: 0});
	}

	toggleTrayOpenClose() {
		this.setState({isTrayOpen: this.isTrayOpen === 0 ? 1 : 0});
	}

}

export default alt.createStore(CatalogPanelStore, 'CatalogPanelStore');
