
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 12/1/15.
 */
'use strict';

import guid from '../libraries/guid'
import React from 'react'
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import SelectionManager from '../libraries/SelectionManager'

import '../styles/Button.scss'

const Button = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState: function () {
		return {};
	},
	getDefaultProps: function () {
		return {
			className: '',
			label: null,
			title: null,
			src: null,
			onClick: () => {}
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
		const src = this.props.src;
		const label = this.props.label;
		const title = this.props.title;
		const draggable = this.props.draggable;
		const className = ClassNames(this.props.className, 'Button');
		return (
			<div className={className} onClick={this.props.onClick} title={title} draggable={draggable} onDragStart={this.props.onDragStart}>
				<img src={src} />
				<span>{label}</span>
			</div>
		);
	}
});

export default Button;
