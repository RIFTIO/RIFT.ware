
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import _ from 'lodash'
import cc from 'change-case'
import React from 'react'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import utils from '../libraries/utils'
import messages from './messages'
import DescriptorModelFactory from '../libraries/model/DescriptorModelFactory'
import CatalogItemCanvasEditor from './CatalogItemCanvasEditor'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import CanvasEditorActions from '../actions/CanvasEditorActions'
import ComposerAppActions from '../actions/ComposerAppActions'
import CanvasZoom from './CanvasZoom'
import CanvasPanelTray from './CanvasPanelTray'
import EditForwardingGraphPaths from './EditorForwardingGraph/EditForwardingGraphPaths'
import SelectionManager from '../libraries/SelectionManager'
import DescriptorModelIconFactory from '../libraries/model/IconFactory'

import '../styles/CanvasPanel.scss'

const CanvasPanel = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return {};
	},
	getDefaultProps() {
		return {
			title: '',
			layout: {
				left: 300,
				right: 300
			},
			showMore: false,
			containers: []
		};
	},
	componentWillMount() {
	},
	componentDidMount() {
	},
	componentDidUpdate() {
		SelectionManager.refreshOutline();
	},
	componentWillUnmount() {
	},
	render() {
		var style = {
			left: this.props.layout.left
		};
		var req = require.context("../", true, /^\.\/.*\.svg$/);
		const hasItem = this.props.containers.length !== 0;
		const isEditingNSD = DescriptorModelFactory.isNetworkService(this.props.containers[0]);
		const hasNoCatalogs = this.props.hasNoCatalogs;
		const bodyComponent = hasItem ? <CatalogItemCanvasEditor zoom={this.props.zoom} isShowingMoreInfo={this.props.showMore} containers={this.props.containers}/> : messages.canvasWelcome();
		return (
			<div id="canvasPanelDiv" className="CanvasPanel" style={style} onDragOver={this.onDragOver} onDrop={this.onDrop}>
				<div onDoubleClick={this.onDblClickOpenFullScreen}  className="CanvasPanelHeader panel-header" data-resizable="limit_bottom">
					<h1>
						{hasItem ? <img src={req('./' + DescriptorModelIconFactory.getUrlForType(this.props.containers[0].type, 'black'))} width="20px" /> : null}
						<span className="model-name">{this.props.title}</span>
					</h1>
				</div>
				<div className="CanvasPanelBody panel-body" style={{marginRight: this.props.layout.right, bottom: this.props.layout.bottom}} >
					{hasNoCatalogs ? null : bodyComponent}
				</div>
				<CanvasZoom zoom={this.props.zoom} style={{bottom: this.props.layout.bottom + 20}}/>
				<CanvasPanelTray layout={this.props.layout} show={isEditingNSD}>
					<EditForwardingGraphPaths containers={this.props.containers} />
				</CanvasPanelTray>
			</div>
		);
	},
	onDragOver(event) {
		const isDraggingFiles = _.contains(event.dataTransfer.types, 'Files');
		if (!isDraggingFiles) {
			event.preventDefault();
			event.dataTransfer.dropEffect = 'copy';
		}
	},
	onDrop(event) {
		// given a drop event determine which action to take in the canvas:
		// open item or add item to an existing, already opened nsd
		// note: nsd is the only editable container
		const data = utils.parseJSONIgnoreErrors(event.dataTransfer.getData('text'));
		if (data.type === 'catalog-item') {
			this.handleDropCatalogItem(event, data);
		} else if (data.type === 'action') {
			this.handleDropCanvasAction(event, data);
		}
	},
	handleDropCanvasAction(event, data) {
		const action = cc.camel('on-' + data.action);
		if (typeof this[action] === 'function') {
			if (this[action]({clientX: event.clientX, clientY: event.clientY})) {
				event.preventDefault();
			}
		} else {
			console.warn(`no action defined for drop event ${data.action}. Did you forget to add CanvasPanel.${action}() event handler?`);
		}
	},
	handleDropCatalogItem(event, data) {
		let openItem = null;
		const currentItem = this.props.containers[0];
		if (data.item.uiState.type === 'nsd') {
			// if item is an nsd then open the descriptor in the canvas
			openItem = data.item;
			// if item is a vnfd or pnfd then check if the current item is an nsd
		} else if (DescriptorModelFactory.isNetworkService(currentItem)) {
			// so add the item to the nsd and re-render the canvas
			switch (data.item.uiState.type) {
			case 'vnfd':
				this.onAddVnfd(data.item, {clientX: event.clientX, clientY: event.clientY});
				break;
			case 'pnfd':
				this.onAddPnfd(data.item, {clientX: event.clientX, clientY: event.clientY});
				break;
			default:
				console.warn(`Unknown catalog-item type. Expect type "nsd", "vnfd" or "pnfd" but got ${data.item.uiState.type}.`);
			}
		} else {
			// otherwise the default action is to open the item
			openItem = data.item;
		}
		if (openItem) {
			event.preventDefault();
			CatalogItemsActions.editCatalogItem(openItem);
		}
	},
	onAddVdu(dropCoordinates) {
		const currentItem = this.props.containers[0];
		if (DescriptorModelFactory.isVirtualNetworkFunction(currentItem)) {
			const vdu = currentItem.createVdu();
			vdu.uiState.dropCoordinates = dropCoordinates;
			CatalogItemsActions.catalogItemDescriptorChanged(currentItem);
		}
	},
	onAddVld(dropCoordinates) {
		const currentItem = this.props.containers[0];
		if (DescriptorModelFactory.isNetworkService(currentItem) || DescriptorModelFactory.isVirtualNetworkFunction(currentItem)) {
			const vld = currentItem.createVld();
			vld.uiState.dropCoordinates = dropCoordinates;
			CatalogItemsActions.catalogItemDescriptorChanged(currentItem);
		}
	},
	onAddVnffgd(dropCoordinates) {
		const currentItem = this.props.containers[0];
		if (DescriptorModelFactory.isNetworkService(currentItem)) {
			const vld = currentItem.createVnffgd();
			vld.uiState.dropCoordinates = dropCoordinates;
			CatalogItemsActions.catalogItemDescriptorChanged(currentItem);
		}
	},
	onAddVnfd(model, dropCoordinates) {
		const currentItem = this.props.containers[0];
		if (DescriptorModelFactory.isNetworkService(currentItem) || DescriptorModelFactory.isVirtualNetworkFunction(currentItem)) {
			const vnfd = DescriptorModelFactory.newVirtualNetworkFunction(model);
			const cvnfd = currentItem.createConstituentVnfdForVnfd(vnfd);
			cvnfd.uiState.dropCoordinates = dropCoordinates;
			CatalogItemsActions.catalogItemDescriptorChanged(currentItem);
		}
	},
	onAddPnfd(model, dropCoordinates) {
		const currentItem = this.props.containers[0];
		if (DescriptorModelFactory.isNetworkService(currentItem)) {
			const pnfd = DescriptorModelFactory.newPhysicalNetworkFunction(model);
			pnfd.uiState.dropCoordinates = dropCoordinates;
			currentItem.createPnfd(pnfd);
			CatalogItemsActions.catalogItemDescriptorChanged(currentItem);
		}
	},
	onDblClickOpenFullScreen(event) {
		event.stopPropagation();
		ComposerAppActions.enterFullScreenMode();
	}
});

export default CanvasPanel;
