
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by kkashalk on 11/10/15.
 */

'use strict';

import utils from '../libraries/utils'
import React from 'react'
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import RiftHeaderActions from '../actions/RiftHeaderActions'
import RiftHeaderStore from '../stores/RiftHeaderStore'

import '../styles/RiftHeader.scss'

const uiTransientState = {
	timeoutId: 0
};

const RiftHeader = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return RiftHeaderStore.getState();
	},
	getDefaultProps() {
	},
	componentWillMount() {
	},
	componentDidMount() {
		RiftHeaderStore.listen(this.onChange);
		const loadCatalogs = function () {
			RiftHeaderStore.loadCatalogs();
			uiTransientState.timeoutId = setTimeout(loadCatalogs, 2000);
		};
		RiftHeaderStore.requestLaunchpadConfig();
		loadCatalogs();
	},
	componentDidUpdate() {
	},
	componentWillUnmount() {
		if (uiTransientState.timeoutId) {
			clearTimeout(uiTransientState.timeoutId);
		}
		RiftHeaderStore.unlisten(this.onChange);
	},

	onChange(state) {
		this.setState(state);
	},
	onClickOpenDashboard() {
		if (uiTransientState.timeoutId) {
			clearTimeout(uiTransientState.timeoutId);
		}
		RiftHeaderStore.unlisten(this.onChange);
		window.location.href = '//' + window.location.hostname + ':8000/index.html?api_server=' + utils.getSearchParams(window.location).api_server + '#/launchpad/' + utils.getSearchParams(window.location).mgmt_domain_name;
	},
	onClickOpenAccounts() {
		if (uiTransientState.timeoutId) {
			clearTimeout(uiTransientState.timeoutId);
		}
		RiftHeaderStore.unlisten(this.onChange);
		window.location.href = '//' + window.location.hostname + ':8000/index.html?api_server=' + utils.getSearchParams(window.location).api_server + '#/launchpad/' + utils.getSearchParams(window.location).mgmt_domain_name + '/cloud-account/dashboard';
	},
	render() {
		let Header = (
	              		<header className="header-app">
							<h1>{this.state.headerTitle}</h1>
							<nav className="header-nav"> </nav>
						</header>
					);
		Header = null;
		return (
			null
		);
	}
});

export default RiftHeader;
