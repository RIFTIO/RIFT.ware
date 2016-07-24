
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import React from 'react';
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import CatalogPanelTrayActions from '../actions/CatalogPanelTrayActions'

import '../styles/CatalogPanelTray.scss'

const CatalogPanelTray = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return {
			isDragging: false
		};
	},
	getDefaultProps() {
		return {show: false};
	},
	preventResizeCursor(e) {
		e.stopPropagation();
		document.body.style.cursor = '';
	},
	componentWillMount() {
	},
	componentDidMount() {
	},
	componentDidUpdate() {
	},
	componentWillUnmount() {
	},
	onDragOver() {
		this.setState({isDragging: true});
	},
	onDragLeave() {
		this.setState({isDragging: false});
	},
	render() {
		const classNames = ClassNames('CatalogPanelTray', {
			'-close': !this.props.show,
			'-is-dragging': this.state.isDragging
		});
		return (
			<div className={classNames} onMouseMove={this.preventResizeCursor} onDragOver={this.onDragOver} onDragLeave={this.onDragLeave}>
				<h1 data-open-close-icon={this.props.show ? 'open' : 'closed'} onClick={CatalogPanelTrayActions.toggleOpenClose}>Catalog Package Manager</h1>
				<div className="tray-body">
					{this.props.children}
				</div>
			</div>
		);
	}
});

export default CatalogPanelTray;
