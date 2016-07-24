
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/14/15.
 */
'use strict';

import React from 'react'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import ClassNames from 'classnames'

import ModalOverlayStore from '../stores/ModalOverlayStore'

import '../styles/ModalOverlay.scss'

const ModalOverlay = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return ModalOverlayStore.getState();
	},
	getDefaultProps() {
	},
	componentWillMount() {
		ModalOverlayStore.listen(this.onChange);
	},
	componentDidMount() {
		window.addEventListener('resize', this.onResizeCapture, true);
		window.addEventListener('mousemove', this.onMouseMoveCapture, true);
	},
	componentDidUpdate() {
	},
	componentWillUnmount() {
		ModalOverlayStore.unlisten(this.onChange);
		window.removeEventListener('resize', this.onResize, true);
		window.removeEventListener('mousemove', this.onMouseMoveCapture, true);
	},
	onChange(state) {
		this.setState(state);
	},
	onResizeCapture(event) {
		if (event.detail && event.detail.side) {
			if (this.state.visible) {
				event.preventDefault();
				event.stopPropagation();
			}
		}
	},
	onMouseMoveCapture(event) {
		if (this.state.visible) {
			event.preventDefault();
			event.stopPropagation();
		}
	},
	render() {
		const className = ClassNames('ModalOverlay', {'-is-visible': this.state.visible});
		return (
			<div className={className}>
				<div className="background" onMouseMoveCapture={this.onMouseMoveCapture}></div>
				{this.state.ui ? <div className="foreground" onMouseMoveCapture={this.onMouseMoveCapture}><div className="ui">{this.state.ui}</div></div> : null}
			</div>
		);
	}
});

export default ModalOverlay;
