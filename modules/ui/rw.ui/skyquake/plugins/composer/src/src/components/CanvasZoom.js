
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import React from 'react'
import Range from './Range'
import numeral from 'numeral'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import CanvasEditorActions from '../actions/CanvasEditorActions'
import SelectionManager from '../libraries/SelectionManager'

import '../styles/CanvasZoom.scss'

const CanvasZoom = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState: function () {
		return {};
	},
	getDefaultProps: function () {
		return {
			min: 25,
			max: 200,
			zoom: 100,
			defaultZoom: 100
		};
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
		const zoom = this.props.zoom || this.props.defaultZoom
		const displayValue = numeral(zoom).format('0.0') + '%';
		return (
			<div ref="canvasZoom" className="CanvasZoom" style={this.props.style} onClick={event => event.preventDefault()}>
				<Range
					   value={zoom} min={this.props.min} max={this.props.max}
					   title="Zoom the canvas. Double Click to reset to 100%."
					   onChange={this.onChange} onDoubleClick={this.onDblClick} />
				<span>{displayValue}</span>
			</div>
		);
	},
	onChange(event) {
		const zoom = event.target.value;
		CanvasEditorActions.setCanvasZoom(zoom);
	},
	onDblClick() {
		const zoom = this.props.defaultZoom;
		CanvasEditorActions.setCanvasZoom(zoom);
	}
});

export default CanvasZoom;
