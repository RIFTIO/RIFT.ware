/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by kkashalk on 11/10/15.
 */
'use strict';

import alt from '../alt'
import React from 'react'
import utils from '../libraries/utils'
import RiftHeaderActions from '../actions/RiftHeaderActions'
import RiftHeaderSource from '../sources/RiftHeaderSource'
import CatalogDataSource from '../sources/CatalogDataSource'
import CatalogDataSourceActions from '../actions/CatalogDataSourceActions'

class RiftHeaderStore {

	constructor() {
		let mgmt_domain_name = unescape(utils.getSearchParams(window.location).mgmt_domain_name);
		if(mgmt_domain_name.toUpperCase() == 'DASHBOARD' || mgmt_domain_name.toUpperCase() == 'UNDEFINED') {
			mgmt_domain_name = '';
		} else {
			mgmt_domain_name = ' : ' + mgmt_domain_name;
		}
		this.headerTitle = 'Launchpad' + mgmt_domain_name;
		this.nsdCount = 0;
		this.isStandAlone = false;
		this.registerAsync(CatalogDataSource);
		this.registerAsync(RiftHeaderSource);
		this.bindAction(CatalogDataSourceActions.LOAD_CATALOGS_SUCCESS, this.loadCatalogsSuccess);
		this.bindActions(RiftHeaderActions);
	}
	requestLaunchpadConfigSuccess(data) {
		if((data.data['operational-mode'] == 'STANDALONE') || (data['operational-mode'] == '')) {
			this.setState({
				isStandAlone: true,
				headerTitle: 'Launchpad'
			})
		}

	}
	loadCatalogsSuccess(data) {
		let self = this; {}
		let descriptorCount = 0;
		data.data.forEach((catalog) => {
			descriptorCount += catalog.descriptors.length;
		});

		self.setState({
			descriptorCount: descriptorCount
		});
	}
}

export default alt.createStore(RiftHeaderStore, 'RiftHeaderStore');
