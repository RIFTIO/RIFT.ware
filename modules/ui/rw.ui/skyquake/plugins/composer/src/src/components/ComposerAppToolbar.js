
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import React from 'react'
import messages from './messages'
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import Button from './Button'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import CanvasEditorActions from '../actions/CanvasEditorActions'
import ComposerAppActions from '../actions/ComposerAppActions'
import SelectionManager from '../libraries/SelectionManager'
import DeletionManager from '../libraries/DeletionManager'

import '../styles/ComposerAppToolbar.scss'

import imgSave from '../../../node_modules/open-iconic/svg/data-transfer-upload.svg'
import imgCancel from '../../../node_modules/open-iconic/svg/circle-x.svg'
import imgLayout from '../../../node_modules/open-iconic/svg/grid-three-up.svg'
import imgVLD from '../../../node_modules/open-iconic/svg/link-intact.svg'
import imgJSONViewer from '../../../node_modules/open-iconic/svg/code.svg'
import imgFG from '../../../node_modules/open-iconic/svg/infinity.svg'
import imgDelete from '../../../node_modules/open-iconic/svg/trash.svg'
import imgVDU from '../../../node_modules/open-iconic/svg/laptop.svg'

const ComposerAppToolbar = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return {};
	},
	getDefaultProps() {
		return {
			disabled: true,
			showMore: false,
			layout: {left: 300},
			isModified: false,
			isEditingNSD: false,
			isEditingVNFD: false,
			showJSONViewer: false,
			isNew: false
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
	onClickSave(event) {
		event.preventDefault();
		event.stopPropagation();
		CatalogItemsActions.saveCatalogItem();
	},
	onClickCancel(event) {
		event.preventDefault();
		event.stopPropagation();
		CatalogItemsActions.cancelCatalogItemChanges();
	},
	onClickToggleShowMoreInfo(event) {
		event.preventDefault();
		event.stopPropagation();
		CanvasEditorActions.toggleShowMoreInfo();
	},
	onClickAutoLayout(event) {
		event.preventDefault();
		event.stopPropagation();
		CanvasEditorActions.applyDefaultLayout();
	},
	onClickAddVld(event) {
		event.preventDefault();
		event.stopPropagation();
		CanvasEditorActions.addVirtualLinkDescriptor();
	},
	onClickAddVnffg(event) {
		event.preventDefault();
		event.stopPropagation();
		CanvasEditorActions.addForwardingGraphDescriptor();
	},
	onClickAddVdu(event) {
		event.preventDefault();
		event.stopPropagation();
		CanvasEditorActions.addVirtualDeploymentDescriptor();
	},
	onDragStartAddVdu(event) {
		const data = {type: 'action', action: 'add-vdu'};
		event.dataTransfer.effectAllowed = 'copy';
		event.dataTransfer.setData('text', JSON.stringify(data));
		ComposerAppActions.setDragState(data);
	},
	onDragStartAddVld(event) {
		const data = {type: 'action', action: 'add-vld'};
		event.dataTransfer.effectAllowed = 'copy';
		event.dataTransfer.setData('text', JSON.stringify(data));
		ComposerAppActions.setDragState(data);
	},
	onDragStartAddVnffg(event) {
		const data = {type: 'action', action: 'add-vnffgd'};
		event.dataTransfer.effectAllowed = 'copy';
		event.dataTransfer.setData('text', JSON.stringify(data));
		ComposerAppActions.setDragState(data);
	},
	onClickDeleteSelected(event) {
		event.preventDefault();
		event.stopPropagation();
		DeletionManager.deleteSelected(event);
	},
	toggleJSONViewer(event) {
		event.preventDefault();
		if (this.props.showJSONViewer) {
			ComposerAppActions.closeJsonViewer();
		} else {
			ComposerAppActions.closeJsonViewer();
			ComposerAppActions.showJsonViewer.defer();
		}
	},
	render() {
		const style = {left: this.props.layout.left};
		const saveClasses = ClassNames('ComposerAppSave', {'primary-action': this.props.isModified || this.props.isNew});
		const cancelClasses = ClassNames('ComposerAppCancel', {'secondary-action': this.props.isModified});
		if (this.props.disabled) {
			return (
				<div className="ComposerAppToolbar" style={style}></div>
			);
		}
		const hasSelection = SelectionManager.getSelections().length > 0;
		return (
			<div className="ComposerAppToolbar" style={style}>
				{(()=>{
					if (this.props.isEditingNSD || this.props.isEditingVNFD) {
						return (
							<div className="FileActions">
								<Button className={saveClasses} onClick={this.onClickSave} label={messages.getSaveActionLabel(this.props.isNew)} src={imgSave} />
								<Button className={cancelClasses} onClick={this.onClickCancel} label="Cancel" src={imgCancel} />
								<Button className="ComposerAppToggleJSONViewerAction" onClick={this.toggleJSONViewer} label="JSON Viewer" src={imgJSONViewer} />
							</div>
						);
					}
				})()}
				<div className="LayoutActions">
					<Button className="action-auto-layout" onClick={this.onClickAutoLayout} label="Auto Layout" src={imgLayout} />
					{this.props.isEditingNSD ||
						this.props.isEditingVNFD ? <Button className="action-add-vld"
														   draggable="true"
														   label={this.props.isEditingNSD ? 'Add VLD' : 'Add IVLD'}
														   src={imgVLD}
														   onDragStart={this.onDragStartAddVld}
														   onClick={this.onClickAddVld} /> : null}
					{this.props.isEditingNSD ? <Button className="action-add-vnffg"
													   draggable="true"
													   label="Add VNFFG"
													   src={imgFG}
													   onDragStart={this.onDragStartAddVnffg}
													   onClick={this.onClickAddVnffg} /> : null}
					{this.props.isEditingVNFD ? <Button className="action-add-vdu"
														draggable="true"
														label="Add VDU"
														src={imgVDU}
														onDragStart={this.onDragStartAddVdu}
														onClick={this.onClickAddVdu} /> : null}
					<Button type="image" title="Delete selected items" className="action-delete-selected-items" disabled={!hasSelection} onClick = {this.onClickDeleteSelected} src={imgDelete} label="Delete" />
				</div>
			</div>
		);
	}
});

export default ComposerAppToolbar;
