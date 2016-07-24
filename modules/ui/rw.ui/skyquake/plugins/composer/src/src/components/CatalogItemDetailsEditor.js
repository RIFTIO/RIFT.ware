/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import React from 'react'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import EditDescriptorModelProperties from './EditDescriptorModelProperties'

const CatalogItemDetailsEditor = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return {};
	},
	getDefaultProps() {
		return {
			container: null,
			width: 0
		};
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

		const container = this.props.container || {model: {}, uiState: {}};
		if (!(container && container.model && container.uiState)) {
			return null;
		}

		return (
			<div className="CatalogItemDetailsEditor">
				<form name="details-descriptor-editor-form">
					<div className="properties-group">
						<EditDescriptorModelProperties container={this.props.container} width={this.props.width} />
					</div>
				</form>
			</div>
		);

	}
});

export default CatalogItemDetailsEditor;
