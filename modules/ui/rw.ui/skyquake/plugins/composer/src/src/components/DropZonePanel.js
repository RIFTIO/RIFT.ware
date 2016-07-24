
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/6/15.
 */
'use strict';

import React from 'react'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import ClassNames from 'classnames'

import '../styles/DropZonePanel.scss'

const DropZonePanel = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return {};
	},
	getDefaultProps() {
		return {show: false, className: 'DropZonePanel', title: 'Drop files to upload.'};
	},
	componentWillMount() {
	},
	componentDidMount() {
	},
	componentDidUpdate() {
	},
	componentWillUnmount() {
	},
	render() {
		const classNames = ClassNames(this.props.className, {'-close': !this.props.show});
		return (
			<div className={classNames}>
				<div className="dz-default dz-message">
					<span>{this.props.title}</span>
				</div>
				{this.props.children}
			</div>
		);
	}
});

export default DropZonePanel;
