
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

const DropTarget = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return {isDragHover: false};
	},
	getDefaultProps() {
		return {onDrop: () => {}, dropZone: null, className: 'DropTarget'};
	},
	componentWillMount() {
	},
	componentDidMount() {
		if (this.props.dropZone) {
			const dropTarget = this;
			const dropZone = this.props.dropZone(dropTarget);
			dropZone.on('dragover', this.onDragOver);
			dropZone.on('dragleave', this.onDragLeave);
			dropZone.on('dragend', this.onDragEnd);
			dropZone.on('drop', this.onDrop);
		}
	},
	componentDidUpdate() {
	},
	componentWillUnmount() {
	},
	render() {
		const classNames = ClassNames(this.props.className, 'dnd-target', {'-drag-hover': this.state.isDragHover});
		return (
				<div className={classNames}
					 onDrop={this.onDrop}
					 onDragEnd={this.onDragEnd}
					 onMouseOut={this.onDragEnd}
					 onDragExit={this.onDragEnd}
					 onDragOver={this.onDragOver}
					 onDragLeave={this.onDragLeave}>{this.props.children}</div>
		);
	},
	onDragOver(e) {
		// NOTE calling preventDefault makes this a valid drop target
		e.preventDefault();
		e.dataTransfer.dropEffect = 'copy';
		this.setState({isDragHover: true});
	},
	onDragLeave() {
		this.setState({isDragHover: false});
	},
	onDragEnd() {
		this.setState({isDragHover: false});
	},
	onDrop(e) {
		this.setState({isDragHover: false});
		this.props.onDrop(e);
	}
});

export default DropTarget;
