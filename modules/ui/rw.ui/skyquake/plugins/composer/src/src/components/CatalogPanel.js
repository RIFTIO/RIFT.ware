
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import _ from 'lodash'
import React from 'react'
import ReactDOM from 'react-dom'
import messages from './messages'
import ClassNames from 'classnames'
import UploadDropZone from '../libraries/CatalogPackageManagerUploadDropZone'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import DropTarget from './DropTarget'
import DropZonePanel from './DropZonePanel'
import CatalogItems from './CatalogItems'
import CatalogFilter from './CatalogFilter'
import CatalogPanelTray from './CatalogPanelTray'
import CatalogPanelToolbar from './CatalogPanelToolbar'
import CatalogPackageManager from './CatalogPackageManager'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import CatalogPanelTrayActions from '../actions/CatalogPanelTrayActions'
import ComposerAppActions from '../actions/ComposerAppActions'
import ComposerAppStore from '../stores/ComposerAppStore'
import CatalogPanelStore from '../stores/CatalogPanelStore'
import LoadingIndicator from './LoadingIndicator'
import SelectionManager from '../libraries/SelectionManager'

import '../styles/CatalogPanel.scss'

const createDropZone = function (action, clickable, dropTarget) {
	const dropZone = new UploadDropZone(ReactDOM.findDOMNode(dropTarget), [clickable], action);
	dropZone.on('dragover', this.onDragOver);
	dropZone.on('dragend', this.onDragEnd);
	dropZone.on('addedfile', this.onFileAdded);
	return dropZone;
};

const uiTransientState = {
	isDrop: false,
	isDragging: false,
	isDraggingFiles: false,
	timeoutId: 0
};

const CatalogPanel = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return CatalogPanelStore.getState();
	},
	getDefaultProps() {
	},
	componentWillMount() {
	},
	componentDidMount() {
		CatalogPanelStore.listen(this.onChange);
		document.body.addEventListener('dragover', this.onDragOver);
		document.body.addEventListener('dragend', this.onDragEnd);
		window.addEventListener('dragend', this.onDragEnd);
	},
	componentDidUpdate() {
	},
	componentWillUnmount() {
		CatalogPanelStore.unlisten(this.onChange);
		document.body.removeEventListener('dragover', this.onDragOver);
		document.body.removeEventListener('dragend', this.onDragEnd);
		window.removeEventListener('dragend', this.onDragEnd);
	},
	render() {

		const onDropCatalogItem = e => {
			e.preventDefault();
			clearTimeout(uiTransientState.timeoutId);
			uiTransientState.isDragging = false;
			uiTransientState.isDrop = true;
			const item = JSON.parse(e.dataTransfer.getData('text')).item;
			CatalogItemsActions.exportSelectedCatalogItems(item);
			CatalogPanelTrayActions.open();
		};

		const onDropUpdatePackage = e => {
			e.preventDefault();
			clearTimeout(uiTransientState.timeoutId);
			uiTransientState.isDragging = false;
			uiTransientState.isDrop = true;
			CatalogPanelTrayActions.open();
		};

		const onDropOnboardPackage = e => {
			e.preventDefault();
			clearTimeout(uiTransientState.timeoutId);
			uiTransientState.isDragging = false;
			uiTransientState.isDrop = true;
			CatalogPanelTrayActions.open();
		};

		const isDraggingItem = uiTransientState.isDragging && !uiTransientState.isDraggingFiles;
		const isDraggingFiles = uiTransientState.isDragging && uiTransientState.isDraggingFiles;
		const updateDropZone = createDropZone.bind(this, UploadDropZone.ACTIONS.update, '.action-update-catalog-package');
		const onboardDropZone = createDropZone.bind(this, UploadDropZone.ACTIONS.onboard, '.action-onboard-catalog-package');
		const className = ClassNames('CatalogPanel', {'-is-tray-open': this.state.isTrayOpen});
		const hasNoCatalogs = this.props.hasNoCatalogs;
		const isLoading = this.props.isLoading;
		return (
			<div className={className} data-resizable="right" data-resizable-handle-offset="0 6" style={{width: this.props.layout.left}}>
				<CatalogPanelToolbar />
				<div className="CatalogPanelBody">
					{(() => {
						if (isLoading) {
							return (
								<div className="LoaderWrapper">
									<LoadingIndicator />
								</div>
							)
						}
						if (hasNoCatalogs) {
							return messages.catalogWelcome;
						}
						return (
							<div>
								<CatalogFilter filterByType={this.props.filterByType} />
								<CatalogItems filterByType={this.props.filterByType} />
							</div>
						);
					})()}
				</div>
				<CatalogPanelTray show={this.state.isTrayOpen}>
					<DropZonePanel show={isDraggingItem} title="Drop catalog item to export.">
						<DropTarget className="action-export-catalog-items" onDrop={onDropCatalogItem}>
							<span>Export catalog item.</span>
						</DropTarget>
					</DropZonePanel>
					<DropZonePanel show={isDraggingFiles}>
						<DropTarget className="action-onboard-catalog-package" dropZone={onboardDropZone} onDrop={onDropOnboardPackage}>
							<span>On-board new catalog package.</span>
						</DropTarget>
						<DropTarget className="action-update-catalog-package" dropZone={updateDropZone} onDrop={onDropUpdatePackage}>
							<span>Update existing catalog package.</span>
						</DropTarget>
					</DropZonePanel>
					<CatalogPackageManager />
				</CatalogPanelTray>
			</div>
		);

	},
	onChange(state) {
		this.setState(state);
	},
	onDragOver(e) {
		// NOTE do not call preventDefault here - see DropTarget
		if (!uiTransientState.isDragging) {
			uiTransientState.isDrop = false;
			uiTransientState.isDragging = true;
			uiTransientState.wasTrayOpen = this.state.isTrayOpen;
			uiTransientState.isDraggingFiles = _.contains(e.dataTransfer.types, 'Files');
			const dragState = ComposerAppStore.getState().drag || {};
			if (uiTransientState.isDraggingFiles || (dragState.type === 'catalog-item')) {
				CatalogPanelTrayActions.open();
			}
		}
		e.dataTransfer.dropEffect = 'none';
		// the drag-end event does not fire on drag events that originate
		// outside of the browser, e.g. dragging files from desktop, so
		// we use this debounced callback to simulate a drag-end event
		clearTimeout(uiTransientState.timeoutId);
		uiTransientState.timeoutId = setTimeout(() => {
			this.onDragEnd();
		}, 400);
	},
	onDragEnd() {
		clearTimeout(uiTransientState.timeoutId);
		if (uiTransientState.isDragging) {
			uiTransientState.isDragging = false;
			if (uiTransientState.isDrop || uiTransientState.wasTrayOpen) {
				CatalogPanelTrayActions.open();
			} else {
				CatalogPanelTrayActions.close();
			}
		}
	},
	onFileAdded() {
		CatalogPanelTrayActions.open();
	}
});

export default CatalogPanel;
