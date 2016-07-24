
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 12/2/15.
 */
'use strict';

import React from 'react'
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import CatalogFilterActions from '../actions/CatalogFilterActions'

import '../styles/CatalogFilter.scss'

const CatalogFilter = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState: function () {
		return {};
	},
	getDefaultProps: function () {
		return {filterByType: 'nsd'};
	},
	componentWillMount: function () {
	},
	componentDidMount: function () {
	},
	componentDidUpdate: function () {
	},
	componentWillUnmount: function () {
	},
	render() {
		const clickFilterByType = function (event) {
			CatalogFilterActions.filterByType(event.target.value);
		};
		return (
			<div className="CatalogFilter">
				<button className={ClassNames('CatalogFilterNSD', {'-selected': this.props.filterByType === 'nsd'})}
						value="nsd" onClick={clickFilterByType}>NSD
				</button>
				<button className={ClassNames('CatalogFilterVNFD', {'-selected': this.props.filterByType === 'vnfd'})}
						value="vnfd" onClick={clickFilterByType}>VNFD
				</button>
			</div>
		);
	}
});

export default CatalogFilter;
